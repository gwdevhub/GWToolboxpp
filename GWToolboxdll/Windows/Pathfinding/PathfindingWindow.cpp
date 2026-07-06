#include "stdafx.h"

#include <atomic>
#include <list>
#include <mutex>
#include <queue>
#include <set>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Timer.h>
#include <filesystem>
#include <fstream>
#include <numeric>

#include <EmbeddedResource.h>
#include <GWToolbox.h>
#include <ImGuiAddons.h>
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/MapContext.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Utils/ArenaNetFileParser.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/NavMesh.h> // viz-only navmesh for the debug overlay

#include "OpenTyriaPathfinder.h"
#include "PathingLog.h"
#include "PathingMapData.h"
#include "PathingMapDataLoader.h"
#include "PortalConnections.h"
#include "maps_constant_data.h"
#include "resource.h"

#include <GWCA/Context/WorldContext.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>

// Define PATHING_VERBOSE to re-enable this file's per-frame Log::Info chatter; PATH_LOG_ERROR/WARNING still reach chat in debug.
// #define PATHING_VERBOSE 1
#ifdef PATHING_VERBOSE
#define PATH_LOG_INFO(...) Log::Info(__VA_ARGS__)
#else
#define PATH_LOG_INFO(...) ((void)0)
// Locals/params that exist only to be formatted into PATH_LOG_INFO are unused once logging is compiled out.
#pragma warning(disable : 4189 4100)
#endif

namespace {
    struct CachedMapInfo {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        Pathing::Vec2f bounds_min{}, bounds_max{}; // game coords
        std::vector<Pathing::PortalProp> portal_props;
    };

    // Per-map caches (low 32 bits = file_hash), LRU-bounded — see MAX_CACHED_MAPS.
    std::unordered_map<uint64_t, Pathing::MilePath*> mile_paths_by_coords;
    std::unordered_map<uint64_t, CachedMapInfo> cached_map_info;

    // Trapezoid pathfinders share the same file_hash key as mile_paths_by_coords.
    // Lazy: populated on first call to ClosestPortalTrapezoidDistanceInMap for a map.
    std::unordered_map<uint64_t, OpenTyria::TrapezoidPathfinder*> trapezoid_pf_by_coords;

    // Serializes whole route computations: the global route caches assume one build at a time (concurrent builds read
    // half-inserted MilePaths → crash). Game thread try-locks so it never blocks; recursive since build helpers re-enter.
    std::recursive_mutex route_mutex;

    // Cache of portal props per file_hash (lightweight, loaded on demand).
    std::unordered_map<uint32_t, std::vector<Pathing::PortalProp>> portal_props_cache;

    // ===== LRU eviction for the per-map caches =====
    // Resident foreign-map cap (current map pinned on top). Memory/recompute trade-off.
    constexpr size_t MAX_CACHED_MAPS = 12;

    std::list<uint64_t> lru_order; // mile_paths_by_coords keys, front = most recent
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> lru_pos;

    // Guards lru_* and route_jobs_active; eviction may only run when no route worker holds a MilePath (route_jobs_active == 0).
    std::mutex lru_mutex;
    int route_jobs_active = 0;

    void TouchLru(uint64_t key)
    {
        std::scoped_lock lock(lru_mutex);
        auto it = lru_pos.find(key);
        if (it != lru_pos.end()) lru_order.erase(it->second);
        lru_order.push_front(key);
        lru_pos[key] = lru_order.begin();
    }

    void EnforceCacheLimitIfIdle(); // GetMapFileId fwd decl must be in scope first

    // RAII guard: defers eviction while it holds MilePath*, then trims the cache on the main thread (renderer reads cached_map_info there).
    struct RouteJobScope {
        RouteJobScope()
        {
            std::scoped_lock lock(lru_mutex);
            ++route_jobs_active;
        }
        ~RouteJobScope()
        {
            {
                std::scoped_lock lock(lru_mutex);
                --route_jobs_active;
            }
            Resources::EnqueueMainTask([] {
                EnforceCacheLimitIfIdle();
            });
        }
        RouteJobScope(const RouteJobScope&) = delete;
        RouteJobScope& operator=(const RouteJobScope&) = delete;
    };

    // In-flight world-map route/path computations. Each async route worker holds a PathCalcScope; the world map
    // polls IsCalculatingPath() to show a "calculating" indicator while one is running. Atomic: incremented on
    // worker threads, read on the game/render thread.
    std::atomic<int> path_calc_in_flight = 0;
    struct PathCalcScope {
        PathCalcScope() { ++path_calc_in_flight; }
        ~PathCalcScope() { --path_calc_in_flight; }
        PathCalcScope(const PathCalcScope&) = delete;
        PathCalcScope& operator=(const PathCalcScope&) = delete;
    };

    bool draw_map_bounds = false;
    bool draw_graph_edges = false;
    bool draw_portals = false;
    // Portal-portal hop metric for the route picker: Euclidean (straight line), Trapezoid (graph AStar, fast), Walk (visgraph, accurate/slow).
    enum class PortalDistanceMode { Euclidean = 0, Trapezoid = 1, Walk = 2 };
    PortalDistanceMode portal_distance_mode = PortalDistanceMode::Trapezoid;
    GW::GamePos ToCurrentMapCoords(const GW::GamePos& pos, GW::Constants::MapID src_map); // forward decl (early)
    Pathing::MilePath* LoadMapFromDAT(GW::Constants::MapID map_id, bool allow_load = true); // forward decl (early)
    void BuildMapGraph();                                                                 // forward decl (early)

    // Walk-cost cache keyed by (owner_map, pos, other_map); cleared per FindMapRoute call so prior queries can't poison later ones.
    struct PortalWalkKey {
        uint32_t mode; // PortalDistanceMode value, so cached costs from a previous mode aren't reused
        uint32_t owner;
        uint32_t other;
        float x, y;
        bool operator==(const PortalWalkKey& o) const = default;
    };
    struct PortalWalkKeyHash {
        size_t operator()(const PortalWalkKey& k) const
        {
            size_t h = std::hash<uint32_t>{}(k.mode);
            h = h * 31 + std::hash<uint32_t>{}(k.owner);
            h = h * 31 + std::hash<uint32_t>{}(k.other);
            h = h * 31 + std::hash<uint32_t>{}(*reinterpret_cast<const uint32_t*>(&k.x));
            h = h * 31 + std::hash<uint32_t>{}(*reinterpret_cast<const uint32_t*>(&k.y));
            return h;
        }
    };
    std::unordered_map<PortalWalkKey, float, PortalWalkKeyHash> portal_walk_cache;
    void ClearEditorHighlightLines();                                                     // forward decl (early)
    void UpdatePortalMarkers();                                                           // forward decl (early)
    void UpdateBoundsLines();                                                             // forward decl (early)
    void EnsureLightweightMapInfo(GW::Constants::MapID map_id, const char* caller = "?"); // forward decl (early)
    const CachedMapInfo* GetCachedMapInfo(GW::Constants::MapID map_id);                   // forward decl (early)
    uint32_t GetMapFileId(GW::Constants::MapID map_id);                                   // forward decl (early)

    bool IsOutpostMap(GW::Constants::MapID map_id); // forward decl

    // Diagnostic: only log when these MapIDs are touched; edit the set to chase a different unintended-load case.
    inline bool IsInterestingMapForCacheTrace(GW::Constants::MapID mid)
    {
        return mid == (GW::Constants::MapID)114 || mid == (GW::Constants::MapID)153;
    }

    // Cache sibling MapIDs sharing this file_hash. Skip outposts: spreading to them would draw stray bounds rectangles
    // at every sibling outpost icon (e.g. fh 0xce65 → 38, 119); their lookups still resolve via GetCachedMapInfo's fallback.
    void CacheSharedFileHashMaps(const CachedMapInfo& source_info)
    {
        uint32_t fh = GetMapFileId(source_info.map_id);
        if (!fh) return;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if ((uint32_t)file_hash != fh) continue;
            for (const auto& entry : entries) {
                auto mid = entry.map_id;
                if (mid == source_info.map_id) continue;
                if (IsOutpostMap(mid)) continue; // see comment above
                bool exists = false;
                for (const auto& [h, inf] : cached_map_info) {
                    if (inf.map_id == mid) {
                        exists = true;
                        break;
                    }
                }
                if (exists) continue;
                uint64_t new_hash = static_cast<uint64_t>(fh) | (static_cast<uint64_t>((uint32_t)mid) << 32);
                CachedMapInfo shared;
                shared.map_id = mid;
                shared.bounds_min = source_info.bounds_min;
                shared.bounds_max = source_info.bounds_max;
                shared.portal_props = source_info.portal_props;
                cached_map_info[new_hash] = shared;
                if (IsInterestingMapForCacheTrace(mid)) {
                    PATH_LOG_INFO("[CacheTrace] CacheSharedFileHashMaps spread mid=%d source=%d fh=0x%X", (int)mid, (int)source_info.map_id, fh);
                }
            }
        }
    }

    // Free every cached allocation for one map and drop it from the LRU.
    // Caller must hold lru_mutex.
    void EvictMapByKey(uint64_t mile_key)
    {
        const uint32_t fh = (uint32_t)(mile_key & 0xFFFFFFFF);

        // Pathfinder holds a raw pointer into the MilePath's map data — free it first.
        if (auto it = trapezoid_pf_by_coords.find(fh); it != trapezoid_pf_by_coords.end()) {
            delete it->second;
            trapezoid_pf_by_coords.erase(it);
        }
        if (auto it = mile_paths_by_coords.find(mile_key); it != mile_paths_by_coords.end()) {
            delete it->second;
            mile_paths_by_coords.erase(it);
        }
        // Several cached_map_info entries can share a file_hash (sibling-map spread).
        std::erase_if(cached_map_info, [fh](const auto& kv) {
            return (uint32_t)(kv.first & 0xFFFFFFFF) == fh;
        });
        portal_props_cache.erase(fh);

        if (auto it = lru_pos.find(mile_key); it != lru_pos.end()) {
            lru_order.erase(it->second);
            lru_pos.erase(it);
        }
    }

    // Trim to MAX_CACHED_MAPS (LRU first, current map pinned), only when idle so no worker is mid-route holding a freed MilePath*.
    void EnforceCacheLimitIfIdle()
    {
        std::vector<uint64_t> evicted;
        {
            std::scoped_lock lock(lru_mutex);
            if (route_jobs_active > 0) return;
            // Current-map keys low32 = MapID, foreign-map keys low32 = file_hash — pin against both forms.
            const uint32_t cur_fh = GetMapFileId(GW::Map::GetMapID());
            const uint32_t cur_mid = (uint32_t)GW::Map::GetMapID();
            auto is_current = [&](uint64_t key) {
                const uint32_t low = (uint32_t)(key & 0xFFFFFFFF);
                return (cur_fh && low == cur_fh) || low == cur_mid;
            };
            size_t over = lru_order.size() > MAX_CACHED_MAPS ? lru_order.size() - MAX_CACHED_MAPS : 0;
            for (auto it = lru_order.rbegin(); over > 0 && it != lru_order.rend(); ++it) {
                if (is_current(*it)) continue;
                evicted.push_back(*it);
                --over;
            }
            for (uint64_t key : evicted)
                EvictMapByKey(key);
        }
        if (!evicted.empty()) {
            // Evicted maps' bounds/portal lines need a refresh (main-thread line pool).
            Resources::EnqueueMainTask([] {
                UpdateBoundsLines();
                if (draw_portals) UpdatePortalMarkers();
            });
        }
    }

    // LoadAllMapsAtPosition defined later in this namespace

    // Portal connection editor
    Pathing::PortalConnections portal_connections;

    struct EditorEndpoint {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Vec2f pos{};
        int zplane = 0;
        bool set = false;
    };

    // NPC editor fields (per-endpoint). Stores either an AgentLiving (player_number)
    // or AgentGadget (gadget_id) depending on agent_kind.
    struct EditorNpcFields {
        uint32_t agent_id = 0;
        uint32_t model_id = 0;
        char name[256] = {};
        uint32_t last_target_id = 0;
        Pathing::AgentKind agent_kind = Pathing::AgentKind::Living;
    };

    // Per-widget state for two-step selection (map → portal)
    struct MapSearchState {
        int pending_map_id = 0; // map selected, waiting for portal pick
        std::vector<Pathing::PortalProp> pending_portals;
    };
    // =========================================================================
    // Unified editor state — all From/To endpoint data in one struct for easy snapshotting and swap.
    // =========================================================================
    struct EndpointEditorState {
        EditorEndpoint endpoint;
        int type = 0; // ConnectionType combo index
        EditorNpcFields npc;
        char search_buf[128] = {};
        MapSearchState search_state;
    };

    struct ConnectionEditorState {
        EndpointEditorState from;
        EndpointEditorState to;
        char notes[256] = {};
        bool no_draw = false;
        bool one_way = false;
        int selected_connection = -1;
    };

    ConnectionEditorState editor;

    // Backward-compat references so existing code doesn't need mass-rename.
    auto& editor_from = editor.from.endpoint;
    auto& editor_to = editor.to.endpoint;

    bool pending_connection_lines_update = false;


    // Deferred line removal so we don't invalidate iterators during WorldMapWidget::Draw; drained next Draw.
    // Stale pointers are safe: RemoveCustomLines only frees pointers still in the live list, never dereferencing the queue.
    std::vector<CustomRenderer::CustomLine*> pending_line_removals;

    void DeferRemoveLines(std::vector<CustomRenderer::CustomLine*>& lines)
    {
        pending_line_removals.insert(pending_line_removals.end(), lines.begin(), lines.end());
        lines.clear();
    }

    void ProcessDeferredRemovals()
    {
        if (pending_line_removals.empty()) return;
        // Single O(N) pass: per-line RemoveCustomLine is O(N) each (linear find + vector erase), so removing the
        // navmesh overlay's thousands of lines one-by-one was O(N^2) and a big chunk of the map-load hitch.
        Minimap::Instance().custom_renderer.RemoveCustomLines(pending_line_removals);
        pending_line_removals.clear();
    }

    std::vector<CustomRenderer::CustomLine*> saved_connection_lines;

    void ClearSavedConnectionLines()
    {
        DeferRemoveLines(saved_connection_lines);
    }

    bool connections_changed = false; // set when connections modified, consumed by BuildMapGraph callers

    void UpdateSavedConnectionLines()
    {
        connections_changed = true; // invalidate graph on next BuildMapGraph call
        ClearSavedConnectionLines();
        ClearEditorHighlightLines();
        if (!draw_portals) return;
        auto cur_map = GW::Map::GetMapID();
        const auto cur_area_cl = GW::Map::GetMapInfo();
        const uint32_t cur_continent_cl = cur_area_cl ? (uint32_t)cur_area_cl->continent : 0;

        for (const auto& conn : portal_connections.GetAll()) {
            // Skip reverse entry of bidirectional connections to avoid double-drawing
            if (!conn.IsOneWay() && conn.from_map > conn.to_map) continue;
            if (conn.no_draw) continue;
            // Skip connections from other continents (campaign=0 always shown for compat)
            if (conn.campaign && conn.campaign != cur_continent_cl) continue;

            // Skip entries with missing world map coords
            if ((conn.from_wm_pos.x == 0.f && conn.from_wm_pos.y == 0.f) || (conn.to_wm_pos.x == 0.f && conn.to_wm_pos.y == 0.f)) continue;

            GW::GamePos p1{}, p2{};
            if (!WorldMapWidget::WorldMapToGamePos(conn.from_wm_pos, p1)) continue;
            if (!WorldMapWidget::WorldMapToGamePos(conn.to_wm_pos, p2)) continue;
            auto* line = Minimap::Instance().custom_renderer.AddCustomLine(p1, p2);
            line->map = cur_map;
            // Color by the "most notable" endpoint type
            auto notable_type = (conn.from_type > conn.to_type) ? conn.from_type : conn.to_type;
            switch (notable_type) {
                case Pathing::ConnectionType::Disabled:
                    line->color = 0xFFFF0000;
                    break;
                case Pathing::ConnectionType::NPC:
                    line->color = 0xFF4488FF;
                    break;
                case Pathing::ConnectionType::Dummy:
                    line->color = 0xFFFFAA00;
                    break;
                default:
                    line->color = 0xFF00FF00;
                    break;
            }
            line->draw_on_mission_map = true;
            line->draw_on_minimap = false;
            line->created_by_toolbox = true;
            saved_connection_lines.push_back(line);
        }
    }

    // LoadAndShowMapsAtWorldPos defined after anonymous namespace

    std::vector<CustomRenderer::CustomLine*> editor_highlight_lines;

    void ClearEditorHighlightLines()
    {
        DeferRemoveLines(editor_highlight_lines);
    }



    void UpdateBoundsLines();    // forward decl
    void UpdatePortalMarkers();  // forward decl
    void UpdateGraphEdgeLines(); // forward decl
    struct PortalPair {
        GW::GamePos pos_a, pos_b; // game coords in each map
        GW::Vec2f wm_mid;         // world map midpoint
        float pair_dist;          // world map distance between the two portals
    };
    std::vector<PortalPair> FindPortalPairs(GW::Constants::MapID map_a, GW::Constants::MapID map_b); // forward decl
    const struct CachedMapInfo* GetCachedMapInfo(GW::Constants::MapID map_id);                       // forward decl
    uint32_t GetMapFileId(GW::Constants::MapID map_id);                                              // forward decl
    GW::GamePos ToCurrentMapCoords(const GW::GamePos& pos, GW::Constants::MapID src_map);            // forward decl

    // Load a map from DAT and create a MilePath for it. Returns the MilePath (may still be processing).
    // allow_load=false makes this game-thread-safe: it returns a resident MilePath if present, else nullptr,
    // and never touches the DAT. Only pass true off the game thread (the readFromDat below blocks).
    Pathing::MilePath* LoadMapFromDAT(GW::Constants::MapID map_id, bool allow_load)
    {
        if (IsInterestingMapForCacheTrace(map_id)) {
            PATH_LOG_INFO("[CacheTrace] LoadMapFromDAT(%d) entered", (int)map_id);
        }
        const uint32_t fid = GetMapFileId(map_id);
        if (!fid) {
            if (allow_load) PATH_LOG_ERROR("LoadMapFromDAT: no file_id for map %d (visit the map once to cache it)", (int)map_id);
            return nullptr;
        }

        // Fast path: file already resident. readFromDat routes into the game's serialized file subsystem, so re-reading it every recompute stalls the game thread; file_id alone keys the map within a session (mirrors GetMilepathForMap).
        for (const auto& [hash, mp] : mile_paths_by_coords) {
            if (static_cast<uint32_t>(hash & 0xFFFFFFFF) == fid) {
                TouchLru(hash);
                return mp;
            }
        }

        // Not resident: the DAT read + parse below blocks the caller. Refuse on the game thread.
        if (!allow_load) return nullptr;

        PATH_LOG_INFO("LoadMapFromDAT: map=%d file_id=%u (0x%X)", (int)map_id, fid, fid);

        auto dat_data = Pathing::PathingMapData();
        if (!Pathing::LoadPathingMapDataFromDAT(fid, &dat_data)) {
            PATH_LOG_ERROR("LoadMapFromDAT: DAT parse failed for map %d file_id=%u", (int)map_id, fid);
            return nullptr;
        }

        // For the current map, cross-check DAT against live memory: a bounds mismatch means a stale file_id, so use the MapContext copy.
        Pathing::PathingMapData ctx_data;
        Pathing::PathingMapData* chosen = &dat_data;
        if (map_id == GW::Map::GetMapID()) {
            if (!Pathing::LoadFromMapContext(GW::GetMapContext(), fid, &ctx_data)) {
                PATH_LOG_ERROR("LoadMapFromDAT: Context parse failed for map %d file_id=%u", (int)map_id, fid);
            }
            else if (ctx_data.bounds_max.x != dat_data.bounds_max.x || ctx_data.bounds_max.y != dat_data.bounds_max.y) {
                PATH_LOG_ERROR("LoadMapFromDAT: Context bounds_max dont match DAT portals for the current map, file_id=%u - check InfoWindow to update the map file id for this map! Using map context data for now.", fid);
                chosen = &ctx_data;
            }
        }
        auto& map_data = *chosen;

        // file_id was not resident above, so this full key (file_id + pathNodeSize) is new too.
        auto hash = static_cast<uint64_t>(fid);
        hash |= static_cast<uint64_t>(map_data.pathNodeSize) << 32;

        // Cache map info for bounds drawing
        CachedMapInfo info;
        info.map_id = map_id;
        info.bounds_min = map_data.bounds_min;
        info.bounds_max = map_data.bounds_max;
        info.portal_props = map_data.portal_props;
        cached_map_info[hash] = info;
        CacheSharedFileHashMaps(info);

        PATH_LOG_INFO("Loaded DAT for map %d: %d portal props, bounds=(%.0f,%.0f)-(%.0f,%.0f)", (int)map_id, (int)map_data.portal_props.size(), info.bounds_min.x, info.bounds_min.y, info.bounds_max.x, info.bounds_max.y);

        // Collect all MapIDs sharing this file_hash so teleporters from any are included
        std::vector<GW::Constants::MapID> all_ids;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if ((uint32_t)file_hash != fid) continue;
            for (const auto& entry : entries)
                all_ids.push_back(entry.map_id);
        }

        // Lightweight (full_build=false): keep only raw map data; the visgraph builds lazily on first AStar walk.
        auto* m = new Pathing::MilePath(std::move(map_data), map_id, all_ids, false);
        mile_paths_by_coords[hash] = m;
        TouchLru(hash); // eviction itself is deferred until the route job ends (RouteJobScope)
        // Defer these line-pool mutations to the main thread; LoadMapFromDAT runs on workers and AddCustomLine isn't thread-safe.
        Resources::EnqueueMainTask([] {
            UpdateBoundsLines();
            if (draw_portals) UpdatePortalMarkers();
        });
        return m;
    }

    // Fallback when no file_id is known: build PathingMapData from the live map context
    // and hand it to the standard MilePath ctor (eager build — it's the player's map).
    // allow_load=false is game-thread-safe: resident lookup only, never runs the deep-copy parse below.
    Pathing::MilePath* LoadMapFromContext(GW::Constants::MapID map_id, bool allow_load = true)
    {
        const auto mc = GW::GetMapContext();
        if (!(mc && mc->path && mc->path->staticData)) return nullptr;

        // pathNodeSize == pathNodes.size() (LoadFromMapContext sets it directly), so the cache key is
        // available without the full deep-copy — check residency before paying the parse cost.
        const auto hash = static_cast<uint64_t>(map_id)
            | (static_cast<uint64_t>(mc->path->pathNodes.size()) << 32);
        if (const auto it = mile_paths_by_coords.find(hash); it != mile_paths_by_coords.end()) {
            TouchLru(hash);
            return it->second;
        }

        // Not resident: LoadFromMapContext below is a full deep-copy + pointer-fixup. Refuse on the game thread.
        if (!allow_load) return nullptr;

        auto map_data = Pathing::PathingMapData();
        if (!Pathing::LoadFromMapContext(mc, 0, &map_data)) return nullptr;

        CachedMapInfo info;
        info.map_id = map_id;
        info.bounds_min = map_data.bounds_min;
        info.bounds_max = map_data.bounds_max;
        cached_map_info[hash] = info;
        CacheSharedFileHashMaps(info);

        auto* m = new Pathing::MilePath(std::move(map_data), map_id, {map_id}, true);
        mile_paths_by_coords[hash] = m;
        TouchLru(hash);
        Resources::EnqueueMainTask([] {
            UpdateBoundsLines();
            if (draw_portals) UpdatePortalMarkers();
        });
        return m;
    }

    // Returns milepath pointer for the current map, nullptr if we're not in a valid state.
    // allow_load=false is game-thread-safe (resident lookup only); true blocks on the DAT and must run off the game thread.
    Pathing::MilePath* GetMilepathForCurrentMap(bool allow_load = true)
    {
        // todo: maybe use bool GetIsMapReady() from other modules?
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) return nullptr;
        const auto map_id = GW::Map::GetMapID();
        if (map_id == GW::Constants::MapID::None) return nullptr;
        // Prefer DAT (shared with coordinate bounds); fall back to map context only with no file_id.
        if (GetMapFileId(map_id)) return LoadMapFromDAT(map_id, allow_load);
        return LoadMapFromContext(map_id, allow_load);
    }

    // Background prewarm so the game thread never blocks on a DAT read. ReadyForPathing/overlay probes call
    // GetResidentMilepathOrPrewarm(); if the current map isn't resident yet, a single worker task loads it.
    std::atomic<bool> prewarm_in_flight = false;

    void PrewarmCurrentMap()
    {
        bool expected = false;
        if (!prewarm_in_flight.compare_exchange_strong(expected, true)) return; // one load in flight at a time
        Resources::EnqueueWorkerTask([] {
            std::scoped_lock route_lock(route_mutex); // same serialization as a route build; see route_mutex
            RouteJobScope job_scope;
            GetMilepathForCurrentMap(true); // the blocking DAT read happens here, on the worker
            prewarm_in_flight = false;
        });
    }

    // Game-thread-safe current-map MilePath accessor: returns it only if already resident, otherwise kicks off
    // the background load and returns nullptr. Never reads the DAT or parses map context on the caller's thread.
    Pathing::MilePath* GetResidentMilepathOrPrewarm()
    {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) return nullptr;
        {
            // Try-lock so we never block the game thread; a held lock means a worker is mid-build → treat as not-ready.
            std::unique_lock route_lock(route_mutex, std::try_to_lock);
            if (route_lock.owns_lock()) {
                if (const auto m = GetMilepathForCurrentMap(false)) return m;
            }
        }
        PrewarmCurrentMap();
        return nullptr;
    }

    std::vector<CustomRenderer::CustomLine*> navmesh_edge_lines; // viz-only navmesh poly-edge overlay
    PathfindingWindow::Settings settings;
    std::vector<CustomRenderer::CustomLine*> bounds_lines;
    std::vector<CustomRenderer::CustomLine*> graph_edge_lines;
    std::vector<CustomRenderer::CustomLine*> portal_marker_lines;
    std::vector<CustomRenderer::CustomLine*> hover_highlight_lines;

    void ClearBoundsLines()
    {
        DeferRemoveLines(bounds_lines);
    }

    void ClearGraphEdgeLines()
    {
        DeferRemoveLines(graph_edge_lines);
    }


    void ClearPortalMarkerLines()
    {
        DeferRemoveLines(portal_marker_lines);
    }

    void ClearHoverHighlightLines()
    {
        DeferRemoveLines(hover_highlight_lines);
    }

    // Check if a portal position on a map has any connection
    bool IsPortalConnected(GW::Constants::MapID map_id, const GW::Vec2f& pos, float threshold_sq = 500.f * 500.f)
    {
        for (const auto& c : portal_connections.GetAll()) {
            if (c.from_map == map_id) {
                float dx = c.from_pos.x - pos.x, dy = c.from_pos.y - pos.y;
                if (dx * dx + dy * dy < threshold_sq) return true;
            }
            if (c.to_map == map_id) {
                float dx = c.to_pos.x - pos.x, dy = c.to_pos.y - pos.y;
                if (dx * dx + dy * dy < threshold_sq) return true;
            }
        }
        return false;
    }

    void UpdatePortalMarkers()
    {
        ClearPortalMarkerLines();
        if (!draw_portals) return;

        auto cur_map = GW::Map::GetMapID();
        const auto cur_area = GW::Map::GetMapInfo();
        if (!cur_area) return;
        auto cur_continent = cur_area->continent;
        constexpr float sz_default = 600.f;
        constexpr float sz_connected = 750.f;
        constexpr float sz_selected_bump = 350.f;

        auto matches_editor = [](GW::Constants::MapID map_id, const GW::Vec2f& pos, const EditorEndpoint& e) {
            if (!e.set || e.map_id != map_id) return false;
            constexpr float eps = 10.f;
            return fabsf(pos.x - e.pos.x) < eps && fabsf(pos.y - e.pos.y) < eps;
        };

        for (const auto& [hash, info] : cached_map_info) {
            // Filter by continent
            const auto area = GW::Map::GetMapInfo(info.map_id);
            if (area && area->continent != cur_continent) continue;

            for (const auto& pp : info.portal_props) {
                bool connected = IsPortalConnected(info.map_id, {pp.pos.x, pp.pos.y});
                bool selected = matches_editor(info.map_id, {pp.pos.x, pp.pos.y}, editor_from) || matches_editor(info.map_id, {pp.pos.x, pp.pos.y}, editor_to);
                DWORD color = connected ? 0xFF00FF00 : 0xFFFF8000;
                float sz = connected ? sz_connected : sz_default;
                if (selected) sz += sz_selected_bump;

                auto p = ToCurrentMapCoords({pp.pos.x, pp.pos.y, 0}, info.map_id);
                auto add = [&](const GW::GamePos& a, const GW::GamePos& b) {
                    auto* line = Minimap::Instance().custom_renderer.AddCustomLine(a, b);
                    line->map = cur_map;
                    line->color = color;
                    line->draw_on_mission_map = true;
                    line->draw_on_minimap = false;
                    line->created_by_toolbox = true;
                    portal_marker_lines.push_back(line);
                };
                // Diamond shape for better visibility
                add({p.x - sz, p.y, p.zplane}, {p.x, p.y + sz, p.zplane});
                add({p.x, p.y + sz, p.zplane}, {p.x + sz, p.y, p.zplane});
                add({p.x + sz, p.y, p.zplane}, {p.x, p.y - sz, p.zplane});
                add({p.x, p.y - sz, p.zplane}, {p.x - sz, p.y, p.zplane});
            }
        }
    }


    void UpdateBoundsLines()
    {
        ClearBoundsLines();
        if (!draw_map_bounds) return;

        auto cur_map = GW::Map::GetMapID();

        for (const auto& [hash, info] : cached_map_info) {
            // 4 corners in source map's game coords
            GW::GamePos tl = {info.bounds_min.x, info.bounds_min.y, 0};
            GW::GamePos tr = {info.bounds_max.x, info.bounds_min.y, 0};
            GW::GamePos br = {info.bounds_max.x, info.bounds_max.y, 0};
            GW::GamePos bl = {info.bounds_min.x, info.bounds_max.y, 0};

            // Convert to current map coords for display
            tl = ToCurrentMapCoords(tl, info.map_id);
            tr = ToCurrentMapCoords(tr, info.map_id);
            br = ToCurrentMapCoords(br, info.map_id);
            bl = ToCurrentMapCoords(bl, info.map_id);

            auto add = [&](const GW::GamePos& a, const GW::GamePos& b) {
                auto* line = Minimap::Instance().custom_renderer.AddCustomLine(a, b);
                line->map = cur_map;
                line->color = 0x8000C8C8; // cyan, semi-transparent
                line->draw_on_mission_map = true;
                line->draw_on_minimap = false;
                line->created_by_toolbox = true;
                bounds_lines.push_back(line);
            };

            add(tl, tr);
            add(tr, br);
            add(br, bl);
            add(bl, tl);
        }
    }

    // UpdateGraphEdgeLines implemented after map graph declarations

    Pathing::AStar* astar = nullptr;

    volatile bool pending_terminate = false;
    volatile bool pending_worker_task = false;
    std::atomic<bool> pathing_enabled = false; // true between Initialize() and SignalTerminate(); polled by QuestModule

    GW::HookEntry gw_ui_hookentry;

    GW::GamePos path_from;
    GW::GamePos path_to;
    GW::Vec2f path_from_world{};
    GW::Vec2f path_to_world{};
    GW::Constants::MapID path_from_map = GW::Constants::MapID::None;
    GW::Constants::MapID path_to_map = GW::Constants::MapID::None;

    // Per-file-hash stored points (survive map switches, shared across maps with same file)
    struct StoredPoints {
        GW::GamePos from{}, to{};
        GW::Vec2f from_world{}, to_world{};
        bool from_set = false, to_set = false;
    };
    std::unordered_map<uint32_t, StoredPoints> points_by_hash; // keyed by file_hash

    std::vector<CustomRenderer::CustomLine*> marker_lines;
    std::vector<CustomRenderer::CustomLine*> path_lines;
    std::vector<CustomRenderer::CustomLine*> portal_pair_lines;

    void ClearMarkerLines()
    {
        DeferRemoveLines(marker_lines);
    }

    void ClearPathLines()
    {
        DeferRemoveLines(path_lines);
    }

    void ClearPortalPairLines()
    {
        DeferRemoveLines(portal_pair_lines);
    }


    void AddMarkerCross(const GW::GamePos& pos, DWORD color, GW::Constants::MapID map_id)
    {
        constexpr float sz = 200.f;
        auto add = [&](const GW::GamePos& a, const GW::GamePos& b) {
            auto* line = Minimap::Instance().custom_renderer.AddCustomLine(a, b);
            line->map = map_id;
            line->color = color;
            line->draw_on_mission_map = true;
            line->draw_on_minimap = true;
            line->created_by_toolbox = true;
            marker_lines.push_back(line);
        };
        add({pos.x - sz, pos.y - sz, pos.zplane}, {pos.x + sz, pos.y + sz, pos.zplane});
        add({pos.x - sz, pos.y + sz, pos.zplane}, {pos.x + sz, pos.y - sz, pos.zplane});
    }

    // Convert a game pos from a source map to current map coords via world map
    GW::GamePos ToCurrentMapCoords(const GW::GamePos& pos, GW::Constants::MapID src_map)
    {
        if (src_map == GW::Map::GetMapID() || src_map == GW::Constants::MapID::None) return pos;
        GW::Vec2f world_pos;
        GW::GamePos cur_pos;
        if (WorldMapWidget::GamePosToWorldMap(pos, world_pos, src_map) && WorldMapWidget::WorldMapToGamePos(world_pos, cur_pos)) {
            cur_pos.zplane = pos.zplane;
            return cur_pos;
        }
        return pos;
    }

    void UpdateMarkers(const GW::GamePos& from, const GW::GamePos& to)
    {
        ClearMarkerLines();
        auto cur_map = GW::Map::GetMapID();
        if (from.x != 0.f || from.y != 0.f) {
            auto display_from = ToCurrentMapCoords(from, path_from_map);
            AddMarkerCross(display_from, 0xFF00FF00, cur_map); // green
        }
        if (to.x != 0.f || to.y != 0.f) {
            auto display_to = ToCurrentMapCoords(to, path_to_map);
            AddMarkerCross(display_to, 0xFFFF0000, cur_map); // red
        }
    }

    // Convert path points from source map coords to current map coords via world map
    bool IsOutpostMap(GW::Constants::MapID map_id); // forward decl

    // "Don't draw a line here" sentinel in flat full_path arrays; inserted between segments around underground (no_draw) maps.
    constexpr float PATH_BREAK_VALUE = FLT_MAX;
    inline bool IsPathBreak(const GW::GamePos& p)
    {
        return p.x == PATH_BREAK_VALUE;
    }
    inline bool IsPathBreak(const GW::Vec2f& p)
    {
        return p.x == PATH_BREAK_VALUE;
    }

    // A path segment hidden from the world map (underground map) but kept in native coords + map tag so it renders once entered.
    struct HiddenPathSegment {
        std::vector<GW::GamePos> points;
        GW::Constants::MapID map_id;
    };

    // True if any manual connection between map_a and map_b (in either direction,
    // with explorable→outpost file_hash spread) has no_draw=true.
    bool HasNoDrawConnection(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        uint32_t fh_a = GetMapFileId(map_a);
        uint32_t fh_b = GetMapFileId(map_b);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        for (const auto& conn : portal_connections.GetAll()) {
            if (!conn.no_draw) continue;
            bool fwd = maps_match(conn.from_map, map_a, fh_a) && maps_match(conn.to_map, map_b, fh_b);
            bool rev = maps_match(conn.from_map, map_b, fh_b) && maps_match(conn.to_map, map_a, fh_a);
            if (fwd || rev) return true;
        }
        return false;
    }

    // src_map game positions -> world coords (the common cross-map space, no overflow);
    // break sentinels pass through.
    bool SegmentToWorld(const std::vector<GW::GamePos>& points, GW::Constants::MapID src_map, std::vector<GW::Vec2f>& out)
    {
        out.reserve(out.size() + points.size());
        for (const auto& p : points) {
            if (IsPathBreak(p)) {
                out.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE});
                continue;
            }
            GW::Vec2f w;
            if (WorldMapWidget::GamePosToWorldMap(p, w, src_map)) out.push_back(w);
        }
        return true;
    }

    std::vector<GW::GamePos> ConvertPathToCurrentMap(const std::vector<GW::GamePos>& points, GW::Constants::MapID src_map)
    {
        if (src_map == GW::Map::GetMapID()) return points;

        std::vector<GW::GamePos> converted;
        converted.reserve(points.size());
        for (const auto& p : points) {
            if (IsPathBreak(p)) {
                converted.push_back(p);
                continue;
            }
            GW::Vec2f world_pos;
            GW::GamePos cur_pos;
            if (WorldMapWidget::GamePosToWorldMap(p, world_pos, src_map) && WorldMapWidget::WorldMapToGamePos(world_pos, cur_pos)) {
                cur_pos.zplane = p.zplane;
                converted.push_back(cur_pos);
            }
        }
        return converted;
    }

    void DrawPathAsLines(const std::vector<GW::GamePos>& points, GW::Constants::MapID src_map)
    {
        ClearPathLines();
        auto cur_map = GW::Map::GetMapID();
        const auto& draw_points = (src_map != cur_map) ? ConvertPathToCurrentMap(points, src_map) : points;

        for (size_t i = 0; i + 1 < draw_points.size(); i++) {
            // Skip lines that touch a path-break sentinel — used to hide segments
            // through underground maps in multi-map paths.
            if (IsPathBreak(draw_points[i]) || IsPathBreak(draw_points[i + 1])) continue;
            auto* line = Minimap::Instance().custom_renderer.AddCustomLine(draw_points[i], draw_points[i + 1]);
            line->map = cur_map;
            line->color = 0xFFFFFF00; // yellow
            line->draw_on_mission_map = true;
            line->draw_on_minimap = true;
            line->created_by_toolbox = true;
            path_lines.push_back(line);
        }
    }

    // Append underground segments in native map coords tagged with their map_id: hidden on the world map but drawn in-world
    // once the player enters that map. Call after DrawPathAsLines (doesn't clear) — segments share path_lines.
    void AddHiddenUndergroundSegmentLines(const std::vector<HiddenPathSegment>& segs)
    {
        for (const auto& seg : segs) {
            for (size_t i = 0; i + 1 < seg.points.size(); i++) {
                auto* line = Minimap::Instance().custom_renderer.AddCustomLine(seg.points[i], seg.points[i + 1]);
                line->map = seg.map_id;   // native underground map, NOT cur_map
                line->color = 0xFFFFFF00; // yellow
                line->draw_on_mission_map = true;
                line->draw_on_minimap = true;
                line->created_by_toolbox = true;
                path_lines.push_back(line);
            }
        }
    }


    // Returns false if our last AStar calculation matches what we're asking for.
    bool NeedsRecalculating(const GW::GamePos& from, const GW::GamePos& to)
    {
        if (!(astar && astar->m_path.ready() && astar->m_path.points().size())) return true;
        return from != astar->m_path.points().at(0) || to != astar->m_path.points().at(astar->m_path.points().size() - 1);
    }

    // Find a cached MilePath for a given MapID
    Pathing::MilePath* GetMilepathForMap(GW::Constants::MapID map_id)
    {
        // Look up by file_hash so shared-hash maps (e.g. 107/135 with hash 0xC77A)
        // can return the same MilePath regardless of which MapID requests it.
        uint32_t fh = GetMapFileId(map_id);
        if (!fh) return nullptr;
        for (const auto& [hash, mp] : mile_paths_by_coords) {
            if ((uint32_t)(hash & 0xFFFFFFFF) == fh) {
                TouchLru(hash);
                return mp;
            }
        }
        return nullptr;
    }

    // =========================================================================
    // Map connectivity graph — adjacency by overlapping world map bounds
    // =========================================================================

    struct MapGraphNode {
        GW::Constants::MapID map_id;
        uint32_t file_hash;
        ImRect wm_bounds; // world map bounds
        GW::Continent continent;
    };

    std::vector<MapGraphNode> map_graph_nodes;
    bool map_graph_built = false;

    void BuildMapGraph()
    {
        if (connections_changed) {
            map_graph_built = false;
            connections_changed = false;
        }
        if (map_graph_built) return;
        map_graph_built = true;
        map_graph_nodes.clear();

        // Collect maps, deduplicated by file_hash (same file = same physical map)
        std::set<uint32_t> seen_file_hashes;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if (!file_hash) continue;
            if (seen_file_hashes.contains((uint32_t)file_hash)) continue;

            // Pick first valid entry for this file_hash
            for (const auto& entry : entries) {
                auto map_id = entry.map_id;
                const auto area = GW::Map::GetMapInfo(map_id);
                if (!area || area->GetIsPvP() || !area->GetIsOnWorldMap()) continue;
                ImRect bounds;
                if (!GW::Map::GetMapWorldMapBounds(area, &bounds)) continue;
                if (bounds.GetWidth() < 1.f || bounds.GetHeight() < 1.f) continue;
                map_graph_nodes.push_back({map_id, (uint32_t)file_hash, bounds, area->continent});
                if (IsInterestingMapForCacheTrace(map_id)) {
                    PATH_LOG_INFO("[CacheTrace] BuildMapGraph added mid=%d fh=0x%X bounds=(%.0f,%.0f)-(%.0f,%.0f) (path=constant_maps_info first)", (int)map_id, (uint32_t)file_hash, bounds.Min.x, bounds.Min.y, bounds.Max.x, bounds.Max.y);
                }
                seen_file_hashes.insert((uint32_t)file_hash);
                break;
            }
        }
        // Add connection-referenced maps not yet in the graph, deduped by EXACT MapID (not file_hash) — else an outpost in a
        // connection chain gets dropped when its explorable sibling was picked, and Dijkstra would expand a missing node.
        for (const auto& conn : portal_connections.GetAll()) {
            for (auto mid : {conn.from_map, conn.to_map}) {
                if (mid == GW::Constants::MapID::None) continue;
                bool exists = false;
                for (const auto& n : map_graph_nodes) {
                    if (n.map_id == mid) {
                        exists = true;
                        break;
                    }
                }
                if (exists) continue;
                uint32_t fh = GetMapFileId(mid);
                const auto* area = GW::Map::GetMapInfo(mid);
                auto continent = area ? area->continent : GW::Continent::Kryta;
                map_graph_nodes.push_back({mid, fh, ImRect(), continent});
                if (IsInterestingMapForCacheTrace(mid)) {
                    PATH_LOG_INFO("[CacheTrace] BuildMapGraph added mid=%d fh=0x%X (path=portal_connections, conn from=%d to=%d)", (int)mid, fh, (int)conn.from_map, (int)conn.to_map);
                }
            }
        }
        PATH_LOG_INFO("Map graph: %d nodes (incl. connection-referenced)", (int)map_graph_nodes.size());
    }

    bool IsOutpostMap(GW::Constants::MapID map_id); // forward decl

    // Find adjacent maps (overlapping bounds, same continent)
    std::vector<GW::Constants::MapID> GetAdjacentMaps(GW::Constants::MapID map_id)
    {
        BuildMapGraph();
        const MapGraphNode* src = nullptr;
        for (const auto& node : map_graph_nodes) {
            if (node.map_id == map_id) {
                src = &node;
                break;
            }
        }
        if (!src) return {};

        // Connection-referenced nodes have empty bounds; skip the overlap branch for them, else two empty rects "overlap" at origin.
        const bool src_empty = src->wm_bounds.GetWidth() < 1.f || src->wm_bounds.GetHeight() < 1.f;

        std::vector<GW::Constants::MapID> result;
        for (const auto& node : map_graph_nodes) {
            if (node.map_id == map_id || node.file_hash == src->file_hash) continue;
            if (node.continent != src->continent) continue;
            if (src_empty) continue;
            if (node.wm_bounds.GetWidth() < 1.f || node.wm_bounds.GetHeight() < 1.f) continue;
            // Check bounds overlap with tolerance for touching edges
            constexpr float tolerance = 5.f; // world map units
            ImRect expanded = node.wm_bounds;
            expanded.Expand(tolerance);
            if (src->wm_bounds.Overlaps(expanded)) {
                result.push_back(node.map_id);
            }
        }

        // Add neighbors from manual connections. Connection-spread rule: a connection on an EXPLORABLE map applies to all
        // file_hash siblings; one on an OUTPOST is local to that exact outpost (prevents 348/244/218 cross-pollination).
        auto contains = [&](GW::Constants::MapID mid) {
            for (auto m : result)
                if (m == mid) return true;
            return false;
        };
        const uint32_t fh_cur = src->file_hash;
        const bool src_is_outpost = IsOutpostMap(map_id);
        auto matches_current = [&](GW::Constants::MapID conn_map_id) -> bool {
            if (conn_map_id == map_id) return true;
            if (IsOutpostMap(conn_map_id)) return false;
            // An outpost can't inherit its explorable siblings' connections: those portals need an extra outpost→explorable
            // hop the picker doesn't model, so treating them as direct yields phantom-cheap edges (e.g. fh 0x85A8 57 inheriting 56→18).
            if (src_is_outpost) return false;
            return fh_cur && GetMapFileId(conn_map_id) == fh_cur;
        };
        PATH_LOG_INFO("[GetAdj] map=%d fh=0x%X is_outpost=%d", (int)map_id, fh_cur, IsOutpostMap(map_id) ? 1 : 0);
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd_match = matches_current(conn.from_map);
            bool rev_match = !conn.IsOneWay() && matches_current(conn.to_map);
            if (fwd_match || rev_match) {
                PATH_LOG_INFO("[GetAdj]   conn from=%d to=%d ftype=%d ttype=%d oneway=%d fwd=%d rev=%d", (int)conn.from_map, (int)conn.to_map, (int)conn.from_type, (int)conn.to_type, conn.IsOneWay() ? 1 : 0, fwd_match ? 1 : 0, rev_match ? 1 : 0);
            }
            // Forward: conn.from matches current → conn.to is neighbor
            if (fwd_match && conn.to_map != map_id && !contains(conn.to_map)) {
                result.push_back(conn.to_map);
            }
            // Reverse: only for bidirectional connections
            if (rev_match && conn.from_map != map_id && !contains(conn.from_map)) {
                result.push_back(conn.from_map);
            }
        }

        return result;
    }

    const std::vector<Pathing::PortalProp>& GetPortalPropsForMap(GW::Constants::MapID map_id)
    {
        static const std::vector<Pathing::PortalProp> empty;
        uint32_t fh = GetMapFileId(map_id);
        if (!fh) return empty;

        auto it = portal_props_cache.find(fh);
        if (it != portal_props_cache.end()) return it->second;

        // Check if already in cached_map_info (from full DAT load)
        for (const auto& [hash, info] : cached_map_info) {
            if (info.map_id == map_id && !info.portal_props.empty()) {
                portal_props_cache[fh] = info.portal_props;
                return portal_props_cache[fh];
            }
        }

        // Lightweight load — just portal props
        auto& props = portal_props_cache[fh];
        Pathing::LoadPortalPropsFromDAT(fh, props);
        return props;
    }

    // Check if two maps have portal props (bounds overlap is already a precondition)
    bool HasPortalConnection(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        const auto& props_a = GetPortalPropsForMap(map_a);
        const auto& props_b = GetPortalPropsForMap(map_b);
        // Both maps need at least one portal prop to be traversable
        return !props_a.empty() && !props_b.empty();
    }

    // Lightweight map info load — only bounds + portal props, no MilePath/vis graph
    void EnsureLightweightMapInfo(GW::Constants::MapID map_id, const char* caller)
    {
        if (IsInterestingMapForCacheTrace(map_id)) {
            PATH_LOG_INFO("[CacheTrace] EnsureLightweightMapInfo(%d) caller=%s already_cached=%d", (int)map_id, caller, GetCachedMapInfo(map_id) ? 1 : 0);
        }
        if (map_id == GW::Map::GetMapID()) return;
        if (GetCachedMapInfo(map_id)) return; // already have info

        uint32_t fid = GetMapFileId(map_id);
        if (!fid) return;

        // Load just the map info chunk for bounds + portal props
        ArenaNetFileParser::ArenaNetFile game_asset;
        if (!game_asset.readFromDat(fid, 1)) return;

        // Get bounds from Map_Info chunk
#pragma pack(push, 1)
        struct MapInfoChunk : ArenaNetFileParser::Chunk {
            uint32_t signature;
            uint8_t version;
            Pathing::Vec2f bounds[2];
        };
#pragma pack(pop)
        const auto map_info_chunk = (MapInfoChunk*)game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_Info);
        if (!map_info_chunk) return;

        // Parse portal props
        std::vector<Pathing::PortalProp> props;
        Pathing::LoadPortalPropsFromDAT(fid, props);

        // Cache as lightweight info (no MilePath created)
        auto hash = static_cast<uint64_t>(fid);
        CachedMapInfo info;
        info.map_id = map_id;
        info.bounds_min = map_info_chunk->bounds[0];
        info.bounds_max = map_info_chunk->bounds[1];
        info.portal_props = std::move(props);
        cached_map_info[hash] = std::move(info);
        CacheSharedFileHashMaps(cached_map_info[hash]);
    }

    bool IsOutpostMap(GW::Constants::MapID map_id); // forward decl

    // Cost from manual portal connections between two maps (directional: a→b)
    float GetConnectionCost(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        // Connection-spread rule: explorable connections apply to all file_hash siblings; outpost connections are local. See GetAdjacentMaps.
        uint32_t fh_a = GetMapFileId(map_a);
        uint32_t fh_b = GetMapFileId(map_b);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        float best = std::numeric_limits<float>::max();
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd = (maps_match(conn.from_map, map_a, fh_a) && maps_match(conn.to_map, map_b, fh_b));
            bool rev = !conn.IsOneWay() && (maps_match(conn.from_map, map_b, fh_b) && maps_match(conn.to_map, map_a, fh_a));
            if (!fwd && !rev) continue;
            // Use the cheaper endpoint type so P-G / G-P connections get the teleport multiplier in either direction.
            float mult_from = Pathing::PortalConnections::GetCostMultiplier(conn.from_type);
            float mult_to = Pathing::PortalConnections::GetCostMultiplier(conn.to_type);
            float mult = std::min(mult_from, mult_to);
            float cost = 500.f * mult;
            best = std::min(best, cost);
        }
        return best;
    }

    // Fixed per-hop cost so the picker prefers fewer maps; high enough to overcome F-P savings a longer route might gain at graph_src.
    constexpr float HOP_PENALTY = 20000.f;
    // Scales the bounds-center geo_cost to penalize "going through" intermediate maps.
    constexpr float TRANSIT_MULT = 1.5f;

    // Edge cost between two adjacent maps
    float GetEdgeCost(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        // Check manual connections first (can provide routes for underground maps)
        float conn_cost = GetConnectionCost(map_a, map_b);

        // Ensure both maps have lightweight info (bounds + portal props) for evaluation
        EnsureLightweightMapInfo(map_a, "GetEdgeCost_a");
        EnsureLightweightMapInfo(map_b, "GetEdgeCost_b");

        const auto& props_a = GetPortalPropsForMap(map_a);
        const auto& props_b = GetPortalPropsForMap(map_b);

        // No portal props means no real walkable connection — only allow if manual connection exists
        if (props_a.empty() || props_b.empty()) {
            return conn_cost + HOP_PENALTY; // infinity if no manual connection
        }

        // Check if there's an actual portal pair
        auto pairs = FindPortalPairs(map_a, map_b);
        if (pairs.empty()) {
            // No portal pair — bounds overlap is not enough. Only allow if manual connection.
            return conn_cost + HOP_PENALTY;
        }

        // World-map distance between bounds centers, scaled by TRANSIT_MULT as an intermediate-map walking proxy.
        float geo_cost = 1000.f;
        const MapGraphNode* na = nullptr;
        const MapGraphNode* nb = nullptr;
        for (const auto& n : map_graph_nodes) {
            if (n.map_id == map_a) na = &n;
            if (n.map_id == map_b) nb = &n;
        }
        if (na && nb) {
            ImVec2 ca = na->wm_bounds.GetCenter(), cb = nb->wm_bounds.GetCenter();
            float dx = ca.x - cb.x, dy = ca.y - cb.y;
            geo_cost = sqrtf(dx * dx + dy * dy);
        }
        return std::min(conn_cost, geo_cost * TRANSIT_MULT) + HOP_PENALTY;
    }

    // F-P / P-T added in raw game-coord units: intentionally dwarfs geo_cost (WM) and conn_cost (~500) so endpoint walk distance dominates.
    constexpr float GAME_TO_WM = 1.f;

    // Forward decl: full definition is below FindPortalPairs (which needs PortalPair).
    float ClosestPortalDistanceInMap(const GW::GamePos& pos, GW::Constants::MapID owner_map, GW::Constants::MapID other_map);
    float ClosestPortalWalkDistanceInMap(const GW::GamePos& pos, GW::Constants::MapID owner_map, GW::Constants::MapID other_map);
    float ClosestPortalTrapezoidDistanceInMap(const GW::GamePos& pos, GW::Constants::MapID owner_map, GW::Constants::MapID other_map);
    bool FindAnyPortalPosInMap(GW::Constants::MapID owner_map, GW::Constants::MapID other_map, GW::Vec2f& out);

    // Blacklisted edges for retry (cleared on each FindPath call)
    std::set<uint64_t> blacklisted_edges;

    uint64_t EdgeKey(GW::Constants::MapID a, GW::Constants::MapID b)
    {
        return (uint64_t)(uint32_t)a | ((uint64_t)(uint32_t)b << 32);
    }

    // Dijkstra for the lowest-cost map sequence. When start_pos/goal_pos are given, source-leaving and dest-entering edges are
    // weighted by distance to the actual portal used, so the picker favors portals near the endpoints over topologically cheap routes.
    std::vector<GW::Constants::MapID> FindMapRoute(GW::Constants::MapID src, GW::Constants::MapID dst, const GW::GamePos* start_pos = nullptr, const GW::GamePos* goal_pos = nullptr)
    {
        BuildMapGraph();
        // Clear the per-call cost cache so a previous query (different src/goal) can't bleed in.
        portal_walk_cache.clear();

        // Resolve src/dst to their graph representative (outpost→explorable with same file_hash)
        auto resolve = [](GW::Constants::MapID mid) -> GW::Constants::MapID {
            for (const auto& n : map_graph_nodes) {
                if (n.map_id == mid) return mid; // already in graph
            }
            // Not in graph — find a node sharing the same file_hash
            uint32_t fh = GetMapFileId(mid);
            if (fh) {
                for (const auto& n : map_graph_nodes) {
                    if (n.file_hash == fh) return n.map_id;
                }
            }
            return mid; // not found, keep original (will fail gracefully)
        };
        auto graph_src = resolve(src);
        auto graph_dst = resolve(dst);
        if (graph_src == graph_dst) return {src, dst};

        struct DijkNode {
            float cost;
            GW::Constants::MapID map_id;
        };
        auto cmp = [](const DijkNode& a, const DijkNode& b) {
            return a.cost > b.cost;
        };
        std::priority_queue<DijkNode, std::vector<DijkNode>, decltype(cmp)> pq(cmp);

        std::unordered_map<uint32_t, float> best_cost;
        std::unordered_map<uint32_t, uint32_t> came_from;

        pq.push({0.f, graph_src});
        best_cost[(uint32_t)graph_src] = 0.f;
        came_from[(uint32_t)graph_src] = (uint32_t)graph_src;

        while (!pq.empty()) {
            auto [cost, current] = pq.top();
            pq.pop();

            if (current == graph_dst) {
                std::vector<GW::Constants::MapID> route;
                auto id = (uint32_t)graph_dst;
                while (id != (uint32_t)graph_src) {
                    route.push_back(static_cast<GW::Constants::MapID>(id));
                    id = came_from[id];
                }
                route.push_back(graph_src);
                std::ranges::reverse(route);
                // Replace graph representatives with original src/dst
                if (!route.empty() && route.front() == graph_src) route.front() = src;
                if (!route.empty() && route.back() == graph_dst) route.back() = dst;
                return route;
            }

            if (cost > best_cost[(uint32_t)current]) continue;

            auto adj = GetAdjacentMaps(current);
            std::string adj_dump;
            for (auto neighbor : adj) {
                if (blacklisted_edges.contains(EdgeKey(current, neighbor))) continue;
                float edge_cost = GetEdgeCost(current, neighbor);

                auto portal_dist = [](const GW::GamePos& from, GW::Constants::MapID a, GW::Constants::MapID b) {
                    switch (portal_distance_mode) {
                        case PortalDistanceMode::Trapezoid:
                            return ClosestPortalTrapezoidDistanceInMap(from, a, b);
                        case PortalDistanceMode::Walk:
                            return ClosestPortalWalkDistanceInMap(from, a, b);
                        case PortalDistanceMode::Euclidean:
                        default:
                            return ClosestPortalDistanceInMap(from, a, b);
                    }
                };
                // F-P: leaving the source map, add the distance from start to this edge's portal.
                if (start_pos && current == graph_src) {
                    float fp_game = portal_dist(*start_pos, current, neighbor);
                    if (fp_game != std::numeric_limits<float>::infinity()) {
                        edge_cost += fp_game * GAME_TO_WM;
                    }
                }
                // Intermediate-map traversal: add the distance from the entry-portal anchor to the neighbor portal,
                // else Dijkstra treats crossing a 50k-unit map as free and prefers long routes through big maps.
                else if (current != graph_dst) {
                    auto prev_it = came_from.find(static_cast<uint32_t>(current));
                    if (prev_it != came_from.end()) {
                        const auto prev_map = static_cast<GW::Constants::MapID>(prev_it->second);
                        GW::Vec2f anchor;
                        if (FindAnyPortalPosInMap(current, prev_map, anchor)) {
                            float traverse = portal_dist({anchor.x, anchor.y, 0}, current, neighbor);
                            if (traverse != std::numeric_limits<float>::infinity()) {
                                edge_cost += traverse * GAME_TO_WM;
                            }
                        }
                    }
                }
                // P-T: entering the destination map, add the distance from the entry portal to the goal.
                if (goal_pos && neighbor == graph_dst) {
                    float pt_game = portal_dist(*goal_pos, neighbor, current);
                    if (pt_game != std::numeric_limits<float>::infinity()) {
                        edge_cost += pt_game * GAME_TO_WM;
                    }
                }

                if (edge_cost < 1e9f) adj_dump += std::format(" {}({:.0f})", (int)neighbor, edge_cost);
                float new_cost = cost + edge_cost;
                auto nid = (uint32_t)neighbor;
                if (!best_cost.contains(nid) || new_cost < best_cost[nid]) {
                    best_cost[nid] = new_cost;
                    came_from[nid] = (uint32_t)current;
                    pq.push({new_cost, neighbor});
                }
            }
            PATH_LOG_INFO("[Dijkstra] map %d cost=%.0f neighbors:%s", (int)current, cost, adj_dump.c_str());
        }
        PATH_LOG_INFO("[Dijkstra] graph_src=%d graph_dst=%d FAIL: queue exhausted", (int)graph_src, (int)graph_dst);
        return {};
    }

    void UpdateGraphEdgeLines()
    {
        ClearGraphEdgeLines();
        if (!draw_graph_edges) return;
        BuildMapGraph();

        auto cur_map = GW::Map::GetMapID();
        const auto cur_info = GW::Map::GetMapInfo();
        if (!cur_info) return;
        auto cur_continent = cur_info->continent;

        std::set<uint64_t> drawn_edges;
        for (size_t i = 0; i < map_graph_nodes.size(); i++) {
            const auto& node = map_graph_nodes[i];
            if (node.continent != cur_continent) continue;
            ImVec2 ca = node.wm_bounds.GetCenter();

            for (auto neighbor_id : GetAdjacentMaps(node.map_id)) {
                uint64_t edge_key = std::min((uint32_t)node.map_id, (uint32_t)neighbor_id);
                edge_key |= static_cast<uint64_t>(std::max((uint32_t)node.map_id, (uint32_t)neighbor_id)) << 32;
                if (drawn_edges.contains(edge_key)) continue;
                drawn_edges.insert(edge_key);

                for (size_t ni = 0; ni < map_graph_nodes.size(); ni++) {
                    if (map_graph_nodes[ni].map_id != neighbor_id) continue;
                    ImVec2 cb = map_graph_nodes[ni].wm_bounds.GetCenter();

                    GW::GamePos ga, gb;
                    GW::Vec2f wm_a = {ca.x, ca.y}, wm_b = {cb.x, cb.y};
                    if (!WorldMapWidget::WorldMapToGamePos(wm_a, ga)) break;
                    if (!WorldMapWidget::WorldMapToGamePos(wm_b, gb)) break;

                    auto* line = Minimap::Instance().custom_renderer.AddCustomLine(ga, gb);
                    line->map = cur_map;
                    line->color = 0x40FFFF00; // yellow, transparent
                    line->draw_on_mission_map = true;
                    line->draw_on_minimap = false;
                    line->created_by_toolbox = true;
                    graph_edge_lines.push_back(line);
                    break;
                }
            }
        }
    }

    const CachedMapInfo* GetCachedMapInfo(GW::Constants::MapID map_id)
    {
        // Direct match by MapID
        for (const auto& [hash, info] : cached_map_info) {
            if (info.map_id == map_id) return &info;
        }
        // Fallback: match by shared file_hash (outpost/explorable pairs share same data)
        uint32_t fh = GetMapFileId(map_id);
        if (fh) {
            for (const auto& [hash, info] : cached_map_info) {
                if (GetMapFileId(info.map_id) == fh) return &info;
            }
        }
        return nullptr;
    }

    // True for outpost/city-type maps (not explorables); their small footprint inside shared bounds means inherited portals may be far off.
    bool IsOutpostMap(GW::Constants::MapID map_id)
    {
        const auto* area = GW::Map::GetMapInfo(map_id);
        if (!area) return false;
        switch (area->type) {
            case GW::RegionType::Outpost:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::City:
            case GW::RegionType::HeroBattleOutpost:
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::CompetitiveMission:
                return true;
            default:
                return false;
        }
    }

    // Maximum world map distance from outpost icon to consider a portal valid (~24000 game units)
    constexpr float OUTPOST_PORTAL_MAX_WM_DIST = 250.f;

    // Returns true if portal_wm is too far from the outpost icon to be reachable.
    // Only filters when map_id is an outpost. Returns false otherwise.
    bool IsPortalTooFarFromOutpost(GW::Constants::MapID map_id, const GW::Vec2f& portal_wm)
    {
        if (!IsOutpostMap(map_id)) return false;
        const auto* area = GW::Map::GetMapInfo(map_id);
        if (!area || (!area->x && !area->y)) return false; // no icon position
        float dx = portal_wm.x - (float)area->x;
        float dy = portal_wm.y - (float)area->y;
        return (dx * dx + dy * dy) > (OUTPOST_PORTAL_MAX_WM_DIST * OUTPOST_PORTAL_MAX_WM_DIST);
    }


    // Find all portal pairs between two adjacent maps, constrained to the overlap region.
    std::vector<PortalPair> FindPortalPairs(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        std::vector<PortalPair> pairs;
        const auto* info_a = GetCachedMapInfo(map_a);
        const auto* info_b = GetCachedMapInfo(map_b);
        if (!info_a || !info_b) return pairs;

        // Compute overlap region in world map coords
        auto* area_a = GW::Map::GetMapInfo(map_a);
        auto* area_b = GW::Map::GetMapInfo(map_b);
        if (!area_a || !area_b) return pairs;
        ImRect wm_a, wm_b;
        if (!GW::Map::GetMapWorldMapBounds(area_a, &wm_a)) return pairs;
        if (!GW::Map::GetMapWorldMapBounds(area_b, &wm_b)) return pairs;

        constexpr float overlap_margin = 50.f; // expand overlap region slightly
        ImRect overlap(std::max(wm_a.Min.x, wm_b.Min.x) - overlap_margin, std::max(wm_a.Min.y, wm_b.Min.y) - overlap_margin, std::min(wm_a.Max.x, wm_b.Max.x) + overlap_margin, std::min(wm_a.Max.y, wm_b.Max.y) + overlap_margin);

        constexpr float max_pair_dist = 100.f; // world map units

        for (const auto& pa : info_a->portal_props) {
            GW::Vec2f wm_pa;
            if (!WorldMapWidget::GamePosToWorldMap({pa.pos.x, pa.pos.y, 0}, wm_pa, map_a)) continue;
            // Only consider portals near the overlap region
            if (!overlap.Contains({wm_pa.x, wm_pa.y})) continue;
            // Outpost filter: portal must be near the outpost icon if map_a is an outpost
            if (IsPortalTooFarFromOutpost(map_a, wm_pa)) continue;

            for (const auto& pb : info_b->portal_props) {
                GW::Vec2f wm_pb;
                if (!WorldMapWidget::GamePosToWorldMap({pb.pos.x, pb.pos.y, 0}, wm_pb, map_b)) continue;
                if (!overlap.Contains({wm_pb.x, wm_pb.y})) continue;
                if (IsPortalTooFarFromOutpost(map_b, wm_pb)) continue;

                float dx = wm_pa.x - wm_pb.x, dy = wm_pa.y - wm_pb.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < max_pair_dist) {
                    pairs.push_back({{pa.pos.x, pa.pos.y, 0}, {pb.pos.x, pb.pos.y, 0}, {(wm_pa.x + wm_pb.x) * 0.5f, (wm_pa.y + wm_pb.y) * 0.5f}, dist});
                }
            }
        }
        return pairs;
    }

    // Straight-line game-coord distance from `pos` to the closest owner_map portal connecting to other_map (manual + auto pairs), or infinity.
    float ClosestPortalDistanceInMap(const GW::GamePos& pos, GW::Constants::MapID owner_map, GW::Constants::MapID other_map)
    {
        uint32_t fh_owner = GetMapFileId(owner_map);
        uint32_t fh_other = GetMapFileId(other_map);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };

        float best_sq = std::numeric_limits<float>::infinity();
        auto try_pos = [&](float px, float py) {
            float dx = px - pos.x, dy = py - pos.y;
            float d = dx * dx + dy * dy;
            if (d < best_sq) best_sq = d;
        };

        bool any_manual = false;
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd = maps_match(conn.from_map, owner_map, fh_owner) && maps_match(conn.to_map, other_map, fh_other);
            bool rev = !conn.IsOneWay() && maps_match(conn.to_map, owner_map, fh_owner) && maps_match(conn.from_map, other_map, fh_other);
            if (fwd) {
                try_pos(conn.from_pos.x, conn.from_pos.y);
                any_manual = true;
            }
            if (rev) {
                try_pos(conn.to_pos.x, conn.to_pos.y);
                any_manual = true;
            }
        }

        // Auto-detected pairs only if no manual connection: trust manual data first, since border-prop matching can pull in false candidates.
        if (!any_manual) {
            auto pairs = FindPortalPairs(owner_map, other_map);
            for (const auto& p : pairs) {
                try_pos(p.pos_a.x, p.pos_a.y);
            }
        }

        return (best_sq == std::numeric_limits<float>::infinity()) ? best_sq : sqrtf(best_sq);
    }

    // Like ClosestPortalDistanceInMap but uses the MilePath visgraph + AStar for the real walk distance; falls back to Euclidean if unavailable.
    float ClosestPortalWalkDistanceInMap(const GW::GamePos& pos, GW::Constants::MapID owner_map, GW::Constants::MapID other_map)
    {
        PortalWalkKey key{(uint32_t)PortalDistanceMode::Walk, (uint32_t)owner_map, (uint32_t)other_map, pos.x, pos.y};
        auto cit = portal_walk_cache.find(key);
        if (cit != portal_walk_cache.end()) return cit->second;

        // Candidate portals in owner_map (manual connections with sibling spread, plus auto-detected pairs).
        std::vector<GW::Vec2f> candidates;
        const uint32_t fh_owner = GetMapFileId(owner_map);
        const uint32_t fh_other = GetMapFileId(other_map);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd = maps_match(conn.from_map, owner_map, fh_owner) && maps_match(conn.to_map, other_map, fh_other);
            bool rev = !conn.IsOneWay() && maps_match(conn.to_map, owner_map, fh_owner) && maps_match(conn.from_map, other_map, fh_other);
            if (fwd) candidates.push_back({conn.from_pos.x, conn.from_pos.y});
            if (rev) candidates.push_back({conn.to_pos.x, conn.to_pos.y});
        }
        // Auto-pairs only if no manual connections — see ClosestPortalDistanceInMap.
        if (candidates.empty()) {
            for (const auto& p : FindPortalPairs(owner_map, other_map)) {
                candidates.push_back({p.pos_a.x, p.pos_a.y});
            }
        }
        if (candidates.empty()) {
            portal_walk_cache[key] = std::numeric_limits<float>::infinity();
            return std::numeric_limits<float>::infinity();
        }

        auto euclidean_min = [&]() -> float {
            float best_sq = std::numeric_limits<float>::infinity();
            for (const auto& c : candidates) {
                float dx = c.x - pos.x, dy = c.y - pos.y;
                float d = dx * dx + dy * dy;
                if (d < best_sq) best_sq = d;
            }
            return best_sq == std::numeric_limits<float>::infinity() ? best_sq : sqrtf(best_sq);
        };

        // Force-load the MilePath; this blocks the worker until the visgraph builds, fine since FindMapRoute runs off the UI thread.
        Pathing::MilePath* mp = nullptr;
        if (owner_map == GW::Map::GetMapID()) {
            mp = GetMilepathForCurrentMap();
        }
        else {
            mp = GetMilepathForMap(owner_map);
            if (!mp) {
                LoadMapFromDAT(owner_map);
                mp = GetMilepathForMap(owner_map);
            }
        }
        if (!mp) {
            float fb = euclidean_min();
            portal_walk_cache[key] = fb;
            return fb;
        }
        while (!mp->ready() && !pending_terminate)
            Sleep(50);
        if (pending_terminate || mp->build_failed()) {
            float fb = euclidean_min();
            portal_walk_cache[key] = fb;
            return fb;
        }

        float best = std::numeric_limits<float>::infinity();
        for (const auto& c : candidates) {
            Pathing::AStar astr(mp);
            auto res = astr.Search(pos, {c.x, c.y, 0});
            if (res == Pathing::Error::OK && astr.m_path.ready()) {
                float cost = astr.m_path.cost();
                if (cost < best) best = cost;
            }
        }
        if (best == std::numeric_limits<float>::infinity()) best = euclidean_min();
        portal_walk_cache[key] = best;
        return best;
    }

    // Trapezoid-graph AStar version of ClosestPortalWalkDistanceInMap: no visgraph needed (faster on first load) but the funnel can be suboptimal.
    float ClosestPortalTrapezoidDistanceInMap(const GW::GamePos& pos, GW::Constants::MapID owner_map, GW::Constants::MapID other_map)
    {
        PortalWalkKey key{(uint32_t)PortalDistanceMode::Trapezoid, (uint32_t)owner_map, (uint32_t)other_map, pos.x, pos.y};
        auto cit = portal_walk_cache.find(key);
        if (cit != portal_walk_cache.end()) return cit->second;

        // Same candidate-collection as the walk variant.
        std::vector<GW::Vec2f> candidates;
        const uint32_t fh_owner = GetMapFileId(owner_map);
        const uint32_t fh_other = GetMapFileId(other_map);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd = maps_match(conn.from_map, owner_map, fh_owner) && maps_match(conn.to_map, other_map, fh_other);
            bool rev = !conn.IsOneWay() && maps_match(conn.to_map, owner_map, fh_owner) && maps_match(conn.from_map, other_map, fh_other);
            if (fwd) candidates.push_back({conn.from_pos.x, conn.from_pos.y});
            if (rev) candidates.push_back({conn.to_pos.x, conn.to_pos.y});
        }
        // Auto-pairs only if no manual connections — see ClosestPortalDistanceInMap.
        if (candidates.empty()) {
            for (const auto& p : FindPortalPairs(owner_map, other_map)) {
                candidates.push_back({p.pos_a.x, p.pos_a.y});
            }
        }
        if (candidates.empty()) {
            portal_walk_cache[key] = std::numeric_limits<float>::infinity();
            return std::numeric_limits<float>::infinity();
        }

        auto euclidean_min = [&]() -> float {
            float best_sq = std::numeric_limits<float>::infinity();
            for (const auto& c : candidates) {
                float dx = c.x - pos.x, dy = c.y - pos.y;
                float d = dx * dx + dy * dy;
                if (d < best_sq) best_sq = d;
            }
            return best_sq == std::numeric_limits<float>::infinity() ? best_sq : sqrtf(best_sq);
        };

        // Acquire MilePath (loads DAT if needed), then borrow its PathingMapData via GetMapData().
        Pathing::MilePath* mp = (owner_map == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(owner_map);
        if (!mp && owner_map != GW::Map::GetMapID()) {
            LoadMapFromDAT(owner_map);
            mp = GetMilepathForMap(owner_map);
        }
        if (!mp) {
            float fb = euclidean_min();
            PATH_LOG_WARNING("[Trapezoid] map %d->%d: no MilePath → Euclidean=%.0f", (int)owner_map, (int)other_map, fb);
            portal_walk_cache[key] = fb;
            return fb;
        }
        const Pathing::PathingMapData* data = mp->GetMapData();
        if (!data || !data->IsValid()) {
            float fb = euclidean_min();
            PATH_LOG_WARNING("[Trapezoid] map %d->%d: PathingMapData missing/invalid → Euclidean=%.0f", (int)owner_map, (int)other_map, fb);
            portal_walk_cache[key] = fb;
            return fb;
        }

        // Get-or-build the trapezoid pathfinder for this map's file_hash.
        const uint32_t fh = GetMapFileId(owner_map);
        if (!fh) {
            float fb = euclidean_min();
            PATH_LOG_WARNING("[Trapezoid] map %d->%d: no file_hash → Euclidean=%.0f", (int)owner_map, (int)other_map, fb);
            portal_walk_cache[key] = fb;
            return fb;
        }
        const uint64_t hash_key = fh; // mile_paths_by_coords also keys by file_hash
        OpenTyria::TrapezoidPathfinder* pf = nullptr;
        auto it = trapezoid_pf_by_coords.find(hash_key);
        if (it != trapezoid_pf_by_coords.end()) {
            pf = it->second;
        }
        else {
            pf = new OpenTyria::TrapezoidPathfinder(data);
            trapezoid_pf_by_coords[hash_key] = pf;
        }

        float best = std::numeric_limits<float>::infinity();
        GW::GamePos src = pos;
        for (const auto& c : candidates) {
            GW::GamePos dst{};
            dst.x = c.x;
            dst.y = c.y;
            dst.zplane = pos.zplane;
            OpenTyria::SearchResult reason = OpenTyria::SearchResult::OK;
            float cost = pf->SearchDistance(src, dst, &reason);
            if (cost < best) best = cost;
        }
        if (best == std::numeric_limits<float>::infinity()) {
            best = euclidean_min(); // portals unreachable via trapezoid walk (disconnected zones) — fall back to straight line
        }
        portal_walk_cache[key] = best;
        return best;
    }

    // Any owner_map portal connecting to other_map (manual first, else first auto-pair); used as a traversal anchor.
    bool FindAnyPortalPosInMap(GW::Constants::MapID owner_map, GW::Constants::MapID other_map, GW::Vec2f& out)
    {
        uint32_t fh_owner = GetMapFileId(owner_map);
        uint32_t fh_other = GetMapFileId(other_map);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd = maps_match(conn.from_map, owner_map, fh_owner) && maps_match(conn.to_map, other_map, fh_other);
            bool rev = !conn.IsOneWay() && maps_match(conn.to_map, owner_map, fh_owner) && maps_match(conn.from_map, other_map, fh_other);
            if (fwd) {
                out = conn.from_pos;
                return true;
            }
            if (rev) {
                out = conn.to_pos;
                return true;
            }
        }
        auto pairs = FindPortalPairs(owner_map, other_map);
        if (!pairs.empty()) {
            out = {pairs[0].pos_a.x, pairs[0].pos_a.y};
            return true;
        }
        return false;
    }

    // Pick the best portal pair for a segment by proximity to start/goal. Priority: manual connections, auto pairs, then boundary fallback.
    bool FindBestPortalPair(GW::Constants::MapID map_a, GW::Constants::MapID map_b, const GW::Vec2f& hint_wm_pos, GW::GamePos& portal_a_out, GW::GamePos& portal_b_out, const GW::GamePos* start_game = nullptr, const GW::GamePos* goal_game = nullptr)
    {
        // Collect candidate pairs (manual + auto), then score by game-coord distance when start/goal are known, else by world-map hint.
        struct Candidate {
            GW::GamePos pos_a, pos_b;
            GW::Vec2f wm_pos; // world map position for hint-based scoring
            const char* source;
        };
        std::vector<Candidate> candidates;

        // 1) Manual portal connections
        {
            uint32_t fh_a = GetMapFileId(map_a);
            uint32_t fh_b = GetMapFileId(map_b);
            // Connection-spread rule (see GetAdjacentMaps): explorable connections apply to all file_hash siblings, outpost ones are local.
            auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
                if (mid == target) return true;
                if (IsOutpostMap(mid)) return false;
                return target_fh && GetMapFileId(mid) == target_fh;
            };
            for (const auto& conn : portal_connections.GetAll()) {
                if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
                bool fwd = (maps_match(conn.from_map, map_a, fh_a) && maps_match(conn.to_map, map_b, fh_b));
                bool rev = !conn.IsOneWay() && (maps_match(conn.from_map, map_b, fh_b) && maps_match(conn.to_map, map_a, fh_a));
                if (!fwd && !rev) continue;
                Candidate c;
                c.pos_a = fwd ? GW::GamePos{conn.from_pos.x, conn.from_pos.y, 0} : GW::GamePos{conn.to_pos.x, conn.to_pos.y, 0};
                c.pos_b = fwd ? GW::GamePos{conn.to_pos.x, conn.to_pos.y, 0} : GW::GamePos{conn.from_pos.x, conn.from_pos.y, 0};
                c.wm_pos = fwd ? conn.from_wm_pos : conn.to_wm_pos;
                c.source = "Manual";
                candidates.push_back(c);
            }
        }

        // 2) Automatic portal prop pairs (only if no manual connections found)
        if (candidates.empty()) {
            auto pairs = FindPortalPairs(map_a, map_b);
            for (const auto& p : pairs) {
                candidates.push_back({p.pos_a, p.pos_b, p.wm_mid, "Portal"});
            }
        }

        // Score and pick the best candidate
        if (!candidates.empty()) {
            if (start_game || goal_game) {
                // Score by actual AStar cost; penalize failures heavily (an unreachable portal must not beat a reachable one).
                constexpr float FAIL_PENALTY = 1e9f;
                float best_score = std::numeric_limits<float>::infinity();
                const Candidate* best = nullptr;
                for (const auto& c : candidates) {
                    float cost_a = 0.f, cost_b = 0.f;
                    const char* method_a = "none";
                    const char* method_b = "none";

                    if (start_game) {
                        auto* mp = (map_a == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(map_a);
                        if (mp && mp->ready()) {
                            auto astr = Pathing::AStar(mp);
                            auto res = astr.Search(*start_game, c.pos_a);
                            if (res == Pathing::Error::OK && astr.m_path.ready()) {
                                cost_a = astr.m_path.cost();
                                method_a = "astar";
                            }
                            else {
                                cost_a = FAIL_PENALTY + GW::GetDistance(*start_game, c.pos_a);
                                method_a = "FAIL";
                            }
                        }
                        else {
                            cost_a = GW::GetDistance(*start_game, c.pos_a);
                            method_a = "line(nomp)";
                        }
                    }
                    if (goal_game) {
                        auto* mp = (map_b == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(map_b);
                        if (mp && mp->ready()) {
                            auto astr = Pathing::AStar(mp);
                            auto res = astr.Search(c.pos_b, *goal_game);
                            if (res == Pathing::Error::OK && astr.m_path.ready()) {
                                cost_b = astr.m_path.cost();
                                method_b = "astar";
                            }
                            else {
                                cost_b = FAIL_PENALTY + GW::GetDistance(c.pos_b, *goal_game);
                                method_b = "FAIL";
                            }
                        }
                        else {
                            cost_b = GW::GetDistance(c.pos_b, *goal_game);
                            method_b = "line(nomp)";
                        }
                    }
                    float cost = cost_a + cost_b;
                    PATH_LOG_INFO("  [%s] a=(%.0f,%.0f) b=(%.0f,%.0f) cost_a=%.0f(%s) cost_b=%.0f(%s) total=%.0f", c.source, c.pos_a.x, c.pos_a.y, c.pos_b.x, c.pos_b.y, cost_a, method_a, cost_b, method_b, cost);
                    if (cost < best_score) {
                        best_score = cost;
                        best = &c;
                    }
                }
                if (best) {
                    portal_a_out = best->pos_a;
                    portal_b_out = best->pos_b;
                    PATH_LOG_INFO("%s winner: map %d (%.0f,%.0f) -- map %d (%.0f,%.0f) [%d candidates, cost=%.0f]", best->source, (int)map_a, portal_a_out.x, portal_a_out.y, (int)map_b, portal_b_out.x, portal_b_out.y, (int)candidates.size(), best_score);
                    return true;
                }
            }
            else {
                // World map hint scoring (fallback when no game positions available)
                float best_score = std::numeric_limits<float>::infinity();
                const Candidate* best = nullptr;
                for (const auto& c : candidates) {
                    float dx = c.wm_pos.x - hint_wm_pos.x, dy = c.wm_pos.y - hint_wm_pos.y;
                    float score = dx * dx + dy * dy;
                    if (score < best_score) {
                        best_score = score;
                        best = &c;
                    }
                }
                if (best) {
                    portal_a_out = best->pos_a;
                    portal_b_out = best->pos_b;
                    PATH_LOG_INFO(
                        "%s connection: map %d (%.0f,%.0f) -- map %d (%.0f,%.0f) [%d candidates, wm_score=%.0f]", best->source, (int)map_a, portal_a_out.x, portal_a_out.y, (int)map_b, portal_b_out.x, portal_b_out.y, (int)candidates.size(), best_score
                    );
                    return true;
                }
            }
        }

        // 3) Fallback: use center of world map bounds overlap as transition point
        auto* area_a = GW::Map::GetMapInfo(map_a);
        auto* area_b = GW::Map::GetMapInfo(map_b);
        if (!area_a || !area_b) return false;

        ImRect wm_a, wm_b;
        if (!GW::Map::GetMapWorldMapBounds(area_a, &wm_a)) return false;
        if (!GW::Map::GetMapWorldMapBounds(area_b, &wm_b)) return false;

        // Overlap center in world map coords
        ImRect overlap(std::max(wm_a.Min.x, wm_b.Min.x), std::max(wm_a.Min.y, wm_b.Min.y), std::min(wm_a.Max.x, wm_b.Max.x), std::min(wm_a.Max.y, wm_b.Max.y));
        GW::Vec2f wm_center = {overlap.GetCenter().x, overlap.GetCenter().y};

        // Convert to each map's game coords (fall back to the current map if unknown)
        if (!WorldMapWidget::WorldMapToGamePos(wm_center, portal_a_out, map_a)) WorldMapWidget::WorldMapToGamePos(wm_center, portal_a_out);
        if (!WorldMapWidget::WorldMapToGamePos(wm_center, portal_b_out, map_b)) WorldMapWidget::WorldMapToGamePos(wm_center, portal_b_out);

        PATH_LOG_INFO("Boundary fallback: map %d (%.0f,%.0f) -- map %d (%.0f,%.0f) wm=(%.0f,%.0f)", (int)map_a, portal_a_out.x, portal_a_out.y, (int)map_b, portal_b_out.x, portal_b_out.y, wm_center.x, wm_center.y);
        return true;
    }

    // Run AStar on a specific map between two game-coord positions
    // Returns the path points in that map's coordinate system
    bool RunAStarOnMap(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::GamePos>& path_out)
    {
        Pathing::MilePath* mp = nullptr;
        if (map_id == GW::Map::GetMapID()) {
            mp = GetMilepathForCurrentMap();
        }
        else {
            mp = GetMilepathForMap(map_id);
        }
        if (!mp) {
            PATH_LOG_ERROR("[AStar] map %d FAILED: GetMilepath returned null", (int)map_id);
            return false;
        }

        while (!mp->ready() && !pending_terminate) {
            Sleep(100);
        }
        if (pending_terminate) {
            PATH_LOG_INFO("[AStar] map %d aborted: pending_terminate", (int)map_id);
            return false;
        }
        if (mp->build_failed()) {
            PATH_LOG_ERROR("[AStar] map %d FAILED: MilePath build OOM'd", (int)map_id);
            return false;
        }

        // Try exact positions first
        {
            auto astr = Pathing::AStar(mp);
            auto res = astr.Search(from, to);
            if (res == Pathing::Error::OK && astr.m_path.ready()) {
                path_out = astr.m_path.points();
                PATH_LOG_INFO("[AStar] map %d OK=direct, cost=%.0f, points=%d", (int)map_id, astr.m_path.cost(), (int)path_out.size());
                return true;
            }
        }

        // Try cardinal offsets on start and goal (handles isolated trapezoids at portals)
        constexpr float step = 600.f;
        constexpr float dirs[4][2] = {{-step, 0}, {step, 0}, {0, -step}, {0, step}};
        float best_cost = std::numeric_limits<float>::max();
        int offset_attempts = 0, offset_successes = 0;

        // Try offsetting start
        for (const auto& d : dirs) {
            offset_attempts++;
            GW::GamePos adj_from = {from.x + d[0], from.y + d[1], from.zplane};
            auto astr = Pathing::AStar(mp);
            auto res = astr.Search(adj_from, to);
            if (res == Pathing::Error::OK && astr.m_path.ready() && astr.m_path.cost() < best_cost) {
                offset_successes++;
                best_cost = astr.m_path.cost();
                path_out = astr.m_path.points();
                path_out.insert(path_out.begin(), from);
            }
        }
        // Try offsetting goal
        for (const auto& d : dirs) {
            offset_attempts++;
            GW::GamePos adj_to = {to.x + d[0], to.y + d[1], to.zplane};
            auto astr = Pathing::AStar(mp);
            auto res = astr.Search(from, adj_to);
            if (res == Pathing::Error::OK && astr.m_path.ready() && astr.m_path.cost() < best_cost) {
                offset_successes++;
                best_cost = astr.m_path.cost();
                path_out = astr.m_path.points();
                path_out.push_back(to);
            }
        }
        if (!path_out.empty()) {
            PATH_LOG_INFO("[AStar] map %d OK=offset(%d/%d), cost=%.0f, points=%d", (int)map_id, offset_successes, offset_attempts, best_cost, (int)path_out.size());
            return true;
        }

        PATH_LOG_ERROR("[AStar] map %d FAILED (direct + %d offset attempts)", (int)map_id, offset_attempts);
        return false;
    }

    // Multi-map pathfinding: run AStar segments connected by portal pairs
    void RecalculateMultiMapPath(const std::vector<GW::Constants::MapID>& route, const GW::GamePos& start, const GW::GamePos& goal)
    {
        ClearPathLines();
        ClearPortalPairLines();
        delete astar;
        astar = nullptr;

        // Capture copies for worker thread
        auto route_copy = route;
        auto start_copy = start;
        auto goal_copy = goal;
        auto start_wm = path_from_world;
        auto goal_wm = path_to_world;
        auto cur_map = GW::Map::GetMapID();

        struct PortalPairDraw {
            GW::GamePos a, b;
            GW::Constants::MapID map_a, map_b;
        };

        Resources::EnqueueWorkerTask([route_copy, start_copy, goal_copy, start_wm, goal_wm, cur_map] {
            RouteJobScope job_scope; // defer eviction while we hold MilePath*
            PathCalcScope calc_scope; // flag a path calculation for the world-map indicator
            std::vector<GW::GamePos> full_path;
            std::vector<HiddenPathSegment> hidden_segments;
            std::vector<PortalPairDraw> portal_draws;

            // Track the last used portal's world map position for chaining
            GW::Vec2f last_portal_wm = start_wm;
            GW::GamePos last_seg_end = start_copy; // last known game position (for portal scoring)

            for (size_t seg = 0; seg < route_copy.size(); seg++) {
                auto map_id = route_copy[seg];
                GW::GamePos seg_from, seg_to;

                // Determine segment start
                if (seg == 0) {
                    seg_from = start_copy;
                }
                else {
                    // Entry portal from previous map — score by distance from last position
                    GW::GamePos prev_exit, this_entry;
                    const GW::GamePos* gg = (seg == route_copy.size() - 1) ? &goal_copy : nullptr;
                    if (!FindBestPortalPair(route_copy[seg - 1], map_id, last_portal_wm, prev_exit, this_entry, &last_seg_end, gg)) {
                        PATH_LOG_ERROR("No portal pair between map %d and %d", (int)route_copy[seg - 1], (int)map_id);
                        return;
                    }
                    seg_from = this_entry;
                    portal_draws.push_back({prev_exit, this_entry, route_copy[seg - 1], map_id});
                    // Update last_portal_wm to the entry portal's world map position
                    WorldMapWidget::GamePosToWorldMap(this_entry, last_portal_wm, map_id);
                }

                // Determine segment end
                if (seg == route_copy.size() - 1) {
                    seg_to = goal_copy;
                }
                else {
                    // Exit portal to next map — score by distance from seg_from + to goal
                    GW::GamePos this_exit, next_entry;
                    GW::Vec2f hint = last_portal_wm;
                    const GW::GamePos* gg = (seg == route_copy.size() - 2) ? &goal_copy : nullptr;
                    if (!FindBestPortalPair(map_id, route_copy[seg + 1], hint, this_exit, next_entry, &seg_from, gg)) {
                        PATH_LOG_ERROR("No portal pair between map %d and %d", (int)map_id, (int)route_copy[seg + 1]);
                        return;
                    }
                    seg_to = this_exit;
                    // Update chain hint to exit portal position
                    WorldMapWidget::GamePosToWorldMap(this_exit, last_portal_wm, map_id);
                }

                PATH_LOG_INFO("Segment %d: map %d from=(%.0f,%.0f) to=(%.0f,%.0f)", (int)seg, (int)map_id, seg_from.x, seg_from.y, seg_to.x, seg_to.y);

                std::vector<GW::GamePos> seg_path;
                if (!RunAStarOnMap(map_id, seg_from, seg_to, seg_path)) {
                    PATH_LOG_ERROR("AStar failed on segment %d (map %d)", (int)seg, (int)map_id);
                    return;
                }

                // Track last position for next portal scoring
                last_seg_end = seg_to;

                // Hide intermediate segments whose entry AND exit are no_draw (underground maps). Never hide first/last or the current map.
                bool is_first = (seg == 0);
                bool is_last = (seg + 1 == route_copy.size());
                bool segment_hidden = !is_first && !is_last && map_id != cur_map && HasNoDrawConnection(route_copy[seg - 1], map_id) && HasNoDrawConnection(map_id, route_copy[seg + 1]);

                if (segment_hidden) {
                    // Native-coord segment for underground rendering; path-break so the world-map line skips the gap.
                    hidden_segments.push_back({seg_path, map_id});
                    if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                        full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE, 0});
                    }
                    continue;
                }

                // Path-break between segments: the cross-map portal hop isn't walkable and would draw an off-map connector.
                if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                    full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE, 0});
                }

                // Convert segment to current map coords and append
                auto converted = ConvertPathToCurrentMap(seg_path, map_id);
                full_path.insert(full_path.end(), converted.begin(), converted.end());
            }

            PATH_LOG_INFO("Multi-map path: %d total points across %d maps (%d hidden underground segments)", (int)full_path.size(), (int)route_copy.size(), (int)hidden_segments.size());
            Resources::EnqueueMainTask([full_path, hidden_segments, cur_map] {
                DrawPathAsLines(full_path, cur_map);
                AddHiddenUndergroundSegmentLines(hidden_segments);
            });
        });
    }

    // Blocking pure-computation core (caller owns the worker + RouteJobScope): builds the route's world-coord points (PATH_BREAK
    // between maps) and hidden underground segments, retrying with edge-blacklisting on AStar failure. False if no route.
    bool BuildCrossMapRoute(GW::Constants::MapID from_map, GW::Constants::MapID to_map, const GW::GamePos& start, const GW::GamePos& goal, const GW::Vec2f& start_wm, std::vector<GW::Vec2f>& out_points, std::vector<HiddenPathSegment>& out_hidden)
    {
        const auto cur_map = GW::Map::GetMapID();
        constexpr int max_retries = 3;
        for (int attempt = 0; attempt <= max_retries; attempt++) {
            auto route = FindMapRoute(from_map, to_map, &start, &goal);
            if (route.empty()) {
                // Expected for unreachable markers — keep it out of chat (the caller backs off).
                PATH_LOG_INFO("No map route found from map %d to map %d", (int)from_map, (int)to_map);
                blacklisted_edges.clear();
                return false;
            }
            std::string route_str;
            for (auto m : route) {
                route_str += std::to_string((int)m) + " ";
            }
            PATH_LOG_INFO("Map route (attempt %d): %s (%d maps)", attempt, route_str.c_str(), (int)route.size());

            for (auto m : route) {
                if (m != cur_map) LoadMapFromDAT(m);
            }

            // Points accumulate as world-map coords (the common cross-map space) — never projected into another map's game space.
            std::vector<GW::Vec2f> full_path;
            std::vector<HiddenPathSegment> hidden_segments;
            GW::Vec2f last_portal_wm = start_wm;
            GW::GamePos last_seg_end = start;
            bool failed = false;

            for (size_t seg = 0; seg < route.size(); seg++) {
                auto map_id = route[seg];
                GW::GamePos seg_from, seg_to;

                if (seg == 0) {
                    seg_from = start;
                }
                else {
                    GW::GamePos prev_exit, this_entry;
                    const GW::GamePos* gg = (seg == route.size() - 1) ? &goal : nullptr;
                    if (!FindBestPortalPair(route[seg - 1], map_id, last_portal_wm, prev_exit, this_entry, &last_seg_end, gg)) {
                        PATH_LOG_ERROR("No portal pair between map %d and %d", (int)route[seg - 1], (int)map_id);
                        blacklisted_edges.clear();
                        return false;
                    }
                    seg_from = this_entry;
                    WorldMapWidget::GamePosToWorldMap(this_entry, last_portal_wm, map_id);
                }

                if (seg == route.size() - 1) {
                    seg_to = goal;
                }
                else {
                    GW::GamePos this_exit, next_entry;
                    const GW::GamePos* gg = (seg == route.size() - 2) ? &goal : nullptr;
                    if (!FindBestPortalPair(map_id, route[seg + 1], last_portal_wm, this_exit, next_entry, &seg_from, gg)) {
                        PATH_LOG_ERROR("No portal pair between map %d and %d", (int)map_id, (int)route[seg + 1]);
                        blacklisted_edges.clear();
                        return false;
                    }
                    seg_to = this_exit;
                    WorldMapWidget::GamePosToWorldMap(this_exit, last_portal_wm, map_id);
                }

                PATH_LOG_INFO("Segment %d: map %d from=(%.0f,%.0f) to=(%.0f,%.0f)", (int)seg, (int)map_id, seg_from.x, seg_from.y, seg_to.x, seg_to.y);

                std::vector<GW::GamePos> seg_path;
                if (!RunAStarOnMap(map_id, seg_from, seg_to, seg_path)) {
                    // Transit map without DAT data (underground/instance): if both entry and exit connections exist, walk it as a straight line.
                    bool has_entry = seg == 0 || GetConnectionCost(route[seg - 1], map_id) < 1e9f;
                    bool has_exit = seg + 1 >= route.size() || GetConnectionCost(map_id, route[seg + 1]) < 1e9f;
                    if (has_entry && has_exit) {
                        PATH_LOG_INFO("AStar unavailable on map %d, using direct transition", (int)map_id);
                        seg_path = {seg_from, seg_to};
                    }
                    else {
                        // Blacklist this map's entry/exit edges and retry. Resolve to graph representatives — Dijkstra works on graph nodes.
                        auto resolve_graph = [](GW::Constants::MapID m) -> GW::Constants::MapID {
                            for (const auto& n : map_graph_nodes) {
                                if (n.map_id == m) return m;
                            }
                            uint32_t fh = GetMapFileId(m);
                            if (fh) {
                                for (const auto& n : map_graph_nodes) {
                                    if (n.file_hash == fh) return n.map_id;
                                }
                            }
                            return m;
                        };
                        auto map_g = resolve_graph(map_id);
                        if (seg > 0) {
                            auto prev_g = resolve_graph(route[seg - 1]);
                            PATH_LOG_WARNING("AStar failed on map %d, blacklisting edge %d->%d (graph %d->%d)", (int)map_id, (int)route[seg - 1], (int)map_id, (int)prev_g, (int)map_g);
                            blacklisted_edges.insert(EdgeKey(prev_g, map_g));
                            blacklisted_edges.insert(EdgeKey(map_g, prev_g));
                        }
                        if (seg + 1 < route.size()) {
                            auto next_g = resolve_graph(route[seg + 1]);
                            blacklisted_edges.insert(EdgeKey(map_g, next_g));
                            blacklisted_edges.insert(EdgeKey(next_g, map_g));
                        }
                        failed = true;
                        break;
                    }
                }

                last_seg_end = seg_to;

                // Hide intermediate segments whose entry AND exit are no_draw (underground); never hide the current map.
                bool is_first_seg = (seg == 0);
                bool is_last_seg = (seg + 1 == route.size());
                bool segment_hidden = !is_first_seg && !is_last_seg && map_id != cur_map && HasNoDrawConnection(route[seg - 1], map_id) && HasNoDrawConnection(map_id, route[seg + 1]);

                if (segment_hidden) {
                    // Record native-coord segment for underground-terrain / mission-map rendering.
                    hidden_segments.push_back({seg_path, map_id});
                    if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                        full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE});
                    }
                    continue;
                }

                // Path-break between segments — the cross-map portal hop isn't walkable in any single coord space.
                if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                    full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE});
                }

                SegmentToWorld(seg_path, map_id, full_path);
            }

            if (!failed) {
                PATH_LOG_INFO("Multi-map path: %d total points across %d maps (%d hidden underground segments)", (int)full_path.size(), (int)route.size(), (int)hidden_segments.size());
                out_points = std::move(full_path);
                out_hidden = std::move(hidden_segments);
                blacklisted_edges.clear();
                return true;
            }

            if (attempt == max_retries) {
                PATH_LOG_ERROR("All %d route attempts failed from map %d to map %d", max_retries + 1, (int)from_map, (int)to_map);
            }
        }
        blacklisted_edges.clear();
        return false;
    }

    void RecalculateMultiMapPath_WithRetry(GW::Constants::MapID from_map, GW::Constants::MapID to_map, const GW::GamePos& start, const GW::GamePos& goal, const GW::Vec2f& start_wm)
    {
        ClearPathLines();
        ClearPortalPairLines();
        delete astar;
        astar = nullptr;

        auto cur_map = GW::Map::GetMapID();

        Resources::EnqueueWorkerTask([from_map, to_map, start, goal, start_wm, cur_map] {
            RouteJobScope job_scope; // defer eviction while we hold MilePath*
            PathCalcScope calc_scope; // flag a path calculation for the world-map indicator
            std::vector<GW::Vec2f> full_path;
            std::vector<HiddenPathSegment> hidden_segments;
            if (!BuildCrossMapRoute(from_map, to_map, start, goal, start_wm, full_path, hidden_segments)) return;
            // World-map-only preview: points are world coords, drawn as world-coord lines.
            Resources::EnqueueMainTask([full_path, cur_map] {
                ClearPathLines();
                for (size_t i = 0; i + 1 < full_path.size(); i++) {
                    if (IsPathBreak(full_path[i]) || IsPathBreak(full_path[i + 1])) continue;
                    auto* line = Minimap::Instance().custom_renderer.AddCustomLine({full_path[i].x, full_path[i].y, 0}, {full_path[i + 1].x, full_path[i + 1].y, 0});
                    line->map = cur_map;
                    line->world_coords = true;
                    line->draw_on_minimap = false;
                    line->draw_on_mission_map = false;
                    line->color = 0xFFFFFF00;
                    line->created_by_toolbox = true;
                    path_lines.push_back(line);
                }
            });
        });
    }

    void RecalculatePath(const GW::GamePos& from, const GW::GamePos& to)
    {
        if (!NeedsRecalculating(from, to)) return;
        ClearPathLines();
        delete astar;
        astar = nullptr;
        // Use path_from_map if both are on the same map, otherwise current map
        auto target_map = GW::Map::GetMapID();
        if (path_from_map != GW::Constants::MapID::None && path_from_map == path_to_map) target_map = path_from_map;
        const auto map_id = target_map;
        Resources::EnqueueWorkerTask([from, to, map_id] {
            RouteJobScope job_scope; // defer eviction while we hold MilePath*
            PathCalcScope calc_scope; // flag a path calculation for the world-map indicator
            Pathing::MilePath* milepath = nullptr;
            if (map_id == GW::Map::GetMapID()) {
                milepath = GetMilepathForCurrentMap();
            }
            else {
                milepath = GetMilepathForMap(map_id);
            }
            if (!milepath) {
                PATH_LOG_ERROR("No milepath for map %d", (int)map_id);
                return;
            }
            // Wait for vis graph to finish building
            if (!milepath->ready()) {
                PATH_LOG_INFO("Waiting for map %d vis graph...", (int)map_id);
                while (!milepath->ready() && !pending_terminate) {
                    Sleep(100);
                }
                if (pending_terminate) return;
                PATH_LOG_INFO("Map %d ready", (int)map_id);
            }
            const auto tmpAstar = new Pathing::AStar(milepath);
            const auto res = tmpAstar->Search(from, to);
            if (res == Pathing::Error::FailedToFinializePath) {
                delete tmpAstar;
                return;
            }
            if (res != Pathing::Error::OK) {
                PATH_LOG_ERROR("Pathing failed; Pathing::Error code %d", res);
                delete tmpAstar;
                return;
            }
            if (!tmpAstar->m_path.ready()) {
                PATH_LOG_ERROR("Pathing failed; tmpAstar->m_path not ready");
                delete tmpAstar;
                return;
            }
            astar = tmpAstar;
            // Draw path on main thread
            const auto points = astar->m_path.points(); // copy
            Resources::EnqueueMainTask([points, map_id] {
                DrawPathAsLines(points, map_id);
            });
        });
    }

    // MapID → file_hash lookup built from maps_constant_data.h
    std::unordered_map<GW::Constants::MapID, uint32_t> map_id_to_file_hash;

    void BuildMapFileHashLookup()
    {
        for (const auto& [file_hash, entries] : constant_maps_info) {
            for (const auto& entry : entries) {
                if (entry.file_hash && !map_id_to_file_hash.contains(entry.map_id)) map_id_to_file_hash[entry.map_id] = (uint32_t)entry.file_hash;
            }
        }
        PATH_LOG_INFO("Built map file hash lookup: %d entries", (int)map_id_to_file_hash.size());
        auto it837 = map_id_to_file_hash.find(GW::Constants::MapID::War_in_Kryta_Talmark_Wilderness);
        PATH_LOG_INFO("  map 837: %s (0x%X)", it837 != map_id_to_file_hash.end() ? "found" : "NOT FOUND", it837 != map_id_to_file_hash.end() ? it837->second : 0);
        auto it381 = map_id_to_file_hash.find(GW::Constants::MapID::Yohlon_Haven_outpost);
        PATH_LOG_INFO("  map 381: %s (0x%X)", it381 != map_id_to_file_hash.end() ? "found" : "NOT FOUND", it381 != map_id_to_file_hash.end() ? it381->second : 0);
        int found_381 = 0;
        for (const auto& [fh, entries] : constant_maps_info) {
            for (const auto& e : entries) {
                if (e.map_id == GW::Constants::MapID::Yohlon_Haven_outpost) {
                    PATH_LOG_INFO("  constant_maps_info: map=381 file_hash=0x%X outer_key=0x%X", e.file_hash, fh);
                    found_381++;
                }
            }
        }
        PATH_LOG_INFO("  found 381 in constant_maps_info %d times", found_381);
    }

    std::set<uint32_t> file_id_mismatch_warned; // maps already warned about

    uint32_t GetMapFileId(GW::Constants::MapID map_id)
    {
        // Check runtime lookup table (populated from constant_maps_info + StoC packets)
        auto it = map_id_to_file_hash.find(map_id);
        if (it != map_id_to_file_hash.end()) return it->second;

        // Fall back to AreaInfo
        const auto area_info = GW::Map::GetMapInfo(map_id);
        uint32_t runtime_fid = (area_info && area_info->file_id) ? area_info->file_id : 0;

        // Fall back to constant_maps_info (covers custom/remapped map IDs)
        uint32_t constant_fid = 0;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if (!file_hash) continue;
            for (const auto& e : entries) {
                if (e.map_id == map_id) {
                    constant_fid = (uint32_t)file_hash;
                    break;
                }
            }
            if (constant_fid) break;
        }

        // Warn once per map about runtime/constant file_id discrepancies; skip map 0 (None has no file_id and is looked up routinely).
        if ((uint32_t)map_id != 0 && !file_id_mismatch_warned.contains((uint32_t)map_id)) {
            if (map_id == GW::Constants::MapID::Shing_Jea_Monastery_outpost) {
                int found_count = 0;
                uint32_t found_fh = 0;
                for (const auto& [fh, entries] : constant_maps_info) {
                    for (const auto& e : entries) {
                        if (e.map_id == GW::Constants::MapID::Shing_Jea_Monastery_outpost) {
                            found_count++;
                            found_fh = (uint32_t)fh;
                        }
                    }
                }
                PATH_LOG_INFO(
                    "[FileId] map 242 debug: runtime=0x%X constant=0x%X lookup=%s brute=%d(0x%X) total_groups=%d", runtime_fid, constant_fid, map_id_to_file_hash.contains(GW::Constants::MapID::Shing_Jea_Monastery_outpost) ? "in table" : "NOT in table",
                    found_count, found_fh, (int)constant_maps_info.size()
                );
            }
            if (runtime_fid && constant_fid && runtime_fid != constant_fid) {
                file_id_mismatch_warned.insert((uint32_t)map_id);
                PATH_LOG_WARNING("[FileId] map %d: runtime=0x%X constant=0x%X MISMATCH", (int)map_id, runtime_fid, constant_fid);
            }
            else if (runtime_fid && !constant_fid) {
                file_id_mismatch_warned.insert((uint32_t)map_id);
                PATH_LOG_WARNING("[FileId] map %d: runtime=0x%X but MISSING from maps_constant_data.h", (int)map_id, runtime_fid);
            }
            else if (!runtime_fid && !constant_fid) {
                file_id_mismatch_warned.insert((uint32_t)map_id);
                PATH_LOG_WARNING("[FileId] map %d: no file_id from runtime or constant data", (int)map_id);
            }
        }

        uint32_t result = runtime_fid ? runtime_fid : constant_fid;
        if (result) map_id_to_file_hash[map_id] = result;
        return result;
    }

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kLoadMapContext: {
                // Cache MapID → file_id from the packet (discovers maps not in maps_constant_data.h)
                const auto packet = static_cast<GW::UI::UIPacket::kLoadMapContext*>(wParam);
                if (packet->file_name && *packet->file_name) {
                    const uint32_t fid = ArenaNetFileParser::FileHashToFileId(packet->file_name);
                    if (fid && !map_id_to_file_hash.contains(packet->map_id)) {
                        map_id_to_file_hash[packet->map_id] = fid;
                        // Rebuild graph to include newly discovered map
                        map_graph_built = false;
                        PATH_LOG_INFO("Discovered map %d file_id=0x%X", (int)packet->map_id, fid);
                    }
                }

                // Clear all lines on map change to avoid stale renders
                ClearBoundsLines();
                ClearGraphEdgeLines();
                ClearPortalMarkerLines();
                ClearHoverHighlightLines();
                ClearMarkerLines();
                ClearPathLines();
                ClearPortalPairLines();
                ClearSavedConnectionLines();
                ClearEditorHighlightLines();
                delete astar;
                astar = nullptr;

                PathfindingWindow::ReadyForPathing();
                pending_connection_lines_update = true;
            } break;
        }
    }

} // namespace

bool PathfindingWindow::IsPathingEnabled()
{
    return pathing_enabled;
}

bool PathfindingWindow::ReadyForPathing()
{
    // Never load the DAT here: probe the resident cache and, if missing, kick a background load. Returns false
    // until that load lands, so callers (e.g. the quest path) simply retry next frame — the game thread never stalls.
    const auto m = GetResidentMilepathOrPrewarm();
    return m && m->ready();
}

bool PathfindingWindow::IsCalculatingPath()
{
    return path_calc_in_flight.load() > 0;
}

void LoadAndShowMapsAtWorldPos(const GW::Vec2f& wm_pos); // forward decl

// Editor UI was trimmed in this branch; the pathing API needs no window. Draw renders nothing but must still drain the
// deferred line-removal queue each frame, else cleared route lines stay on screen. WndProc is a no-op vtable stub.
static void UpdateNavmeshOverlay()
{
    static bool was_on = false;
    static GW::Constants::MapID built_map = GW::Constants::MapID::None;
    static GW::GamePos last_build_pos{};
    static std::vector<Pathing::NavMesh::DebugEdge> cached_edges; // whole-map edges, extracted once per map
    if (!settings.draw_navmesh_overlay) {
        if (was_on) {
            GameWorldRenderer::ClearNavmeshLines();
            DeferRemoveLines(navmesh_edge_lines); // drop any lines left by the old per-line path
            cached_edges.clear();
            built_map = GW::Constants::MapID::None;
            was_on = false;
        }
        return;
    }
    was_on = true;

    const auto cur_map = GW::Map::GetMapID();
    const auto me = GW::Agents::GetControlledCharacter();
    const bool map_changed = cur_map != built_map;
    float moved2 = 1e30f;
    if (me && !map_changed) { const float dx = me->pos.x - last_build_pos.x, dy = me->pos.y - last_build_pos.y; moved2 = dx * dx + dy * dy; }
    if (!map_changed && moved2 < 600.f * 600.f) return; // near set still fresh; nothing to rebuild

    auto* mp = GetResidentMilepathOrPrewarm(); // Draw runs on the game thread — must not block on a DAT read
    if (!mp || !mp->ready()) return;
    auto* nav = mp->GetNavMeshForDebug();
    if (!nav || !nav->IsReady()) {
        static Pathing::MilePath* build_requested = nullptr;
        if (build_requested != mp && !mp->build_failed()) {
            build_requested = mp;
            Resources::EnqueueWorkerTask([mp] { mp->EnsureFullBuild(); });
        }
        return;
    }

    auto edge_color = [](const Pathing::NavMesh::DebugEdge& e) -> unsigned int {
        const bool hi = e.a.zplane != 0; // edges off the ground plane get the "hi" colour
        return e.wall ? (hi ? settings.navmesh_wall_color_hi : settings.navmesh_wall_color)
                      : (hi ? settings.navmesh_connection_color_hi : settings.navmesh_connection_color);
    };

    if (map_changed) {
        cached_edges.clear();
        nav->DebugExtractEdges(cached_edges); // extract the whole mesh once; re-cull cheaply on movement
        // Hand the FULL mesh to the 2D world map (it's flat + cheap there, no draping / no per-move re-cull needed).
        std::vector<GameWorldRenderer::BatchedLine> full;
        full.reserve(cached_edges.size());
        for (const auto& e : cached_edges) full.push_back({e.a, e.b, edge_color(e)});
        GameWorldRenderer::SetNavmeshWorldMapLines(cur_map, std::move(full));
    }

    // In-world: cull to the shared in-world render distance around the player so the batch stays small (drapes fast,
    // tracks the player) — handed to the renderer as ONE double-buffered batched VB (one draw call, no flicker).
    const float draw_range = GameWorldRenderer::GetRenderMaxDistance();
    const float range2 = draw_range * draw_range;
    std::vector<GameWorldRenderer::BatchedLine> lines;
    lines.reserve(cached_edges.size());
    for (const auto& e : cached_edges) {
        if (me) { const float dx = me->pos.x - e.a.x, dy = me->pos.y - e.a.y; if (dx * dx + dy * dy > range2) continue; }
        lines.push_back({e.a, e.b, edge_color(e)});
    }
    GameWorldRenderer::SetNavmeshLines(cur_map, std::move(lines));
    built_map = cur_map;
    if (me) last_build_pos = me->pos;
}

float PathfindingWindow::GetPathRecalcDistance() { return settings.path_recalc_distance; }

bool PathfindingWindow::DebugDumpNavMeshNear(const GW::GamePos& center, float radius)
{
    auto* mp = GetResidentMilepathOrPrewarm();
    if (!mp || !mp->ready()) { Log::Log("[navdump] milepath not ready (retry)"); return false; }
    auto* nav = mp->GetNavMeshForDebug();
    if (!nav || !nav->IsReady()) {
        if (!mp->build_failed()) Resources::EnqueueWorkerTask([mp] { mp->EnsureFullBuild(); });
        Log::Log("[navdump] navmesh building (retry)");
        return false;
    }
    Log::Log("[navdump] map=%d", (int)GW::Map::GetMapID());
    nav->DebugDumpNear(center, radius);
    return true;
}

Pathing::NavMesh* PathfindingWindow::GetResidentNavMesh()
{
    auto* mp = GetResidentMilepathOrPrewarm();
    if (!mp || !mp->ready()) return nullptr;
    auto* nav = mp->GetNavMeshForDebug();
    return (nav && nav->IsReady()) ? nav : nullptr;
}

void PathfindingWindow::Draw(IDirect3DDevice9*)
{
    ProcessDeferredRemovals();
    UpdateNavmeshOverlay();
}

void PathfindingWindow::DrawSettingsInternal()
{
    ImGui::DragFloat("Path recalc distance", &settings.path_recalc_distance, 1.f, 1.f, 1000.f, "%.0f");
    ImGui::ShowHelp("How far you must move (game units / gwinches) before the rendered quest path recomputes. Lower = more responsive but heavier; the recompute is also rate-capped to ~30/s.");
    ImGui::Checkbox("Navmesh overlay", &settings.draw_navmesh_overlay);
    ImGui::ShowHelp("Draw the navmesh's polygon edges on the ground near you, at correct terrain heights (bridges included).");
    ImGui::Separator();
    if (settings.draw_navmesh_overlay) {
        auto color_edit = [](const char* label, uint32_t* argb) {
            float c[4] = {((*argb >> 16) & 0xFF) / 255.f, ((*argb >> 8) & 0xFF) / 255.f, (*argb & 0xFF) / 255.f, ((*argb >> 24) & 0xFF) / 255.f};
            if (ImGui::ColorEdit4(label, c, ImGuiColorEditFlags_AlphaBar)) {
                const auto q = [](const float f) { return static_cast<uint32_t>(std::clamp(f * 255.f + 0.5f, 0.f, 255.f)); };
                *argb = (q(c[3]) << 24) | (q(c[0]) << 16) | (q(c[1]) << 8) | q(c[2]);
            }
        };
        if (ImGui::DragFloat("Terrain sample spacing", &settings.navmesh_sample_spacing, 0.5f, 1.f, 100.f, "%.0f gw")) {
            GameWorldRenderer::SetNavmeshSampleSpacing(settings.navmesh_sample_spacing);
            GameWorldRenderer::RedrapeNavmesh(); // re-drape live so the change is visible immediately
        }
        ImGui::ShowHelp("How often the overlay samples terrain height when draping edges (game units). Lower = lines hug the floor/steps more exactly, but more vertices to build and draw.");
        color_edit("Wall colour (ground plane)", &settings.navmesh_wall_color);
        color_edit("Wall colour (other planes)", &settings.navmesh_wall_color_hi);
        color_edit("Connection colour (ground plane)", &settings.navmesh_connection_color);
        color_edit("Connection colour (other planes)", &settings.navmesh_connection_color_hi);
    }
}

void PathfindingWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    GameWorldRenderer::SetNavmeshSampleSpacing(settings.navmesh_sample_spacing);
}

void PathfindingWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

bool PathfindingWindow::WndProc(UINT, WPARAM, LPARAM)
{
    return false;
}

void PathfindingWindow::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    pathing_enabled = false;
    pending_terminate = true;
    GW::UI::RemoveUIMessageCallback(&gw_ui_hookentry);
    ClearBoundsLines();
    ClearGraphEdgeLines();
    ClearPortalMarkerLines();
    ClearHoverHighlightLines();
    ClearMarkerLines();
    ClearPathLines();
    ClearPortalPairLines();
    ClearSavedConnectionLines();
    ClearEditorHighlightLines();
    // Signal all milepaths to stop but don't delete yet (Terminate does that)
    for (const auto mile_path : mile_paths_by_coords | std::views::values) {
        mile_path->stopProcessing();
    }
}

bool PathfindingWindow::CanTerminate()
{
    if (!pending_terminate && pending_worker_task) return false;
    for (const auto m : mile_paths_by_coords) {
        if (m.second->isProcessing()) return false;
    }
    return true;
}

clock_t PathfindingWindow::CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args)
{
    if (pending_terminate) return 0;

    if (!ReadyForPathing()) return 0;

    pending_worker_task = true;

    Resources::EnqueueWorkerTask([from, to, callback, args] {
        RouteJobScope job_scope; // defer eviction while we hold MilePath*
        // Always fire the callback exactly once; silent-failing leaves the caller's "calculating" flag stuck and the path frozen.
        auto fire_empty = [callback, args] {
            Resources::EnqueueMainTask([callback, args] {
                std::vector<GW::GamePos> empty_vec = {};
                callback(empty_vec, args);
            });
        };

        if (pending_terminate) {
            pending_worker_task = false;
            return;
        }

        const auto milepath = GetMilepathForCurrentMap();
        if (!milepath || !milepath->ready()) {
            if (milepath && milepath->build_failed()) {
                PATH_LOG_ERROR("Pathing failed; MilePath build OOM'd, visgraph unavailable");
            }
            fire_empty();
            pending_worker_task = false;
            return;
        }

        auto astr = Pathing::AStar(milepath);
        const auto res = astr.Search(from, to);
        if (res == Pathing::Error::FailedToFinializePath) {
            // Path route is blocked; this is a valid result.
            fire_empty();
            pending_worker_task = false;
            return;
        }
        if (res != Pathing::Error::OK) {
            PATH_LOG_ERROR("Pathing failed; Pathing::Error code %d", res);
            fire_empty();
            pending_worker_task = false;
            return;
        }
        if (!astr.m_path.ready()) {
            Log::Log("Pathing failed; astar.m_path not ready");
            fire_empty();
            pending_worker_task = false;
            return;
        }

        const auto& points = astr.m_path.points();
        auto waypoints = new std::vector<GW::GamePos>();
        waypoints->reserve(points.size());
        for (const auto& p : points) {
            waypoints->emplace_back(p);
        }

        Resources::EnqueueMainTask([waypoints, callback, args] {
            callback(*waypoints, args);
            delete waypoints;
        });
        pending_worker_task = false;
    });
    return TIMER_INIT();
}

void PathfindingWindow::Terminate()
{
    ToolboxModule::Terminate();
    // Workers are already stopped/joined, so each `delete mp` is pure allocator work; parallelize since the Debug allocator is slow serially.
    if (!mile_paths_by_coords.empty()) {
        std::vector<std::thread> deletes;
        deletes.reserve(mile_paths_by_coords.size());
        for (const auto& [hash, mp] : mile_paths_by_coords) {
            deletes.emplace_back([mp] {
                delete mp;
            });
        }
        for (auto& t : deletes)
            t.join();
    }
    mile_paths_by_coords.clear();
    {
        std::scoped_lock lock(lru_mutex);
        lru_order.clear();
        lru_pos.clear();
        route_jobs_active = 0;
    }
    path_calc_in_flight = 0; // force-clear the indicator on shutdown (mirrors route_jobs_active above)
    // Trapezoid pathfinders own only scratch buffers (no threads), so serial frees are fine.
    for (const auto& [hash, pf] : trapezoid_pf_by_coords)
        delete pf;
    trapezoid_pf_by_coords.clear();
    cached_map_info.clear();
    portal_props_cache.clear();
    map_graph_nodes.clear();
    map_graph_built = false;
    points_by_hash.clear();
    map_id_to_file_hash.clear();
    delete astar;
    astar = nullptr;
}

uint32_t PathfindingWindow::GetMapFileId(GW::Constants::MapID map_id)
{
    return ::GetMapFileId(map_id);
}

void PathfindingWindow::SetFrom(const GW::GamePos& pos)
{
    path_from = pos;
    path_from_map = GW::Map::GetMapID();
    WorldMapWidget::GamePosToWorldMap(pos, path_from_world);
    uint32_t fh = GetMapFileId(path_from_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.from = pos;
        sp.from_world = path_from_world;
        sp.from_set = true;
    }
    UpdateMarkers(path_from, path_to);
}

void PathfindingWindow::SetTo(const GW::GamePos& pos)
{
    path_to = pos;
    path_to_map = GW::Map::GetMapID();
    WorldMapWidget::GamePosToWorldMap(pos, path_to_world);
    uint32_t fh = GetMapFileId(path_to_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.to = pos;
        sp.to_world = path_to_world;
        sp.to_set = true;
    }
    UpdateMarkers(path_from, path_to);
}

bool PathfindingWindow::GetNextPortalToward(GW::Constants::MapID from_map, const GW::GamePos& from_pos, GW::Constants::MapID to_map, const GW::Vec2f& goal_world_pos, GW::GamePos& out_portal_pos)
{
    if (from_map == GW::Constants::MapID::None || to_map == GW::Constants::MapID::None) return false;
    if (from_map == to_map) return false;
    // Same physical map (outpost ↔ explorable variant): no portal needed.
    const uint32_t fh_from = GetMapFileId(from_map);
    const uint32_t fh_to = GetMapFileId(to_map);
    if (fh_from && fh_from == fh_to) return false;

    // Resolve goal world pos → game pos in target map (force-load DAT if needed).
    if (!GetCachedMapInfo(to_map)) LoadMapFromDAT(to_map);
    GW::GamePos goal_game{};
    const bool have_goal = WorldMapWidget::WorldMapToGamePos(goal_world_pos, goal_game, to_map);

    auto route = FindMapRoute(from_map, to_map, &from_pos, have_goal ? &goal_game : nullptr);
    if (route.size() < 2) return false;

    const auto next = route[1];

    GW::Vec2f hint_wm{};
    WorldMapWidget::GamePosToWorldMap(from_pos, hint_wm, from_map);

    GW::GamePos exit_portal{}, next_entry{};
    const GW::GamePos* gg = (route.size() == 2 && have_goal) ? &goal_game : nullptr;
    if (!FindBestPortalPair(from_map, next, hint_wm, exit_portal, next_entry, &from_pos, gg)) return false;
    out_portal_pos = exit_portal;
    return true;
}

namespace {
    // Blocking. Full cross-map route between two world-map positions into `out` (world coords, PATH_BREAK between maps). False on failure.
    bool ComputeRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::Vec2f>& out)
    {
        out.clear();
        const auto from_map = GW::Map::GetMapID();
        // Prefer a same-map route when the goal is within current bounds — GetMapIdForLocation can otherwise resolve to an overlapping map.
        const auto to_map = PathfindingWindow::IsWorldPosOnMap(to_world, from_map) ? from_map : WorldMapWidget::GetMapIdForLocation(to_world);
        if (from_map == GW::Constants::MapID::None || to_map == GW::Constants::MapID::None) return false;

        GW::GamePos start;
        if (!WorldMapWidget::WorldMapToGamePos(from_world, start, from_map)) return false;

        if (!GetCachedMapInfo(to_map)) LoadMapFromDAT(to_map);
        GW::GamePos goal;
        if (!WorldMapWidget::WorldMapToGamePos(to_world, goal, to_map)) return false;

        std::vector<HiddenPathSegment> hidden;
        return BuildCrossMapRoute(from_map, to_map, start, goal, from_world, out, hidden);
    }

    // Blocking. A* across `map_id` (its game coords), leg out in world coords. No shared state, so callers keep their route. False if no path.
    bool ComputeSegment(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::Vec2f>& out, std::vector<GW::GamePos>* out_game = nullptr)
    {
        out.clear();
        if (out_game) out_game->clear();
        if ((uint32_t)map_id == 0) map_id = GW::Map::GetMapID();
        if (map_id == GW::Constants::MapID::None) return false;
        std::vector<GW::GamePos> leg;
        if (!RunAStarOnMap(map_id, from, to, leg) || leg.empty()) return false;
        // Hand back the raw leg (game coords with zplane, in map_id's space) before SegmentToWorld flattens it to plane-less world Vec2f.
        if (out_game) *out_game = leg;
        SegmentToWorld(leg, map_id, out); // leg game -> world
        return true;
    }
} // namespace

// Detect if click is near previous position (within ~5 world map units)
static bool IsNearby(const GW::Vec2f& a, const GW::Vec2f& b)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return (dx * dx + dy * dy) < 25.f;
}

// Resolve map ID for a world map click, cycling through overlapping maps
static GW::Constants::MapID ResolveMapForClick(const GW::Vec2f& world_map_pos, const GW::Vec2f& prev_world_pos, GW::Constants::MapID prev_map)
{
    auto map_id = WorldMapWidget::GetMapIdForLocation(world_map_pos);
    // If clicking near the same spot, cycle to next overlapping map
    if (map_id != GW::Constants::MapID::None && prev_map != GW::Constants::MapID::None && map_id == prev_map && IsNearby(world_map_pos, prev_world_pos)) {
        auto next = WorldMapWidget::GetMapIdForLocation(world_map_pos, prev_map);
        if (next != GW::Constants::MapID::None) map_id = next;
    }
    return (map_id != GW::Constants::MapID::None) ? map_id : GW::Map::GetMapID();
}

// Load all maps whose world map bounds contain this position
void LoadAllMapsAtPosition(const GW::Vec2f& world_map_pos)
{
    RouteJobScope job_scope; // trim cache only after the whole batch is loaded
    BuildMapGraph();
    const auto cur_area = GW::Map::GetMapInfo();
    const auto cur_continent = cur_area ? cur_area->continent : GW::Continent::Kryta;
    const auto cur_map = GW::Map::GetMapID();

    // Load maps whose bounds contain the click, plus their adjacent/overlapping neighbors
    std::set<uint32_t> to_load;
    for (const auto& node : map_graph_nodes) {
        if (node.continent != cur_continent) continue;
        if (node.wm_bounds.Contains({world_map_pos.x, world_map_pos.y})) {
            to_load.insert((uint32_t)node.map_id);
            if (IsInterestingMapForCacheTrace(node.map_id)) {
                PATH_LOG_INFO(
                    "[CacheTrace] LoadAllMapsAtPosition click=(%.0f,%.0f) bounds=(%.0f,%.0f)-(%.0f,%.0f) added direct mid=%d", world_map_pos.x, world_map_pos.y, node.wm_bounds.Min.x, node.wm_bounds.Min.y, node.wm_bounds.Max.x, node.wm_bounds.Max.y,
                    (int)node.map_id
                );
            }
            for (auto adj : GetAdjacentMaps(node.map_id)) {
                to_load.insert((uint32_t)adj);
                if (IsInterestingMapForCacheTrace(adj)) {
                    PATH_LOG_INFO("[CacheTrace] LoadAllMapsAtPosition click=(%.0f,%.0f) added adj mid=%d via parent=%d", world_map_pos.x, world_map_pos.y, (int)adj, (int)node.map_id);
                }
            }
        }
    }
    for (auto mid : to_load) {
        if (static_cast<GW::Constants::MapID>(mid) != cur_map) {
            LoadMapFromDAT(static_cast<GW::Constants::MapID>(mid));
        }
    }
}

void LoadAndShowMapsAtWorldPos(const GW::Vec2f& wm_pos)
{
    LoadAllMapsAtPosition(wm_pos);
    UpdateBoundsLines();
    if (draw_portals) UpdatePortalMarkers();
}

void PathfindingWindow::SetFromWorldMap(const GW::Vec2f& world_map_pos)
{
    path_from_map = ResolveMapForClick(world_map_pos, path_from_world, path_from_map);
    path_from_world = world_map_pos;

    LoadAllMapsAtPosition(world_map_pos);

    GW::GamePos game_pos;
    if (!WorldMapWidget::WorldMapToGamePos(world_map_pos, game_pos, path_from_map)) return;
    path_from = game_pos;

    uint32_t fh = GetMapFileId(path_from_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.from = game_pos;
        sp.from_world = world_map_pos;
        sp.from_set = true;
    }

    PATH_LOG_INFO("SetFrom: wm=(%.1f,%.1f) game=(%.1f,%.1f) map=%d hash=0x%X cur=%d", world_map_pos.x, world_map_pos.y, game_pos.x, game_pos.y, (int)path_from_map, fh, (int)GW::Map::GetMapID());
    UpdateMarkers(path_from, path_to);
}

void PathfindingWindow::SetToWorldMap(const GW::Vec2f& world_map_pos)
{
    path_to_map = ResolveMapForClick(world_map_pos, path_to_world, path_to_map);
    path_to_world = world_map_pos;

    LoadAllMapsAtPosition(world_map_pos);

    GW::GamePos game_pos;
    if (!WorldMapWidget::WorldMapToGamePos(world_map_pos, game_pos, path_to_map)) return;
    path_to = game_pos;

    uint32_t fh = GetMapFileId(path_to_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.to = game_pos;
        sp.to_world = world_map_pos;
        sp.to_set = true;
    }

    PATH_LOG_INFO("SetTo: wm=(%.1f,%.1f) game=(%.1f,%.1f) map=%d hash=0x%X cur=%d", world_map_pos.x, world_map_pos.y, game_pos.x, game_pos.y, (int)path_to_map, fh, (int)GW::Map::GetMapID());
    UpdateMarkers(path_from, path_to);
}

void PathfindingWindow::FindPath()
{
    UpdateMarkers(path_from, path_to);

    if (path_from_map != GW::Constants::MapID::None && path_to_map != GW::Constants::MapID::None && path_from_map != path_to_map) {
        // Check if To is within From map's game bounds — if so, try direct single-map path
        const auto* from_info = GetCachedMapInfo(path_from_map);
        if (from_info && path_to_world.x != 0.f) {
            GW::GamePos to_on_from_map;
            if (WorldMapWidget::WorldMapToGamePos(path_to_world, to_on_from_map, path_from_map)) {
                // Verify the converted position is within the map's actual game bounds
                bool in_game_bounds = to_on_from_map.x >= from_info->bounds_min.x && to_on_from_map.x <= from_info->bounds_max.x && to_on_from_map.y >= from_info->bounds_min.y && to_on_from_map.y <= from_info->bounds_max.y;
                if (in_game_bounds) {
                    PATH_LOG_INFO("Overlap: To (map %d) at wm=(%.0f,%.0f) -> game=(%.0f,%.0f) on map %d, trying direct", (int)path_to_map, path_to_world.x, path_to_world.y, to_on_from_map.x, to_on_from_map.y, (int)path_from_map);
                    std::vector<GW::GamePos> direct_path;
                    if (RunAStarOnMap(path_from_map, path_from, to_on_from_map, direct_path) && !direct_path.empty()) {
                        PATH_LOG_INFO("Direct path on map %d: %d points", (int)path_from_map, (int)direct_path.size());
                        DrawPathAsLines(direct_path, path_from_map);
                        return;
                    }
                    PATH_LOG_INFO("Direct path failed, falling back to multi-map");
                }
            }
        }

        // Cross-map pathfinding with retry on AStar segment failure
        blacklisted_edges.clear();
        auto pfm = path_from_map;
        auto ptm = path_to_map;
        auto pf = path_from;
        auto pt = path_to;
        auto pfw = path_from_world;
        RecalculateMultiMapPath_WithRetry(pfm, ptm, pf, pt, pfw);
    }
    else {
        RecalculatePath(path_from, path_to);
    }
}

void PathfindingWindow::ShowRouteToWorldMap(const GW::GamePos& from, const GW::Vec2f& goal_world_pos)
{
    SetFrom(from);
    SetToWorldMap(goal_world_pos);
    FindPath();
}

void PathfindingWindow::ClearWorldMapRoute()
{
    ClearPathLines();
    ClearPortalPairLines();
}

bool PathfindingWindow::CalculateRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::Vec2f>* out)
{
    if (!out) return false;
    std::scoped_lock route_lock(route_mutex); // one build at a time; see route_mutex
    RouteJobScope job_scope;                 // defer eviction while we hold MilePath*
    return ComputeRoute(from_world, to_world, *out);
}

bool PathfindingWindow::RecalculateSegment(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::Vec2f>* out, std::vector<GW::GamePos>* out_game)
{
    if (!out) return false;
    std::scoped_lock route_lock(route_mutex); // one build at a time; see route_mutex
    RouteJobScope job_scope;                 // defer eviction while we hold MilePath*
    return ComputeSegment(map_id, from, to, *out, out_game);
}

bool PathfindingWindow::IsWorldPosOnMap(const GW::Vec2f& world_pos, GW::Constants::MapID map_id)
{
    if ((uint32_t)map_id == 0) map_id = GW::Map::GetMapID();
    if (map_id == GW::Constants::MapID::None) return false;
    GW::GamePos g;
    if (!WorldMapWidget::WorldMapToGamePos(world_pos, g, map_id)) return false;
    Pathing::Vec2f bmin, bmax;
    if (!Pathing::GetMapGameBoundsFromDAT(GetMapFileId(map_id), bmin, bmax)) return false;
    return g.x >= bmin.x && g.x <= bmax.x && g.y >= bmin.y && g.y <= bmax.y;
}

bool PathfindingWindow::IsRouteBreak(const GW::Vec2f& p)
{
    return IsPathBreak(p);
}

void PathfindingWindow::Initialize()
{
    ToolboxModule::Initialize();
    pending_terminate = false; // module is now optional; a prior disable left this set, which would abort all routing
    pathing_enabled = true;
    BuildMapFileHashLookup();
    RegisterUIMessageCallback(&gw_ui_hookentry, GW::UI::UIMessage::kLoadMapContext, OnUIMessage, 0x4000);

    // Load portal connections from the JSON embedded as an RCDATA resource, so no loose file need sit next to the DLL at runtime.
    const EmbeddedResource portal_json(IDR_PORTAL_CONNECTIONS_JSON, RT_RCDATA, GWToolbox::GetDLLModule());
    if (portal_json.data() && portal_json.size()) {
        portal_connections.LoadFromMemory(
            static_cast<const char*>(portal_json.data()), portal_json.size(), "<embedded resource>");
    }
    else {
        PATH_LOG_ERROR("Failed to load embedded portal connections resource");
    }
    pending_connection_lines_update = true;
}
