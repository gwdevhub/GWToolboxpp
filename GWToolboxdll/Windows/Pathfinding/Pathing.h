#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include "MapSpecificData.h"
#include "PathingMapData.h"

namespace Pathing {
    #define PATHING_MAX_PLANE_COUNT 192 // Be sure to ASSERT if this is ever higher!

    using BlockedPlaneBitset = std::bitset<PATHING_MAX_PLANE_COUNT>;

    enum class Error : uint32_t {
        OK,
        Unknown,
        FailedToFindStartPathingTrapezoid,
        FailedToFindGoalPathingTrapezoid,
        FailedToFinializePath,
        InvalidMapContext,
        BuildPathLengthExceeded,
        FailedToGetPathingMapBlock,
        MilePathBuildOOM // worker thread aborted with std::bad_alloc; visgraph is unsafe to use
    };

	typedef uint16_t PointId;

    class NavMesh; // hand-built Detour mesh exposed for the debug overlay

    // Snap `point` (x/y and zplane) to the closest position on the current map's live
    // pathing trapezoids. Game thread only. Returns the trapezoid, or nullptr if no map.
    GW::PathingTrapezoid* FindClosestPositionOnTrapezoid(GW::GamePos& point);

    // True if `point` is inside a walkable trapezoid on the current map. Cheap (a BSP descent),
    // unlike the snap above, which falls back to scanning every trapezoid when the point is
    // outside the walkable area. Game thread only.
    bool IsPositionWalkable(const GW::GamePos& point);

    // BFS from the player's trapezoid through adjacency and unblocked portals. Blocking matches
    // the game's native A* (IPath_ExpandPortalLeft/Right): portal.flags & 0x04, and
    // blockedPlanes[neighbor_plane] & 1. Empty when the player's position is unknown, which
    // callers should read as "assume everything is reachable" rather than "nothing is".
    // Game thread only.
    std::unordered_set<const GW::PathingTrapezoid*> FindReachableTrapezoids();

    struct TrapezoidRef {
        const GW::PathingTrapezoid* trapezoid;
        uint32_t plane;
        bool reachable;
    };

    // False while the walk above cannot find the player to start from - the state in which callers
    // assume everything is reachable. Anything that would cache a verdict built on that assumption
    // should wait instead. Cheap: the answer is cached, and the walk bails early in that state.
    bool IsReachabilityKnown();

    // Every walkable trapezoid on the current map, with its plane and whether the player can reach
    // it. Anything that asks "is there ground inside this box" needs the geometry: sampling a
    // lattice walks straight past a sliver the router will happily path onto. Game thread only.
    std::vector<TrapezoidRef> GetTrapezoidsWithReachability();

    // True if `p` lies within this trapezoid, in 2D. Game thread only.
    bool IsOnTrapezoid(const GW::PathingTrapezoid* t, const GW::Vec2f& p);

    // Exact 2D overlap between `t` and the axis-aligned box [box_min, box_max], in game
    // coordinates - any of it counts, down to a sliver along one edge. A lattice of IsOnTrapezoid
    // probes instead walks past every sliver narrower than its spacing, and next to fog a sliver is
    // often the only footing there is. `out_point` is the overlap's centroid, so it lands inside
    // the walkable geometry rather than on its edge. Pure geometry, so any thread.
    bool TrapezoidOverlapsBox(const GW::PathingTrapezoid* t, const GW::Vec2f& box_min, const GW::Vec2f& box_max, GW::Vec2f& out_point);

    // True if `point` is walkable AND the player can actually get there. Terrain behind a closed
    // gate or across a gap is walkable but not reachable, and the difference matters to anything
    // that suggests somewhere to go. Result is cached until the map or the gate state changes.
    // Game thread only.
    bool IsPositionReachable(const GW::GamePos& point);

    // True if walking straight from `a` to `b` passes through a travel portal's doorway, treated
    // as a disc the width of the prop. Stepping into one changes map, so the far side is not
    // somewhere this map's pathing can actually deliver you. Game thread only.
    bool CrossesTravelPortal(const GW::Vec2f& a, const GW::Vec2f& b);

    // Current blocked-plane state. Comparing the contents beats watching for a change event: it
    // is exact, and it survives callers that were not listening at the moment a gate moved.
    // False if there is no pathing context. Game thread only.
    bool CopyBlockedPlanes(std::vector<uint32_t>& out);

    class MilePath {
        volatile bool m_processing = false;
        volatile bool m_done = false;
        volatile bool m_build_failed = false; // worker thread caught std::bad_alloc during visgraph build
        volatile int m_progress = 0;

        // Lazy full-build state. A lightweight MilePath (full_build=false) keeps only
        // raw map data; EnsureFullBuild() builds the visgraph on first walk.
        // m_constructed_full marks maps built eagerly on the worker (live current map).
        volatile bool m_full_built = false;
        bool m_constructed_full = false;
        std::mutex m_build_mutex;
        // MapIDs sharing this file_hash — for teleport collection in a (possibly
        // deferred) full build. Kept here (not in opaque Impl) to keep Impl small.
        std::vector<GW::Constants::MapID> m_all_map_ids;

        std::thread* worker_thread = nullptr;

    public:
        MilePath(Pathing::PathingMapData&& map_data, GW::Constants::MapID map_id, const std::vector<GW::Constants::MapID>& all_map_ids = {}, bool full_build = true);
        ~MilePath();

        // Build the full pathing graph (incl. visgraph) if not already present.
        // Idempotent, thread-safe; called by AStar::Search to upgrade lazily.
        void EnsureFullBuild();

        // Signals terminate to worker thread. Usually followed late by shutdown() to grab the thread again.
        void stopProcessing();
        bool isProcessing() { return m_processing; }
        // Signals terminate to worker thread, waits for thread to finish. Blocking.
        void shutdown()
        {
            stopProcessing();
            while (isProcessing())
                Sleep(10);
            if (worker_thread) {
                if (worker_thread->joinable())
                    worker_thread->join();
                delete worker_thread;
                worker_thread = nullptr;
            }
        }

        int progress()
        {
            return m_progress;
        }

        // Read-only access to the underlying pathing map data (owned by MilePath).
        // Lets callers reuse the loaded DAT data without re-reading it. Pointer
        // remains valid for the MilePath's lifetime.
        const Pathing::PathingMapData* GetMapData() const;

        NavMesh* GetNavMeshForDebug();

        bool ready()
        {
            return m_progress >= 100;
        }

        // True if the worker thread aborted with std::bad_alloc. The MilePath is unusable —
        // search will return early. The caller is expected to surface an error / not retry.
        bool build_failed() const { return m_build_failed; }

        void* GetImpl() { return opaque; };

        // Export vis graph data: per-point position + edge count + edge targets
        // Returns JSON string
        std::string ExportVisGraph() const;

    private:
        // Pad sized for the largest (Debug) Impl; guarded by static_assert(sizeof(opaque) >= sizeof(Impl)).
        int opaque[1024 / sizeof(int)];
    };

    class AStar {
    public:
        class Path {
        public:
            Path(AStar* _parent)
                : m_astar(_parent),
				  m_mp(),
                  m_cost(0.0f) {}

            const std::vector<GW::GamePos>& points()
            {
                return m_points;
            }

            float cost() const
            {
                return m_cost;
            }

            bool ready() const
            {
                return finalized;
            }

            void clear()
            {
                finalized = false;
                m_points.clear();
            }

            void insertPoint(const GW::GamePos& point)
            {
                m_points.emplace_back(point);
            }

            void setCost(float cost)
            {
                m_cost = cost;
            }

            void finalize()
            {
                if (!finalized)
                    std::ranges::reverse(m_points);
                finalized = true;
            }

        MilePath* m_mp; //TODO: move to private?
		
        private:
            bool finalized = false;
            AStar* m_astar;
            std::vector<GW::GamePos> m_points;
            float m_cost; // distance
        };

        Path m_path;

        AStar(MilePath* mp);

        Error Search(const GW::GamePos& start_pos, const GW::GamePos& goal_pos);
    };
}
