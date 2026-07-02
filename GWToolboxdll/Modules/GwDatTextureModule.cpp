#include "stdafx.h"

#include <cstring>

#include <Logger.h>
#include "GwDatTextureModule.h"

#include "Resources.h"
#include <Utils/ArenaNetFileParser.h>
#include <Utils/GwDat/GwDatArchive.h>
#include <Utils/GwDat/AtexReader.h>

#include <DirectXTex.h>

namespace {

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

    // Decodes file_id to tightly-packed A8R8G8B8: ATEX/ATTX, ffna inline DXT chunk, or DDS.
    // stream_id picks a stream within the file (item models keep their UI icon at stream 1).
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

    // Per-dye colour transforms. GW recolours a dyed item's icon with a linear operation
    // (verified against the client: a hue-rotation about the grey axis with saturation/luma
    // scaling, plus a neutral bias added equally to R,G,B). Each entry is a 3x3 matrix (m0..m8,
    // out_c = m[3c+0]*R + m[3c+1]*G + m[3c+2]*B) followed by that neutral bias. GW's rotation
    // is near grey-preserving, so a neutral input maps back to neutral and shadows keep their
    // dark hue instead of taking on a colour cast; the bias lets bright dyes lift shadows
    // without tinting them. Indexed by GW::DyeColor - Blue, fitted to the client's own dyed
    // exports (white/pink are the least exact - their icon region is mostly clipped highlights).
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

    // Builds an item icon: stream 1 is the base colour, stream 0xc a single-channel dye
    // mask. Where masked, the base is recoloured by the dye colour matrix + neutral bias
    // (clamped) and blended back by the mask. `dyes` packs up to four GW::DyeColor values,
    // one per byte (as GW combines up to four dye slots into one icon colour); each applied
    // slot's matrix is averaged, which is exact since the matrices are linear. No applied
    // slot (or a missing/mismatched mask) yields the plain, undyed icon.
    // Decodes an item icon to A8R8G8B8 (worker-thread safe; no device). See MakeTextureFromArgb for upload.
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
        bool m_pending = false;         // a decode task is queued and hasn't finished yet
        uint16_t m_attempts = 0;        // pre-map decode attempts, capped to bound retries before the dat maps
        uint32_t m_first_missed_ms = 0; // GetTickCount of the first mapped-archive miss (0 = none); opens the retry window
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

    constexpr uint16_t kMaxDecodeAttempts = 240;   // ~a few seconds at 60fps, before the dat is mapped
    constexpr uint32_t kMissRetryWindowMs = 60000; // keep retrying a mapped-but-missing file ~1 min

    // Keep retrying a decode until it lands or we give up. Before the dat is mapped, retries are
    // bounded by a frame count. Once mapped-but-missing, we retry over a time window instead: the
    // client streams files into the dat as you play and GwDatArchive::ReadFile re-parses on a miss,
    // so a file that arrives mid-window is picked up on a later attempt.
    bool WantsDecode(const GwImg* img)
    {
        if (img->m_tex || img->m_pending)
            return false;
        if (img->m_first_missed_ms != 0)
            return GetTickCount() - img->m_first_missed_ms < kMissRetryWindowMs;
        return img->m_attempts < kMaxDecodeAttempts;
    }

    struct ArgbImage {
        std::vector<uint32_t> pixels;
        Vec2i dims;
    };

    // Decodes on a worker thread (the dat read + re-parse + pixel work all run off the render loop),
    // then uploads on the DX thread. `decode(pixels, dims)` returns whether it produced an image.
    template <typename Decode>
    void QueueDecode(GwImg* img, Decode decode)
    {
        img->m_pending = true;
        ++img->m_attempts;
        Resources::Instance().EnqueueWorkerTask([img, decode] {
            auto result = std::make_shared<ArgbImage>();
            const bool ok = decode(result->pixels, result->dims) && result->dims.x > 0 && result->dims.y > 0;
            Resources::Instance().EnqueueDxTask([img, result, ok](IDirect3DDevice9* device) {
                img->m_tex = ok ? MakeTextureFromArgb(device, result->pixels, result->dims) : nullptr;
                img->m_dims = result->dims;
                img->m_pending = false;
                if (!img->m_tex && img->m_first_missed_ms == 0 && GwDatArchive::Instance().Loaded())
                    img->m_first_missed_ms = GetTickCount(); // open the retry window on the first mapped miss
            });
        });
    }

    // Keyed by (stream_id << 32 | file_id) so different streams of one file cache separately.
    uint64_t TextureKey(uint32_t file_id, uint32_t stream_id) { return (static_cast<uint64_t>(stream_id) << 32) | file_id; }

    std::map<uint64_t, GwImg*> textures_by_file_id;
    std::map<uint32_t, GwImg*> greyscale_textures_by_file_id;
    std::map<uint64_t, GwImg*> item_images_by_file_id; // key = dyes << 32 | model_file_id
} // namespace

IDirect3DTexture9** GwDatTextureModule::LoadGreyscaleTextureFromFileId(uint32_t file_id)
{
    auto found = greyscale_textures_by_file_id.find(file_id);
    GwImg* gwimg_ptr = found != greyscale_textures_by_file_id.end() ? found->second : nullptr;
    if (!gwimg_ptr) {
        gwimg_ptr = new GwImg(file_id);
        greyscale_textures_by_file_id[file_id] = gwimg_ptr;
    }
    if (WantsDecode(gwimg_ptr))
        QueueDecode(gwimg_ptr, [file_id = gwimg_ptr->m_file_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeGreyscaleToArgb(file_id, argb, dims);
        });
    return &gwimg_ptr->m_tex;
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
    // The dat is indexed lazily on the first read, so there's nothing to set up here.
}

IDirect3DTexture9** GwDatTextureModule::LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id)
{
    const uint64_t key = TextureKey(file_id, stream_id);
    auto found = textures_by_file_id.find(key);
    GwImg* gwimg_ptr = found != textures_by_file_id.end() ? found->second : nullptr;
    if (!gwimg_ptr) {
        gwimg_ptr = new GwImg(file_id, stream_id);
        textures_by_file_id[key] = gwimg_ptr;
    }
    if (WantsDecode(gwimg_ptr))
        QueueDecode(gwimg_ptr, [file_id, stream_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeTextureToArgb(file_id, argb, dims, stream_id);
        });
    return &gwimg_ptr->m_tex;
}

IDirect3DTexture9** GwDatTextureModule::LoadItemImage(uint32_t model_file_id, uint32_t dyes)
{
    const uint64_t key = (static_cast<uint64_t>(dyes) << 32) | model_file_id;
    auto found = item_images_by_file_id.find(key);
    GwImg* gwimg_ptr = found != item_images_by_file_id.end() ? found->second : nullptr;
    if (!gwimg_ptr) {
        gwimg_ptr = new GwImg(model_file_id, 1, dyes);
        item_images_by_file_id[key] = gwimg_ptr;
    }
    if (WantsDecode(gwimg_ptr))
        QueueDecode(gwimg_ptr, [model_file_id, dyes](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeItemToArgb(model_file_id, dyes, argb, dims);
        });
    return &gwimg_ptr->m_tex;
}

void GwDatTextureModule::SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path)
{
    if (!file_id)
        return;
    // Pure CPU (decode + block-compress + write); run it off the render loop.
    Resources::Instance().EnqueueWorkerTask([file_id, file_path] {
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
