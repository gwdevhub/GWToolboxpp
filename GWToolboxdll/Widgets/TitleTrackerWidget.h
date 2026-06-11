#pragma once

#include <ToolboxWidget.h>

struct IDirect3DDevice9;
namespace GW {
    struct Title;
    namespace Constants {
        enum class TitleID : uint32_t;
    }
}


class TitleTrackerWidget : public ToolboxWidget {
    TitleTrackerWidget() = default;
    ~TitleTrackerWidget() override = default;

public:
    static TitleTrackerWidget& Instance()
    {
        static TitleTrackerWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Title Tracker"; }

    [[nodiscard]] const char* Description() const override { return "Shows on-screen progress bar of selected title progress without having to open the Guild Wars Hero window all the time"; }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_CHART_BAR; }

    struct Settings {
        std::string show_title_progress_widgets_str;
        bool automatically_show_title_progress_for_current_map = true;
        bool hide_completed_titles = true;
        bool override_title_sort_order = true;
        bool show_overall_title_progress = false;
        Colors::SettingColor progress_bar_background_color = IM_COL32(30, 30, 30, 255);
        Colors::SettingColor title_label_color = IM_COL32_WHITE;
        Colors::SettingColor tier_label_color = IM_COL32_WHITE;
        Colors::SettingColor progress_bar_foreground_color = IM_COL32(110, 83, 123, 255);
        Colors::SettingColor progress_overlay_label_color = IM_COL32(255, 255, 255, 255);
        float progress_bar_height = 24.f;
    };

    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;

    float GetTitleProgressRatio(GW::Constants::TitleID title_id, GW::Title* title, bool current_tier = false);

    void SignalTerminate() override;

    bool CanTerminate() override;

    void DrawSettingsInternal() override;

    void SaveSettings(SettingsDoc& doc) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
};
