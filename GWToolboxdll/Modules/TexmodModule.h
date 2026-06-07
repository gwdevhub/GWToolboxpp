#pragma once

#include <ToolboxModule.h>

// ---------------------------------------------------------------------------
// TexmodModule
// GWToolbox module that loads gMod.dll at runtime and hands it the game's
// already-created D3D9 device via gMod's exported SetDevice(), then loads texture
// replacement packs (TPF/ZIP/DDS) through AddFile/RemoveFile. The list order is
// priority: when two packs replace the same texture, the one higher in the list
// wins, so reordering re-adds packs in the desired order.
// ---------------------------------------------------------------------------
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

    void Update(float dt) override;
    void Terminate() override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

private:
    TexmodModule() = default;
};
