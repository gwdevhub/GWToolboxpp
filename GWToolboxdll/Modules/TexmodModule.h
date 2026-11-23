#pragma once

#include <ToolboxModule.h>

class TexmodModule final : public ToolboxModule {
public:
    static TexmodModule& Instance()
    {
        static TexmodModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "gMod/uMod/Texmod"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_IMAGE; }
    [[nodiscard]] const char* Description() const override
    {
        return "Load texture replacement packs (TPF/ZIP) via gMod at runtime.";
    }

    // Persisted shape of one texture pack entry (path stored as UTF-8)
    struct PackSetting {
        std::string path;
        bool loaded = false;
    };

    void Update(float dt) override;
    void Terminate() override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

private:
    TexmodModule() = default;
};
