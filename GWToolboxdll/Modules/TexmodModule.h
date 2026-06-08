#pragma once

#include <ToolboxWidget.h>

// ---------------------------------------------------------------------------
// TexmodModule
// GWToolbox module that loads gMod.dll at runtime and hands it the game's
// already-created D3D9 device via gMod's exported SetDevice(), then loads texture
// replacement packs (TPF/ZIP/DDS) through AddFile/RemoveFile. The list order is
// priority: when two packs replace the same texture, the one higher in the list
// wins, so reordering re-adds packs in the desired order.
//
// It is a widget (rather than a plain module) only so it gets a per-frame Draw():
// while texture recording is active it paints an on-screen overlay. Its settings
// are still registered like a plain module (no visibility/position controls).
// ---------------------------------------------------------------------------
class TexmodModule final : public ToolboxWidget {
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
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    // Register settings like a plain module: just DrawSettingsInternal(), without
    // the visibility radio and size/position controls a widget normally adds.
    void RegisterSettingsContent() override;

private:
    TexmodModule() = default;
};
