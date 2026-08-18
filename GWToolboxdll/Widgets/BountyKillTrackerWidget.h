#pragma once

#include <ToolboxWidget.h>

class BountyKillTrackerWidget : public ToolboxWidget {
    BountyKillTrackerWidget() { is_movable = is_resizable = false; }
    ~BountyKillTrackerWidget() override = default;

public:
    static BountyKillTrackerWidget& Instance()
    {
        static BountyKillTrackerWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "赏金击杀追踪"; }

    [[nodiscard]] const char* Description() const override
    {
        return "在激活的赏金效果图标上覆盖击杀计数器，类似于困难模式效果图标上的征服计数器";
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_SKULL; }

    struct Settings {
        float font_size = 18.f;
        Colors::SettingColor color_text = Colors::White();
        Colors::SettingColor color_text_shadow = Colors::Black();
        Colors::SettingColor color_background = Colors::ARGB(128, 0, 0, 0);
    };

    void Initialize() override;
    void Terminate() override;
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
