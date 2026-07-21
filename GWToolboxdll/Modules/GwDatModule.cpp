#include "stdafx.h"

#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include <Logger.h>
#include "GwDatModule.h"

#include "Resources.h"
#include <Utils/ArenaNetFileParser.h>
#include <Utils/GwDat/AtexReader.h>
#include <Utils/GwDat/xentax.h>

#include <DirectXTex.h>

// GwDatModule reads/decodes textures straight from the client's on-disk Gw.dat. It does NOT interface with
// the game's streaming/download subsystem: whatever is in the dat, we can decode; a file the dat doesn't
// contain simply won't load (see ReadFile's one-time -image hint). On a Steam streaming install the dat can
// be incomplete until the user runs Guild Wars with -image to download everything.

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

        // Model files (ffna) carry the texture as an inline DXT3 chunk - confirmed to be a real icon, not a skin texture.
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
    // A few items store their icon at stream 0 instead of stream 1 (standalone, or an ffna model's inline texture); undyeable, no stream 0xc mask for it.
    bool DecodeItemToArgb(uint32_t file_id, uint32_t dyes, std::vector<uint32_t>& base, Vec2i& dims)
    {
        if (!file_id)
            return false;
        const bool has_icon_stream = DecodeTextureToArgb(file_id, base, dims, 1);
        if (!has_icon_stream && !DecodeTextureToArgb(file_id, base, dims, 0))
            return false;
        if (!dims.x || !dims.y)
            return false;
        if (!has_icon_stream)
            return true;

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

    // Decodes file_id and converts it to greyscale A8R8G8B8 in place.
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
        Vec2i m_dims;
        IDirect3DTexture9* m_tex = nullptr;
        bool m_pending = false;   // a decode has been dispatched; don't dispatch twice
        bool m_completed = false; // the dispatched decode has run (success or failure)
        ~GwImg()
        {
            if (m_tex)
                m_tex->Release();
        }
    };

    // Dispatch once. On failure (file not in the dat) m_tex stays null - a placeholder - and m_pending stays
    // set, so we don't re-decode the same missing file every frame.
    bool WantsDecode(const GwImg* img) { return !img->m_tex && !img->m_pending; }

    // True once the dispatched decode has actually run and produced no texture - i.e. a genuine, permanent
    // failure (missing dat data) rather than merely not-yet-decoded.
    bool HasFailed(const GwImg* img) { return img->m_completed && !img->m_tex; }

    using DecodeFn = std::function<bool(std::vector<uint32_t>&, Vec2i&)>;

    // Decodes dispatched before the client opened Gw.dat (e.g. injected at process start): parked here and
    // retried from Update() once the dat appears, instead of caching the failure for the whole session.
    std::mutex deferred_mutex;
    std::vector<std::pair<std::shared_ptr<GwImg>, DecodeFn>> deferred_decodes;
    std::atomic<size_t> deferred_count{0};
    std::atomic<bool> deferred_retry_in_flight{false};

    void RunDecode(const std::shared_ptr<GwImg>& img, const DecodeFn& decode)
    {
        std::vector<uint32_t> argb;
        Vec2i dims;
        const bool ok = decode(argb, dims) && dims.x > 0 && dims.y > 0;
        if (!ok && !GwDatModule::IsLoaded()) {
            std::scoped_lock lock(deferred_mutex);
            deferred_decodes.emplace_back(img, decode);
            deferred_count.store(deferred_decodes.size(), std::memory_order_release);
            Log::Log("[dat] decode deferred until Gw.dat is mapped (%u parked)", static_cast<unsigned>(deferred_decodes.size()));
            return;
        }
        Resources::Instance().EnqueueDxTask([img, argb, dims, ok](IDirect3DDevice9* device) {
            if (ok)
                img->m_tex = MakeTextureFromArgb(device, argb, dims);
            img->m_dims = dims;
            img->m_completed = true;
        });
    }

    // Reads/decodes on a worker thread (was inline on the render thread, stalling the game); only the GPU upload needs the DX thread.
    void QueueDecode(std::shared_ptr<GwImg> img, DecodeFn decode)
    {
        img->m_pending = true;
        Resources::Instance().EnqueueWorkerTask([img = std::move(img), decode = std::move(decode)] {
            RunDecode(img, decode);
        });
    }

    // Keyed by (stream_id << 32 | file_id) so different streams of one file cache separately.
    uint64_t TextureKey(uint32_t file_id, uint32_t stream_id) { return (static_cast<uint64_t>(stream_id) << 32) | file_id; }

    // Shared-owned so an in-flight DX-upload task never touches a GwImg freed by Terminate().
    std::map<uint64_t, std::shared_ptr<GwImg>> textures_by_file_id;
    std::map<uint32_t, std::shared_ptr<GwImg>> greyscale_textures_by_file_id;
    std::map<uint64_t, std::shared_ptr<GwImg>> item_images_by_file_id; // key = dyes << 32 | model_file_id

    template <typename Map, typename Key>
    const std::shared_ptr<GwImg>& GetOrCreate(Map& map, Key key)
    {
        std::shared_ptr<GwImg>& slot = map[key];
        if (!slot)
            slot = std::make_shared<GwImg>();
        return slot;
    }
} // namespace

IDirect3DTexture9** GwDatModule::LoadGreyscaleTextureFromFileId(uint32_t file_id)
{
    const auto& img = GetOrCreate(greyscale_textures_by_file_id, file_id);
    if (WantsDecode(img.get()))
        QueueDecode(img, [file_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeGreyscaleToArgb(file_id, argb, dims);
        });
    return &img->m_tex;
}

IDirect3DTexture9** GwDatModule::LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id)
{
    const auto& img = GetOrCreate(textures_by_file_id, TextureKey(file_id, stream_id));
    if (WantsDecode(img.get()))
        QueueDecode(img, [file_id, stream_id](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeTextureToArgb(file_id, argb, dims, stream_id);
        });
    return &img->m_tex;
}

IDirect3DTexture9** GwDatModule::LoadItemImage(uint32_t model_file_id, uint32_t dyes, bool* failed_out)
{
    const uint64_t key = (static_cast<uint64_t>(dyes) << 32) | model_file_id;
    const auto& img = GetOrCreate(item_images_by_file_id, key);
    if (WantsDecode(img.get()))
        QueueDecode(img, [model_file_id, dyes](std::vector<uint32_t>& argb, Vec2i& dims) {
            return DecodeItemToArgb(model_file_id, dyes, argb, dims);
        });
    if (failed_out)
        *failed_out = HasFailed(img.get());
    return &img->m_tex;
}



void GwDatModule::SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path)
{
    std::vector<uint32_t> argb;
    Vec2i dims;
    if (!file_id || !DecodeTextureToArgb(file_id, argb, dims) || dims.x <= 0 || dims.y <= 0)
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
    if (SUCCEEDED(DirectX::Compress(src, target, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressed)))
        DirectX::SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DirectX::DDS_FLAGS_NONE, file_path.c_str());
}

bool GwDatModule::ReadDatFile(const wchar_t* file_name, std::vector<uint8_t>* bytes_out, uint32_t stream_id)
{
    if (!(file_name && *file_name && bytes_out))
        return false;
    const uint32_t file_id = ArenaNetFileParser::FileHashToFileId(file_name);
    return file_id && ReadFile(file_id, *bytes_out, stream_id);
}

void GwDatModule::Terminate()
{
    // The cache is shared_ptr-owned: clearing it here only drops our references; any DX-upload task still in
    // flight holds its own reference and frees its GwImg (releasing the texture) when it finishes. The dat
    // mapping is left open (written once, read lock-free) and released by the OS on unload.
    {
        std::scoped_lock lock(deferred_mutex);
        deferred_decodes.clear();
        deferred_count.store(0, std::memory_order_release);
    }
    textures_by_file_id.clear();
    greyscale_textures_by_file_id.clear();
    item_images_by_file_id.clear();
}

// ============================================================================
// Archive reader: map the client's on-disk Gw.dat once, parse its MFT, read + decompress by file id.
// ============================================================================
namespace {
    const uint8_t kDatMagic[4] = {0x33, 0x41, 0x4e, 0x1a}; // "3ANa"
    constexpr size_t kMaxView = 0x800000;                  // cap each mapped window at 8 MB

    DWORD AllocationGranularity()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwAllocationGranularity;
    }

    // Read via mapped views, never ReadFile: the client's dat handle is bound to its I/O port.
    bool ReadAt(HANDLE mapping, int64_t offset, void* buffer, size_t count)
    {
        if (!mapping)
            return false;
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
} // namespace

bool GwDatModule::EnsureLoaded()
{
    if (m_loaded.load(std::memory_order_acquire))
        return true;
    std::scoped_lock<std::mutex> lock(m_load_mutex);
    if (!m_loaded.load(std::memory_order_relaxed) && ParseIndex())
        m_loaded.store(true, std::memory_order_release);
    return m_loaded.load(std::memory_order_relaxed);
}

bool GwDatModule::IsLoaded()
{
    return Instance().m_loaded.load(std::memory_order_acquire);
}

void GwDatModule::Update(float)
{
    if (!deferred_count.load(std::memory_order_acquire))
        return;
    if (deferred_retry_in_flight.load(std::memory_order_acquire))
        return;
    static ULONGLONG next_retry_ms = 0;
    const ULONGLONG now = GetTickCount64();
    if (now < next_retry_ms)
        return;
    next_retry_ms = now + 2000;
    deferred_retry_in_flight.store(true, std::memory_order_release);
    // EnsureLoaded's handle scan blocks, so the retry itself runs on a worker, never the game thread.
    Resources::Instance().EnqueueWorkerTask([this] {
        if (EnsureLoaded()) {
            std::vector<std::pair<std::shared_ptr<GwImg>, DecodeFn>> retry;
            {
                std::scoped_lock lock(deferred_mutex);
                retry.swap(deferred_decodes);
                deferred_count.store(0, std::memory_order_release);
            }
            if (!retry.empty())
                Log::Log("[dat] Gw.dat mapped, retrying %u deferred decodes", static_cast<unsigned>(retry.size()));
            for (const auto& [img, decode] : retry)
                RunDecode(img, decode);
        }
        deferred_retry_in_flight.store(false, std::memory_order_release);
    });
}

bool GwDatModule::AcquireMappingInto(void*& out_mapping, long long& out_size)
{
    // The client holds Gw.dat exclusively, so we locate its open handle and map that instead.
    const std::vector<uint8_t> snapshot = QueryProcessHandles();
    if (snapshot.empty()) {
        // Scan failed (NtQuerySystemInformation blocked, usually AV/anti-cheat); flag for the diagnostic.
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
        if (path.empty())
            continue;
        if (!path.ends_with(L".dat") && !path.ends_with(L".snapshot"))
            continue;
        const HANDLE mapping = MapDat(h);
        if (!mapping)
            continue;
        LARGE_INTEGER sz;
        if (!GetFileSizeEx(h, &sz))
            continue;
        if (sz.QuadPart < 1024 * 1024 * 100)
            continue; // Gw.dat is always > 100 MB; ignore small .dat files (e.g. the Steam overlay's .dat)
        out_size = sz.QuadPart;
        out_mapping = mapping;
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

    // Map each file id to its base slot; reserve + index rather than range-for to avoid rehashing and debug-iterator overhead on this ~150k+ entry table.
    fileid_to_slot.reserve(static_cast<size_t>(expansion_count));
    for (int i = 0; i < expansion_count; ++i) {
        const MftExpansion& e = mftx[static_cast<size_t>(i)];
        if (e.file_offset >= 16 && e.file_offset < slot_count)
            fileid_to_slot[static_cast<uint32_t>(e.file_number)] = e.file_offset;
    }
    return !fileid_to_slot.empty();
}

bool GwDatModule::ParseIndex()
{
    // First load only (under m_load_mutex, before any reads): map once and parse straight into members. We
    // don't re-map/re-parse afterwards - whatever the dat holds when first touched is what we can read.
    void* mapping = nullptr;
    long long size = 0;
    if (!AcquireMappingInto(mapping, size))
        return false;
    if (!ParseFrom(mapping, size, m_slots, m_fileid_to_slot)) {
        CloseHandle(reinterpret_cast<HANDLE>(mapping));
        m_slots.clear();
        m_fileid_to_slot.clear();
        return false;
    }
    m_mapping = mapping;
    return true;
}

// Copies the raw (still-compressed) bytes for file_id/stream_id. The index is immutable after EnsureLoaded().
// file_known is set true once file_id itself is found, so callers can tell "no such file" apart from "file exists, just not this stream".
bool GwDatModule::ReadRaw(uint32_t file_id, uint32_t stream_id, std::vector<uint8_t>& input, bool& compressed, bool& file_known)
{
    file_known = false;
    const auto found = m_fileid_to_slot.find(file_id);
    if (found == m_fileid_to_slot.end())
        return false;
    file_known = true;
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
    return ReadAt(reinterpret_cast<HANDLE>(m_mapping), e->offset, input.data(), static_cast<size_t>(e->size));
}

bool GwDatModule::MissingDatData()
{
    return Instance().m_missing_data.load(std::memory_order_relaxed);
}

bool GwDatModule::ReadFile(uint32_t file_id, std::vector<uint8_t>& out, uint32_t stream_id)
{
    auto& self = Instance();
    out.clear();
    if (!self.EnsureLoaded()) {
        // Only warn about the AV/anti-cheat case; a not-yet-open handle is transient and retries silently.
        if (self.m_handle_enum_failed.load(std::memory_order_acquire)) {
            static std::once_flag warned;
            std::call_once(warned, [] {
                Log::Warning("GWToolbox can't read Gw.dat: it was blocked from listing its own file handles, "
                             "usually by anti-virus or anti-cheat. In-game images won't load until it's allowed.");
            });
        }
        return false;
    }
    if (!file_id)
        return false;

    std::vector<uint8_t> input;
    bool compressed = false;
    bool file_known = false;
    if (!self.ReadRaw(file_id, stream_id, input, compressed, file_known)) {
        if (!file_known) {
            // The dat doesn't have this file at all - likely an incomplete Steam/streaming install; a missing stream on a file that does exist doesn't count (routine - several icon streams are speculatively probed).
            self.m_missing_data.store(true, std::memory_order_relaxed);
            static std::once_flag warned;
            std::call_once(warned, [file_id] {
                Log::Warning("GWToolbox couldn't find some image data in your Gw.dat - your local game data looks "
                             "incomplete (Steam downloads it on demand). Run Guild Wars once with the -image "
                             "command-line option to download all game data, then restart. file_id %d. Setup steps: "
                             "https://gwtoolbox.com/docs/troubleshooting/#missing-images-in-toolbox",
                             file_id);
            });
        }
        return false;
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
