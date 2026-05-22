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

    [[nodiscard]] const char* Name() const override { return "Bounty Kill Tracker"; }

    [[nodiscard]] const char* Description() const override
    {
        return "Overlays a kill counter on active bounty effect icons, similar to the vanquish counter on the Hard Mode effect icon";
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_SKULL; }

    void Initialize() override;
    void Terminate() override;
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
