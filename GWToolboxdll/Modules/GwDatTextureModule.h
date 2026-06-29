#pragma once

#include <ToolboxModule.h>
#include <filesystem>

namespace GW {
    struct RecObject;
}

class GwDatTextureModule : public ToolboxModule {
    GwDatTextureModule() = default;
    ~GwDatTextureModule() override = default;

public:
    static GwDatTextureModule& Instance()
    {
        static GwDatTextureModule instance;
        return instance;
    }

    const char* Name() const override { return "GW Dat Texture Module"; };

    bool HasSettings() override { return false; }

    void Initialize() override;
    
    void Terminate() override;

    static bool CloseHandle(GW::RecObject* handle);
    static IDirect3DTexture9** LoadGreyscaleTextureFromFileId(uint32_t file_id);
    static bool ReadDatFile(const wchar_t* fileHash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id);

    // Decodes the texture for file_id from the dat and writes it to disk (format chosen by extension).
    static void SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path);
};
