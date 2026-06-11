#pragma once

#include <ToolboxWidget.h>

class EffectsMonitorWidget : public ToolboxWidget {
    EffectsMonitorWidget() { is_movable = is_resizable = false; }
    ~EffectsMonitorWidget() override = default;

public:
    static EffectsMonitorWidget& Instance()
    {
        static EffectsMonitorWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override
    {
        return "Effect Durations";
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_HISTORY; }

    struct Settings {
        float font_effects = 18.f;
        Colors::SettingColor color_text_effects = Colors::White();
        Colors::SettingColor color_background = Colors::ARGB(128, 0, 0, 0);
        Colors::SettingColor color_text_shadow = Colors::Black();
        // duration -> string settings
        int decimal_threshold = 600;   // when to start displaying decimals
        int only_under_seconds = 3600; // hide effect durations over n seconds
        bool round_up = true;          // round up or down?
        bool show_vanquish_counter = true;
        bool track_spirit_effects = false;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    void Draw(IDirect3DDevice9* pDevice) override;

};
