#pragma once

#include <ToolboxModule.h>

// Cosmetic lava "rivers" drawn into the 3D world via the shared GameWorldCompositor: a lava-textured
// ribbon draped along an authored polyline on the floor, composited under the in-game UI and depth-tested
// against the terrain. Reuses GW's own lava texture from the .dat (configurable file id). Its own settings
// section; rivers are authored in-game (drop points at the player) and persisted per-map in LavaRivers.json.
class RiverModule : public ToolboxModule {
    RiverModule() = default;
    ~RiverModule() override = default;

public:
    static RiverModule& Instance()
    {
        static RiverModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Lava Rivers"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Draws flowing lava-textured rivers onto the world floor - composited under the in-game UI and occluded by terrain.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_FIRE; }

    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

private:
    static void RegisterSettings(ToolboxModule* module);
    static void DrawSettings();

    // The in-world draw, registered with the shared compositor while the module is enabled.
    static void DrawInWorld(IDirect3DDevice9* device);
};
