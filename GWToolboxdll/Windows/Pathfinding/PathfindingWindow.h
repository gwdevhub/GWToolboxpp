#pragma once

#include <ToolboxModule.h>
#include <Windows/Pathfinding/Pathing.h>

using CalculatedCallback = std::function<void (std::vector<GW::GamePos>& waypoints, void* args)>;

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
        float    path_recalc_distance = 5.f;               // game units the player must move before the quest path recomputes
        float    navmesh_sample_spacing = 5.f;             // gw between terrain-height samples when draping the overlay (lower = tighter to floor)
    };

    // Game units the player must move before the rendered quest path recomputes (persisted setting). Read by
    // QuestModule each tick; the recompute is still rate-capped by Update's 33ms throttle.
    static float GetPathRecalcDistance();

    // Diagnostic (harness): dump the current map's navmesh polys near `center` to log.txt. Triggers the full
    // build if the nav isn't ready yet (returns false so the caller can retry). Game-thread safe.
    static bool DebugDumpNavMeshNear(const GW::GamePos& center, float radius);

    // The current map's overlay navmesh, only if resident and fully built (else nullptr). Game/render-thread safe
    // (resident lookup only, never blocks on the DAT). Used by the in-world draper for per-sample plane resolution.
    static Pathing::NavMesh* GetResidentNavMesh();

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
    // True while one or more cross-map/world-map route computations are running on a worker thread. Lock-free;
    // safe to poll every frame (the world map shows a "calculating" indicator while true).
    static bool IsCalculatingPath();
    static clock_t CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args = nullptr);

    static void SetFrom(const GW::GamePos& pos);
    static void SetTo(const GW::GamePos& pos);

    // Set from world map coordinates (handles cross-map detection + DAT loading)
    static void SetFromWorldMap(const GW::Vec2f& world_map_pos);
    static void SetToWorldMap(const GW::Vec2f& world_map_pos);
    static void FindPath();

    static void ShowRouteToWorldMap(const GW::GamePos& from, const GW::Vec2f& goal_world_pos);
    // Remove any route previously drawn by ShowRouteToWorldMap / FindPath.
    static void ClearWorldMapRoute();

    // ---- Compute-only route API (no drawing). QuestModule owns + renders the points. ----

    static bool CalculateRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::Vec2f>* out);
    static bool RecalculateSegment(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::Vec2f>* out, std::vector<GW::GamePos>* out_game = nullptr);
    // True if `world_pos` falls within `map_id`'s game bounds (0 = current map).
    static bool IsWorldPosOnMap(const GW::Vec2f& world_pos, GW::Constants::MapID map_id = (GW::Constants::MapID)0);
    // True if `p` is the inter-map break sentinel in CalculateRoute output.
    static bool IsRouteBreak(const GW::Vec2f& p);

    // Robust file_hash lookup (GW::AreaInfo -> runtime table -> constant_maps_info);
    // GW::Map::GetMapInfo() alone returns 0 for outposts and many maps.
    static uint32_t GetMapFileId(GW::Constants::MapID map_id);

    static bool GetNextPortalToward(
        GW::Constants::MapID from_map,
        const GW::GamePos& from_pos,
        GW::Constants::MapID to_map,
        const GW::Vec2f& goal_world_pos,
        GW::GamePos& out_portal_pos);

private:

};
