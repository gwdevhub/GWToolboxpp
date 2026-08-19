#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameContainers/GamePos.h>

// Debug-only cartography helper, implementing numma_cway's rules (r/GuildWars, "Cartography
// Explained: Where to Stand for Any Sliver You Are Missing"): exploration is tracked per 32x32
// world-map-unit tile, standing anywhere in a tile credits it plus the ring around it - Chebyshev
// distance, three rings with a Bird's Eye Compass - and nothing beyond the grid is creditable.
// So the module routes to tiles to stand in rather than at the fog, taking reachability from the
// current map's pathing data.
//
// It only ever draws: the tile grid, the tiles worth visiting, the suggested tile, queued fog
// points and a status line go on the world map and mission map, and the right-click context menu
// of either manages the helper (toggle, add/remove fog points, skip suggestions). Walking there
// is the player's business - nothing here touches the quest marker.
class CartographerModule : public ToolboxModule {
    CartographerModule() = default;
    ~CartographerModule() override = default;

public:
    static CartographerModule& Instance()
    {
        static CartographerModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Cartographer Helper"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Works out which 32x32 world-map squares you need to stand in to uncover the fog around you, and draws them on the world map and mission map.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    static void SetEnabled(bool on);
    static bool GetEnabled();
    // World-map position of the current suggestion (fog cell or custom point); false if none.
    static bool GetCurrentTargetWorldPos(GW::Vec2f& out);
    static void SkipCurrentTarget(bool forever);
    static void AddCustomPoint(const GW::Vec2f& world_map_pos);
    static void RemoveCustomPointNear(const GW::Vec2f& world_map_pos, float max_dist_wm);
    static void ClearCustomPoints();
    static void ClearDeclined();
    static void GetStatus(char* buf, size_t len);
};
