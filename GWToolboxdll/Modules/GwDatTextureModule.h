#pragma once

#include <ToolboxModule.h>

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
    static bool ReadDatFile(const wchar_t* fileHash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id);
};
