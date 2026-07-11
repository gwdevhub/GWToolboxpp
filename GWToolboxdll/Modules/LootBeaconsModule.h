#pragma once

#include <ToolboxModule.h>

// Draws a beacon (vertical pillar of light + draped base ring) in the 3D world on dropped items
// worth picking up - by rarity and/or trader value - via the shared GameWorldCompositor, so the
// beacons composite under the in-game UI and are depth-tested against the scene.
class LootBeaconsModule : public ToolboxModule {
    LootBeaconsModule() = default;
    ~LootBeaconsModule() override = default;

public:
    static LootBeaconsModule& Instance()
    {
        static LootBeaconsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Loot Beacons"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Draws a pillar of light in the game world on valuable drops (by rarity or trader price).";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GEM; }

    void Initialize() override;
    void SignalTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

private:
    static void RegisterSettings(ToolboxModule* module);
    static void DrawInWorld(IDirect3DDevice9* device);
};
