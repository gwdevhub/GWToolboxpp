#pragma once

#include <ToolboxModule.h>
#include <Windows/Pathfinding/Pathing.h>

using CalculatedCallback = std::function<void (std::vector<GW::GamePos>& waypoints, void* args)>;

/*
    Pathing manager (a module, NOT a window): builds/queries the visgraph, drives the optional navmesh overlay,
    and exposes the cross-map route API used by QuestModule/WorldMapWidget. It has no window of its own — its
    only UI is its settings page. Per-frame work runs in Draw() (called for every enabled module).
    (The class is still named PathfindingWindow because it's referenced as a static API in several places.)
*/
class PathfindingWindow : public ToolboxModule {
    PathfindingWindow() = default;
    ~PathfindingWindow() = default;

public:
    static PathfindingWindow& Instance()
    {
        static PathfindingWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Pathfinding"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_DOOR_OPEN; }

    bool HasSettings() { return true; }

    struct Settings {
        bool     draw_navmesh_overlay = false;
        uint32_t navmesh_wall_color = 0xC0FF3030;          // ARGB: wall edge on plane 0 (red)
        uint32_t navmesh_wall_color_hi = 0xC0FF30FF;       // wall edge on planes != 0 (magenta)
        uint32_t navmesh_connection_color = 0x6030FF30;    // connection edge on plane 0 (green)
        uint32_t navmesh_connection_color_hi = 0x6030C0FF; // connection edge on planes != 0 (cyan)
        float    path_recalc_distance = 100.f;              // game units the player must move before the quest path recomputes
    };

    // Game units the player must move before the rendered quest path recomputes (persisted setting). Read by
    // QuestModule each tick; the recompute is still rate-capped by Update's 33ms throttle.
    static float GetPathRecalcDistance();

    // Diagnostic (harness): dump the current map's navmesh polys near `center` to log.txt. Triggers the full
    // build if the nav isn't ready yet (returns false so the caller can retry). Game-thread safe.
    static bool DebugDumpNavMeshNear(const GW::GamePos& center, float radius);

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Initialize() override;
    void Terminate() override;
    // True while the module is initialised and not terminating. Lock-free; safe to poll every frame.
    // Quest pathing checks this before touching the route API (the module is now optional).
    static bool IsPathingEnabled();
    // False if still calculating current map
    static bool ReadyForPathing();
    // False if still calculating current map
    static clock_t CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args = nullptr);

    static void SetFrom(const GW::GamePos& pos);
    static void SetTo(const GW::GamePos& pos);

    // Set from world map coordinates (handles cross-map detection + DAT loading)
    static void SetFromWorldMap(const GW::Vec2f& world_map_pos);
    static void SetToWorldMap(const GW::Vec2f& world_map_pos);
    static void FindPath();

    // Compute and draw the full cross-map route from `from` (in the current map)
    // to goal_world_pos (world-map coords) as lines on the world map + minimap.
    // Convenience wrapper around SetFrom + SetToWorldMap + FindPath.
    static void ShowRouteToWorldMap(const GW::GamePos& from, const GW::Vec2f& goal_world_pos);
    // Remove any route previously drawn by ShowRouteToWorldMap / FindPath.
    static void ClearWorldMapRoute();

    // ---- Compute-only route API (no drawing). QuestModule owns + renders the points. ----

    // Blocking (worker). Full cross-map route between two world-map positions into `out`,
    // in WORLD coords (IsRouteBreak sentinel between maps) to avoid foreign-map projection
    // overflow. False if no route.
    static bool CalculateRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::Vec2f>* out);
    // Blocking (worker). A* across `map_id` (0 = current) from `from` to `to` (that map's
    // game coords) into `out` as WORLD coords. Pass the leg's map explicitly so a deferred
    // recompute isn't tied to where the player has since wandered. Holds no shared state —
    // the caller splices the untouched remainder of its route on. False if no path.
    // out_game (optional): the raw CURRENT-map A* leg in that map's game coords, with each waypoint's
    // pathfinder zplane preserved. `out` flattens to world coords (losing the plane); out_game keeps it so
    // the caller can drape the rendered line on the surface the path traverses. Only meaningful when map_id
    // resolves to the current map (the only case callers request it).
    static bool RecalculateSegment(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::Vec2f>* out, std::vector<GW::GamePos>* out_game = nullptr);
    // True if `world_pos` falls within `map_id`'s game bounds (0 = current map).
    static bool IsWorldPosOnMap(const GW::Vec2f& world_pos, GW::Constants::MapID map_id = (GW::Constants::MapID)0);
    // True if `p` is the inter-map break sentinel in CalculateRoute output.
    static bool IsRouteBreak(const GW::Vec2f& p);

    // Robust file_hash lookup (GW::AreaInfo -> runtime table -> constant_maps_info);
    // GW::Map::GetMapInfo() alone returns 0 for outposts and many maps.
    static uint32_t GetMapFileId(GW::Constants::MapID map_id);

    // Resolve the next portal in `from_map` along the multi-map route to a
    // point on `to_map`. Out-param is in `from_map` game coords. Returns false
    // if no route exists or if from_map shares a file_hash with to_map (same
    // map, no portal needed). Blocks on visgraph builds — call from a worker.
    static bool GetNextPortalToward(
        GW::Constants::MapID from_map,
        const GW::GamePos& from_pos,
        GW::Constants::MapID to_map,
        const GW::Vec2f& goal_world_pos,
        GW::GamePos& out_portal_pos);

private:

};
