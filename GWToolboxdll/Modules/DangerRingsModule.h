#pragma once

#include <ToolboxModule.h>

// Draws the hostile ground AoE effects tracked by Utils/AoeEffects (Meteor Shower, Maelstrom,
// traps, ...) as rings draped onto the terrain in the 3D world, via the shared
// GameWorldCompositor: composited under the in-game UI and depth-tested against the scene.
class DangerRingsModule : public ToolboxModule {
    DangerRingsModule() = default;
    ~DangerRingsModule() override = default;

public:
    static DangerRingsModule& Instance()
    {
        static DangerRingsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "危险范围指示"; }
    [[nodiscard]] const char* Description() const override
    {
        return "在游戏世界中，将敌人的范围效果（如流星雨、陷阱等）以危险光环的形式绘制在地面上。";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BULLSEYE; }

    void Initialize() override;
    void SignalTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

private:
    static void RegisterSettings(ToolboxModule* module);
    static void DrawInWorld(IDirect3DDevice9* device);
};
