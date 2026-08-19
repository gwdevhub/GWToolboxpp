#pragma once

#include <ToolboxWidget.h>
#include <GWCA/GameContainers/GamePos.h>

// Cartography helper, implementing numma_cway's rules (r/GuildWars, "Cartography Explained: Where
// to Stand for Any Sliver You Are Missing"): exploration is tracked per 32x32 world-map-unit tile,
// standing anywhere in a tile credits it plus the ring around it - Chebyshev distance, three rings
// with a Bird's Eye Compass - and nothing beyond the grid is creditable. So it points at tiles to
// stand in rather than at the fog, taking reachability from the current map's pathing data.
//
// It only ever draws: the tile grid, the tiles worth visiting, the suggested tile, queued fog
// points and a status line go on the world map and mission map, with a toggle button on the
// mission map and the extra options nested under the world map's own checkbox. Walking there is
// the player's business - nothing here touches the quest marker.
class CartographerWidget : public ToolboxWidget {
    CartographerWidget()
    {
        visible = false;
        can_show_in_main_window = false;
        has_closebutton = false;
        has_titlebar = false;
        is_resizable = false;
        is_movable = false;
    }

    ~CartographerWidget() override = default;

public:
    static CartographerWidget& Instance()
    {
        static CartographerWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Cartographer"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Works out which 32x32 world-map squares you need to stand in to uncover the fog around you, and draws them on the world map and mission map.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    static void SetEnabled(bool on);
    static bool GetEnabled();
    // The overlay options, drawn nested under the world map's own "Cartographer" checkbox so they
    // are only in the way when the helper is on.
    static void DrawWorldMapOptions();
    // World-map position of the current suggestion (fog tile or custom point); false if none.
    static bool GetCurrentTargetWorldPos(GW::Vec2f& out);
    static void SkipCurrentTarget(bool forever);
    static void AddCustomPoint(const GW::Vec2f& world_map_pos);
    static void RemoveCustomPointNear(const GW::Vec2f& world_map_pos, float max_dist_wm);
    static void ClearCustomPoints();
    static void ClearDeclined();
    static void GetStatus(char* buf, size_t len);
};
