#pragma once

#include <ToolboxModule.h>
#include <filesystem>

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

    static IDirect3DTexture9** LoadGreyscaleTextureFromFileId(uint32_t file_id);
    static bool ReadDatFile(const wchar_t* fileHash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

    // stream_id picks a stream within the file; item/armor UI icons live at stream 1 of the model file.
    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id, uint32_t stream_id = 0);

    // Loads an item's UI icon (stream 1 of the model file), recolouring its dyeable region
    // (the stream 0xc mask) for the given GW::DyeColor value. 0/None yields the undyed icon.
    static IDirect3DTexture9** LoadItemImage(uint32_t model_file_id, uint32_t dye = 0);

    // Decodes the texture for file_id from the dat and writes it to disk (format chosen by extension).
    static void SaveTextureFromFileIdToFile(uint32_t file_id, const std::filesystem::path& file_path);
};
