#pragma once

#include <ToolboxWidget.h>
#include <Utils/FontLoader.h>

class DistanceWidget : public ToolboxWidget {
    DistanceWidget()
    {
        is_resizable = false;
        auto_size = true;
        has_titlebar = true;
    }
    ~DistanceWidget() override = default;

public:
    static DistanceWidget& Instance()
    {
        static DistanceWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Distance"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_RULER; }

    struct Settings {
        bool hide_in_outpost = false;
        float font_size_header = static_cast<float>(FontLoader::FontSize::header1);
        float font_size_perc_value = static_cast<float>(FontLoader::FontSize::widget_small);
        float font_size_abs_value = static_cast<float>(FontLoader::FontSize::widget_label);
        Colors::SettingColor color_adjacent = 0xFFFFFFFF;
        Colors::SettingColor color_nearby = 0xFFFFFFFF;
        Colors::SettingColor color_area = 0xFFFFFFFF;
        Colors::SettingColor color_earshot = 0xFFFFFFFF;
        Colors::SettingColor color_cast = 0xFFFFFFFF;
        Colors::SettingColor color_spirit = 0xFFFFFFFF;
        Colors::SettingColor color_compass = 0xFFFFFFFF;
    };

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
