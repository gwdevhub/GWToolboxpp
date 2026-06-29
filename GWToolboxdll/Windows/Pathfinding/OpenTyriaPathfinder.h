#pragma once

// Trapezoid-graph pathfinder ported from OpenTyria
// (https://github.com/ldufr/OpenTyria, Unlicense / public-domain).
// Replaces visgraph + AStar for portal-portal
// distance estimation only — much faster on first-time map loads because
// we don't need a visgraph build, but occasionally produces suboptimal
// paths through tight funnels. Used as one of three modes in the route
// picker (Euclidean / Trapezoid / Walk).
//
// Operates directly on GWTB's Pathing::PathingMapData (deep copy of game
// memory or DAT). No second-level data adapter needed — the structures
// are 1:1 modulo field-name differences which are translated inline.

#include <vector>
#include <unordered_map>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include <Windows/Pathfinding/PathingMapData.h>

namespace OpenTyria {

    struct Vec2f { float x = 0, y = 0; };

    struct Waypoint {
        GW::GamePos pos;
        uint32_t trap_id = 0; // global index across all planes
    };

    struct PathFindNode {
        bool                       closed = false;
        float                      cost_to_node = 0.f;
        GW::GamePos                pos{};
        const GW::PathingTrapezoid* trap = nullptr;
        // Linked-list pointer back through visited-node chain. Null at start.
        PathFindNode*              next = nullptr;
    };

    struct PathHeapNode {
        float    cost;
        uint32_t trap_id;
    };

    struct PathBuildStep {
        Vec2f                       pos;
        Vec2f                       dir;
        uint16_t                    plane = 0;
        const GW::PathingTrapezoid* next_trap = nullptr;
    };

    // Why a search returned without a full path.
    enum class SearchResult {
        OK,                       // src→dst reached
        InvalidData,              // pathfinder has no map data
        SrcTrapezoidNotFound,     // src not inside any trapezoid (bad zplane / outside bounds)
        DstTrapezoidNotFound,     // dst not inside any trapezoid
        Unreachable,              // AStar queue exhausted; out_waypoints holds partial-to-closest
    };

    const char* SearchResultName(SearchResult r);

    // One pathfinder per map. Owns scratch buffers; not thread-safe — wrap with a
    // mutex if shared across workers.
    class TrapezoidPathfinder {
    public:
        explicit TrapezoidPathfinder(const Pathing::PathingMapData* data);

        // Returns OK on a full path, or one of the failure variants. On
        // Unreachable, out_waypoints may still contain a partial path to the
        // closest reachable trapezoid (matches OpenTyria semantics).
        SearchResult Search(const GW::GamePos& src, const GW::GamePos& dst,
                            std::vector<Waypoint>& out_waypoints);

        // Sum of segment lengths of the produced waypoint chain. Returns
        // INFINITY on any failure. `out_reason` (optional) reports why.
        float SearchDistance(const GW::GamePos& src, const GW::GamePos& dst,
                             SearchResult* out_reason = nullptr);

        size_t TrapezoidCount() const { return total_traps; }

        // Pointer-to-global-index lookup. Public so the algorithm's free helpers
        // can use it; cheap to call (single hash lookup).
        uint32_t IndexOf(const GW::PathingTrapezoid* t) const;

        // Diagnostic: walk the BSP for `pos` against every plane and return the
        // zplane value of the first plane whose BSP returns a containing
        // trapezoid. Returns -1 if no plane contains pos. Used to detect zplane
        // mismatches (caller passes pos.zplane=X but data has it under Y).
        int FindContainingPlane(const GW::GamePos& pos) const;

        // Number of planes in the underlying data, for diagnostic logging.
        size_t PlaneCount() const;
        // Read the i'th plane's stored zplane id (i in [0..PlaneCount)).
        uint32_t PlaneZAt(size_t i) const;

        // O(1) blocked-plane lookup against the live game array (or null for
        // DAT-loaded maps, which are always treated as unblocked). Portal
        // crossings into a blocked plane are skipped during Search.
        bool IsPlaneBlocked(uint32_t plane_idx) const;

    private:
        const Pathing::PathingMapData* m_data;
        size_t                         total_traps = 0;
        // Maps a trapezoid pointer to a global 0..N-1 index.
        std::unordered_map<const GW::PathingTrapezoid*, uint32_t> trap_index;
        // Per-search scratch — reused across calls.
        std::vector<PathFindNode>  nodes;
        std::vector<PathHeapNode>  prioq;
        std::vector<PathBuildStep> steps;
    };

} // namespace OpenTyria
