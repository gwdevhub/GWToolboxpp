#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>

#include <Logger.h>
#include "GwDatTextureModule.h"

#include "Resources.h"

namespace {

    class RecObj;

    struct Vec2i {
        int x = 0;
        int y = 0;
    };

    typedef enum : uint32_t {
        GR_FORMAT_A8R8G8B8 = 0, // raw?
        GR_FORMAT_UNK = 0x4,    //.bmp,...
        GR_FORMAT_DXT1 = 0xF,
        GR_FORMAT_DXT2,
        GR_FORMAT_DXT3,
        GR_FORMAT_DXT4,
        GR_FORMAT_DXT5,
        GR_FORMAT_DXTA,
        GR_FORMAT_DXTL,
        GR_FORMAT_DXTN,
        GR_FORMATS
    } GR_FORMAT;

    typedef uint8_t* gw_image_bits; // array of pointers to mipmap images

    typedef RecObj*(__cdecl* FileIdToRecObj_pt)(wchar_t* fileHash, int unk1_1, int unk2_0);
    FileIdToRecObj_pt FileHashToRecObj_func;

    typedef uint8_t*(__cdecl* GetRecObjectBytes_pt)(RecObj* rec, int* size_out);
    GetRecObjectBytes_pt GetRecObjectBytes_func;

    typedef uint32_t(__cdecl* DecodeImage_pt)(int size, uint8_t* bytes, gw_image_bits* bits, uint8_t* pallete, GR_FORMAT* format, Vec2i* dims, int* levels);
    DecodeImage_pt DecodeImage_func;

    typedef void(__cdecl* UnkRecObjBytes_pt)(RecObj* rec, uint8_t* bytes);
    UnkRecObjBytes_pt UnkRecObjBytes_func;

    typedef void(__cdecl* CloseRecObj_pt)(RecObj* rec);
    CloseRecObj_pt CloseRecObj_func;

    typedef gw_image_bits(__cdecl* AllocateImage_pt)(GR_FORMAT format, Vec2i* destDims, uint32_t levels, uint32_t unk2);
    AllocateImage_pt AllocateImage_func;

    typedef void(__cdecl* FreeImage_pt)(gw_image_bits bits);
    FreeImage_pt FreeImage_func;

    typedef void(__cdecl* Depalletize_pt)(
        gw_image_bits destBits, uint8_t* destPalette, GR_FORMAT destFormat, int* destMipWidths, gw_image_bits sourceBits, uint8_t* sourcePallete, GR_FORMAT sourceFormat, int* sourceMipWidths, Vec2i* sourceDims, uint32_t sourceLevels,
        uint32_t unk1_0, int* unk2_0
    );
    Depalletize_pt Depalletize_func;

    // typedef void(__cdecl *ConvertImage_pt) (uint8_t *destBytes, int *destPallete, uint32_t destFormat, Vec2i *destDims,
    //                                         uint8_t *sourceBytes, int *sourcePallete, uint32_t sourceFormat, Vec2i *sourceDims, float sharpness);
    // ConvertImage_pt ConvertImage_func;

    // typedef uint8_t*(__cdecl *ConvertToRaw_pt) (uint8_t *sourceBits, int *sourcePallete, uint32_t sourceFormat, Vec2i *sourceDims,
    //     Vec2i *maybeDestDims, uint32_t levelsProvided, uint32_t levelsRequested, float sharpness);
    // ConvertUnk_pt ConvertUnk_func;

    // typedef void(__cdecl *GetLevelWidths_pt) (int format, int width, uint32_t levels, int *widths);
    // GetLevelWidths_pt GetLevelWidths_func;


    void FileIdToFileHash(uint32_t file_id, wchar_t* fileHash) {
        fileHash[0] = static_cast<wchar_t>(((file_id - 1) % 0xff00) + 0x100);
        fileHash[1] = static_cast<wchar_t>(((file_id - 1) / 0xff00) + 0x100);
        fileHash[2] = 0;
    }

    const char* strnstr(char* str, const char* substr, size_t n)
    {
        char* p = str, * pEnd = str + n;
        size_t substr_len = strlen(substr);

        if (0 == substr_len)
            return str; // the empty string is contained everywhere.

        pEnd -= (substr_len - 1);
        for (; p < pEnd; ++p)
        {
            if (0 == strncmp(p, substr, substr_len))
                return p;
        }
        return NULL;
    }


    // OpenImage converts any GW format to ARGB. It is possible to skip conversion if gw format is compatible with D3FMT.
    uint32_t OpenImage(uint32_t file_id, gw_image_bits* dst_bits, Vec2i& dims, int& levels, GR_FORMAT& format)
    {
        int size = 0;
        uint8_t* pallete = nullptr;
        gw_image_bits bits = nullptr;

        wchar_t fileHash[4] = { 0 };
        FileIdToFileHash(file_id, fileHash);

        auto rec = FileHashToRecObj_func(fileHash, 1, 0);
        if (!rec) return 0;

        const auto bytes = GetRecObjectBytes_func(rec, &size);
        if (!bytes) {
            CloseRecObj_func(rec);
            return 0;
        }
        int image_size = size;
        auto image_bytes = bytes;
        if (memcmp((char*)bytes, "ffna", 4) == 0) {
            // Model file format; try to find first instance of image from this.
            const auto found = strnstr((char*)bytes, "ATEX",size);
            if (!found) {
                UnkRecObjBytes_func(rec, bytes);
                CloseRecObj_func(rec);
                return 0;
            }
            image_bytes = (uint8_t*)found;
            image_size = *(int*)(found - 4);
        }

        uint32_t result = DecodeImage_func(image_size, image_bytes, &bits, pallete, &format, &dims, &levels);
        if (rec) {
            UnkRecObjBytes_func(rec, bytes);
            if (levels > 13)                return 0;

            CloseRecObj_func(rec);
        }

        if (format >= GR_FORMATS || !result) return 0;

        levels = 1;
        
        *dst_bits = AllocateImage_func(GR_FORMAT_A8R8G8B8, &dims, levels, 0);
        Depalletize_func((gw_image_bits)dst_bits, nullptr, GR_FORMAT_A8R8G8B8, nullptr, bits, pallete, format, nullptr, &dims, levels, 0, 0);

        FreeImage_func(bits);

        // todo: free bytes;
        return result;
    }

    IDirect3DTexture9* CreateTexture(IDirect3DDevice9* device, uint32_t file_id, Vec2i &dims)
    {
        if (!device || !file_id) {
            return nullptr;
        }
        
        gw_image_bits bits = nullptr;
        int levels;
        GR_FORMAT format;
        auto ret = OpenImage(file_id, &bits, dims, levels, format);
        if (!ret || !bits || !dims.x || !dims.y) {
            if (bits) {
                FreeImage_func(bits);
            }
            return nullptr;
        }

        // Create a texture: http://msdn.microsoft.com/en-us/library/windows/desktop/bb174363(v=vs.85).aspx
        IDirect3DTexture9* tex = nullptr;
        if (device->CreateTexture(dims.x, dims.y, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK) {
            return nullptr;
        }

        // Lock the texture for writing: http://msdn.microsoft.com/en-us/library/windows/desktop/bb205913(v=vs.85).aspx
        D3DLOCKED_RECT rect;
        if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK) {
            return nullptr;
        }

        unsigned int* srcdata = (unsigned int*)bits;
        for (int y = 0; y < dims.y; y++) {
            uint8_t* destAddr = ((uint8_t*)rect.pBits + y * rect.Pitch);
            memcpy(destAddr, srcdata, dims.x * 4);
            srcdata += dims.x;
            /* for (int x = 0; x < dims.x; ++x) {
                uint8_t* destAddr = ((uint8_t*)rect.pBits + y * rect.Pitch + 4 * x);

                // unsigned int data = 0xFF000000 | (*srcdata >> 24 & 0xFF) | (*srcdata >> 16 & 0xFF00) | (*srcdata >> 8 & 0xFF0000);
                // memcpy(destAddr, &data, 4);
                memcpy(destAddr, srcdata, 4);
                srcdata++;
            }*/
        }
        
        FreeImage_func(bits);

        // Unlock the texture so it can be used.
        tex->UnlockRect(0);
        return tex;
    }

    struct GwImg {
        uint32_t m_file_id = 0;
        Vec2i m_dims;
        IDirect3DTexture9* m_tex = nullptr;
    };

    std::map<uint32_t,GwImg*> textures_by_file_id;
}



void GwDatTextureModule::Initialize()
{
    ToolboxModule::Initialize();

    using namespace GW;

    // @Cleanup: Reduce size of signature and offset jumps
    uintptr_t address = (uintptr_t)Scanner::Find("\x83\xc4\x0c\x33\xc0\x89\x45\xfc\x85\xf6\x74\x14\x8d\x45\xfc", "xxxxxxxxxxxxxxx", 0);
    if (address) {
        FileHashToRecObj_func = (FileIdToRecObj_pt)Scanner::FunctionFromNearCall(address - 7);
        GetRecObjectBytes_func = (GetRecObjectBytes_pt)Scanner::FunctionFromNearCall(address + 0x11);
        DecodeImage_func = (DecodeImage_pt)Scanner::FunctionFromNearCall(address + 0x33);
        UnkRecObjBytes_func = (UnkRecObjBytes_pt)Scanner::FunctionFromNearCall(address + 0x43);
        CloseRecObj_func = (CloseRecObj_pt)Scanner::FunctionFromNearCall(address + 0x49);
        AllocateImage_func = (AllocateImage_pt)Scanner::FunctionFromNearCall((uintptr_t)DecodeImage_func + 0x1ef);
        FreeImage_func = (FreeImage_pt)Scanner::FunctionFromNearCall((uintptr_t)DecodeImage_func + 0x298);
        Depalletize_func = (Depalletize_pt)Scanner::Find("\x83\xc4\x18\x39\xb5\x70\xff\xff\xff\x74\x21\x8b\x57\x04\x0f\xaf\x17\xc1\xe2\x02", "xxxxxxxxxxxxxxxxxxxx", -0x127);
    }
#ifdef _DEBUG
    ASSERT(FileHashToRecObj_func);
    ASSERT(GetRecObjectBytes_func);
    ASSERT(DecodeImage_func);
    ASSERT(UnkRecObjBytes_func);
    ASSERT(CloseRecObj_func);
    ASSERT(AllocateImage_func);
    ASSERT(FreeImage_func);
    ASSERT(Depalletize_func);
#endif
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
void GwDatTextureModule::Terminate()
{
    for (auto gwimg_ptr : textures_by_file_id) {
        delete gwimg_ptr.second;
    }
    textures_by_file_id.clear();
}
uint32_t GwDatTextureModule::FileHashToFileId(const wchar_t* fileHash) {
    if (!fileHash)
        return 0;
    if (((0xff < *fileHash) && (0xff < fileHash[1])) &&
        ((fileHash[2] == 0 || ((0xff < fileHash[2] && (fileHash[3] == 0)))))) {
        return (*fileHash - 0xff00ff) + (uint32_t)fileHash[1] * 0xff00;
    }
    return 0;
}
