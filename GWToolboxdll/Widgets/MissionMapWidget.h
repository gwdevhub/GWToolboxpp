#pragma once

#include <ToolboxWidget.h>

class MissionMapWidget : public ToolboxWidget {
    MissionMapWidget()
    {
        can_show_in_main_window = false;
        has_closebutton = false;
        is_resizable = false;
        is_movable = false;
    }
    ~MissionMapWidget() override = default;

public:
    static MissionMapWidget& Instance()
    {
        static MissionMapWidget w;
        return w;
    }
    [[nodiscard]] const char* Name() const override { return "Mission Map"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void Terminate() override;
    bool WndProc(UINT Message, WPARAM, LPARAM lParam) override;
};
