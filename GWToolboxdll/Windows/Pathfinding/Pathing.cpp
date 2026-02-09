#include "stdafx.h"

#include <GWCA/Context/MapContext.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Logger.h>
#include "MathUtility.h"
#include "Pathing.h"
#include "constant_vector.hpp"

namespace {
    __forceinline float Cross(const GW::Vec2f& lhs, const GW::Vec2f& rhs)
    {
        return (lhs.x * rhs.y) - (lhs.y * rhs.x);
    }

    __forceinline float Dot(const GW::Vec2f& lhs, const GW::Vec2f& rhs)
    {
        return (lhs.x * rhs.x) + (lhs.y * rhs.y);
    }

    class Timing {
#ifdef DEBUG_PATHING
        std::string label;
        std::chrono::time_point<std::chrono::high_resolution_clock> start;

    public:
        Timing(std::string_view label) : label(std::move(label)), start(std::chrono::high_resolution_clock::now()) {}
        ~Timing()
        {
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            Log::Info("%s: %ld ms.", label.c_str(), std::chrono::duration_cast<std::chrono::milliseconds>(elapsed));
        }
#else
    public:
        Timing(...) {};
#endif
    };
} // namespace


namespace Pathing {
    bool IsOnPathingTrapezoid(const GW::PathingTrapezoid* pt, const GW::Vec2f& p)
    {
        if (pt->YT < p.y || pt->YB > p.y) return false;
        if (pt->XBL > p.x && pt->XTL > p.x) return false;
        if (pt->XBR < p.x && pt->XTR < p.x) return false;
        GW::Vec2f a{pt->XTL, pt->YT}, b{pt->XBL, pt->YB}, c{pt->XBR, pt->YB}, d{pt->XTR, pt->YT};
        GW::Vec2f ab = b - a, cd = d - c;
        GW::Vec2f pa = p - a, pc = p - c;
        const float tolerance = -1.0f;
        if (Cross(ab, pa) < tolerance) return false;
        if (Cross(cd, pc) < tolerance) return false;
        return true;
    }


    GW::PathingTrapezoid* FindTrapezoid(const GW::GamePos& point, GW::PathingMapArray* map)
    {
        if (!(map && map->size() <= point.zplane)) return nullptr;

        auto& pm = (*map)[point.zplane];
        GW::Node* n = pm.root_node;

        int cnt = 50000;
        while (n && cnt--) {
            switch (n->type) {
                case 0: // XNode = 0,
                {
                    GW::XNode* xn = static_cast<GW::XNode*>(n);
                    float d = (point.y - xn->pos.y) * xn->dir.x - (point.x - xn->pos.x) * xn->dir.y;

                    if (d >= 0.0f) {
                        n = xn->right;
                    }
                    else {
                        n = xn->left;
                    }
                    break;
                }
                case 1: // YNode = 1,
                {
                    GW::YNode* yn = static_cast<GW::YNode*>(n);
                    if (point.y > yn->pos.y) {
                        n = yn->above;
                    }
                    else if (point.y < yn->pos.y) {
                        n = yn->below;
                    }
                    else if (point.x >= yn->pos.x) {
                        n = yn->above;
                    }
                    else {
                        n = yn->below;
                    }
                    break;
                }
                case 2: // SinkNode = 2
                {
                    GW::SinkNode* sn = static_cast<GW::SinkNode*>(n);
                    return sn ? sn->trapezoid : nullptr;
                }
            }
        }

        return nullptr;
    }


    GW::PathingTrapezoid* FindClosestTrapezoid(const GW::GamePos& point, GW::PathingMapArray* map)
    {
        if (!(map && map->size())) return nullptr;

        GW::PathingTrapezoid* closest = FindTrapezoid(point, map);
        if (closest) return closest;

        float closest_dist = std::numeric_limits<float>::infinity();
        constexpr float tolerance = 50.0f;

        std::vector<GW::Node*> open;

        for (uint32_t i = 0; i < map->size(); ++i) {
            auto& pm = (*map)[i];
            open.push_back(pm.root_node);

            int cnt = 50000;
            while (open.size() && cnt--) {
                GW::Node* n = std::move(open.back());
                open.pop_back();

                if (!n) continue;

                switch (n->type) {
                    case 0: // XNode = 0,
                    {
                        GW::XNode* xn = static_cast<GW::XNode*>(n);
                        float d = ((point.y - xn->pos.y) * xn->dir.x - (point.x - xn->pos.x) * xn->dir.y) / GW::GetNorm(xn->dir);

                        if ((d > tolerance) || (open.push_back(xn->left), (d >= -tolerance))) {
                            open.push_back(xn->right);
                        }
                        break;
                    }
                    case 1: // YNode = 1,
                    {
                        GW::YNode* yn = static_cast<GW::YNode*>(n);
                        float d = point.y - yn->pos.y;
                        if ((d > tolerance) || (open.push_back(yn->below), (d >= -tolerance))) {
                            open.push_back(yn->above);
                        }
                        break;
                    }
                    case 2: // SinkNode = 2
                    {
                        GW::SinkNode* sn = static_cast<GW::SinkNode*>(n);
                        if (!sn->trapezoid) continue;
                        GW::PathingTrapezoid* pt = reinterpret_cast<GW::PathingTrapezoid*>(sn->trapezoid);

                        bool onTrap = IsOnPathingTrapezoid(pt, point);
                        if (onTrap) {
                            return pt;
                        }
                        else {
                            float d1 = MathUtil::getDistanceFromLine({pt->XTL, pt->YT}, {pt->XTR, pt->YT}, point);
                            float d2 = MathUtil::getDistanceFromLine({pt->XTR, pt->YT}, {pt->XBR, pt->YB}, point);
                            float d3 = MathUtil::getDistanceFromLine({pt->XBR, pt->YB}, {pt->XBL, pt->YB}, point);
                            float d4 = MathUtil::getDistanceFromLine({pt->XBL, pt->YB}, {pt->XTL, pt->YT}, point);
                            float d = std::min({d1, d2, d3, d4});
                            if (closest_dist > d) {
                                closest_dist = d;
                                closest = pt;
                            }
                        }
                        break;
                    }
                }
            }
        }

        return closest;
    }

    GW::PathingTrapezoid* FindClosestPositionOnTrapezoid(GW::GamePos& point, GW::PathingMapArray* map)
    {
        if (!(map && map->size())) return nullptr;

        GW::PathingTrapezoid* pt = FindClosestTrapezoid(point, map);
        if (pt) return pt;

        float closest_dist = std::numeric_limits<float>::infinity();
        GW::PathingTrapezoid* closest = nullptr;

        for (uint32_t z = 0; z < map->size(); ++z) {
            const auto& m = (*map)[z];
            for (uint32_t i = 0; i < m.trapezoid_count; ++i) {
                pt = &m.trapezoids[i];
                float d1 = MathUtil::getDistanceFromLine({pt->XTL, pt->YT}, {pt->XTR, pt->YT}, point);
                float d2 = MathUtil::getDistanceFromLine({pt->XTR, pt->YT}, {pt->XBR, pt->YB}, point);
                float d3 = MathUtil::getDistanceFromLine({pt->XBR, pt->YB}, {pt->XBL, pt->YB}, point);
                float d4 = MathUtil::getDistanceFromLine({pt->XBL, pt->YB}, {pt->XTL, pt->YT}, point);
                float d = std::min({d1, d2, d3, d4});
                if (closest_dist > d) {
                    closest_dist = d;
                    closest = pt;
                    point.zplane = z;
                }
            }
        }
        if (!closest) return nullptr;

        // Located outside pathing trapezoids. Find closest point lying on the edge of trapezoid.
        GW::Vec2f p0, p1;
        if (closest_dist == MathUtil::getDistanceFromLine({closest->XTL, closest->YT}, {closest->XTR, closest->YT}, point)) {
            p0 = {closest->XTL, closest->YT};
            p1 = {closest->XTR, closest->YT};
        }
        else if (closest_dist == MathUtil::getDistanceFromLine({closest->XTR, closest->YT}, {closest->XBR, closest->YB}, point)) {
            p0 = {closest->XTR, closest->YT};
            p1 = {closest->XBR, closest->YB};
        }
        else if (closest_dist == MathUtil::getDistanceFromLine({closest->XBR, closest->YB}, {closest->XBL, closest->YB}, point)) {
            p0 = {closest->XBR, closest->YB};
            p1 = {closest->XBL, closest->YB};
        }
        else /*(closest_dist == MathUtil::getDistanceFromLine({ closest->XTR, closest->YT }, { closest->XBR, closest->YB }, point))*/ {
            p0 = {closest->XBL, closest->YB};
            p1 = {closest->XTL, closest->YT};
        }

        // Calculate new point by projecting start point on the closest line segment.
        auto ba = p1 - p0, pa = point - p0;
        float h = std::clamp(Dot(pa, ba) / Dot(ba, ba), 0.0f, 1.0f);
        point = p0 + h * ba;

        return closest;
    }


    std::mutex pathing_mutex;

    GW::Array<GW::MapProp*>* GetMapProps()
    {
        const auto m = GW::GetMapContext();
        const auto p = m ? m->props : nullptr;
        return p ? &p->propArray : nullptr;
    }


    // Grab a copy of map_context->sub1->pathing_map_block for processing on a different thread - Blocks until copy is complete
    Pathing::Error CopyPathingMapBlocks(GW::MapContext* mapContext, Pathing::BlockedPlaneBitset* dest)
    {
        volatile Pathing::Error res = Pathing::Error::Unknown;
        std::mutex mutex;
        *dest = {0};
        if (!(mapContext && mapContext->path)) {
            return Pathing::Error::InvalidMapContext;
        }
        // Enqueue
        GW::GameThread::Enqueue([mapContext, dest, &res, &mutex] {
            const std::lock_guard lock(mutex);
            auto& block = mapContext->path->blockedPlanes;
            ASSERT(block.size() < dest->size());
            for (size_t i = 0; i < block.size(); i++) {
                dest->set(i, block[i] != 0);
            }
            res = Pathing::Error::OK;
        });
        // Wait
        do {
            const std::lock_guard lock(mutex);
            if (res != Pathing::Error::Unknown) break;
        } while (true);
        return res;
    }

    // Overload that gets MapContext automatically
    Pathing::Error CopyPathingMapBlocks(Pathing::BlockedPlaneBitset* dest)
    {
        return CopyPathingMapBlocks(GW::GetMapContext(), dest);
    }

    // Helper wrappers that get the map from the current MapContext
    GW::PathingTrapezoid* FindClosestTrapezoid(const GW::GamePos& point)
    {
        auto* mapContext = GW::GetMapContext();
        if (!mapContext || !mapContext->path || !mapContext->path->staticData) return nullptr;
        return FindClosestTrapezoid(point, &mapContext->path->staticData->map);
    }

    GW::PathingTrapezoid* FindClosestPositionOnTrapezoid(GW::GamePos& point)
    {
        auto* mapContext = GW::GetMapContext();
        if (!mapContext || !mapContext->path || !mapContext->path->staticData) return nullptr;
        return FindClosestPositionOnTrapezoid(point, &mapContext->path->staticData->map);
    }

    uint32_t FileHashToFileId(wchar_t* param_1)
    {
        if (!param_1) return 0;
        if (((0xff < *param_1) && (0xff < param_1[1])) && ((param_1[2] == 0 || ((0xff < param_1[2] && (param_1[3] == 0)))))) {
            return (*param_1 - 0xff00ff) + (uint32_t)param_1[1] * 0xff00;
        }
        return 0;
    }

    const uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4])) return 0;
        uint32_t* sub_deets = (uint32_t*)prop->h0034[4];
        return FileHashToFileId((wchar_t*)sub_deets[1]);
    };
    bool IsTeleporter(GW::MapProp* prop)
    {
        if (!prop) return false;
        switch (GetMapPropModelFileId(prop)) {
            case 0xefd0: // Crystal desert
                return true;
        }
        return false;
    }
    bool IsTravelPortal(GW::MapProp* prop)
    {
        if (!prop) return false;
        switch (GetMapPropModelFileId(prop)) {
            case 0x4e6b2: // Eotn asura gate
            case 0x3c5ac: // Eotn, Nightfall
            case 0xa825:  // Prophecies, Factions
                return true;
        }
        return false;
    }

    bool IsPathValid(const GW::PathContext* path)
    {
        return path && path->staticData && path->staticData->map.size();
    }

    typedef struct {
        float distance;
        BlockedPlaneBitset blocked_planes;
        PointId point_id; // other point
    } PointVisElement;

    struct DefferedTeleport {
        PointId m_enter, m_exit;
        MapSpecific::Teleport::direction m_directionality;
    };
    std::vector<DefferedTeleport> m_defferedPortalLinks;

    enum class Edge : uint8_t { top, right, bottom, left };

    class Portal {
    public:
        typedef uint16_t id;
        Portal(Portal::id id, Portal::id other_id, PointId start_id, PointId goal_id, const GW::PathingTrapezoid* pt, uint8_t pt_layer, Edge edge);
        const GW::PathingTrapezoid* m_pt; // pathing trapezoid id
        PointId m_point[2];
        Portal::id m_id;
        Portal::id m_other_id;
        uint8_t m_pt_layer;
        Edge m_edge; // deprecated
    };

    Portal::Portal(Portal::id id, Portal::id other_id, PointId start_id, PointId goal_id, const GW::PathingTrapezoid* pt, uint8_t pt_layer, Edge edge)
        : m_id(id), m_other_id(other_id), m_point{start_id, goal_id}, m_pt(pt), m_pt_layer(pt_layer), m_edge(edge)
    {}

    class Point {
    public:
        Point() : m_id{}, m_pos{}, m_portals{}, is_viable{} {}
        Point(const GW::Vec2f& pos, PointId id, Portal::id portal1_id, Portal::id portal2_id, bool is_viable) : m_id(id), m_pos(pos), m_portals{portal1_id, portal2_id}, is_viable(is_viable) {};
        GW::Vec2f m_pos;
        Portal::id m_portals[2];
        PointId m_id;
        bool is_viable;
    };

    typedef struct {
        const GW::PathingTrapezoid* t;
        uint8_t layer;
        Edge loc;
    } Neighbour;

    typedef struct {
        const GW::PathingTrapezoid* t;
        const GW::PathingTrapezoid* came_from;
    } Explore;

    struct node {
        BlockedPlaneBitset blocked_planes;
        size_t visited_checkpoint; // rollback checkpoint
        GW::Vec2f funnel[2];
        Portal::id next;
    };

    struct VisitedState {
        cv::vector<uint16_t> visited; // size = portals.size()
        cv::vector<Portal::id> stack; // change history
        uint16_t stamp = 1;

        void init(size_t n)
        {
            visited.resize(n);
            stack.reserve(2048);
            stamp = 1;
        }

        inline bool is_visited(Portal::id id) const { return visited[static_cast<uint16_t>(id)] == stamp; }

        inline void visit(Portal::id id)
        {
            if (visited[static_cast<uint16_t>(id)] != stamp) {
                visited[id] = stamp;
                stack.push_back(id);
            }
        }

        inline size_t checkpoint() const { return stack.size(); }

        inline void rollback(size_t checkpoint)
        {
            while (stack.size() > checkpoint) {
                visited[stack.back()] = 0;
                stack.pop_back();
            }
        }
    };


    struct VisitedPoints {
        cv::vector<uint32_t> gen;
        uint32_t cur = 1;

        void init(size_t n)
        {
            gen.resize(n);
            cur = 1;
        }

        inline void reset()
        {
            ++cur;
            if (cur == 0) { // overflow safety
                std::fill(gen.begin(), gen.end(), 0);
                cur = 1;
            }
        }

        inline bool test_and_set(PointId id)
        {
            if (gen[id] == cur) return false;
            gen[id] = cur;
            return true;
        }
    };

    struct Impl {
        Impl() : m_msd(), m_visGraph(), points(), portals(), pt_portal_map(), portal_pt_map(), portal_portal_map(), tmp_portal_pt_map(), ptneighbours(), m_teleports(), travel_portals(), m_teleportGraph(), m_copiedMapData() {}

        ~Impl() { CleanupCopiedMapData(); }

        MapSpecific::MapSpecificData m_msd;
        std::vector<std::vector<PointVisElement>> m_visGraph;                          // PointId
        std::vector<Point> points;                                                     // PointId
        std::vector<Portal> portals;                                                   // Portal::id
        std::vector<std::vector<Portal::id>> pt_portal_map;                            // PathingTrapezoid->id
        std::vector<const GW::PathingTrapezoid*> portal_pt_map;                        // Portal::id
        std::vector<std::vector<Portal::id>> portal_portal_map;                        // Portal::id
        std::unordered_map<Portal::id, const GW::PathingTrapezoid*> tmp_portal_pt_map; // Portal::id
        std::unordered_map<const GW::PathingTrapezoid*, std::vector<Neighbour>> ptneighbours;
        MapSpecific::Teleports m_teleports;
        std::vector<GW::MapProp*> travel_portals;
        std::vector<MapSpecific::teleport_node> m_teleportGraph;
        volatile bool m_terminateThread = false;

        // Runtime/tmp vars that would otherwise have been static for cca - maybe add mutex?
        std::unordered_map<const GW::PathingTrapezoid*, bool> GetTrapezoidNeighbours_visited;
        std::vector<Explore> GetTrapezoidNeighbours_to_explore;
        std::vector<Neighbour> isNeighbourOf_neighbours;

        // Deep copied map data - NO external pointers allowed!
        struct CopiedPathingMap {
            uint32_t trapezoid_count;
            GW::PathingTrapezoid* trapezoids;
            uint32_t portal_count;
            GW::Portal* portals;
            GW::Node* root_node; // We'll keep tree nodes from game (they're const data)
            uint32_t zplane;

            CopiedPathingMap() : trapezoid_count(0), trapezoids(nullptr), portal_count(0), portals(nullptr), root_node(nullptr), zplane(0) {}
        };

        struct CopiedMapData {
            std::vector<CopiedPathingMap> maps;
            uint32_t pathNodeSize;
        };

        CopiedMapData m_copiedMapData;

        void CleanupCopiedMapData()
        {
            for (auto& map : m_copiedMapData.maps) {
                if (map.trapezoids) {
                    delete[] map.trapezoids;
                    map.trapezoids = nullptr;
                }
                if (map.portals) {
                    // First delete the trapezoid pointer arrays in each portal
                    for (uint32_t i = 0; i < map.portal_count; ++i) {
                        if (map.portals[i].trapezoids) {
                            delete[] map.portals[i].trapezoids;
                        }
                    }
                    delete[] map.portals;
                    map.portals = nullptr;
                }
                // root_node points to game memory - don't delete
            }
            m_copiedMapData.maps.clear();
        }

        void CopyMapData(const GW::PathContext* src)
        {
            if (!src || !src->staticData) return;

            CleanupCopiedMapData();

            const auto& srcMaps = src->staticData->map;
            m_copiedMapData.maps.resize(srcMaps.size());
            m_copiedMapData.pathNodeSize = src->pathNodes.size();

            // Build ID->pointer mappings for fixup
            std::unordered_map<uint32_t, GW::PathingTrapezoid*> idToTrapezoid;
            std::unordered_map<const GW::Portal*, GW::Portal*> oldToNewPortal;

            // Pass 1: Copy all trapezoids and build ID map
            for (size_t mapIdx = 0; mapIdx < srcMaps.size(); ++mapIdx) {
                const auto& srcMap = srcMaps[mapIdx];
                auto& dstMap = m_copiedMapData.maps[mapIdx];

                dstMap.zplane = srcMap.zplane;
                dstMap.trapezoid_count = srcMap.trapezoid_count;
                dstMap.portal_count = srcMap.portal_count;

                // Copy trapezoids
                if (srcMap.trapezoids && srcMap.trapezoid_count > 0) {
                    dstMap.trapezoids = new GW::PathingTrapezoid[srcMap.trapezoid_count];
                    for (uint32_t i = 0; i < srcMap.trapezoid_count; ++i) {
                        // Copy the trapezoid data
                        dstMap.trapezoids[i] = srcMap.trapezoids[i];
                        // Clear adjacent pointers for now - will fix up in pass 2
                        dstMap.trapezoids[i].adjacent[0] = nullptr;
                        dstMap.trapezoids[i].adjacent[1] = nullptr;
                        dstMap.trapezoids[i].adjacent[2] = nullptr;
                        dstMap.trapezoids[i].adjacent[3] = nullptr;

                        // Build ID map
                        idToTrapezoid[srcMap.trapezoids[i].id] = &dstMap.trapezoids[i];
                    }
                }

                // Copy portals (before fixup)
                if (srcMap.portals && srcMap.portal_count > 0) {
                    dstMap.portals = new GW::Portal[srcMap.portal_count];
                    for (uint32_t i = 0; i < srcMap.portal_count; ++i) {
                        // Copy portal data
                        dstMap.portals[i] = srcMap.portals[i];
                        dstMap.portals[i].pair = nullptr;       // Will fix up in pass 2
                        dstMap.portals[i].trapezoids = nullptr; // Will fix up in pass 2

                        // Build portal map
                        oldToNewPortal[&srcMap.portals[i]] = &dstMap.portals[i];
                    }
                }
            }

            // Pass 2: Fix up all pointers using IDs/maps
            for (size_t mapIdx = 0; mapIdx < srcMaps.size(); ++mapIdx) {
                const auto& srcMap = srcMaps[mapIdx];
                auto& dstMap = m_copiedMapData.maps[mapIdx];

                // Fix up adjacent pointers in trapezoids
                if (srcMap.trapezoids && srcMap.trapezoid_count > 0) {
                    for (uint32_t i = 0; i < srcMap.trapezoid_count; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            if (srcMap.trapezoids[i].adjacent[j]) {
                                uint32_t adjacentId = srcMap.trapezoids[i].adjacent[j]->id;
                                auto it = idToTrapezoid.find(adjacentId);
                                if (it != idToTrapezoid.end()) {
                                    dstMap.trapezoids[i].adjacent[j] = it->second;
                                }
                                // else leave as nullptr
                            }
                        }
                    }
                }

                // Fix up portal pointers
                if (srcMap.portals && srcMap.portal_count > 0) {
                    for (uint32_t i = 0; i < srcMap.portal_count; ++i) {
                        // Fix up pair pointer
                        if (srcMap.portals[i].pair) {
                            auto it = oldToNewPortal.find(srcMap.portals[i].pair);
                            if (it != oldToNewPortal.end()) {
                                dstMap.portals[i].pair = it->second;
                            }
                            // else leave as nullptr
                        }

                        // Fix up trapezoid pointers in portal
                        if (srcMap.portals[i].trapezoids && srcMap.portals[i].count > 0) {
                            auto newTrapPtrs = new GW::PathingTrapezoid*[srcMap.portals[i].count];
                            for (uint32_t j = 0; j < srcMap.portals[i].count; ++j) {
                                if (srcMap.portals[i].trapezoids[j]) {
                                    uint32_t trapId = srcMap.portals[i].trapezoids[j]->id;
                                    auto it = idToTrapezoid.find(trapId);
                                    if (it != idToTrapezoid.end()) {
                                        newTrapPtrs[j] = it->second;
                                    }
                                    else {
                                        newTrapPtrs[j] = nullptr;
                                    }
                                }
                                else {
                                    newTrapPtrs[j] = nullptr;
                                }
                            }
                            dstMap.portals[i].trapezoids = newTrapPtrs;
                        }
                        else {
                            dstMap.portals[i].trapezoids = nullptr;
                        }
                    }
                }
            }
        }

        // Helper to get pathNodes size for reserving
        size_t GetPathNodesSize() const { return m_copiedMapData.pathNodeSize; }

        // Helper to check if map data is valid
        bool HasMapData() const { return !m_copiedMapData.maps.empty(); }

        // Helper to get map count
        size_t GetMapCount() const { return m_copiedMapData.maps.size(); }

        // Helper to get specific map
        const CopiedPathingMap& GetMap(size_t index) const { return m_copiedMapData.maps[index]; }

        void createPortalPair(const GW::PathingTrapezoid* pt1, uint8_t pt1_layer, const GW::PathingTrapezoid* pt2, uint8_t pt2_layer, GW::Vec2f p1, bool p1_viability, GW::Vec2f p2, bool p2_viability, Edge edge)
        {
            auto id = static_cast<Portal::id>(portals.size());
            auto p_id = static_cast<PointId>(points.size());

            points.emplace_back(p1, static_cast<PointId>(points.size()), id, id + 1, p1_viability);

            points.emplace_back(p2, static_cast<PointId>(points.size()), id, id + 1, p2_viability);

            portals.emplace_back(id, id + 1, p_id, p_id + 1, pt1, pt1_layer, edge);

            portals.emplace_back(id + 1, id, p_id + 1, p_id, pt2, pt2_layer, (Edge)(((uint32_t)edge + 2u) % 4u));

            pt_portal_map[pt1->id].push_back(id);
            pt_portal_map[pt2->id].push_back(id + 1);
            tmp_portal_pt_map[id] = pt1;
            tmp_portal_pt_map[id + 1] = pt2;
        }

        void GetTrapezoidNeighbours(const CopiedPathingMap& m, const GW::PathingTrapezoid* trapezoid, uint8_t trapezoid_layer, std::vector<Neighbour>& neighbours)
        {
            using namespace MathUtil;
            if (!trapezoid) return;
            if (trapezoid->YB == trapezoid->YT) return;

            auto& visited = GetTrapezoidNeighbours_visited;
            auto& to_explore = GetTrapezoidNeighbours_to_explore;


            to_explore.clear();
            to_explore.reserve(10);
            visited.clear();                     // and reserve an unordered map. -> cca. 20x faster
            visited.reserve(GetPathNodesSize()); //

            to_explore.push_back({trapezoid, nullptr});
            visited[trapezoid] = true;

            while (to_explore.size()) {
                const Explore n = std::move(to_explore.back());
                to_explore.pop_back();

                if (n.t->portal_left < m.portal_count) {
                    auto portal = &m.portals[n.t->portal_left];
                    if (portal && portal->pair) {
                        portal = portal->pair;
                        if (portal->flags & 0x04) continue;

                        GW::PathingTrapezoid** begin = portal->trapezoids;
                        GW::PathingTrapezoid** end = begin + portal->count;
                        for (auto it = begin; it < end; ++it) {
                            if (!it || !(*it)) continue;
                            if (visited[(*it)]) continue;
                            if ((*it)->YB == (*it)->YT) { // Trapezoid is degenerate
                                if (between(trapezoid->XTL, (*it)->XBL, (*it)->XBR) || between(trapezoid->XTR, (*it)->XBL, (*it)->XBR) || between((*it)->XBR, trapezoid->XTL, trapezoid->XTR) || between((*it)->XBL, trapezoid->XTL, trapezoid->XTR)) {
                                    to_explore.emplace_back(*it, n.t);
                                    visited[(*it)] = true;
                                }
                            }
                            else {
                                if (onSegment({trapezoid->XTL, trapezoid->YT}, {(*it)->XTR, (*it)->YT}, {trapezoid->XBL, trapezoid->YB}) || onSegment({trapezoid->XTL, trapezoid->YT}, {(*it)->XBR, (*it)->YB}, {trapezoid->XBL, trapezoid->YB}) ||
                                    onSegment({(*it)->XTR, (*it)->YT}, {trapezoid->XTL, trapezoid->YT}, {(*it)->XBR, (*it)->YB}) || onSegment({(*it)->XTR, (*it)->YT}, {trapezoid->XTL, trapezoid->YT}, {(*it)->XBR, (*it)->YB})) {
                                    neighbours.emplace_back(*it, (uint8_t)portal->portal_plane, Edge::left);
                                    visited[(*it)] = true;
                                }
                            }
                        }
                    }
                }

                if (n.t->portal_right < m.portal_count) {
                    auto portal = &m.portals[n.t->portal_right];
                    if (portal && portal->pair) {
                        portal = portal->pair;
                        if (portal->flags & 0x04) continue;

                        GW::PathingTrapezoid** begin = portal->trapezoids;
                        GW::PathingTrapezoid** end = begin + portal->count;
                        for (auto it = begin; it < end; ++it) {
                            if (!it || !(*it)) continue;
                            if (visited[(*it)]) continue;
                            if ((*it)->YT == (*it)->YB) {
                                if (between(trapezoid->XBL, (*it)->XTL, (*it)->XTR) || between(trapezoid->XBR, (*it)->XTL, (*it)->XTR) || between((*it)->XTR, trapezoid->XBL, trapezoid->XBR) || between((*it)->XTL, trapezoid->XBL, trapezoid->XBR)) {
                                    to_explore.emplace_back(*it, n.t);
                                    visited[(*it)] = true;
                                }
                            }
                            else {
                                if (onSegment({trapezoid->XTR, trapezoid->YT}, {(*it)->XTL, (*it)->YT}, {trapezoid->XBR, trapezoid->YB}) || onSegment({trapezoid->XTR, trapezoid->YT}, {(*it)->XBL, (*it)->YB}, {trapezoid->XBR, trapezoid->YB}) ||
                                    onSegment({(*it)->XTL, (*it)->YT}, {trapezoid->XTR, trapezoid->YT}, {(*it)->XBL, (*it)->YB}) || onSegment({(*it)->XTL, (*it)->YT}, {trapezoid->XTR, trapezoid->YT}, {(*it)->XBL, (*it)->YB})) {
                                    neighbours.emplace_back(*it, (uint8_t)portal->neighbor_plane, Edge::right);
                                    visited[(*it)] = true;
                                }
                            }
                        }
                    }
                }

                auto ProcessTrapezoid = [&](const GW::PathingTrapezoid* t, Edge loc, const GW::PathingTrapezoid* came_from) {
                    if (!t || t == came_from) return;
                    if (visited[t]) return;
                    if (t->YB == t->YT) {
                        to_explore.emplace_back(t, came_from);
                        visited[t] = true;
                    }
                    else {
                        neighbours.emplace_back(t, trapezoid_layer, loc);
                        visited[t] = true;
                    }
                };

                ProcessTrapezoid(n.t->adjacent[0], Edge::top, n.t);
                ProcessTrapezoid(n.t->adjacent[1], Edge::top, n.t);
                ProcessTrapezoid(n.t->adjacent[2], Edge::bottom, n.t);
                ProcessTrapezoid(n.t->adjacent[3], Edge::bottom, n.t);
            }
        }


        bool isNeighbourOf(const GW::PathingTrapezoid* pt1, uint8_t pt1_layer, const GW::PathingTrapezoid* pt2)
        {
            auto& neighbours = isNeighbourOf_neighbours;

            neighbours.clear();
            if (!HasMapData() || pt1_layer >= GetMapCount()) return false;

            auto& m = GetMap(pt1_layer);
            GetTrapezoidNeighbours(m, pt1, pt1_layer, neighbours);
            return std::ranges::any_of(neighbours, [&](const auto& n) {
                return n.t == pt2;
            });
        }

        void linkTrapezoids(const GW::PathingTrapezoid* pt1, uint8_t pt1_layer, const Neighbour& n)
        {
            const float tolerance = 0.1f;

            const GW::PathingTrapezoid* pt2 = n.t;
            uint8_t pt2_layer = n.layer;
            Edge edge = n.loc; // neighbour location relative to pt1

            // Definitios of portals and points
            //         \        \        
        //     _____\a......b\____
            //     |                 a|___
            // ____|b                 .
            //     .                  .
            //     .                 b.___
            // ____.a                 |
            //     |____b..........a__|
            //          /         /

            bool point1_viability = false; // pt1_layer != pt2_layer;
            bool point2_viability = false;

            if (edge == Edge::top) {
                if (pt1->YT != pt2->YB) return;

                auto ax = std::max(pt1->XTL, pt2->XBL);
                auto bx = std::min(pt1->XTR, pt2->XBR);
                if (fabsf(ax - bx) < tolerance) return;

                GW::Vec2f dtl, dbl, dtr, dbr; // diff top left, diff bottom left, diff top right, diff top right
                auto a = GW::Vec2f{ax, pt1->YT};
                auto b = GW::Vec2f{bx, pt1->YT};

                if (pt1->XTL > pt2->XBL) { // Top left trapezoid is longer than pt1.
                    point1_viability = true;
                }
                else if (pt1->XTL < pt2->XBL) { // Top left trapezoid is shorther than pt1.
                    point1_viability = true;
                }
                else {                                              // if (pt1->XTL == pt2->XBL)
                    auto p1 = GW::Vec2f{pt2->XTL, pt2->YT};         // p1\   PT2        //     /p1   PT2    //   |p1   PT2
                    auto p2 = GW::Vec2f{pt1->XBL, pt1->YB};         //    \             //    /             //   |
                                                                    //     \_______     // a /_______       // a |_______
                    dtl = p1 - a;                                   //     /a           //   \              //   |
                    dbl = p2 - a;                                   // p2 /     PT1     //  p2\     PT1     // p2|     PT1
                                                                    //                  //                  //
                    point1_viability = Cross(dtl, dbl) > tolerance; // determine if they form convex shape
                    if (Cross(dtl, dbl) < -tolerance) {
                        point1_viability |= pt1->portal_left != 0xFFFF;
                        point1_viability |= pt2->portal_left != 0xFFFF;
                    }
                }

                if (pt1->XTR < pt2->XBR) { // Top right trapezoid is logner than pt1.
                    point2_viability = true;
                }
                else if (pt1->XTR > pt2->XBR) { // Top right trapezoid is shorther than pt1.
                    point2_viability = true;
                }
                else {                                               // if (pt1->XTR == pt2->XBR)
                    auto p1 = GW::Vec2f{pt2->XTR, pt2->YT};          // PT2 \p1         // PT2     /p1      // PT2   |p1
                    auto p2 = GW::Vec2f{pt1->XBR, pt1->YB};          //      \          //        /         //       |
                                                                     // ______\         // ______/          // ______|
                    dtr = p1 - b;                                    // PT1   /b        // PT1   \b         // PT1   |b
                    dbr = p2 - b;                                    //      /          //        \         //       |
                                                                     //     /p2         //         \p2      //       |p2
                    point2_viability = Cross(dtr, dbr) < -tolerance; // determine if they form convex shape
                    if (Cross(dtr, dbr) > tolerance) {
                        point2_viability |= pt1->portal_right != 0xFFFF;
                        point2_viability |= pt2->portal_right != 0xFFFF;
                    }
                }

                createPortalPair(pt1, pt1_layer, pt2, pt2_layer, a, point1_viability, b, point2_viability, edge);
            }
            else if (edge == Edge::bottom) {
                if (pt1->YB != pt2->YT) return;

                auto ax = std::min(pt1->XBR, pt2->XTR);
                auto bx = std::max(pt1->XBL, pt2->XTL);
                if (fabsf(ax - bx) < tolerance) return;

                GW::Vec2f dtl, dbl, dtr, dbr;
                auto a = GW::Vec2f{ax, pt1->YB};
                auto b = GW::Vec2f{bx, pt1->YB};

                if (pt1->XBR > pt2->XTR) { // Bottom right trapezoid is shorter than pt1.
                    point1_viability = true;
                }
                else if (pt1->XBR < pt2->XTR) { // Bottom right trapezoid is longer than pt1.
                    point1_viability = true;
                }
                else {                                      // if (pt1->XTR == pt2->XBR)
                    auto p1 = GW::Vec2f{pt1->XTR, pt1->YT}; // PT1 \p1         // PT1     /p1      // PT1   |p1
                    auto p2 = GW::Vec2f{pt2->XBR, pt2->YB}; //      \          //        /         //       |
                                                            // ______\         // ______/          // ______|
                    dtr = p1 - a;                           // PT2   /a        // PT2   \a         // PT2   |a
                    dbr = p2 - a;                           //      /          //        \         //       |
                                                            //     /p2         //         \p2      //       |p2
                    point1_viability = Cross(dtr, dbr) < -tolerance;
                    if (Cross(dtr, dbr) > tolerance) {
                        point1_viability |= pt1->portal_right != 0xFFFF;
                        point1_viability |= pt2->portal_right != 0xFFFF;
                    }
                }

                if (pt1->XBL > pt2->XTL) { // Bottom left trapezoid is longer than pt1.
                    point2_viability = true;
                }
                else if (pt1->XBL < pt2->XTL) { // Bottom left trapezoid is shorther than pt1.
                    point2_viability = true;
                }
                else {                                      // if (pt1->XTL == pt2->XBL)
                    auto p1 = GW::Vec2f{pt1->XTL, pt1->YT}; // p1\   PT1        //     /p1   PT1    //   |p1   PT1
                    auto p2 = GW::Vec2f{pt2->XBL, pt2->YB}; //    \             //    /             //   |
                                                            //     \_______     // b /_______       // b |_______
                    dtl = p1 - b;                           //     /b           //   \              //   |
                    dbl = p2 - b;                           // p2 /     PT2     //  p2\     PT2     // p2|     PT2
                                                            //                  //                  //
                    point2_viability = Cross(dtl, dbl) > tolerance;
                    if (Cross(dtl, dbl) < -tolerance) {
                        point2_viability |= pt1->portal_left != 0xFFFF;
                        point2_viability |= pt2->portal_left != 0xFFFF;
                    }
                }

                createPortalPair(pt1, pt1_layer, pt2, pt2_layer, a, point1_viability, b, point2_viability, edge);
            }
            else if (edge == Edge::left) {
                if (pt1->YT < pt2->YB || pt1->YB > pt2->YT) return;

                GW::Vec2f a;
                if (pt1->YB <= pt2->YB) {
                    a = {pt2->XBR, pt2->YB};
                }
                else {
                    a = {pt1->XBL, pt1->YB};
                }
                GW::Vec2f b;
                if (pt1->YT <= pt2->YT) {
                    b = {pt1->XTL, pt1->YT};
                }
                else {
                    b = {pt2->XTR, pt2->YT};
                }

                if (pt1->YB < pt2->YB) { // bottom of trapezoid on left of pt1 is higher.
                    point1_viability = !(isNeighbourOf(pt2->bottom_left, n.layer, pt1) || isNeighbourOf(pt2->bottom_right, n.layer, pt1));
                }
                else if (pt1->YB > pt2->YB) { // bottom of trapezoid on left of pt1 is lower.
                    point1_viability = !(isNeighbourOf(pt1->bottom_left, pt1_layer, pt2) || isNeighbourOf(pt1->bottom_right, pt1_layer, pt2));
                }
                else { // pt1->YB == pt2->YB
                    point1_viability = pt1_layer != pt2_layer;
                }

                if (pt1->YT < pt2->YT) { // top of trapezoid on left of pt1 is higher.
                    point2_viability = !(isNeighbourOf(pt1->top_left, pt1_layer, pt2) || isNeighbourOf(pt1->top_right, pt1_layer, pt2));
                }
                else if (pt1->YT > pt2->YT) { // top of trapezoid on left of pt1 is lower.
                    point2_viability = !(isNeighbourOf(pt2->top_left, pt1_layer, pt1) || isNeighbourOf(pt2->top_right, pt1_layer, pt1));
                }
                else { // if (pt1->YT == pt2->YT)
                    point2_viability = pt1_layer != pt2_layer;
                }

                createPortalPair(pt1, pt1_layer, pt2, pt2_layer, a, point1_viability, b, point2_viability, edge);
            }
            else if (edge == Edge::right) {
                if (pt1->YT < pt2->YB || pt1->YB > pt2->YT) return;

                GW::Vec2f a;
                if (pt1->YT <= pt2->YT) {
                    a = {pt1->XTR, pt1->YT};
                }
                else {
                    a = {pt2->XTL, pt2->YT};
                }
                GW::Vec2f b;
                if (pt1->YB <= pt2->YB) {
                    b = {pt2->XBL, pt2->YB};
                }
                else {
                    b = {pt1->XBR, pt1->YB};
                }

                if (pt1->YT < pt2->YT) { // top of trapezoid on right of pt1 is higher.
                    point1_viability = !(isNeighbourOf(pt1->top_left, pt1_layer, pt2) || isNeighbourOf(pt1->top_right, pt1_layer, pt2));
                }
                else if (pt1->YT > pt2->YT) { // top of trapezoid on right of pt1 is lower.
                    point1_viability = !(isNeighbourOf(pt2->top_left, n.layer, pt1) || isNeighbourOf(pt2->top_right, n.layer, pt1));
                }
                else { // if (pt1->YT == pt2->YT)
                    point1_viability = pt1_layer != pt2_layer;
                }

                if (pt1->YB < pt2->YB) { // bottom of trapezoid on right of pt1 is higher.
                    point2_viability = !(isNeighbourOf(pt2->bottom_left, n.layer, pt1) || isNeighbourOf(pt2->bottom_right, n.layer, pt1));
                }
                else if (pt1->YB > pt2->YB) { // bottom of trapezoid on right of pt1 is lower.
                    point2_viability = !(isNeighbourOf(pt1->bottom_left, pt1_layer, pt2) || isNeighbourOf(pt1->bottom_right, pt1_layer, pt2));
                }
                else { // pt1->YB == pt2->YB
                    point2_viability = pt1_layer != pt2_layer;
                }

                createPortalPair(pt1, pt1_layer, pt2, pt2_layer, a, point1_viability, b, point2_viability, edge);
            }
            else {
                Log::Error("linkTrapezoids(): edge == Edge::none");
            }
        }

        void GenerateTrapezoidNeighbours()
        {
            Timing time(__FUNCTION__);
            if (!HasMapData()) return;

            ptneighbours.clear();
            ptneighbours.reserve(GetPathNodesSize());

            auto size = GetMapCount();
            for (uint8_t i = 0; i < size; ++i) {
                auto& m = GetMap(i);
                for (uint32_t j = 0; j < m.trapezoid_count; j++) {
                    const GW::PathingTrapezoid* t = &m.trapezoids[j];
                    auto& n = ptneighbours[t];
                    GetTrapezoidNeighbours(m, t, i, n);
                }
            }
        }


        void GeneratePortals()
        {
            Timing time(__FUNCTION__);
            if (!HasMapData()) return;

            auto reserved_points = static_cast<int>(GetPathNodesSize() * 2.2) + m_teleports.size() * 2;
            points.clear();
            points.reserve(reserved_points);
            portals.clear();
            portals.reserve(reserved_points);
            pt_portal_map.clear();
            pt_portal_map.resize(GetPathNodesSize());

            portal_pt_map.clear();

            int pitch = GetPathNodesSize();

            std::vector<uint8_t> visited(pitch * pitch);

            for (uint8_t i = 0; i < GetMapCount(); ++i) {
                const auto& m = GetMap(i);
                for (uint32_t j = 0; j < m.trapezoid_count; j++) {
                    const auto* t = &m.trapezoids[j];

                    for (const auto& n : ptneighbours[t]) {
                        if (visited[t->id * pitch + n.t->id]) continue;
                        visited[t->id * pitch + n.t->id] = true;
                        visited[t->id + n.t->id * pitch] = true;

                        linkTrapezoids(t, i, n);
                    }
                }
            }

            // Some old stuff; check if anything below here can be removed:
            portal_portal_map.clear();
            portal_portal_map.resize(tmp_portal_pt_map.size() + m_teleports.size() * 2 + 2);
            portal_pt_map.resize(tmp_portal_pt_map.size() + m_teleports.size() * 2 + 2);

            std::ranges::for_each(tmp_portal_pt_map, [&](auto& kvp) {
                portal_pt_map[kvp.first] = std::move(kvp.second);
            });
            tmp_portal_pt_map.clear();

            int idx = 0;
            std::ranges::for_each(portal_pt_map, [&](const auto& pt) {
                if (!pt) return;
                const auto& pts = pt_portal_map[pt->id];
                std::ranges::for_each(pts, [&](const auto& pt) {
                    portal_portal_map[idx].push_back(pt);
                });
                idx++;
            });

#ifdef DEBUG_PATHING
            Log::Info("total trapezoid count: %d", mapContex->path->pathNodes.size());
            Log::Info("new portals: %d", portals.size());
            Log::Info("viable points: %d", std::ranges::count_if(points, [](const auto& point) {
                          return point.is_viable;
                      }));
            Log::Info("total points: %d", points.size());
            ;
#endif
        }

        // Generate distance graph among teleports
        void GenerateTeleportGraph()
        {
            if (m_terminateThread) return;

            using namespace MapSpecific;

            m_defferedPortalLinks.clear();
            for (const auto& teleport : m_teleports) {
                auto pt = Pathing::FindClosestTrapezoid(teleport.m_enter);
                if (!pt) continue;
                auto& enter_id = createSinglePointPortal(pt, teleport.m_enter);

                pt = Pathing::FindClosestTrapezoid(teleport.m_exit);
                if (!pt) continue;
                auto& exit_id = createSinglePointPortal(pt, teleport.m_enter);
                m_defferedPortalLinks.emplace_back(enter_id.m_id, exit_id.m_id, teleport.m_directionality);
            }

            m_teleportGraph.clear();

            auto size = m_teleports.size();
            for (size_t i = 0; i < size; ++i) {
                auto& p1 = m_teleports[i];
                for (size_t j = i; j < size; ++j) {
                    auto& p2 = m_teleports[j];

                    if (p1.m_directionality == Teleport::direction::both_ways && p2.m_directionality == Teleport::direction::both_ways) {
                        float dist = std::min({GetDistance(p1.m_enter, p2.m_exit), GetDistance(p1.m_exit, p2.m_enter), GetDistance(p1.m_exit, p2.m_exit), GetDistance(p1.m_enter, p2.m_enter)});
                        m_teleportGraph.push_back({&p1, &p2, dist});
                        if (&p1 == &p2) continue;
                        m_teleportGraph.push_back({&p2, &p1, dist});
                    }
                    else if (p1.m_directionality == Teleport::direction::both_ways) {
                        float dist = std::min(GetDistance(p1.m_enter, p2.m_enter), GetDistance(p1.m_exit, p2.m_enter));
                        m_teleportGraph.push_back({&p1, &p2, dist});
                        dist = std::min(GetDistance(p2.m_exit, p1.m_enter), GetDistance(p2.m_exit, p1.m_exit));
                        m_teleportGraph.push_back({&p2, &p1, dist});
                    }
                    else if (p2.m_directionality == Teleport::direction::both_ways) {
                        float dist = std::min(GetDistance(p2.m_enter, p1.m_enter), GetDistance(p2.m_exit, p1.m_enter));
                        m_teleportGraph.push_back({&p2, &p1, dist});
                        dist = std::min(GetDistance(p1.m_exit, p2.m_enter), GetDistance(p1.m_exit, p2.m_exit));
                        m_teleportGraph.push_back({&p1, &p2, dist});
                    }
                    else {
                        float dist = GetDistance(p1.m_exit, p2.m_enter);
                        m_teleportGraph.push_back({&p1, &p2, dist});
                        if (&p1 == &p2) continue;
                        dist = GetDistance(p2.m_exit, p1.m_enter);
                        m_teleportGraph.push_back({&p2, &p1, dist});
                    }
                }
            }
        }

        inline void process_portal(std::vector<node>& open, size_t& sp, VisitedState& visited, const Portal& portal, Portal::id other_portal_id)
        {
            size_t checkpoint = visited.checkpoint();

            visited.visit(portal.m_id);
            visited.visit(other_portal_id);

            for (auto pid : portal_portal_map[portal.m_id]) {
                if (pid == portal.m_id || pid == other_portal_id) continue;

                if (sp >= open.size()) return; // safety guard

                node& n = open[sp++];
                n.next = pid;
                n.visited_checkpoint = checkpoint;

                n.blocked_planes.reset();
                if (portal.m_pt_layer) n.blocked_planes.set(portal.m_pt_layer, true);

                n.funnel[0] = points[portals[pid].m_point[0]].m_pos;
                n.funnel[1] = points[portals[pid].m_point[1]].m_pos;
            }
        }

        inline void process_point(std::vector<node>& open, size_t& sp, VisitedState& visited, const Point& point)
        {
            process_portal(open, sp, visited, portals[point.m_portals[0]], point.m_portals[1]);

            if (point.m_portals[0] != point.m_portals[1]) {
                process_portal(open, sp, visited, portals[point.m_portals[1]], point.m_portals[0]);
            }
        }

        Point& createSinglePointPortal(const GW::PathingTrapezoid* pt, const GW::GamePos& gp)
        {
            auto point_id = static_cast<PointId>(points.size());
            auto id = static_cast<Portal::id>(portals.size());
            points.emplace_back(gp, point_id, id, id, true);

            portals.emplace_back(id, id, point_id, point_id, pt, static_cast<uint8_t>(gp.zplane), Edge::top);

            portal_pt_map[id] = pt;

            for (const auto& pid : pt_portal_map[pt->id]) {
                portal_portal_map[id].push_back(pid);
                portal_portal_map[pid].push_back(id);
            }
            pt_portal_map[pt->id].push_back(id);

            return points[point_id];
        }

        void VisGraphInsertPoint(std::vector<node>& open, const Point& point)
        {
            // Timing time(__FUNCTION__);

            size_t sp = 0;

            VisitedState visited; // portals (rollback)
            visited.init(portals.size());

            if (!point.is_viable) return;

            sp = 0;
            visited.stack.clear();
            visited.stamp++;

            process_point(open, sp, visited, point);

            int cnt = 20000;
            const auto& p = point;
            while (sp && --cnt) {
                node cur = open[--sp];
                visited.rollback(cur.visited_checkpoint); // rollback to this node's state

                const auto& portal = portals[cur.next];
                if (visited.is_visited(portal.m_id)) continue;

                visited.visit(portal.m_id);

                if (portal.m_pt_layer) cur.blocked_planes.set(portal.m_pt_layer, true);

                const auto& p0 = points[portal.m_point[0]];
                const auto& p1 = points[portal.m_point[1]];

                auto& f0 = cur.funnel[0];
                auto& f1 = cur.funnel[1];

                auto fl = f0 - p.m_pos;
                auto fr = f1 - p.m_pos;
                auto nl = p0.m_pos - p.m_pos;
                auto nr = p1.m_pos - p.m_pos;

                float fl_nl = Cross(fl, nl);
                float fr_nr = Cross(fr, nr);
                float fl_nr = Cross(fl, nr);
                float fr_nl = Cross(fr, nl);

                constexpr float tolerance = 1.0f;
                if (fr_nl < -tolerance || fl_nr > tolerance) continue;

                if (fl_nl <= tolerance) {
                    if (p0.is_viable) {
                        float dist = GW::GetDistance(p.m_pos, p0.m_pos);
                        m_visGraph[p.m_id].emplace_back(dist, cur.blocked_planes, p0.m_id);
                        m_visGraph[p0.m_id].emplace_back(dist, cur.blocked_planes, p.m_id);
                    }
                    f0 = p0.m_pos;
                }

                if (fr_nr >= -tolerance) {
                    if (p1.is_viable) {
                        float dist = GW::GetDistance(p.m_pos, p1.m_pos);
                        m_visGraph[p.m_id].emplace_back(dist, cur.blocked_planes, p1.m_id);
                        m_visGraph[p1.m_id].emplace_back(dist, cur.blocked_planes, p.m_id);
                    }
                    f1 = p1.m_pos;
                }

                size_t cp = visited.checkpoint();
                visited.visit(portal.m_other_id);

                for (auto pid : portal_portal_map[portal.m_other_id]) {
                    if (visited.is_visited(pid)) continue;

                    if (sp >= open.size()) break;

                    node& child = open[sp++];
                    child = cur;
                    child.next = pid;
                    child.visited_checkpoint = cp;
                }
            }
        }

#pragma optimize("gty", on) // Enable optimizations
        void GenerateVisGraph()
        {
            Timing time(__FUNCTION__);

            m_visGraph.clear();
            m_visGraph.resize(points.size() + 2); // reserve also for start and goal points

            for (auto& v : m_visGraph)
                v.reserve(32);

#pragma omp parallel
            {
                std::vector<node> open;
                open.resize(2000); // max DFS depth
                size_t sp = 0;

                VisitedState visited;     // portals (rollback)
                VisitedPoints vis_points; // points (per source)
                visited.init(portals.size());
                vis_points.init(points.size());
                auto size = (int)points.size();

#pragma omp for schedule(static, 4)
                for (int i = 0; i < size; ++i) {
                    if (m_terminateThread) break;
                    const auto& point = points[i];
                    if (!point.is_viable) continue;

                    vis_points.reset();

                    sp = 0;
                    visited.stack.clear();
                    visited.stamp++;

                    process_point(open, sp, visited, point);

                    int cnt = 20000;
                    const auto& p = point;
                    while (sp && --cnt) {
                        node cur = open[--sp];

                        visited.rollback(cur.visited_checkpoint); // rollback to this node's state

                        const auto& portal = portals[cur.next];
                        if (visited.is_visited(portal.m_id)) continue;

                        visited.visit(portal.m_id);

                        if (portal.m_pt_layer) cur.blocked_planes.set(portal.m_pt_layer, true);

                        const auto& p0 = points[portal.m_point[0]];
                        const auto& p1 = points[portal.m_point[1]];

                        auto& f0 = cur.funnel[0];
                        auto& f1 = cur.funnel[1];

                        auto fl = f0 - p.m_pos;       // funnel left direction
                        auto fr = f1 - p.m_pos;       // funnel right direction
                        auto nl = p0.m_pos - p.m_pos; // portal left direction
                        auto nr = p1.m_pos - p.m_pos; // portal right direction

                        constexpr float tolerance = 1.0f;
                        if (Cross(fr, nl) < -tolerance || Cross(fl, nr) > tolerance) continue;

                        if (Cross(fl, nl) <= tolerance) {
                            if (p0.is_viable && vis_points.test_and_set(p0.m_id)) {
                                {
                                    float dist = GW::GetDistance(p.m_pos, p0.m_pos);
                                    m_visGraph[p.m_id].emplace_back(dist, cur.blocked_planes, p0.m_id);
                                }
                            }
                            f0 = p0.m_pos;
                        }

                        if (Cross(fr, nr) >= -tolerance) {
                            if (p1.is_viable && vis_points.test_and_set(p1.m_id)) {
                                {
                                    float dist = GW::GetDistance(p.m_pos, p1.m_pos);
                                    m_visGraph[p.m_id].emplace_back(dist, cur.blocked_planes, p1.m_id);
                                }
                            }
                            f1 = p1.m_pos;
                        }

                        size_t checkpoint = visited.checkpoint();
                        visited.visit(portal.m_other_id);

                        for (auto pid : portal_portal_map[portal.m_other_id]) {
                            if (visited.is_visited(pid)) continue;

                            if (sp >= open.size()) break;

                            node& child = open[sp++];
                            child = cur;
                            child.next = pid;
                            child.visited_checkpoint = checkpoint;
                        }
                    }
                }
            }

            // could not link portals earlier because vis graph is cleared at the beginning of this function.
            for (const auto& dp : m_defferedPortalLinks) {
                auto dist = GW::GetDistance(points[dp.m_enter].m_pos, points[dp.m_exit].m_pos);

                BlockedPlaneBitset bp;
                m_visGraph[dp.m_enter].emplace_back(dist * 0.01f, bp, dp.m_exit);
                if (dp.m_directionality == MapSpecific::Teleport::direction::both_ways) m_visGraph[dp.m_exit].emplace_back(dist * 0.01f, bp, dp.m_enter);
            }

            uint32_t total = 0;
            for (const auto& v : m_visGraph) {
                total += v.size();
            }

#ifdef DEBUG_PATHING
            Log::Info("m_visGraph total elements = %d", total);
#endif
        }
#pragma optimize("", on) // Restore global optimizations to project default

#define mImpl ((Impl*)opaque)
    }; // Impl
} // namespace Pathing

namespace Pathing {
    // using namespace MathUtil;

    // Traverse map props and copy an array of valid in-game portals; later used for travel calcs
    void MilePath::LoadMapSpecificData()
    {
        MapSpecific::MapSpecificData map_data(GW::Map::GetMapID());
        mImpl->m_teleports = map_data.m_teleports;
        mImpl->travel_portals.clear();
        const auto props = GetMapProps();
        if (!props) return;
        for (const auto prop : *props) {
            if (IsTravelPortal(prop)) {
                // NB: May need to guess height and width for these - 1100.f ?
                mImpl->travel_portals.push_back(prop);
            }
        }
    }

    MilePath::MilePath([[maybe_unused]] GW::MapContext* map_context)
    {
        //constexpr size_t sz = sizeof(Impl);
        static_assert(sizeof(opaque) >= sizeof(Impl));
        new (opaque) Impl();

        m_processing = true;
        const clock_t start = clock();

        // Copy map data from game into Impl
        auto* mapContext = GW::GetMapContext();
        if (mapContext && mapContext->path) {
            mImpl->CopyMapData(mapContext->path);
        }

        ASSERT(!worker_thread);
        worker_thread = new std::thread([&, start] {
            LoadMapSpecificData();
            mImpl->GenerateTrapezoidNeighbours(); // needs to run in game thread
            mImpl->GeneratePortals();             // needs to run in game thread
            mImpl->GenerateTeleportGraph();
            mImpl->GenerateVisGraph();
#ifdef _DEBUG
            const clock_t stop = clock();
            Log::Flash("Processing %s in %d ms", mImpl->m_terminateThread ? "terminated" : "done", stop - start);
#endif
            m_processing = false;
            m_done = true;
            m_progress = 100;
        });
        worker_thread->detach();
    }

    MilePath::~MilePath()
    {
        shutdown();
        mImpl->~Impl();
    }

    void MilePath::stopProcessing()
    {
        mImpl->m_terminateThread = true;
    }


    AStar::AStar(MilePath* mp) : m_path(this)
    {
        m_path.m_mp = mp;
    };

    class Path {
    public:
        std::vector<GW::GamePos> points{};
        float cost{}; // distance
        int visited_index{};
    };

    // https://github.com/Rikora/A-star/blob/master/src/AStar.cpp
    Error BuildPath(AStar& astar, const Point& start, const Point& goal, const std::vector<PointId>& came_from)
    {
        Point current(goal);

        astar.m_path.clear();
        auto* mp = (Impl*)astar.m_path.m_mp->GetImpl();

        int count = 0;
        while (current.m_id != start.m_id) {
            if (count++ > 256) {
                Log::Error("build path failed\n");
                return Error::BuildPathLengthExceeded;
            }
            if (current.m_id < 0) {
                break;
            }

            // Finding zplane here and in this way is more of a workaround:
            auto zplane = std::max(mp->portals[current.m_portals[0]].m_pt_layer, mp->portals[current.m_portals[1]].m_pt_layer);
            astar.m_path.insertPoint({current.m_pos.x, current.m_pos.y, zplane});

            auto& id = came_from[current.m_id];
            if (id == start.m_id) break;

            current = mp->points[id];
        }

        auto zplane = std::max(mp->portals[start.m_portals[0]].m_pt_layer, mp->portals[start.m_portals[1]].m_pt_layer);
        astar.m_path.insertPoint({start.m_pos.x, start.m_pos.y, zplane});

        return Error::OK;
    }

    float TeleporterHeuristic(const MapSpecific::Teleports& teleports, const std::vector<MapSpecific::teleport_node>& teleportGraph, const Point& start, const Point& goal)
    {
        if (teleports.empty()) return 0.0f;

        using namespace MapSpecific;

        float cost = INFINITY;
        const Teleport* ts = nullptr;
        const Teleport* tg = nullptr;
        float dist_start = cost;
        float dist_goal = cost;
        for (const auto& tp : teleports) {
            float dist = GW::GetSquareDistance(start.m_pos, tp.m_enter);
            if (tp.m_directionality == Teleport::direction::both_ways) dist = std::min(dist, GW::GetSquareDistance(start.m_pos, tp.m_exit));

            if (dist_start > dist) {
                dist_start = dist;
                ts = &tp;
            }

            dist = GW::GetSquareDistance(goal.m_pos, tp.m_exit);
            if (tp.m_directionality == Teleport::direction::both_ways) dist = std::min(dist, GW::GetSquareDistance(goal.m_pos, tp.m_enter));

            if (dist_goal > dist) {
                dist_goal = dist;
                tg = &tp;
            }
        }

        for (const auto& ttd : teleportGraph) {
            if (ttd.tp1 == ts && ttd.tp2 == tg) {
                // cost = sqrtf(dist_start) + ttd.distance + sqrtf(dist_goal);
                cost = sqrtf(dist_start);
                break;
            }
        }
        return cost;
    }

    typedef std::pair<float, PointId> PQElement;
    class MyPQueue : public std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>> {
    public:
        MyPQueue(size_t reserve_size) { this->c.reserve(reserve_size); }
    };

    Error AStar::Search(const GW::GamePos& _start_pos, const GW::GamePos& _goal_pos)
    {
        Timing time(__FUNCTION__);

        std::lock_guard lock(pathing_mutex);

        BlockedPlaneBitset current_blocked_planes;
        const Error res = CopyPathingMapBlocks(&current_blocked_planes);

        if (res != Error::OK) return res;

        auto start_pos = _start_pos;
        auto goal_pos = _goal_pos;
        auto spt = FindClosestPositionOnTrapezoid(start_pos);
        auto gpt = FindClosestPositionOnTrapezoid(goal_pos);
        if (!spt) return Error::FailedToFindStartPathingTrapezoid;
        if (!gpt) return Error::FailedToFindGoalPathingTrapezoid;

        auto* mp = (Impl*)m_path.m_mp->GetImpl();

        auto cleanup = [&](const Point& point) {
            auto& elements = mp->m_visGraph[point.m_id];
            for (auto& elem : elements) {
                std::erase_if(mp->m_visGraph[elem.point_id], [&point](auto& elem) {
                    return elem.point_id == point.m_id;
                });
            }
            elements.clear();

            mp->points.pop_back();

            auto& adjacent = mp->portal_portal_map[point.m_portals[0]];
            for (auto& a : adjacent) {
                std::erase_if(mp->portal_portal_map[a], [&point](auto& portal) {
                    return portal == point.m_portals[0];
                });
            }
            adjacent.clear();

            auto pt = std::exchange(mp->portal_pt_map[point.m_portals[0]], nullptr);
            std::erase_if(mp->pt_portal_map[pt->id], [&point](auto& portal) {
                return portal == point.m_portals[0];
            });

            mp->portals.pop_back();
        };

        Point start = mp->createSinglePointPortal(spt, start_pos);
        Point goal = mp->createSinglePointPortal(gpt, goal_pos);

        {
            std::vector<node> open_points;
            open_points.resize(2000);
            mp->VisGraphInsertPoint(open_points, start);
            open_points.clear();
            open_points.resize(2000);
            mp->VisGraphInsertPoint(open_points, goal);
        }

        if (mp->m_visGraph.size() <= start.m_id || mp->m_visGraph.size() <= goal.m_id) {
            m_path.insertPoint(start_pos);
            m_path.insertPoint(goal_pos);
            m_path.setCost(GW::GetDistance(start_pos, goal_pos));
            m_path.finalize();

            // Log::Error("invalid index into vis graph: m_visGraph.size(): %d, start.m_id: %d, goal.m_id: %d", m_visGraph.size(), start.m_id, goal.m_id);

            cleanup(goal);
            cleanup(start);
            return Error::FailedToFinializePath;
        }

        // Check if points have a direct line of sight
        for (const auto& vis : mp->m_visGraph[start.m_id]) {
            if (vis.point_id != goal.m_id) continue;
            if ((vis.blocked_planes & current_blocked_planes).none()) {
                m_path.insertPoint(start_pos);
                m_path.insertPoint(goal_pos);
                m_path.setCost(vis.distance);
                m_path.finalize();

                cleanup(goal);
                cleanup(start);
                return Error::OK;
            }
        };

        std::vector<float> cost_so_far;
        std::vector<PointId> came_from;
        MyPQueue open(mp->points.size() + 2);

        cost_so_far.resize(mp->points.size() + 2, -INFINITY);
        cost_so_far[start.m_id] = 0.0f;

        came_from.resize(mp->points.size() + 2);
        came_from[start.m_id] = start.m_id;
        open.emplace(0.0f, start.m_id);

        bool teleports = mp->m_teleports.size();
        PointId current = 0;
        while (!open.empty()) {
            current = open.top().second;
            open.pop();
            if (current == goal.m_id) break;

            for (auto& vis : mp->m_visGraph[current]) {
                if ((vis.blocked_planes & current_blocked_planes).any()) continue;
                float new_cost = cost_so_far[current] + vis.distance;
                if (cost_so_far[vis.point_id] == -INFINITY || new_cost < cost_so_far[vis.point_id]) {
                    cost_so_far[vis.point_id] = new_cost;
                    came_from[vis.point_id] = current;

                    float priority = new_cost;
                    if (teleports) {
                        auto& point = mp->points[vis.point_id];
                        float tp_cost = TeleporterHeuristic(mp->m_teleports, mp->m_teleportGraph, point, goal);
                        priority += std::min(GetDistance(point.m_pos, goal.m_pos), tp_cost);
                    }
                    open.emplace(priority, vis.point_id);
                }
            }
        }

        if (current == goal.m_id) {
            m_path.setCost(cost_so_far[current]);
            if (BuildPath(*this, start, goal, came_from) == Error::OK) {
                m_path.finalize();
            }
        }

        cleanup(goal);
        cleanup(start);

        return m_path.ready() ? Error::OK : Error::FailedToFinializePath;
    }
} // namespace Pathing
