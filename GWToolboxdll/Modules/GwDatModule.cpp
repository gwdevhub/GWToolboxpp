#include "stdafx.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <utility>

#include <Logger.h>
#include "GwDatModule.h"

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include "Resources.h"
#include <Utils/ArenaNetFileParser.h>
#include <Utils/GwDat/AtexReader.h>
#include <Utils/GwDat/xentax.h>

#include <DirectXTex.h>

namespace {

    // Asks the client to queue a file id for download (the same FUN_0082da30 the client uses for its
    // own preloads). Resolved from a unique call site via FunctionFromNearCall, which survives byte
    // shifts better than a prologue scan.
    using RequestFiles_pt = void(__cdecl*)(uint32_t count, const uint32_t* file_ids, uint32_t flags);
    // 0x2 = enqueue + FLUSH. In the wrapper (FUN_0082da30), only `flags & 2` calls FUN_00832f00(), the
    // flush that actually kicks the download queue on-the-fly; without it the id is enqueued but never
    // fetched until the client flushes on its own (e.g. a map transition). We avoid 0x8 (asserts unless
    // the download system is in a specific state), 0x10000/0x1 (cancel-tracking - returns a handle we'd
    // leak, and 0x10001 hits the DAT_01075280 "offline" bail-out). 0x2 enqueues at priority 1, no leak.
    constexpr uint32_t kTriggerFlags = 0x2;
    RequestFiles_pt RequestFiles_Func = nullptr;

    void ResolveRequestFn()
    {
        // PUSH 0x10008; PUSH EAX; PUSH [EBP-4]; CALL <wrapper> - the ...E8 near call resolves the target.
        const uintptr_t call_addr = GW::Scanner::Find("\x68\x08\x00\x01\x00\x50\xff\x75\xfc\xe8", "xxxxxxxxxx", 9);
        RequestFiles_Func = call_addr ? reinterpret_cast<RequestFiles_pt>(GW::Scanner::FunctionFromNearCall(call_addr)) : nullptr;
        if (RequestFiles_Func)
            Log::Log("[GwDat] game file-request trigger resolved at %p", reinterpret_cast<void*>(RequestFiles_Func));
        else
            Log::Log("[GwDat] game file-request trigger not found; async reads will wait passively.");
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

    // Dispatch once: ReadFileAsync has no timeout, so it keeps retrying the read (and re-requesting the
    // file) until it resolves, then delivers - an image whose file streams in later (e.g. on a map
    // change) still loads without us re-dispatching here.
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
        GwDatModule::ReadFileAsync(file_id, [img, decode](std::vector<uint8_t>& bytes) {
            auto result = std::make_shared<ArgbImage>();
            const bool ok = !bytes.empty() && decode(result->pixels, result->dims) && result->dims.x > 0 && result->dims.y > 0;
            Resources::Instance().EnqueueDxTask([img, result, ok](IDirect3DDevice9* device) {
                img->m_tex = ok ? MakeTextureFromArgb(device, result->pixels, result->dims) : nullptr;
                img->m_dims = result->dims;
                // Delivered (the file resolved): leave m_pending set so we don't re-decode - if the
                // decode itself failed on present bytes, retrying would just loop on the same data.
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

IDirect3DTexture9** GwDatModule::LoadGreyscaleTextureFromFileId(uint32_t file_id)
{
    GwImg* img = GetOrCreate(greyscale_textures_by_file_id, file_id, file_id);
    if (WantsDecode(img))
        QueueDecode(img, file_id, 0, [file_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeGreyscaleToArgb(file_id, argb, dims);
        });
    return &img->m_tex;
}

bool GwDatModule::ReadDatFile(const wchar_t* file_name, std::vector<uint8_t>* bytes_out, uint32_t stream_id)
{
    if (!(file_name && *file_name && bytes_out))
        return false;
    const uint32_t file_id = ArenaNetFileParser::FileHashToFileId(file_name);
    if (!file_id)
        return false;
    const bool ok = ReadFile(file_id, *bytes_out, stream_id);
    auto& dat = Instance(); // for the diagnostics below
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

void GwDatModule::Initialize()
{
    ToolboxModule::Initialize();
    // The dat is indexed lazily on the first read; start the async-read poll worker.
    StartWorker();
    // Let a read miss tickle the client to fetch streamed-only files: it batches the missing ids here,
    // and we send them (on the game thread) via the client's request+flush wrapper.
    ResolveRequestFn();
    if (RequestFiles_Func)
        SetTrigger([](const uint32_t* ids, size_t count) {
            if (!count)
                return;
            std::vector<uint32_t> batch(ids, ids + count); // copy for the game-thread lambda
            GW::GameThread::Enqueue([batch = std::move(batch)] {
                if (RequestFiles_Func && !batch.empty())
                    RequestFiles_Func(static_cast<uint32_t>(batch.size()), batch.data(), kTriggerFlags);
            });
        });
}

IDirect3DTexture9** GwDatModule::LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id)
{
    GwImg* img = GetOrCreate(textures_by_file_id, TextureKey(file_id, stream_id), file_id, stream_id);
    if (WantsDecode(img))
        QueueDecode(img, file_id, stream_id, [file_id, stream_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeTextureToArgb(file_id, argb, dims, stream_id);
        });
    return &img->m_tex;
}

IDirect3DTexture9** GwDatModule::LoadItemImage(uint32_t model_file_id, uint32_t dyes)
{
    const uint64_t key = (static_cast<uint64_t>(dyes) << 32) | model_file_id;
    GwImg* img = GetOrCreate(item_images_by_file_id, key, model_file_id, 1u, dyes);
    if (WantsDecode(img))
        QueueDecode(img, model_file_id, 1, [model_file_id, dyes](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeItemToArgb(model_file_id, dyes, argb, dims);
        });
    return &img->m_tex;
}

void GwDatModule::SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path)
{
    if (!file_id)
        return;
    // Wait for the file (fetching it if streamed-only), then decode + block-compress + write, all on
    // the dat worker off the render loop.
    GwDatModule::ReadFileAsync(file_id, [file_id, file_path](std::vector<uint8_t>& bytes) {
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

void GwDatModule::Terminate()
{
    // Stop the worker before freeing the cache so no in-flight decode touches a deleted GwImg.
    StopWorker();

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

// ============================================================================
// Archive reader (dat mapping, MFT parsing, decompression, async reads, tickle)
// ============================================================================
namespace {
    const uint8_t kDatMagic[4] = {0x33, 0x41, 0x4e, 0x1a}; // "3ANa"
    constexpr size_t kMaxView = 0x800000;                  // cap each mapped window at 8 MB
    constexpr uint32_t kRefreshThrottleMs = 2000;          // min gap between re-parses on a read miss
    constexpr uint32_t kAsyncPollMs = 250;                 // worker re-scan cadence for pending async reads
    constexpr uint32_t kTickleThrottleMs = 10000;          // re-ask the client for a given missed file at most this often

    DWORD AllocationGranularity()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwAllocationGranularity;
    }

    // Read via mapped views, never ReadFile: the client's dat handle is bound to its I/O port.
    bool ReadAt(HANDLE mapping, int64_t offset, void* buffer, size_t count)
    {
        static const DWORD gran = AllocationGranularity();
        auto* dst = static_cast<uint8_t*>(buffer);
        while (count > 0) {
            const int64_t base = offset - (offset % gran);
            const size_t delta = static_cast<size_t>(offset - base);
            const size_t chunk = count < kMaxView ? count : kMaxView;
            void* view = MapViewOfFile(mapping, FILE_MAP_READ,
                                       static_cast<DWORD>((static_cast<uint64_t>(base) >> 32) & 0xFFFFFFFF),
                                       static_cast<DWORD>(base & 0xFFFFFFFF), delta + chunk);
            if (!view)
                return false;
            memcpy(dst, static_cast<uint8_t*>(view) + delta, chunk);
            UnmapViewOfFile(view);
            dst += chunk;
            offset += static_cast<int64_t>(chunk);
            count -= chunk;
        }
        return true;
    }

    // Full DOS path of a handle (empty on failure); length-sized so installs past MAX_PATH resolve.
    std::wstring HandlePath(HANDLE file)
    {
        const DWORD needed = GetFinalPathNameByHandleW(file, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!needed)
            return {};
        std::wstring path(needed, L'\0');
        const DWORD n = GetFinalPathNameByHandleW(file, path.data(), needed, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!n || n >= needed)
            return {};
        path.resize(n);
        return path;
    }

    // Match by extension, not "Gw.dat", so a renamed/-dat-relocated archive resolves (MapDat checks the magic).
    bool HasDatExtension(const std::wstring& path)
    {
        return path.ends_with(L".dat") || path.ends_with(L".snapshot");
    }

    // Whole-file read-only mapping, or nullptr unless the contents start with the 3ANa magic.
    HANDLE MapDat(HANDLE file)
    {
        const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping)
            return nullptr;
        void* head = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(kDatMagic));
        const bool is_dat = head && memcmp(head, kDatMagic, sizeof(kDatMagic)) == 0;
        if (head)
            UnmapViewOfFile(head);
        if (is_dat)
            return mapping;
        CloseHandle(mapping);
        return nullptr;
    }

    // NtQuerySystemInformation(SystemExtendedHandleInformation) enumerates our real open handles.
    constexpr ULONG kSystemExtendedHandleInformation = 64;
    constexpr ULONG kStatusInfoLengthMismatch = 0xC0000004UL;

    struct SystemHandleEntryEx {
        void* Object;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR HandleValue;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
    };
    struct SystemHandleInfoEx {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        SystemHandleEntryEx Handles[1];
    };
    using NtQuerySystemInformation_t = LONG(__stdcall*)(ULONG, void*, ULONG, ULONG*);

    // Snapshots this process's handle table. Empty on failure.
    std::vector<uint8_t> QueryProcessHandles()
    {
        const auto ntdll = GetModuleHandleW(L"ntdll.dll");
        const auto NtQuerySystemInformation =
            ntdll ? reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(ntdll, "NtQuerySystemInformation"))
                  : nullptr;
        if (!NtQuerySystemInformation)
            return {};

        std::vector<uint8_t> buffer(0x10000);
        for (;;) {
            ULONG needed = 0;
            const LONG status = NtQuerySystemInformation(kSystemExtendedHandleInformation, buffer.data(),
                                                         static_cast<ULONG>(buffer.size()), &needed);
            if (static_cast<ULONG>(status) == kStatusInfoLengthMismatch) {
                buffer.resize(needed ? needed + 0x10000 : buffer.size() * 2);
                continue;
            }
            if (status != 0)
                return {};
            return buffer;
        }
    }

    // CRC32 (zlib/IEEE: reflected, init 0xFFFFFFFF, final XOR) - matches the client's own FUN_00471560.
    // `crc` is the running (pre-final-XOR) value so buffers can be chained; seed with 0xFFFFFFFF.
    uint32_t Crc32Acc(uint32_t crc, const uint8_t* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int k = 0; k < 8; ++k)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return crc;
    }

    // TEMP probe (not gating): logs the first few reads so we can confirm whether the MFT entry's crc is
    // CRC32(payload) or CRC32(payload + file_id) before we trust it to reject mid-write reads.
    void ProbeEntryCrc(uint32_t file_id, const std::vector<uint8_t>& payload, int32_t stored_crc)
    {
        static std::atomic<int> logged{0};
        if (logged.fetch_add(1, std::memory_order_relaxed) >= 8)
            return;
        const uint32_t acc = Crc32Acc(0xFFFFFFFFu, payload.data(), payload.size());
        const uint32_t crc_payload = ~acc;
        const uint8_t id[4] = {static_cast<uint8_t>(file_id), static_cast<uint8_t>(file_id >> 8),
                               static_cast<uint8_t>(file_id >> 16), static_cast<uint8_t>(file_id >> 24)};
        const uint32_t crc_both = ~Crc32Acc(acc, id, 4);
        Log::Log("[GwDat][crcprobe] id=%u size=%u stored=%08X payload=%08X both=%08X%s%s", file_id,
                 static_cast<unsigned>(payload.size()), static_cast<unsigned>(stored_crc), crc_payload, crc_both,
                 crc_payload == static_cast<uint32_t>(stored_crc) ? " MATCH:payload" : "",
                 crc_both == static_cast<uint32_t>(stored_crc) ? " MATCH:both" : "");
    }
} // namespace


void GwDatModule::WorkerLoop()
{
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_worker_mutex);
            m_worker_cv.wait_for(lock, std::chrono::milliseconds(kAsyncPollMs), [this] { return m_worker_stop; });
            if (m_worker_stop)
                return;
        }
        ProcessPendingReads();
        FlushTickles(); // send the reads' fetch requests as one batch
    }
}

void GwDatModule::StartWorker()
{
    std::scoped_lock<std::mutex> lock(m_worker_mutex);
    if (m_worker_running)
        return;
    m_worker_stop = false;
    m_worker_running = true;
    m_worker = std::thread([this] { WorkerLoop(); });
}

void GwDatModule::StopWorker()
{
    {
        std::scoped_lock<std::mutex> lock(m_worker_mutex);
        if (!m_worker_running)
            return;
        m_worker_stop = true;
    }
    m_worker_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
    {
        std::scoped_lock<std::mutex> lock(m_worker_mutex);
        m_worker_running = false;
    }
    {
        std::scoped_lock<std::mutex> lock(m_pending_mutex);
        m_pending_reads.clear(); // undelivered async reads are dropped on stop
    }
    std::scoped_lock<std::mutex> tlock(m_trigger_mutex);
    m_tickle_batch.clear(); // undelivered fetch requests are dropped on stop
}

bool GwDatModule::EnsureLoaded()
{
    if (m_loaded.load(std::memory_order_acquire))
        return true;
    std::scoped_lock<std::mutex> lock(m_load_mutex);
    if (!m_loaded.load(std::memory_order_relaxed) && ParseIndex())
        m_loaded.store(true, std::memory_order_release);
    return m_loaded.load(std::memory_order_relaxed);
}

bool GwDatModule::AcquireMappingInto(void*& out_mapping, long long& out_size, std::wstring& out_path)
{
    // The client holds Gw.dat exclusively, so we locate its open handle and map that instead.
    const std::vector<uint8_t> snapshot = QueryProcessHandles();
    if (snapshot.empty()) {
        // Scan failed (NtQuerySystemInformation blocked, usually AV/anti-cheat); flag for the UI.
        m_handle_enum_failed.store(true, std::memory_order_release);
        return false;
    }
    m_handle_enum_failed.store(false, std::memory_order_release);
    const auto* info = reinterpret_cast<const SystemHandleInfoEx*>(snapshot.data());
    const ULONG_PTR pid = GetCurrentProcessId();
    for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
        const SystemHandleEntryEx& entry = info->Handles[i];
        if (entry.UniqueProcessId != pid)
            continue;
        const HANDLE h = reinterpret_cast<HANDLE>(entry.HandleValue);
        if (GetFileType(h) != FILE_TYPE_DISK)
            continue;
        const std::wstring path = HandlePath(h);
        if (path.empty() || !HasDatExtension(path))
            continue;
        // Re-map whole-file each time: the dat grows as files stream in, and a size-0 mapping is fixed at creation.
        const HANDLE mapping = MapDat(h);
        if (!mapping)
            continue;
        LARGE_INTEGER sz;
        out_size = GetFileSizeEx(h, &sz) ? sz.QuadPart : 0;
        out_mapping = mapping;
        out_path = path; // the archive we actually bound to, for diagnostics
        return true;
    }
    return false;
}

// Reads the header + MFT + hash list from `mapping` into fresh temporaries (untouched on failure).
bool GwDatModule::ParseFrom(void* mapping_v, long long file_size, std::vector<MftEntry>& slots,
                             std::unordered_map<uint32_t, int>& fileid_to_slot)
{
    const HANDLE mapping = reinterpret_cast<HANDLE>(mapping_v);
    MainHeader head = {};
    if (!ReadAt(mapping, 0, &head, sizeof(head)))
        return false;
    if (memcmp(head.id, kDatMagic, sizeof(kDatMagic)) != 0) // "3ANa" archive magic
        return false;

    MftHeader mft_head = {};
    if (!ReadAt(mapping, head.mft_offset, &mft_head, sizeof(mft_head)))
        return false;
    if (mft_head.entry_count <= 16 || file_size <= head.mft_offset)
        return false;

    // Copy the whole 24-byte-per-slot MFT out in one pass rather than mapping a view per 600k+ entry.
    const int64_t table_bytes = static_cast<int64_t>(mft_head.entry_count) * 24;
    const int64_t avail = file_size - head.mft_offset;
    const size_t read_bytes = static_cast<size_t>(table_bytes < avail ? table_bytes : avail);
    if (read_bytes < static_cast<size_t>(17 * 24))
        return false;
    std::vector<uint8_t> table(read_bytes);
    if (!ReadAt(mapping, head.mft_offset, table.data(), read_bytes))
        return false;
    const int slot_count = static_cast<int>(read_bytes / 24);

    // Keep every physical slot: the game addresses a file as base_slot + stream_id.
    slots.resize(static_cast<size_t>(slot_count));
    memcpy(slots.data(), table.data(), static_cast<size_t>(slot_count) * 24);

    // Hash list is MFT[1] = slot 2 (slot 0 is the header, slots 1..15 reserved).
    const MftEntry& hash_list = slots[2];
    std::vector<MftExpansion> mftx;
    const int expansion_count = hash_list.size / static_cast<int>(sizeof(MftExpansion));
    mftx.resize(static_cast<size_t>(expansion_count));
    if (expansion_count > 0 && !ReadAt(mapping, hash_list.offset, mftx.data(),
                                       expansion_count * sizeof(MftExpansion)))
        return false;

    // Map each file id to its base slot; several ids may alias the same slot.
    for (const auto& e : mftx) {
        if (e.file_offset >= 16 && e.file_offset < slot_count)
            fileid_to_slot[static_cast<uint32_t>(e.file_number)] = e.file_offset;
    }
    return !fileid_to_slot.empty();
}

bool GwDatModule::ParseIndex()
{
    // First load (under m_load_mutex, before any reads): map and parse straight into members.
    if (m_mapping) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mapping));
        m_mapping = nullptr;
    }
    m_file_size = 0;
    m_slots.clear();
    m_fileid_to_slot.clear();

    void* mapping = nullptr;
    long long size = 0;
    std::wstring path;
    if (!AcquireMappingInto(mapping, size, path))
        return false;
    if (!ParseFrom(mapping, size, m_slots, m_fileid_to_slot)) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping));
        return false;
    }
    m_mapping = mapping;
    m_file_size = size;
    m_indexed_size = size;
    m_dat_path = path;
    Log::Log("[GwDat] bound to archive %S (%lld bytes, %u files indexed)", // TEMP
             path.c_str(), size, static_cast<unsigned>(m_fileid_to_slot.size()));
    return true;
}

// Re-read the on-disk MFT (on a read miss, throttled) to pick up files streamed in since the last parse.
void GwDatModule::MaybeRefresh()
{
    long long last_size = 0;
    {
        std::scoped_lock<std::mutex> guard(m_load_mutex);
        const uint32_t now = GetTickCount();
        if (m_last_refresh_ms && now - m_last_refresh_ms < kRefreshThrottleMs)
            return;
        m_last_refresh_ms = now;
        last_size = m_indexed_size;
    }

    void* mapping = nullptr;
    long long size = 0;
    std::wstring path;
    if (!AcquireMappingInto(mapping, size, path))
        return;
    // The dat only grows as files stream in, so an unchanged size means nothing new to index - skip
    // the costly MFT re-read (the fresh handle above still catches an atomic rename, which resizes).
    if (size == last_size && size != 0) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping));
        return;
    }
    std::vector<MftEntry> slots;
    std::unordered_map<uint32_t, int> fileid_to_slot;
    if (!ParseFrom(mapping, size, slots, fileid_to_slot)) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping));
        return;
    }
    Log::Log("[GwDat] dat grew %lld -> %lld bytes, reindexed %u files (streamed-in files now readable)", // TEMP
             last_size, size, static_cast<unsigned>(fileid_to_slot.size()));

    {
        std::unique_lock<std::shared_mutex> lock(m_index_mutex);
        CloseHandle(reinterpret_cast<HANDLE>(m_mapping));
        m_mapping = mapping;
        m_file_size = size;
        m_dat_path = path;
        m_slots.swap(slots);
        m_fileid_to_slot.swap(fileid_to_slot);
    }
    {
        std::scoped_lock<std::mutex> guard(m_load_mutex);
        m_indexed_size = size; // next MaybeRefresh skips the re-parse until the dat grows again
    }
    // The dat grew (files streamed in, e.g. a map load, which also clears the client's request queue),
    // so drop the tickle throttle: still-missing pending reads re-request their files on the next pass.
    std::scoped_lock<std::mutex> tlock(m_trigger_mutex);
    m_tickled_at.clear();
}

// Copies the raw (still-compressed) bytes for file_id/stream_id, shared-locked against MaybeRefresh.
bool GwDatModule::ReadRaw(uint32_t file_id, uint32_t stream_id, std::vector<uint8_t>& input, bool& compressed)
{
    std::shared_lock<std::shared_mutex> lock(m_index_mutex);
    const auto found = m_fileid_to_slot.find(file_id);
    if (found == m_fileid_to_slot.end())
        return false;
    // A file's streams are a linked list: `c` is the stream number, `id` the next stream's slot.
    int idx = found->second;
    const MftEntry* e = nullptr;
    for (int guard = 0; guard < 256; ++guard) {
        if (idx < 16 || idx >= static_cast<int>(m_slots.size()))
            return false;
        const MftEntry& candidate = m_slots[static_cast<size_t>(idx)];
        if (candidate.c == stream_id) {
            e = &candidate;
            break;
        }
        idx = candidate.id; // follow the chain to the next stream
        if (idx <= 0)
            return false;
    }
    if (!e || !e->b || e->size <= 0) // b == 0 marks an empty/base slot with no payload
        return false;

    input.resize(static_cast<size_t>(e->size));
    compressed = e->a != 0;
    if (!ReadAt(reinterpret_cast<HANDLE>(m_mapping), e->offset, input.data(), static_cast<size_t>(e->size)))
        return false;
    ProbeEntryCrc(file_id, input, e->crc); // TEMP: confirm the crc semantics before gating reads on it
    return true;
}

bool GwDatModule::ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id)
{
    auto& self = Instance();
    out.clear();
    if (!self.EnsureLoaded() || !file_id)
        return false;

    std::vector<uint8_t> input;
    bool compressed = false;
    if (!self.ReadRaw(file_id, stream_id, input, compressed)) {
        // Missing: the client may have streamed this file in since our last parse - re-read and retry.
        self.MaybeRefresh();
        if (!self.ReadRaw(file_id, stream_id, input, compressed)) {
            self.TickleFetch(file_id); // not resident; ask the client to fetch it for a later read
            return false;
        }
    }

    if (compressed) {
        unsigned char* output = nullptr;
        int out_size = 0;
        UnpackGWDat(input.data(), static_cast<int>(input.size()), output, out_size);
        if (output) {
            if (out_size > 0)
                out.assign(output, output + out_size);
            delete[] output;
        }
    }
    else {
        out = std::move(input);
    }
    return !out.empty();
}

void GwDatModule::ReadFileAsync(uint32_t file_id, ReadCallback callback, uint32_t stream_id)
{
    if (!callback)
        return;
    if (!file_id) {
        std::vector<uint8_t> empty;
        callback(empty);
        return;
    }
    auto& self = Instance();
    self.StartWorker(); // the poll loop delivers
    std::scoped_lock<std::mutex> lock(self.m_pending_mutex);
    self.m_pending_reads.push_back({file_id, stream_id, std::move(callback)});
}

void GwDatModule::SetTrigger(TriggerFn trigger)
{
    std::scoped_lock<std::mutex> lock(m_trigger_mutex);
    m_trigger = std::move(trigger);
}

// Queue a just-missed file for the client to fetch so a later read may find it; throttled per id so a
// per-frame retry loop doesn't spam. The batch is sent by FlushTickles (worker thread).
void GwDatModule::TickleFetch(uint32_t file_id)
{
    std::scoped_lock<std::mutex> lock(m_trigger_mutex);
    if (!m_trigger)
        return;
    const uint32_t now = GetTickCount();
    const auto it = m_tickled_at.find(file_id);
    if (it != m_tickled_at.end() && now - it->second < kTickleThrottleMs)
        return; // asked recently (elapsed is unsigned, wrap-safe for a session)
    m_tickled_at[file_id] = now;
    m_tickle_batch.push_back(file_id);
}

// Hand every queued fetch id to the client in one request instead of one call per file.
void GwDatModule::FlushTickles()
{
    std::vector<uint32_t> batch;
    TriggerFn trigger;
    {
        std::scoped_lock<std::mutex> lock(m_trigger_mutex);
        if (m_tickle_batch.empty() || !m_trigger)
            return;
        batch.swap(m_tickle_batch);
        trigger = m_trigger;
    }
    size_t base_present = 0; // TEMP: how many of these are already indexed (resident) - i.e. read-path bug,
    {                        // not a download problem, if this is high
        std::shared_lock<std::shared_mutex> ilock(m_index_mutex);
        for (const uint32_t id : batch)
            base_present += m_fileid_to_slot.count(id);
    }
    Log::Log("[GwDat] requesting %u file(s) (first id=%u; %u already have a base slot indexed)", // TEMP
             static_cast<unsigned>(batch.size()), batch[0], static_cast<unsigned>(base_present));
    trigger(batch.data(), batch.size());
}

// Worker pass: deliver reads that now resolve; a miss stays queued (no timeout) and tickles the client
// via ReadFile, so the read persists until the file streams in (e.g. on a later map change).
void GwDatModule::ProcessPendingReads()
{
    std::vector<std::pair<ReadCallback, std::vector<uint8_t>>> ready; // callbacks fire outside the lock
    {
        std::scoped_lock<std::mutex> lock(m_pending_mutex);
        for (auto it = m_pending_reads.begin(); it != m_pending_reads.end();) {
            std::vector<uint8_t> data;
            if (ReadFile(it->file_id, data, it->stream_id)) {
                ready.emplace_back(std::move(it->callback), std::move(data));
                it = m_pending_reads.erase(it);
            }
            else
                ++it;
        }
    }
    for (auto& [callback, data] : ready)
        callback(data);
}
