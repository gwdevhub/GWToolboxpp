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

    [[nodiscard]] const char* Name() const override { return "天气"; }
    [[nodiscard]] const char* Description() const override
    {
        return "将视角定位的雨水效果引入3D游戏世界 —— 在游戏内用户界面下进行合成，并由地形遮挡。";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TINT; }

    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    void Reset();

private:
    static void RegisterSettings(ToolboxModule* module);
    static void OnSettingsLoaded();
    static void DrawSettings();

    // The in-world draw, registered with the shared compositor while the module is enabled.
    static void DrawInWorld(IDirect3DDevice9* device);
};
