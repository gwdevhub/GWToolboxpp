#pragma once

#include <ToolboxWidget.h>

class VanquishMapOverlayWidget : public ToolboxWidget {
    VanquishMapOverlayWidget()
    {
        visible = false;
        can_show_in_main_window = false;
        has_closebutton = false;
        has_titlebar = false;
        is_resizable = false;
        is_movable = false;
    }
    ~VanquishMapOverlayWidget() override = default;

public:
    static VanquishMapOverlayWidget& Instance()
    {
        static VanquishMapOverlayWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Vanquish Overlay"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SKULL; }

    void Initialize() override;
    void Draw(IDirect3DDevice9* pDevice) override;
    
    void Update(float delta) override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    static void GetTrackedEnemyCounts(int& alive, int& stale);
    static bool IsOverlayActive();
    static bool IsNavigating();
    static void StopNavigating();
    static bool ContextMenuItems();
private:
    void DrawVanquishToggleButton();
};
