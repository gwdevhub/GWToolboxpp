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

    struct Settings {
        Colors::SettingColor vq_color_inaccessible = IM_COL32(0, 0, 0, 190);
        Colors::SettingColor vq_color_fog_unexplored = IM_COL32(0, 0, 0, 140);
        Colors::SettingColor vq_color_border = IM_COL32(200, 220, 255, 160);
        Colors::SettingColor vq_color_frontier = IM_COL32(255, 200, 50, 200);
        Colors::SettingColor vq_color_compass = IM_COL32(180, 220, 255, 100);
        Colors::SettingColor vq_color_enemy_alive = IM_COL32(70, 130, 255, 255);
        Colors::SettingColor vq_color_enemy_stale = IM_COL32(255, 180, 50, 180);
        Colors::SettingColor vq_color_enemy_outline = IM_COL32(0, 0, 0, 200);
        float vq_enemy_marker_size = 9.0f;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;
    void Terminate() override;
    void DrawSettingsInternal() override;

    static void GetTrackedEnemyCounts(int& alive, int& stale);
    static bool IsOverlayActive();
    static bool IsNavigating();
    static void StopNavigating();
    static bool ContextMenuItems();
private:
    void DrawVanquishToggleButton();
};
