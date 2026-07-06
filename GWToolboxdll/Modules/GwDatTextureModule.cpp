#include "stdafx.h"

#include <cstring>
#include <mutex>

#include <Logger.h>
#include "GwDatTextureModule.h"

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include "Resources.h"
#include <Utils/ArenaNetFileParser.h>
#include <Utils/GwDat/GwDatArchive.h>
#include <Utils/GwDat/AtexReader.h>

#include <DirectXTex.h>

namespace {

    // Asks the client to fetch a file id (dat or network). Found via its unique DOWNLOAD_FLAG assert
    // string (survives client updates); safe in-game and enqueue-only - the client pumps its own queue.
    using RequestFiles_pt = void(__cdecl*)(uint32_t count, const uint32_t* file_ids, uint32_t flags);
    // 0 = non-cancelable: client frees its own request batch, fire-and-forget. Don't add a cancelable
    // flag (0x1/0x10000) - that returns a handle that leaks unless released.
    constexpr uint32_t kTriggerFlags = 0;
    RequestFiles_pt RequestFiles_Func = nullptr;

    // Flush the download queue so the request is sent now instead of at the next map change - it just
    // clears one bit of a client flag (what the wrapper's 0x2 flag does after enqueuing). The 8-byte
    // 'AND dword[flag],~1; RET' is unique in the client; best-effort, null just falls back to the
    // client's own pump.
    using FlushDownloads_pt = void(__cdecl*)();
    FlushDownloads_pt FlushDownloads_Func = nullptr;

    void WireGameFileTrigger()
    {
        RequestFiles_Func = reinterpret_cast<RequestFiles_pt>(GW::Scanner::ToFunctionStart(
            GW::Scanner::FindUseOfString("!(flags & DOWNLOAD_FLAG_LOW_PRIORITY) || !ITrackIsLoadingScreenActive()"), 0xfff));
        if (!RequestFiles_Func) {
            Log::Log("[GwDat] game file-request trigger not found; async reads will wait passively.");
            return;
        }
        FlushDownloads_Func = reinterpret_cast<FlushDownloads_pt>(
            GW::Scanner::Find("\x83\x25\x00\x00\x00\x00\xfe\xc3", "xx????xx", 0));
        Log::Log("[GwDat] game file-request trigger resolved at %p (flush %s)", reinterpret_cast<void*>(RequestFiles_Func),
                 FlushDownloads_Func ? "ok" : "missing");
        GwDatArchive::Instance().SetTrigger([](uint32_t file_id) {
            GW::GameThread::Enqueue([file_id] { // touches client state; run on the game thread
                if (!RequestFiles_Func)
                    return;
                RequestFiles_Func(1, &file_id, kTriggerFlags);
                if (FlushDownloads_Func)
                    FlushDownloads_Func(); // send now, don't wait for a map change
            });
        });
    }

    struct Vec2i {
        int x = 0;
        int y = 0;
    };

    // Decodes a DDS payload (raw or block-compressed) to tightly-packed A8R8G8B8.
    bool DecodeDdsToArgb(const uint8_t* bytes, size_t size, std::vector<uint32_t>& argb, Vec2i& dims)
    {
        DirectX::ScratchImage loaded;
        if (FAILED(DirectX::LoadFromDDSMemory(bytes, size, DirectX::DDS_FLAGS_NONE, nullptr, loaded)))
            return false;

        DirectX::ScratchImage converted;
        const auto& meta = loaded.GetMetadata();
        const auto hr = DirectX::IsCompressed(meta.format)
            ? DirectX::Decompress(loaded.GetImages(), loaded.GetImageCount(), meta, DXGI_FORMAT_B8G8R8A8_UNORM, converted)
            : DirectX::Convert(loaded.GetImages(), loaded.GetImageCount(), meta, DXGI_FORMAT_B8G8R8A8_UNORM,
                               DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr))
            return false;

        const DirectX::Image* img = converted.GetImage(0, 0, 0);
        if (!img || !img->width || !img->height)
            return false;

        dims.x = static_cast<int>(img->width);
        dims.y = static_cast<int>(img->height);
        argb.resize(img->width * img->height);
        for (size_t y = 0; y < img->height; ++y) {
            // B8G8R8A8 is byte-identical to D3DFMT_A8R8G8B8, so copy each row straight across.
            memcpy(&argb[y * img->width], img->pixels + y * img->rowPitch, img->width * 4);
        }
        return true;
    }

    // Decodes file_id to A8R8G8B8 (ATEX/ATTX, ffna inline DXT chunk, or DDS); stream_id picks a stream.
    bool DecodeTextureToArgb(uint32_t file_id, std::vector<uint32_t>& argb, Vec2i& dims, uint32_t stream_id = 0)
    {
        ArenaNetFileParser::GameAssetFile asset;
        if (!asset.readFromDat(file_id, stream_id))
            return false;

        uint8_t* image_bytes = asset.data.data();
        size_t image_size = asset.data.size();
        if (image_size < 4)
            return false;

        // Model files (ffna) carry the texture as an inline DXT3 chunk.
        if (memcmp(image_bytes, "ffna", 4) == 0) {
            const auto anet_file = reinterpret_cast<ArenaNetFileParser::ArenaNetFile*>(&asset);
            if (!anet_file->isValid())
                return false;
            const auto found = anet_file->FindChunk(ArenaNetFileParser::ChunkType::FA3_InlineTextureDXT3);
            if (!found)
                return false;
            const auto chunk = reinterpret_cast<const ArenaNetFileParser::UnknownChunk*>(found);
            image_bytes = const_cast<uint8_t*>(chunk->data);
            image_size = chunk->chunk_size;
            if (image_size < 4)
                return false;
        }

        if (memcmp(image_bytes, "DDS", 3) == 0)
            return DecodeDdsToArgb(image_bytes, image_size, argb, dims);

        if (memcmp(image_bytes, "ATEX", 4) != 0 && memcmp(image_bytes, "ATTX", 4) != 0)
            return false;

        const DatTexture tex = ProcessImageFile(image_bytes, static_cast<int>(image_size));
        if (tex.width <= 0 || tex.height <= 0 || tex.rgba_data.empty())
            return false;

        dims.x = tex.width;
        dims.y = tex.height;
        // The decoder's RGBA is really B8G8R8A8 in memory, matching D3DFMT_A8R8G8B8, so copy as-is.
        argb.resize(static_cast<size_t>(tex.width) * tex.height);
        for (size_t i = 0; i < argb.size() && i < tex.rgba_data.size(); ++i)
            argb[i] = tex.rgba_data[i].dw;
        return true;
    }

    // Uploads a tightly-packed A8R8G8B8 buffer into a fresh managed D3D texture.
    IDirect3DTexture9* MakeTextureFromArgb(IDirect3DDevice9* device, const std::vector<uint32_t>& argb, const Vec2i& dims)
    {
        if (!device || dims.x <= 0 || dims.y <= 0 || argb.size() < static_cast<size_t>(dims.x) * dims.y)
            return nullptr;

        IDirect3DTexture9* tex = nullptr;
        if (device->CreateTexture(dims.x, dims.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK)
            return nullptr;

        D3DLOCKED_RECT rect;
        if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK) {
            tex->Release();
            return nullptr;
        }
        const uint32_t* srcdata = argb.data();
        for (int y = 0; y < dims.y; y++) {
            memcpy((uint8_t*)rect.pBits + y * rect.Pitch, srcdata, dims.x * 4);
            srcdata += dims.x;
        }
        tex->UnlockRect(0);
        return tex;
    }

    // Per-dye linear recolour (index = GW::DyeColor - Blue): 3x3 matrix m0..m8 (out_c = row.RGB) + neutral bias m9, fitted to the client's dyed icon exports.
    constexpr float kDyeColorMatrix[12][10] = {
        {  0.0063f,  1.5704f, -0.8800f,  0.2402f,  0.3491f,  0.1148f,  0.4809f, -0.2126f,  0.4076f, -1.7549f }, // Blue
        {  0.0400f,  1.5283f, -0.9086f,  0.5069f,  0.2335f, -0.1058f,  0.0880f,  0.4994f,  0.0077f, -2.5436f }, // Green
        {  0.3772f,  1.1171f, -0.5635f,  0.0830f,  0.7292f,  0.0987f,  0.4554f, -0.3237f,  0.7615f, -1.6173f }, // Purple
        {  0.7145f,  0.5876f, -0.5985f, -0.0546f,  1.2018f, -0.4863f, -0.0552f,  0.3371f,  0.3935f, -1.9912f }, // Red
        {  1.0416f, -0.3918f,  0.0173f,  0.6754f,  0.7037f, -0.8175f, -0.0567f,  0.3604f,  0.2142f, -2.3883f }, // Yellow
        {  0.4120f,  0.9786f, -0.6260f,  0.1970f,  0.7051f, -0.1848f,  0.0856f,  0.2242f,  0.4018f, -1.7041f }, // Brown
        {  0.8177f,  0.2268f, -0.2246f,  0.3377f,  0.7852f, -0.3681f, -0.1749f,  0.5605f,  0.3787f, -2.0703f }, // Orange
        {  0.4087f,  0.8914f, -0.1474f,  0.4254f,  0.2711f,  0.4090f,  0.4590f, -0.1482f,  0.7493f, -0.3684f }, // Silver
        {  0.1580f,  1.5613f, -1.2757f,  0.1257f,  0.9275f, -0.6480f,  0.1152f,  0.4434f, -0.1769f, -5.8428f }, // Black
        {  0.3537f,  1.0440f, -0.4641f,  0.2955f,  0.4642f,  0.1454f,  0.2728f,  0.0251f,  0.5898f, -1.6518f }, // Gray
        {  0.7139f, -0.3890f,  0.8050f,  0.6868f, -0.7796f,  1.1579f,  0.6846f, -1.0262f,  1.3027f, 58.4929f }, // White
        {  1.8355f, -2.6588f,  2.6553f,  0.0835f,  0.6695f,  0.4196f,  0.3850f, -0.8128f,  1.8695f, 36.8231f }, // Pink
    };

    // Item icon to A8R8G8B8: stream 1 base, stream 0xc dye mask; masked pixels use the averaged dye matrices (`dyes` = up to 4 GW::DyeColor, one per byte).
    bool DecodeItemToArgb(uint32_t file_id, uint32_t dyes, std::vector<uint32_t>& base, Vec2i& dims)
    {
        if (!file_id || !DecodeTextureToArgb(file_id, base, dims, 1) || !dims.x || !dims.y)
            return false;

        // Average the applied dye slots' matrices into one combined transform.
        float M[10] = {};
        int applied = 0;
        for (int slot = 0; slot < 4; ++slot) {
            const uint32_t d = (dyes >> (slot * 8)) & 0xFF;
            if (d >= 2 && d <= 13) {
                const float* const K = kDyeColorMatrix[d - 2];
                for (int j = 0; j < 10; ++j)
                    M[j] += K[j];
                ++applied;
            }
        }

        std::vector<uint32_t> mask;
        Vec2i mask_dims;
        if (applied && DecodeTextureToArgb(file_id, mask, mask_dims, 0xc) &&
            mask_dims.x == dims.x && mask_dims.y == dims.y && mask.size() == base.size()) {
            for (float& v : M)
                v /= static_cast<float>(applied);
            const float bias = M[9]; // neutral: added equally to R,G,B
            const auto clamp8 = [](float v) -> uint32_t {
                return static_cast<uint32_t>(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v);
            };
            for (size_t i = 0; i < base.size(); ++i) {
                const float m = (mask[i] & 0xFF) / 255.0f; // DXTA mask is greyscale; any channel is the strength
                const uint32_t c = base[i];
                const float r = static_cast<float>((c >> 16) & 0xFF);
                const float g = static_cast<float>((c >> 8) & 0xFF);
                const float b = static_cast<float>(c & 0xFF);
                const float nr = M[0] * r + M[1] * g + M[2] * b + bias;
                const float ng = M[3] * r + M[4] * g + M[5] * b + bias;
                const float nb = M[6] * r + M[7] * g + M[8] * b + bias;
                const auto mix = [&](float base_c, float dyed) -> uint32_t {
                    return clamp8(base_c * (1.0f - m) + dyed * m);
                };
                base[i] = (c & 0xFF000000) | (mix(r, nr) << 16) | (mix(g, ng) << 8) | mix(b, nb);
            }
        }
        return true;
    }

    // Decodes file_id and converts it to greyscale A8R8G8B8 in place (worker-thread safe; no device).
    bool DecodeGreyscaleToArgb(uint32_t file_id, std::vector<uint32_t>& argb, Vec2i& dims)
    {
        if (!file_id || !DecodeTextureToArgb(file_id, argb, dims) || !dims.x || !dims.y)
            return false;
        for (uint32_t& c : argb) {
            const uint8_t r = (c >> 16) & 0xFF;
            const uint8_t g = (c >> 8) & 0xFF;
            const uint8_t b = c & 0xFF;
            const uint8_t a = (c >> 24) & 0xFF;
            const uint8_t grey = static_cast<uint8_t>(r * 0.299f + g * 0.587f + b * 0.114f);
            c = (a << 24) | (grey << 16) | (grey << 8) | grey;
        }
        return true;
    }

    struct GwImg {
        uint32_t m_file_id = 0;
        uint32_t m_stream_id = 0;
        uint32_t m_dyes = 0;
        Vec2i m_dims;
        IDirect3DTexture9* m_tex = nullptr;
        bool m_pending = false; // a decode has been dispatched (async); don't dispatch twice
        explicit GwImg(uint32_t file_id, uint32_t stream_id = 0, uint32_t dyes = 0)
            : m_file_id(file_id), m_stream_id(stream_id), m_dyes(dyes) {}
        ~GwImg()
        {
            if (m_tex) {
                m_tex->Release();
                m_tex = nullptr;
            }
        }
    };

    // One async attempt per image; ReadFileAsync owns the wait/retry/trigger and the ~1 min timeout.
    bool WantsDecode(const GwImg* img) { return !img->m_tex && !img->m_pending; }

    struct ArgbImage {
        std::vector<uint32_t> pixels;
        Vec2i dims;
    };

    // Waits for file_id/stream_id to be resident (fetching it if the client must stream it), decodes on
    // the dat worker, then uploads on the DX thread. Gating on one stream is enough - a file's streams
    // arrive together, so multi-stream decoders (item dye masks) find theirs too.
    template <typename Decode>
    void QueueDecode(GwImg* img, uint32_t file_id, uint32_t stream_id, Decode decode)
    {
        img->m_pending = true;
        GwDatArchive::Instance().ReadFileAsync(file_id, [img, decode](std::vector<uint8_t>& bytes) {
            auto result = std::make_shared<ArgbImage>();
            const bool ok = !bytes.empty() && decode(result->pixels, result->dims) && result->dims.x > 0 && result->dims.y > 0;
            Resources::Instance().EnqueueDxTask([img, result, ok](IDirect3DDevice9* device) {
                img->m_tex = ok ? MakeTextureFromArgb(device, result->pixels, result->dims) : nullptr;
                img->m_dims = result->dims;
            });
        }, stream_id);
    }

    // Keyed by (stream_id << 32 | file_id) so different streams of one file cache separately.
    uint64_t TextureKey(uint32_t file_id, uint32_t stream_id) { return (static_cast<uint64_t>(stream_id) << 32) | file_id; }

    std::map<uint64_t, GwImg*> textures_by_file_id;
    std::map<uint32_t, GwImg*> greyscale_textures_by_file_id;
    std::map<uint64_t, GwImg*> item_images_by_file_id; // key = dyes << 32 | model_file_id

    // Cached GwImg for key, created from the given GwImg ctor args if absent.
    template <typename Map, typename Key, typename... Args>
    GwImg* GetOrCreate(Map& map, Key key, Args... args)
    {
        const auto found = map.find(key);
        return found != map.end() ? found->second : (map[key] = new GwImg(args...));
    }
} // namespace

IDirect3DTexture9** GwDatTextureModule::LoadGreyscaleTextureFromFileId(uint32_t file_id)
{
    GwImg* img = GetOrCreate(greyscale_textures_by_file_id, file_id, file_id);
    if (WantsDecode(img))
        QueueDecode(img, file_id, 0, [file_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeGreyscaleToArgb(file_id, argb, dims);
        });
    return &img->m_tex;
}

bool GwDatTextureModule::ReadDatFile(const wchar_t* file_name, std::vector<uint8_t>* bytes_out, uint32_t stream_id)
{
    if (!(file_name && *file_name && bytes_out))
        return false;
    const uint32_t file_id = ArenaNetFileParser::FileHashToFileId(file_name);
    if (!file_id)
        return false;
    auto& dat = GwDatArchive::Instance();
    const bool ok = dat.ReadFile(file_id, *bytes_out, stream_id);
    if (dat.Loaded()) {
        static std::once_flag reported; // log the bound archive once for support reports
        std::call_once(reported, [&dat] {
            Log::LogW(L"[GwDat] Mapped client's Gw.dat ('%s')", dat.DatPath().c_str());
        });
    }
    else if (dat.HandleEnumerationBlocked()) {
        // Handle scan blocked (almost always AV/anti-cheat) - tell the user once.
        static std::once_flag warned;
        std::call_once(warned, [] {
            Log::Warning("GWToolbox can't read Gw.dat: it was blocked from listing its own file handles, "
                         "usually by anti-virus or anti-cheat. In-game images won't load until it's allowed.");
            Log::Log("[GwDat] NtQuerySystemInformation returned no data - handle enumeration blocked (AV/anti-cheat?).");
        });
    }
    else {
        static std::once_flag noted; // dat handle not open yet; transient, note once
        std::call_once(noted, [] {
            Log::Log("[GwDat] Gw.dat not mapped yet (client handle not found); will keep retrying.");
        });
    }
    return ok;
}

void GwDatTextureModule::Initialize()
{
    ToolboxModule::Initialize();
    // The dat is indexed lazily on the first read; start its dedicated read/decode worker.
    GwDatArchive::Instance().StartWorker();
    WireGameFileTrigger(); // lets async reads tickle the client to fetch streamed-only files
}

IDirect3DTexture9** GwDatTextureModule::LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id)
{
    GwImg* img = GetOrCreate(textures_by_file_id, TextureKey(file_id, stream_id), file_id, stream_id);
    if (WantsDecode(img))
        QueueDecode(img, file_id, stream_id, [file_id, stream_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeTextureToArgb(file_id, argb, dims, stream_id);
        });
    return &img->m_tex;
}

IDirect3DTexture9** GwDatTextureModule::LoadItemImage(uint32_t model_file_id, uint32_t dyes)
{
    const uint64_t key = (static_cast<uint64_t>(dyes) << 32) | model_file_id;
    GwImg* img = GetOrCreate(item_images_by_file_id, key, model_file_id, 1u, dyes);
    if (WantsDecode(img))
        QueueDecode(img, model_file_id, 1, [model_file_id, dyes](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeItemToArgb(model_file_id, dyes, argb, dims);
        });
    return &img->m_tex;
}

void GwDatTextureModule::SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path)
{
    if (!file_id)
        return;
    // Wait for the file (fetching it if streamed-only), then decode + block-compress + write, all on
    // the dat worker off the render loop.
    GwDatArchive::Instance().ReadFileAsync(file_id, [file_id, file_path](std::vector<uint8_t>& bytes) {
        if (bytes.empty())
            return;
        std::vector<uint32_t> argb;
        Vec2i dims;
        if (!DecodeTextureToArgb(file_id, argb, dims) || !dims.x || !dims.y)
            return;

        // Decoded pixels are A8R8G8B8 (D3D byte order is BGRA).
        DirectX::Image src = {};
        src.width = dims.x;
        src.height = dims.y;
        src.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        src.rowPitch = static_cast<size_t>(dims.x) * 4;
        src.slicePitch = src.rowPitch * dims.y;
        src.pixels = reinterpret_cast<uint8_t*>(argb.data());
        DirectX::ScratchImage scratch;
        if (!SUCCEEDED(scratch.InitializeFromImage(src)))
            return;

        // BC1 encodes color better than BC3, so prefer it unless real alpha must be preserved.
        const auto target = scratch.IsAlphaAllOpaque() ? DXGI_FORMAT_BC1_UNORM : DXGI_FORMAT_BC3_UNORM;

        DirectX::ScratchImage compressed;
        if (SUCCEEDED(DirectX::Compress(src, target, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressed))) {
            DirectX::SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DirectX::DDS_FLAGS_NONE, file_path.c_str());
        }
    });
}

void GwDatTextureModule::Terminate()
{
    // Stop the worker before freeing the cache so no in-flight decode touches a deleted GwImg.
    GwDatArchive::Instance().StopWorker();

    for (auto gwimg_ptr : textures_by_file_id) {
        delete gwimg_ptr.second;
    }
    for (auto gwimg_ptr : greyscale_textures_by_file_id) {
        delete gwimg_ptr.second;
    }
    for (auto gwimg_ptr : item_images_by_file_id) {
        delete gwimg_ptr.second;
    }
    textures_by_file_id.clear();
    greyscale_textures_by_file_id.clear();
    item_images_by_file_id.clear();
}
