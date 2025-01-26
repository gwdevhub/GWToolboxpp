#pragma once

#include <ToolboxWidget.h>

class DistanceWidget : public ToolboxWidget {
    DistanceWidget()
    {
        is_resizable = false;
        auto_size = true;
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

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    bool hide_in_outpost = false;
    bool show_abs_value = true;
    bool show_perc_value = true;
    bool show_header = true;
    float font_scale = 1.f;

    Color color_adjacent = 0xFFFFFFFF;
    Color color_nearby = 0xFFFFFFFF;
    Color color_area = 0xFFFFFFFF;
    Color color_earshot = 0xFFFFFFFF;
    Color color_cast = 0xFFFFFFFF;
    Color color_spirit = 0xFFFFFFFF;
    Color color_compass = 0xFFFFFFFF;
};
