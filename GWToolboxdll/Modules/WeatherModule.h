#pragma once

#include <ToolboxModule.h>

// Camera-anchored weather (rain) drawn into the 3D world via the shared GameWorldCompositor:
// composited under the in-game UI and depth-tested against the terrain. Its own settings section.
class WeatherModule : public ToolboxModule {
    WeatherModule() = default;
    ~WeatherModule() override = default;

public:
    static WeatherModule& Instance()
    {
        static WeatherModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Weather"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Draws camera-anchored rain into the 3D game world - composited under the in-game UI and occluded by terrain.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TINT; }

    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    // Reset the live weather to an idle state and rebuild it: all drops, splashes and settled marks are
    // cleared so any active condition reseeds fresh on the next update. Conditions/settings are untouched.
    // Safe to call from any thread - the reset is deferred to the next render tick.
    void Reset();

private:
    static void RegisterSettings(ToolboxModule* module);
    static void OnSettingsLoaded();
    static void DrawSettings();

    // The in-world draw, registered with the shared compositor while the module is enabled.
    static void DrawInWorld(IDirect3DDevice9* device);
};
