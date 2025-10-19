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

    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;

    float GetTitleProgressRatio(GW::Constants::TitleID title_id, GW::Title* title, bool current_tier = false);

    void SignalTerminate() override;

    bool CanTerminate() override;

    void DrawSettingsInternal() override;

    void SaveSettings(ToolboxIni* ini) override;

    void LoadSettings(ToolboxIni* ini) override;
};
