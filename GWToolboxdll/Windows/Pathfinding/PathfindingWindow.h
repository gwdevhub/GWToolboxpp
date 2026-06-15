#pragma once

#include <ToolboxWindow.h>
#include <Windows/Pathfinding/Pathing.h>

using CalculatedCallback = std::function<void (std::vector<GW::GamePos>& waypoints, void* args)>;

/*
    This should really have been a module to just manage pathing - its used in a lot of places.
    Instead, disable the Draw function in release
*/
class PathfindingWindow : public ToolboxWindow {
    PathfindingWindow() = default;
    ~PathfindingWindow() = default;

public:
    static PathfindingWindow& Instance()
    {
        static PathfindingWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Pathfinding Window"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_DOOR_OPEN; }
    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }

    bool HasSettings() { return true; }

    void Draw(IDirect3DDevice9* pDevice) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Initialize() override;
    void Terminate() override;
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

    // ---- Compute-only route API (no drawing). Used by QuestModule, which owns the
    // resulting points and renders them as the quest path. ----

    // Blocking. Compute the full cross-map route between two world-map positions and
    // fill `out` with the points in current-map coords (a PATH_BREAK sentinel — see
    // IsRouteBreak — separates maps). Caches the current-map exit portal + downstream
    // tail so RecalculateRouteLeg can cheaply refresh the player's leg. Returns false
    // if no route. Blocks on pathing builds — call from a worker thread.
    static bool CalculateRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::GamePos>* out);
    // Blocking. Re-walk only from_player -> the cached current-map exit portal and fill
    // `out` with that leg + the cached downstream tail. False if there is no cached
    // route for the current map (caller should fall back to CalculateRoute).
    static bool RecalculateRouteLeg(const GW::GamePos& from_player, std::vector<GW::GamePos>* out);
    // True if a cached route exists for the current map (RecalculateRouteLeg will work).
    static bool HasRouteForCurrentMap();
    // Forget the cached route.
    static void ClearRoute();
    // True if `p` is the inter-map break sentinel in CalculateRoute output.
    static bool IsRouteBreak(const GW::GamePos& p);

    // Robust file_hash lookup — falls through GW::AreaInfo, the runtime
    // packet-populated table, and constant_maps_info. GW::Map::GetMapInfo()
    // alone returns 0 for outposts and many maps; use this everywhere a
    // file_hash is needed (e.g. matching outpost+explorable variants).
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
