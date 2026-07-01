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

    // Reads file_id from the dat and decodes it to tightly-packed A8R8G8B8 pixels.
    // Handles ATEX/ATTX textures, model files with an inline DXT texture chunk, and DDS.
    bool DecodeTextureToArgb(uint32_t file_id, std::vector<uint32_t>& argb, Vec2i& dims)
    {
        ArenaNetFileParser::GameAssetFile asset;
        if (!asset.readFromDat(file_id))
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
        argb.resize(static_cast<size_t>(tex.width) * tex.height);
        for (size_t i = 0; i < argb.size() && i < tex.rgba_data.size(); ++i) {
            const RGBA p = tex.rgba_data[i];
            argb[i] = (static_cast<uint32_t>(p.a) << 24) | (static_cast<uint32_t>(p.r) << 16) |
                      (static_cast<uint32_t>(p.g) << 8) | static_cast<uint32_t>(p.b);
        }
        return true;
    }

    IDirect3DTexture9* CreateTexture(IDirect3DDevice9* device, uint32_t file_id, Vec2i& dims)
    {
        if (!device || !file_id)
            return nullptr;

        std::vector<uint32_t> argb;
        if (!DecodeTextureToArgb(file_id, argb, dims) || !dims.x || !dims.y)
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

    IDirect3DTexture9* CreateGreyscaleTexture(IDirect3DDevice9* device, uint32_t file_id, Vec2i& dims)
    {
        if (!device || !file_id)
            return nullptr;

        std::vector<uint32_t> argb;
        if (!DecodeTextureToArgb(file_id, argb, dims) || !dims.x || !dims.y)
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
            auto* dst = reinterpret_cast<uint32_t*>((uint8_t*)rect.pBits + y * rect.Pitch);
            for (int x = 0; x < dims.x; x++) {
                const uint32_t c = *srcdata++;
                const uint8_t r = (c >> 16) & 0xFF;
                const uint8_t g = (c >> 8) & 0xFF;
                const uint8_t b = c & 0xFF;
                const uint8_t a = (c >> 24) & 0xFF;
                const uint8_t grey = static_cast<uint8_t>(r * 0.299f + g * 0.587f + b * 0.114f);
                *dst++ = (a << 24) | (grey << 16) | (grey << 8) | grey;
            }
        }
        tex->UnlockRect(0);
        return tex;
    }

    struct GwImg {
        uint32_t m_file_id = 0;
        Vec2i m_dims;
        IDirect3DTexture9* m_tex = nullptr;
        ~GwImg()
        {
            if (m_tex) {
                m_tex->Release();
                m_tex = nullptr;
            }
        }
    };

    std::map<uint32_t, GwImg*> textures_by_file_id;
    std::map<uint32_t, GwImg*> greyscale_textures_by_file_id;
} // namespace

IDirect3DTexture9** GwDatTextureModule::LoadGreyscaleTextureFromFileId(uint32_t file_id)
{
    auto found = greyscale_textures_by_file_id.find(file_id);
    if (found != greyscale_textures_by_file_id.end()) return &found->second->m_tex;
    auto gwimg_ptr = new GwImg(file_id);
    greyscale_textures_by_file_id[file_id] = gwimg_ptr;
    Resources::Instance().EnqueueDxTask([gwimg_ptr](IDirect3DDevice9* device) {
        gwimg_ptr->m_tex = CreateGreyscaleTexture(device, gwimg_ptr->m_file_id, gwimg_ptr->m_dims);
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
    // Log once, the first time the archive is available (early failures are
    // transient - the client may not have opened the dat yet).
    if (dat.Loaded()) {
        static std::once_flag reported;
        std::call_once(reported, [&dat] {
            Log::LogW(L"[GwDat] Mapped client's Gw.dat ('%s')", dat.DatPath().c_str());
        });
    }
    return ok;
}

void GwDatTextureModule::Initialize()
{
    ToolboxModule::Initialize();
    // The dat is indexed lazily on the first read (GwDatArchive::EnsureLoaded is
    // guarded by std::call_once), so there's nothing to scan or set up here.
}

IDirect3DTexture9** GwDatTextureModule::LoadTextureFromFileId(uint32_t file_id)
{
    auto found = textures_by_file_id.find(file_id);
    if (found != textures_by_file_id.end())
        return &found->second->m_tex;
    auto gwimg_ptr = new GwImg(file_id);
    textures_by_file_id[file_id] = gwimg_ptr;
    Resources::Instance().EnqueueDxTask([gwimg_ptr](IDirect3DDevice9* device) {
        gwimg_ptr->m_tex = CreateTexture(device, gwimg_ptr->m_file_id, gwimg_ptr->m_dims);
        });
    return &gwimg_ptr->m_tex;
}

void GwDatTextureModule::SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path)
{
    if (!file_id)
        return;
    Resources::Instance().EnqueueDxTask([file_id, file_path](IDirect3DDevice9*) {
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

        // DXT1/BC1 encodes color better (it can place a texel at the midpoint of its two RGB565
        // endpoints, which DXT3/DXT5 cannot), so prefer it whenever the icon has no real alpha and
        // only fall back to BC3 (DXT5) when smooth transparency must be preserved.
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
    textures_by_file_id.clear();
    greyscale_textures_by_file_id.clear();
}
