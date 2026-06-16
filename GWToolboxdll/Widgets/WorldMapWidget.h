#pragma once

#include <ToolboxWidget.h>

namespace GW {
    struct GamePos;
    struct Vec2f;
    namespace Constants {
        enum class MapID : uint32_t;
    }
}

class WorldMapWidget : public ToolboxWidget {
    WorldMapWidget() = default;
    ~WorldMapWidget() override = default;

public:
    static WorldMapWidget& Instance()
    {
        static WorldMapWidget w;
        return w;
    }

    void Initialize() override;

    void SignalTerminate() override;

    void RegisterSettingsContent() override { };

    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }
    [[nodiscard]] const char* Name() const override { return "World Map"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    struct Settings {
        bool showing_all_outposts = false;
        bool highlight_locked_areas = false;
        bool show_lines_on_world_map = false;
        bool showing_all_quests = true;
        bool apply_quest_colors = false;
        Colors::SettingColor locked_area_highlight_color = IM_COL32(255, 160, 0, 96);
        bool hide_captured_elites = false;
        bool show_any_elite_capture_locations = false;
        // Bitmask backing the per-profession show_elite_capture_locations runtime array
        unsigned int show_elite_capture_locations_val = 0xffffffff;
    };

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static void ShowAllOutposts(bool show);
    static GW::Constants::MapID GetMapIdForLocation(const GW::Vec2f& world_map_pos, GW::Constants::MapID exclude_map_id = (GW::Constants::MapID)0);
    // World-map position where `map_id`'s marker/label is shown (used to point a quest at
    // its destination map when the objective is on another map). False if it has none.
    static bool GetMapMarkerWorldPos(GW::Constants::MapID map_id, GW::Vec2f& out);
    // Convert between game and world-map coords for `map_id` (0 = current); game bounds
    // come from the cached DAT.
    static bool WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos, GW::Constants::MapID map_id = (GW::Constants::MapID)0);
    static bool GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f& world_map_pos, GW::Constants::MapID map_id = (GW::Constants::MapID)0);
    static bool& ShowLinesOnWorldMap();
};
