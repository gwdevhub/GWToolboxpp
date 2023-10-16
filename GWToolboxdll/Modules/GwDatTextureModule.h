#pragma once

#include <ToolboxModule.h>

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

    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id);
    static uint32_t FileHashToFileId(const wchar_t* fileHash);
};
