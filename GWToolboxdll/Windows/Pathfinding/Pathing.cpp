#include "stdafx.h"

#include <GWCA/Context/MapContext.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Logger.h>
#include "MathUtility.h"
#include "Pathing.h"
#include "PathingMapDataLoader.h"

// Define PATHING_VERBOSE to re-enable per-stage Log::Info chatter
// (`[AStar:...]`, `Pathing::Impl::Generate*: N ms.`, `[VisGraph]`, `[Portals]`).
// Errors and warnings always log.
// #define PATHING_VERBOSE 1
#ifdef PATHING_VERBOSE
#define PATH_LOG_INFO(...) Log::Info(__VA_ARGS__)
#else
#define PATH_LOG_INFO(...) ((void)0)
// log_timings(tag) and similar take args only used by the logger.
#pragma warning(disable: 4189 4100)
#endif

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
        Timing(std::string_view label) : label(label), start(std::chrono::high_resolution_clock::now()) {}
        ~Timing()
        {
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            PATH_LOG_INFO("%s: %lld ms.", label.c_str(), std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }

        long long elapsed_ms() const
        {
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        }
#else
    public:
        Timing(...) {};
        long long elapsed_ms() const { return 0; }
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
        *dest = {0};
        if (!(mapContext && mapContext->path)) {
            return Pathing::Error::InvalidMapContext;
        }
        std::atomic<Pathing::Error> res{Pathing::Error::Unknown};
        GW::GameThread::Enqueue([mapContext, dest, &res] {
            Pathing::Error r;
            if (!(mapContext && mapContext->path)) {
                r = Pathing::Error::InvalidMapContext;
            }
            else {
                auto& block = mapContext->path->blockedPlanes;
                if (block.size() >= dest->size()) {
                    r = Pathing::Error::InvalidMapContext;
                }
                else {
                    for (size_t i = 0; i < block.size(); i++) {
                        dest->set(i, block[i] != 0);
                    }
                    r = Pathing::Error::OK;
                }
            }
            res.store(r, std::memory_order_release);
        });
        Pathing::Error r;
        while ((r = res.load(std::memory_order_acquire)) == Pathing::Error::Unknown) {
            std::this_thread::yield();
        }
        return r;
    }

    // Overload that gets MapContext automatically
    Pathing::Error CopyPathingMapBlocks(Pathing::BlockedPlaneBitset* dest)
    {
        return CopyPathingMapBlocks(GW::GetMapContext(), dest);
    }

    // Overload using PathingMapData's blockedPlanesPtr (no MapContext needed)
    Pathing::Error CopyPathingMapBlocks(const Pathing::PathingMapData* mapData, Pathing::BlockedPlaneBitset* dest)
    {
        *dest = {0};
        if (!mapData || !mapData->blockedPlanesPtr) {
            // DAT-loaded or no pointer: all planes unblocked
            return Pathing::Error::OK;
        }
        // Use live MapContext to validate the pointer is still valid
        // (blockedPlanesPtr may dangle after map change)
        std::atomic<Pathing::Error> res{Pathing::Error::Unknown};
        GW::GameThread::Enqueue([mapData, dest, &res] {
            Pathing::Error r;
            const auto mc = GW::GetMapContext();
            if (!(mc && mc->path && &mc->path->blockedPlanes == mapData->blockedPlanesPtr)) {
                // Pointer is stale (map changed), treat as all unblocked
                r = Pathing::Error::OK;
            }
            else {
                const auto& block = mc->path->blockedPlanes;
                if (block.size() >= dest->size()) {
                    r = Pathing::Error::InvalidMapContext;
                }
                else {
                    for (size_t i = 0; i < block.size(); i++) {
                        dest->set(i, block[i] != 0);
                    }
                    r = Pathing::Error::OK;
                }
            }
            res.store(r, std::memory_order_release);
        });
        Pathing::Error r;
        while ((r = res.load(std::memory_order_acquire)) == Pathing::Error::Unknown) {
            std::this_thread::yield();
        }
        return r;
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

    // PathingMapData-based overloads (no MapContext needed)
    GW::PathingTrapezoid* FindTrapezoid(const GW::GamePos& point, const Pathing::PathingMapData* mapData)
    {
        if (!mapData || point.zplane >= mapData->planes.size()) return nullptr;

        const auto& pm = mapData->planes[point.zplane];
        GW::Node* n = pm.root_node;

        int cnt = 50000;
        while (n && cnt--) {
            switch (n->type) {
                case 0: {
                    GW::XNode* xn = static_cast<GW::XNode*>(n);
                    float d = (point.y - xn->pos.y) * xn->dir.x - (point.x - xn->pos.x) * xn->dir.y;
                    n = (d >= 0.0f) ? xn->right : xn->left;
                    break;
                }
                case 1: {
                    GW::YNode* yn = static_cast<GW::YNode*>(n);
                    if (point.y > yn->pos.y) n = yn->above;
                    else if (point.y < yn->pos.y) n = yn->below;
                    else n = (point.x >= yn->pos.x) ? yn->above : yn->below;
                    break;
                }
                case 2: {
                    GW::SinkNode* sn = static_cast<GW::SinkNode*>(n);
                    return sn ? sn->trapezoid : nullptr;
                }
            }
        }
        return nullptr;
    }

    GW::PathingTrapezoid* FindClosestTrapezoid(const GW::GamePos& point, const Pathing::PathingMapData* mapData, uint32_t* resolved_layer = nullptr)
    {
        if (!mapData || mapData->planes.empty()) return nullptr;

        GW::PathingTrapezoid* closest = FindTrapezoid(point, mapData);
        if (closest) {
            if (resolved_layer) *resolved_layer = point.zplane;
            return closest;
        }

        float closest_dist = std::numeric_limits<float>::infinity();
        uint32_t closest_layer = point.zplane;
        constexpr float tolerance = 50.0f;

        std::vector<GW::Node*> open;

        for (size_t i = 0; i < mapData->planes.size(); ++i) {
            const auto& pm = mapData->planes[i];
            open.push_back(pm.root_node);

            int cnt = 50000;
            while (open.size() && cnt--) {
                GW::Node* n = std::move(open.back());
                open.pop_back();

                if (!n) continue;

                switch (n->type) {
                    case 0: {
                        GW::XNode* xn = static_cast<GW::XNode*>(n);
                        float d = ((point.y - xn->pos.y) * xn->dir.x - (point.x - xn->pos.x) * xn->dir.y) / GW::GetNorm(xn->dir);
                        if ((d > tolerance) || (open.push_back(xn->left), (d >= -tolerance))) {
                            open.push_back(xn->right);
                        }
                        break;
                    }
                    case 1: {
                        GW::YNode* yn = static_cast<GW::YNode*>(n);
                        float d = point.y - yn->pos.y;
                        if ((d > tolerance) || (open.push_back(yn->below), (d >= -tolerance))) {
                            open.push_back(yn->above);
                        }
                        break;
                    }
                    case 2: {
                        GW::SinkNode* sn = static_cast<GW::SinkNode*>(n);
                        if (!sn->trapezoid) continue;
                        GW::PathingTrapezoid* pt = sn->trapezoid;

                        if (IsOnPathingTrapezoid(pt, point)) {
                            if (resolved_layer) *resolved_layer = static_cast<uint32_t>(i);
                            return pt;
                        }

                        float d1 = MathUtil::getDistanceFromLine({pt->XTL, pt->YT}, {pt->XTR, pt->YT}, point);
                        float d2 = MathUtil::getDistanceFromLine({pt->XTR, pt->YT}, {pt->XBR, pt->YB}, point);
                        float d3 = MathUtil::getDistanceFromLine({pt->XBR, pt->YB}, {pt->XBL, pt->YB}, point);
                        float d4 = MathUtil::getDistanceFromLine({pt->XBL, pt->YB}, {pt->XTL, pt->YT}, point);
                        float d = std::min({d1, d2, d3, d4});
                        if (closest_dist > d) {
                            closest_dist = d;
                            closest = pt;
                            closest_layer = static_cast<uint32_t>(i);
                        }
                        break;
                    }
                }
            }
        }

        if (resolved_layer) *resolved_layer = closest_layer;
        return closest;
    }

    GW::PathingTrapezoid* FindClosestPositionOnTrapezoid(GW::GamePos& point, const Pathing::PathingMapData* mapData)
    {
        if (!mapData || mapData->planes.empty()) return nullptr;

        uint32_t resolved_layer = point.zplane;
        GW::PathingTrapezoid* pt = FindClosestTrapezoid(point, mapData, &resolved_layer);
        if (pt) {
            point.zplane = resolved_layer;
            return pt;
        }

        float closest_dist = std::numeric_limits<float>::infinity();
        GW::PathingTrapezoid* closest = nullptr;

        for (size_t z = 0; z < mapData->planes.size(); ++z) {
            const auto& m = mapData->planes[z];
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
                    point.zplane = static_cast<uint32_t>(z);
                }
            }
        }
        if (!closest) return nullptr;

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
        else {
            p0 = {closest->XBL, closest->YB};
            p1 = {closest->XTL, closest->YT};
        }

        auto ba = p1 - p0, pa = point - p0;
        float h = std::clamp(Dot(pa, ba) / Dot(ba, ba), 0.0f, 1.0f);
        point = p0 + h * ba;

        return closest;
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
            case 0xe723:  // Prophecies
            case 0x858b:  // Prophecies
            case 0x1c533:
            case 0x5e77a:
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
        GW::Vec2f funnel[2];            // 16 bytes
        uint32_t visited_checkpoint;    // 4 bytes
        uint16_t blocked_idx;           // 2 bytes - index into BlockedPlanesPool
        Portal::id next;                // 2 bytes
    };  // 24 bytes total

    struct BlockedPlanesPool {
        BlockedPlaneBitset* data = nullptr;
        uint16_t size = 0;
        uint16_t capacity = 0;

        BlockedPlanesPool() = default;
        ~BlockedPlanesPool() { delete[] data; }
        BlockedPlanesPool(const BlockedPlanesPool&) = delete;
        BlockedPlanesPool& operator=(const BlockedPlanesPool&) = delete;

        void init(uint16_t cap)
        {
            if (capacity < cap) {
                delete[] data;
                data = new BlockedPlaneBitset[cap];
                capacity = cap;
            }
            size = 0;
        }

        __forceinline void reset() { size = 0; }

        __forceinline uint16_t alloc_empty()
        {
            if (size >= capacity) grow();
            uint16_t idx = size++;
            data[idx].reset();
            return idx;
        }

        __forceinline uint16_t alloc_with_bit(uint16_t src, size_t bit)
        {
            if (size >= capacity) grow();
            uint16_t idx = size++;
            data[idx] = data[src];
            data[idx].set(bit, true);
            return idx;
        }

        void grow()
        {
            uint16_t new_cap = capacity ? capacity * 2 : 4096;
            auto* new_data = new BlockedPlaneBitset[new_cap];
            if (data) {
                memcpy(new_data, data, size * sizeof(BlockedPlaneBitset));
                delete[] data;
            }
            data = new_data;
            capacity = new_cap;
        }
    };

    struct VisitedState {
        uint16_t* visited = nullptr;  // flat array indexed by portal id
        Portal::id* stack = nullptr;  // rollback history stack
        size_t stack_size = 0;
        size_t stack_capacity = 0;
        size_t visited_count = 0;
        uint16_t stamp = 1;

        VisitedState() = default;
        ~VisitedState() { delete[] visited; delete[] stack; }
        VisitedState(const VisitedState&) = delete;
        VisitedState& operator=(const VisitedState&) = delete;

        void init(size_t n)
        {
            if (visited_count < n) {
                delete[] visited;
                visited = new uint16_t[n]();
                visited_count = n;
                stamp = 1; // only reset stamp on fresh allocation
            }
            if (stack_capacity == 0) {
                stack_capacity = 4096;
                stack = new Portal::id[stack_capacity];
            }
            stack_size = 0;
        }

        __forceinline bool is_visited(Portal::id id) const { return visited[static_cast<uint16_t>(id)] == stamp; }

        __forceinline void visit(Portal::id id)
        {
            if (visited[static_cast<uint16_t>(id)] != stamp) {
                visited[id] = stamp;
                stack[stack_size++] = id;
            }
        }

        __forceinline size_t checkpoint() const { return stack_size; }

        __forceinline void rollback(size_t cp)
        {
            while (stack_size > cp) {
                visited[stack[--stack_size]] = 0;
            }
        }
    };


    struct VisitedPoints {
        uint32_t* gen = nullptr;
        size_t count = 0;
        uint32_t cur = 1;

        VisitedPoints() = default;
        ~VisitedPoints() { delete[] gen; }
        VisitedPoints(const VisitedPoints&) = delete;
        VisitedPoints& operator=(const VisitedPoints&) = delete;

        void init(size_t n)
        {
            if (count < n) {
                delete[] gen;
                gen = new uint32_t[n]();
                count = n;
            }
            cur = 1;
        }

        __forceinline void reset()
        {
            ++cur;
            if (cur == 0) { // overflow safety
                memset(gen, 0, count * sizeof(uint32_t));
                cur = 1;
            }
        }

        __forceinline bool test_and_set(PointId id)
        {
            if (gen[id] == cur) return false;
            gen[id] = cur;
            return true;
        }
    };

    struct Impl {
        Impl() : m_msd(), m_visGraph(), points(), portals(), pt_portal_map(), portal_pt_map(), portal_portal_map(), tmp_portal_pt_map(), ptneighbours(), m_teleports(), travel_portals(), m_teleportGraph(), m_mapData() {}

        ~Impl() = default;

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
        std::vector<DefferedTeleport> m_defferedPortalLinks;
        // MapIDs sharing this map's file_hash — needed to collect static teleport
        // data during a (possibly deferred) full build. Stored so a lightweight
        // map can be upgraded later without the caller re-supplying them.
        std::vector<GW::Constants::MapID> m_all_map_ids;
        volatile bool m_terminateThread = false;

        // Runtime/tmp vars that would otherwise have been static for cca - maybe add mutex?
        std::unordered_map<const GW::PathingTrapezoid*, bool> GetTrapezoidNeighbours_visited;
        std::vector<Explore> GetTrapezoidNeighbours_to_explore;
        std::vector<Neighbour> isNeighbourOf_neighbours;

        // Reusable AStar::Search buffers (avoid per-call heap allocations)
        struct SearchBuffers {
            float* cost_so_far = nullptr;
            PointId* came_from = nullptr;
            bool* closed = nullptr;
            int32_t* goal_edge_idx = nullptr; // per-point index into goal_edges, -1 if no goal edge from this point
            size_t buf_capacity = 0;

            // Binary heap priority queue on raw arrays. g_cost is recovered from cost_so_far[id]
            // after pop, so it doesn't need to live in the heap entry — keeps the entry at 8 bytes.
            struct PQEntry { float priority; PointId id; };
            PQEntry* pq_data = nullptr;
            size_t pq_size = 0;
            size_t pq_capacity = 0;

            // VisGraphInsertPoint reusable state
            node* insert_open = nullptr;
            VisitedState insert_visited;
            BlockedPlanesPool insert_bp_pool;

            // Per-query visibility edges for virtual start/goal (no m_visGraph mutation)
            std::vector<PointVisElement> start_edges;
            std::vector<PointVisElement> goal_edges;

            ~SearchBuffers() { delete[] cost_so_far; delete[] came_from; delete[] closed; delete[] goal_edge_idx; delete[] pq_data; delete[] insert_open; }

            void init_buffers(size_t n)
            {
                if (buf_capacity < n) {
                    delete[] cost_so_far;
                    delete[] came_from;
                    delete[] closed;
                    delete[] goal_edge_idx;
                    cost_so_far = new float[n];
                    came_from = new PointId[n];
                    closed = new bool[n];
                    goal_edge_idx = new int32_t[n];
                    buf_capacity = n;
                }
                if (pq_capacity < n) {
                    delete[] pq_data;
                    pq_data = new PQEntry[n];
                    pq_capacity = n;
                }
                pq_size = 0;
                if (!insert_open) {
                    insert_open = new node[2000];
                }
            }

            void reset(size_t n, PointId start_id)
            {
                for (size_t i = 0; i < n; ++i)
                    cost_so_far[i] = INFINITY;
                cost_so_far[start_id] = 0.0f;
                memset(closed, 0, n * sizeof(bool));
                memset(goal_edge_idx, 0xFF, n * sizeof(int32_t)); // fill with -1
                pq_size = 0;
            }

            // Lazy deletion means A* can push a node once per cost improvement, so worst-case
            // heap size isn't bounded by n — pq_push grows on demand instead of asserting.
            void pq_grow()
            {
                const size_t new_cap = pq_capacity ? pq_capacity * 2 : 64;
                auto* new_data = new PQEntry[new_cap];
                if (pq_data) {
                    memcpy(new_data, pq_data, pq_size * sizeof(PQEntry));
                    delete[] pq_data;
                }
                pq_data = new_data;
                pq_capacity = new_cap;
            }

            // Min-heap operations
            __forceinline void pq_push(float priority, PointId id)
            {
                if (pq_size == pq_capacity) pq_grow();
                size_t i = pq_size++;
                pq_data[i] = {priority, id};
                // sift up
                while (i > 0) {
                    size_t parent = (i - 1) >> 1;
                    if (pq_data[parent].priority <= pq_data[i].priority) break;
                    PQEntry tmp = pq_data[parent];
                    pq_data[parent] = pq_data[i];
                    pq_data[i] = tmp;
                    i = parent;
                }
            }

            __forceinline PQEntry pq_pop()
            {
                PQEntry top = pq_data[0];
                pq_data[0] = pq_data[--pq_size];
                // sift down
                size_t i = 0;
                for (;;) {
                    size_t left = (i << 1) + 1;
                    size_t right = left + 1;
                    size_t smallest = i;
                    if (left < pq_size && pq_data[left].priority < pq_data[smallest].priority)
                        smallest = left;
                    if (right < pq_size && pq_data[right].priority < pq_data[smallest].priority)
                        smallest = right;
                    if (smallest == i) break;
                    PQEntry tmp = pq_data[i];
                    pq_data[i] = pq_data[smallest];
                    pq_data[smallest] = tmp;
                    i = smallest;
                }
                return top;
            }

            __forceinline bool pq_empty() const { return pq_size == 0; }
        } m_searchBufs;

        Pathing::PathingMapData m_mapData;

        size_t GetPathNodesSize() const { return m_mapData.pathNodeSize; }
        bool HasMapData() const { return !m_mapData.planes.empty(); }
        size_t GetMapCount() const { return m_mapData.planes.size(); }
        const Pathing::CopiedPathingMap& GetMap(size_t index) const { return m_mapData.planes[index]; }

        void createPortalPair(const GW::PathingTrapezoid* pt1, uint8_t pt1_layer, const GW::PathingTrapezoid* pt2, uint8_t pt2_layer, GW::Vec2f p1, bool p1_viability, GW::Vec2f p2, bool p2_viability, Edge edge)
        {
            auto id = static_cast<Portal::id>(portals.size());
            auto p_id = static_cast<PointId>(points.size());

            points.emplace_back(p1, static_cast<PointId>(points.size()), id, id + 1, p1_viability);

            points.emplace_back(p2, static_cast<PointId>(points.size()), id, id + 1, p2_viability);

            portals.emplace_back(id, id + 1, p_id, p_id + 1, pt1, pt1_layer, edge);

            portals.emplace_back(id + 1, id, p_id + 1, p_id, pt2, pt2_layer, (Edge)(((uint32_t)edge + 2u) % 4u));

            if (pt1->id < pt_portal_map.size()) {
                pt_portal_map[pt1->id].push_back(id);
            }
            if (pt2->id < pt_portal_map.size()) {
                pt_portal_map[pt2->id].push_back(id + 1);
            }
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
                                    neighbours.emplace_back(*it, (uint8_t)portal->portal_plane, Edge::right);
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
            PATH_LOG_INFO("[Portals] trapezoids=%zu, portals=%zu, points=%zu",
                GetPathNodesSize(), portals.size(), points.size());
            PATH_LOG_INFO("[Portals] viable_points=%d",
                (int)std::ranges::count_if(points, [](const auto& point) { return point.is_viable; }));
#endif
        }

        // Generate distance graph among teleports
        void GenerateTeleportGraph()
        {
            if (m_terminateThread) return;

            using namespace MapSpecific;

            m_defferedPortalLinks.clear();
            for (const auto& teleport : m_teleports) {
                auto pt = Pathing::FindClosestTrapezoid(teleport.m_enter, &m_mapData);
                if (!pt) continue;
                auto& enter_id = createSinglePointPortal(pt, teleport.m_enter);

                pt = Pathing::FindClosestTrapezoid(teleport.m_exit, &m_mapData);
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

        static constexpr size_t OPEN_CAPACITY = 2000;

        inline void process_portal(node* open, size_t& sp, VisitedState& visited, BlockedPlanesPool& bp_pool, const Portal& portal, Portal::id other_portal_id)
        {
            size_t checkpoint = visited.checkpoint();

            visited.visit(portal.m_id);
            visited.visit(other_portal_id);

            // All siblings share the same blocked_planes - allocate once
            uint16_t bp_idx = bp_pool.alloc_empty();
            if (portal.m_pt_layer) bp_pool.data[bp_idx].set(portal.m_pt_layer, true);

            for (auto pid : portal_portal_map[portal.m_id]) {
                if (pid == portal.m_id || pid == other_portal_id) continue;

                if (sp >= OPEN_CAPACITY) return; // safety guard

                node& n = open[sp++];
                n.next = pid;
                n.visited_checkpoint = static_cast<uint32_t>(checkpoint);
                n.blocked_idx = bp_idx;

                n.funnel[0] = points[portals[pid].m_point[0]].m_pos;
                n.funnel[1] = points[portals[pid].m_point[1]].m_pos;
            }
        }

        inline void process_point(node* open, size_t& sp, VisitedState& visited, BlockedPlanesPool& bp_pool, const Point& point)
        {
            process_portal(open, sp, visited, bp_pool, portals[point.m_portals[0]], point.m_portals[1]);

            if (point.m_portals[0] != point.m_portals[1]) {
                process_portal(open, sp, visited, bp_pool, portals[point.m_portals[1]], point.m_portals[0]);
            }
        }

        Point& createSinglePointPortal(const GW::PathingTrapezoid* pt, const GW::GamePos& gp)
        {
            auto point_id = static_cast<PointId>(points.size());
            auto id = static_cast<Portal::id>(portals.size());
            points.emplace_back(gp, point_id, id, id, true);

            portals.emplace_back(id, id, point_id, point_id, pt, static_cast<uint8_t>(gp.zplane), Edge::top);

            if (id < portal_pt_map.size()) {
                portal_pt_map[id] = pt;
            }

            if (pt && pt->id < pt_portal_map.size()) {
                for (const auto& pid : pt_portal_map[pt->id]) {
                    if (id < portal_portal_map.size() && pid < portal_portal_map.size()) {
                        portal_portal_map[id].push_back(pid);
                        portal_portal_map[pid].push_back(id);
                    }
                }
                pt_portal_map[pt->id].push_back(id);
            }

            return points[point_id];
        }

        // Compute all visibility edges from `from_pos` (located in `from_trap`) to real visgraph points,
        // writing them to `out_edges` without mutating m_visGraph / portals / points / portal maps.
        // If `extra_pos` is non-null, also emit edges to `extra_id` whenever `extra_pos` is found
        // visible from `from_pos` (extra_pos is assumed to be in `extra_trap`). Used to detect direct
        // LOS from start to goal without inserting a virtual portal for goal.
        void ComputeVisibleEdges(node* open, VisitedState& visited, BlockedPlanesPool& bp_pool,
                                 const GW::GamePos& from_pos, const GW::PathingTrapezoid* from_trap,
                                 std::vector<PointVisElement>& out_edges,
                                 const GW::GamePos* extra_pos, const GW::PathingTrapezoid* extra_trap, PointId extra_id)
        {
            out_edges.clear();
            visited.init(portals.size());
            bp_pool.init(20004);

            if (!from_trap) return;

            size_t sp = 0;
            visited.stack_size = 0;
            visited.stamp++;
            bp_pool.reset();

            // Same-trapezoid shortcut: extra_pos is directly visible (no portal traversal needed).
            if (extra_pos && extra_trap == from_trap) {
                const uint16_t bp_idx = bp_pool.alloc_empty();
                out_edges.emplace_back(GW::GetDistance(from_pos, *extra_pos), bp_pool.data[bp_idx], extra_id);
            }

            // Seed DFS from each portal adjacent to from_trap (replaces the single-point-portal trick).
            const uint16_t initial_bp = bp_pool.alloc_empty();
            for (auto pid : pt_portal_map[from_trap->id]) {
                if (sp >= OPEN_CAPACITY) break;
                node& n = open[sp++];
                n.next = pid;
                n.visited_checkpoint = 0;
                n.blocked_idx = initial_bp;
                n.funnel[0] = points[portals[pid].m_point[0]].m_pos;
                n.funnel[1] = points[portals[pid].m_point[1]].m_pos;
            }

            int cnt = 20000;
            while (sp && --cnt) {
                node cur = open[--sp];
                visited.rollback(cur.visited_checkpoint);

                const auto& portal = portals[cur.next];
                if (visited.is_visited(portal.m_id)) continue;
                visited.visit(portal.m_id);

                uint16_t bp_idx = cur.blocked_idx;
                if (portal.m_pt_layer) {
                    bp_idx = bp_pool.alloc_with_bit(cur.blocked_idx, portal.m_pt_layer);
                }

                const auto& p0 = points[portal.m_point[0]];
                const auto& p1 = points[portal.m_point[1]];

                auto& f0 = cur.funnel[0];
                auto& f1 = cur.funnel[1];

                auto fl = f0 - from_pos;
                auto fr = f1 - from_pos;
                auto nl = p0.m_pos - from_pos;
                auto nr = p1.m_pos - from_pos;

                float fl_nl = Cross(fl, nl);
                float fr_nr = Cross(fr, nr);
                float fl_nr = Cross(fl, nr);
                float fr_nl = Cross(fr, nl);

                constexpr float tolerance = 1.0f;
                if (fr_nl < -tolerance || fl_nr > tolerance) continue;

                if (fl_nl <= tolerance) {
                    if (p0.is_viable) {
                        const float dist = GW::GetDistance(from_pos, p0.m_pos);
                        out_edges.emplace_back(dist, bp_pool.data[bp_idx], p0.m_id);
                    }
                    f0 = p0.m_pos;
                }
                if (fr_nr >= -tolerance) {
                    if (p1.is_viable) {
                        const float dist = GW::GetDistance(from_pos, p1.m_pos);
                        out_edges.emplace_back(dist, bp_pool.data[bp_idx], p1.m_id);
                    }
                    f1 = p1.m_pos;
                }

                // Extra-point visibility: if this portal opens into extra_trap, test extra_pos against
                // the (narrowed) funnel. Emits at most one entry per portal chain into extra_trap.
                if (extra_pos && portal_pt_map[portal.m_other_id] == extra_trap) {
                    const auto ng = *extra_pos - from_pos;
                    const float fl_ng = Cross(f0 - from_pos, ng);
                    const float fr_ng = Cross(f1 - from_pos, ng);
                    if (fl_ng <= tolerance && fr_ng >= -tolerance) {
                        out_edges.emplace_back(GW::GetDistance(from_pos, *extra_pos), bp_pool.data[bp_idx], extra_id);
                    }
                }

                const size_t cp = visited.checkpoint();
                visited.visit(portal.m_other_id);

                for (auto pid : portal_portal_map[portal.m_other_id]) {
                    if (visited.is_visited(pid)) continue;
                    if (sp >= OPEN_CAPACITY) break;
                    node& child = open[sp++];
                    child.funnel[0] = cur.funnel[0];
                    child.funnel[1] = cur.funnel[1];
                    child.blocked_idx = bp_idx;
                    child.next = pid;
                    child.visited_checkpoint = static_cast<uint32_t>(cp);
                }
            }
        }

        void VisGraphInsertPoint(node* open, VisitedState& visited, BlockedPlanesPool& bp_pool, const Point& point)
        {
            visited.init(portals.size());
            bp_pool.init(20004);

            if (!point.is_viable) return;

            size_t sp = 0;
            visited.stack_size = 0;
            visited.stamp++;
            bp_pool.reset();

            process_point(open, sp, visited, bp_pool, point);

            int cnt = 20000;
            const auto& p = point;
            while (sp && --cnt) {
                node cur = open[--sp];
                visited.rollback(cur.visited_checkpoint); // rollback to this node's state

                const auto& portal = portals[cur.next];
                if (visited.is_visited(portal.m_id)) continue;

                visited.visit(portal.m_id);

                uint16_t bp_idx = cur.blocked_idx;
                if (portal.m_pt_layer) {
                    bp_idx = bp_pool.alloc_with_bit(cur.blocked_idx, portal.m_pt_layer);
                }

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
                        m_visGraph[p.m_id].emplace_back(dist, bp_pool.data[bp_idx], p0.m_id);
                        m_visGraph[p0.m_id].emplace_back(dist, bp_pool.data[bp_idx], p.m_id);
                    }
                    f0 = p0.m_pos;
                }

                if (fr_nr >= -tolerance) {
                    if (p1.is_viable) {
                        float dist = GW::GetDistance(p.m_pos, p1.m_pos);
                        m_visGraph[p.m_id].emplace_back(dist, bp_pool.data[bp_idx], p1.m_id);
                        m_visGraph[p1.m_id].emplace_back(dist, bp_pool.data[bp_idx], p.m_id);
                    }
                    f1 = p1.m_pos;
                }

                size_t cp = visited.checkpoint();
                visited.visit(portal.m_other_id);

                for (auto pid : portal_portal_map[portal.m_other_id]) {
                    if (visited.is_visited(pid)) continue;

                    if (sp >= OPEN_CAPACITY) break;

                    node& child = open[sp++];
                    child.funnel[0] = cur.funnel[0];
                    child.funnel[1] = cur.funnel[1];
                    child.blocked_idx = bp_idx;
                    child.next = pid;
                    child.visited_checkpoint = static_cast<uint32_t>(cp);
                }
            }
        }

#pragma optimize("gty", on) // Enable optimizations

        struct VisGraphThreadStats {
            int viable_count = 0;
            int64_t dfs_nodes_total = 0;
            int max_dfs_depth = 0;
            int edges_found = 0;
            int funnel_culled = 0;
        };

        void VisGraphWorker(int begin, int end, [[maybe_unused]] VisGraphThreadStats& stats)
        {
            node* open = new node[OPEN_CAPACITY];
            size_t sp = 0;

            VisitedState visited;     // portals (rollback)
            VisitedPoints vis_points; // points (per source)
            BlockedPlanesPool bp_pool;
            visited.init(portals.size());
            vis_points.init(points.size());
            bp_pool.init(20004);

            for (int i = begin; i < end; ++i) {
                if (m_terminateThread) break;
                const auto& point = points[i];
                if (!point.is_viable) continue;

#ifdef DEBUG_PATHING
                stats.viable_count++;
#endif

                vis_points.reset();
                bp_pool.reset();

                sp = 0;
                visited.stack_size = 0;
                visited.stamp++;

                process_point(open, sp, visited, bp_pool, point);

                int cnt = 20000;
                const auto& p = point;
                while (sp && --cnt) {
                    node cur = open[--sp];  // 24-byte copy

                    visited.rollback(cur.visited_checkpoint); // rollback to this node's state

                    const auto& portal = portals[cur.next];
                    if (visited.is_visited(portal.m_id)) continue;

                    visited.visit(portal.m_id);

#ifdef DEBUG_PATHING
                    stats.dfs_nodes_total++;
#endif

                    // Only allocate new pool entry when layer actually changes
                    uint16_t bp_idx = cur.blocked_idx;
                    if (portal.m_pt_layer) {
                        bp_idx = bp_pool.alloc_with_bit(cur.blocked_idx, portal.m_pt_layer);
                    }

                    const auto& p0 = points[portal.m_point[0]];
                    const auto& p1 = points[portal.m_point[1]];

                    auto& f0 = cur.funnel[0];
                    auto& f1 = cur.funnel[1];

                    auto fl = f0 - p.m_pos;       // funnel left direction
                    auto fr = f1 - p.m_pos;       // funnel right direction
                    auto nl = p0.m_pos - p.m_pos; // portal left direction
                    auto nr = p1.m_pos - p.m_pos; // portal right direction

                    constexpr float tolerance = 1.0f;
                    if (Cross(fr, nl) < -tolerance || Cross(fl, nr) > tolerance) {
#ifdef DEBUG_PATHING
                        stats.funnel_culled++;
#endif
                        continue;
                    }

                    if (Cross(fl, nl) <= tolerance) {
                        if (p0.is_viable && vis_points.test_and_set(p0.m_id)) {
                            float dist = GW::GetDistance(p.m_pos, p0.m_pos);
                            m_visGraph[p.m_id].emplace_back(dist, bp_pool.data[bp_idx], p0.m_id);
#ifdef DEBUG_PATHING
                            stats.edges_found++;
#endif
                        }
                        f0 = p0.m_pos;
                    }

                    if (Cross(fr, nr) >= -tolerance) {
                        if (p1.is_viable && vis_points.test_and_set(p1.m_id)) {
                            float dist = GW::GetDistance(p.m_pos, p1.m_pos);
                            m_visGraph[p.m_id].emplace_back(dist, bp_pool.data[bp_idx], p1.m_id);
#ifdef DEBUG_PATHING
                            stats.edges_found++;
#endif
                        }
                        f1 = p1.m_pos;
                    }

                    uint32_t checkpoint = static_cast<uint32_t>(visited.checkpoint());
                    visited.visit(portal.m_other_id);

                    for (auto pid : portal_portal_map[portal.m_other_id]) {
                        if (visited.is_visited(pid)) continue;

                        if (sp >= OPEN_CAPACITY) break;

                        node& child = open[sp++];
                        child.funnel[0] = cur.funnel[0];
                        child.funnel[1] = cur.funnel[1];
                        child.blocked_idx = bp_idx;
                        child.next = pid;
                        child.visited_checkpoint = checkpoint;
                    }

#ifdef DEBUG_PATHING
                    if (sp > static_cast<size_t>(stats.max_dfs_depth))
                        stats.max_dfs_depth = static_cast<int>(sp);
#endif
                }
            }
            delete[] open;
        }

        void GenerateVisGraph()
        {
            Timing time(__FUNCTION__);

            PATH_LOG_INFO("[VisGraph] start: points=%d portals=%d deferred=%d teleports=%d",
                (int)points.size(), (int)portals.size(), (int)m_defferedPortalLinks.size(), (int)m_teleports.size());

            m_visGraph.clear();
            m_visGraph.resize(points.size() + 2); // reserve also for start and goal points

            for (auto& v : m_visGraph)
                v.reserve(32);

            const int size = (int)points.size();
            const unsigned int hw_threads = std::thread::hardware_concurrency();
            const int num_threads = std::max(1, std::min((int)hw_threads, std::min(size, 8)));

            if (num_threads <= 1) {
                // Single-threaded path
                VisGraphThreadStats stats;
                VisGraphWorker(0, size, stats);

#ifdef DEBUG_PATHING
                uint32_t total = 0;
                for (const auto& v : m_visGraph)
                    total += static_cast<uint32_t>(v.size());
                PATH_LOG_INFO("[VisGraph] edges=%u, viable=%d/%d, portals=%d (1 thread)",
                    total, stats.viable_count, size, (int)portals.size());
#endif
            }
            else {
                // Multi-threaded path
                std::vector<std::thread> threads;
                std::vector<VisGraphThreadStats> thread_stats(num_threads);
                threads.reserve(num_threads);

                const int chunk = (size + num_threads - 1) / num_threads;
                for (int t = 0; t < num_threads; ++t) {
                    int begin = t * chunk;
                    int end = std::min(begin + chunk, size);
                    if (begin >= end) break;
                    threads.emplace_back(&Impl::VisGraphWorker, this, begin, end, std::ref(thread_stats[t]));
                }

                for (auto& t : threads)
                    t.join();

#ifdef DEBUG_PATHING
                VisGraphThreadStats merged;
                for (const auto& s : thread_stats) {
                    merged.viable_count += s.viable_count;
                    merged.dfs_nodes_total += s.dfs_nodes_total;
                    merged.max_dfs_depth = std::max(merged.max_dfs_depth, s.max_dfs_depth);
                    merged.edges_found += s.edges_found;
                    merged.funnel_culled += s.funnel_culled;
                }

                uint32_t total = 0;
                for (const auto& v : m_visGraph)
                    total += static_cast<uint32_t>(v.size());
                PATH_LOG_INFO("[VisGraph] edges=%u, viable=%d/%d, portals=%d (%d threads)",
                    total, merged.viable_count, size, (int)portals.size(), (int)threads.size());
#endif
            }

            // could not link portals earlier because vis graph is cleared at the beginning of this function.
            for (const auto& dp : m_defferedPortalLinks) {
                if (dp.m_enter >= points.size() || dp.m_exit >= points.size()) {
                    Log::Error("[VisGraph] deferred portal link out of bounds: enter=%d exit=%d points=%d",
                        (int)dp.m_enter, (int)dp.m_exit, (int)points.size());
                    continue;
                }
                auto dist = GW::GetDistance(points[dp.m_enter].m_pos, points[dp.m_exit].m_pos);

                BlockedPlaneBitset bp;
                m_visGraph[dp.m_enter].emplace_back(dist * 0.01f, bp, dp.m_exit);
                if (dp.m_directionality == MapSpecific::Teleport::direction::both_ways) m_visGraph[dp.m_exit].emplace_back(dist * 0.01f, bp, dp.m_enter);
            }
        }
#pragma optimize("", on) // Restore global optimizations to project default

        // Free build-only scratch once the graph is built. None of these are read
        // by AStar::Search / BuildPath, so a cached map need not carry them for its
        // whole lifetime. swap-with-empty actually releases capacity (clear() alone
        // would keep the backing storage).
        void ReleaseBuildScratch()
        {
            decltype(ptneighbours){}.swap(ptneighbours);
            decltype(tmp_portal_pt_map){}.swap(tmp_portal_pt_map);
            std::vector<DefferedTeleport>{}.swap(m_defferedPortalLinks);
            decltype(GetTrapezoidNeighbours_visited){}.swap(GetTrapezoidNeighbours_visited);
            std::vector<Explore>{}.swap(GetTrapezoidNeighbours_to_explore);
            std::vector<Neighbour>{}.swap(isNeighbourOf_neighbours);
        }

        // Full pathing-graph build (teleports + neighbours + portals + visgraph).
        // Shared by the eager worker build and the lazy EnsureFullBuild() upgrade.
        void BuildFullGraph()
        {
            MapSpecific::MapSpecificData msd;
            for (auto id : m_all_map_ids) msd.AddTeleportsForMap(id);
            m_teleports = msd.m_teleports;
            travel_portals.clear();

            GenerateTrapezoidNeighbours();
            GeneratePortals();
            GenerateTeleportGraph();
            GenerateVisGraph();
            ReleaseBuildScratch();
        }

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
        m_constructed_full = true; // live current map: always built eagerly + fully
        const clock_t start = clock();

        // Load map data from game into Impl's PathingMapData
        auto* mapContext = GW::GetMapContext();
        if (mapContext && mapContext->path) {
            Pathing::LoadFromMapContext(mapContext, 0, &mImpl->m_mapData);
        }

        ASSERT(!worker_thread);
        worker_thread = new std::thread([&, start] {
            try {
                LoadMapSpecificData();
                mImpl->GenerateTrapezoidNeighbours();
                mImpl->GeneratePortals();
                mImpl->GenerateTeleportGraph();
                mImpl->GenerateVisGraph();
                mImpl->ReleaseBuildScratch();
                const clock_t stop = clock();
                PATH_LOG_INFO("Processing %s in %d ms", mImpl->m_terminateThread ? "terminated" : "done", stop - start);
                m_full_built = true;
                m_progress = 100;
            }
            catch (const std::bad_alloc&) {
                // Worker-thread OOM during visgraph build. Mark failed and bail without crashing —
                // a thrown exception escaping a std::thread body would call std::terminate().
                Log::Error("[pathing] MilePath worker OOM (game-context build); marking build_failed");
                m_build_failed = true;
            }
            m_processing = false;
            m_done = true;
        });
        worker_thread->detach();
    }

    MilePath::MilePath(Pathing::PathingMapData&& map_data, GW::Constants::MapID map_id, const std::vector<GW::Constants::MapID>& all_map_ids, bool full_build)
    {
        static_assert(sizeof(opaque) >= sizeof(Impl));
        new (opaque) Impl();

        const clock_t start = clock();

        mImpl->m_mapData = std::move(map_data);
        mImpl->m_all_map_ids = all_map_ids.empty() ? std::vector{map_id} : all_map_ids;
        m_constructed_full = full_build;

        // Lightweight load: keep only the raw map data (enough for the OpenTyria
        // trapezoid pathfinder used by route-cost queries). The visibility graph is
        // built on demand by EnsureFullBuild() the first time an AStar walk runs.
        if (!full_build) {
            m_progress = 100; // map data is immediately usable; no worker needed
            return;
        }

        m_processing = true;
        ASSERT(!worker_thread);
        worker_thread = new std::thread([this, start] {
            try {
                mImpl->BuildFullGraph();
                const clock_t stop = clock();
                PATH_LOG_INFO("DAT processing %s in %d ms", mImpl->m_terminateThread ? "terminated" : "done", stop - start);
                m_full_built = true;
                m_progress = 100;
            }
            catch (const std::bad_alloc&) {
                Log::Error("[pathing] MilePath worker OOM (DAT build); marking build_failed");
                m_build_failed = true;
            }
            m_processing = false;
            m_done = true;
        });
        worker_thread->detach();
    }

    MilePath::~MilePath()
    {
        shutdown();
        mImpl->~Impl();
    }

    std::string MilePath::ExportVisGraph() const
    {
        std::string out = "{\n";
        out += "  \"point_count\": " + std::to_string(mImpl->points.size()) + ",\n";
        out += "  \"visgraph_size\": " + std::to_string(mImpl->m_visGraph.size()) + ",\n";
        out += "  \"portal_count\": " + std::to_string(mImpl->portals.size()) + ",\n";
        out += "  \"points\": [\n";
        for (size_t i = 0; i < mImpl->points.size(); i++) {
            const auto& p = mImpl->points[i];
            size_t edges = (i < mImpl->m_visGraph.size()) ? mImpl->m_visGraph[i].size() : 0;
            out += "    {\"id\":" + std::to_string(p.m_id) +
                   ",\"x\":" + std::to_string(p.m_pos.x) +
                   ",\"y\":" + std::to_string(p.m_pos.y) +
                   ",\"viable\":" + (p.is_viable ? "true" : "false") +
                   ",\"edges\":" + std::to_string(edges) +
                   ",\"portals\":[" + std::to_string(p.m_portals[0]) + "," + std::to_string(p.m_portals[1]) + "]";
            if (edges > 0 && edges <= 200) {
                out += ",\"targets\":[";
                for (size_t e = 0; e < edges; e++) {
                    if (e) out += ",";
                    out += std::to_string(mImpl->m_visGraph[i][e].point_id);
                }
                out += "]";
            }
            out += "}";
            if (i + 1 < mImpl->points.size()) out += ",";
            out += "\n";
        }
        out += "  ]\n}\n";
        return out;
    }

    void MilePath::stopProcessing()
    {
        mImpl->m_terminateThread = true;
    }

    const Pathing::PathingMapData* MilePath::GetMapData() const
    {
        return &mImpl->m_mapData;
    }

    void MilePath::EnsureFullBuild()
    {
        if (m_full_built || m_build_failed) return;
        if (m_constructed_full) {
            // Built eagerly on the worker thread — just wait for it to finish.
            while (!m_full_built && !m_build_failed && !mImpl->m_terminateThread) Sleep(10);
            return;
        }
        // Lightweight map: build the graph now, on the calling (worker) thread.
        std::lock_guard lock(m_build_mutex);
        if (m_full_built || m_build_failed) return; // another thread already built it
        try {
            mImpl->BuildFullGraph();
            m_full_built = true;
        }
        catch (const std::bad_alloc&) {
            Log::Error("[pathing] MilePath lazy full build OOM; marking build_failed");
            m_build_failed = true;
        }
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
    Error BuildPath(AStar& astar,
                    const GW::GamePos& start_pos, PointId start_id,
                    const GW::GamePos& goal_pos, PointId goal_id,
                    const PointId* came_from)
    {
        astar.m_path.clear();
        auto* mp = (Impl*)astar.m_path.m_mp->GetImpl();

        astar.m_path.insertPoint(goal_pos);

        PointId curr = came_from[goal_id];
        int count = 0;
        while (curr != start_id) {
            if (count++ > 256) {
                Log::Error("build path failed\n");
                return Error::BuildPathLengthExceeded;
            }
            const auto& p = mp->points[curr];
            const auto zplane = std::max(mp->portals[p.m_portals[0]].m_pt_layer, mp->portals[p.m_portals[1]].m_pt_layer);
            astar.m_path.insertPoint({p.m_pos.x, p.m_pos.y, zplane});
            curr = came_from[curr];
        }

        astar.m_path.insertPoint(start_pos);
        return Error::OK;
    }

    float TeleporterHeuristic(const MapSpecific::Teleports& teleports, const std::vector<MapSpecific::teleport_node>& teleportGraph, const GW::GamePos& start_pos, const GW::GamePos& goal_pos)
    {
        if (teleports.empty()) return 0.0f;

        using namespace MapSpecific;

        float cost = INFINITY;
        const Teleport* ts = nullptr;
        const Teleport* tg = nullptr;
        float dist_start = cost;
        float dist_goal = cost;
        for (const auto& tp : teleports) {
            float dist = GW::GetSquareDistance(start_pos, tp.m_enter);
            if (tp.m_directionality == Teleport::direction::both_ways) dist = std::min(dist, GW::GetSquareDistance(start_pos, tp.m_exit));

            if (dist_start > dist) {
                dist_start = dist;
                ts = &tp;
            }

            dist = GW::GetSquareDistance(goal_pos, tp.m_exit);
            if (tp.m_directionality == Teleport::direction::both_ways) dist = std::min(dist, GW::GetSquareDistance(goal_pos, tp.m_enter));

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

    Error AStar::Search(const GW::GamePos& _start_pos, const GW::GamePos& _goal_pos)
    {
        Timing time(__FUNCTION__);

        // TODO: remove this microsecond phase timing once AStar perf is locked in.
        // Logs `[AStar:<tag>] trap=… insert=… search=… cleanup=… total=…` per call,
        // adds noise to log.txt. Drop t_trap/t_insert/t_search and log_timings calls.
        using clk = std::chrono::high_resolution_clock;
        const auto t_begin = clk::now();
        auto t_trap = t_begin, t_insert = t_begin, t_search = t_begin;
        auto log_timings = [&](const char* tag) {
            const auto t_end = clk::now();
            auto us = [](auto a, auto b) {
                return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
            };
            PATH_LOG_INFO("[AStar:%s] trap=%lldus insert=%lldus search=%lldus cleanup=%lldus total=%lldus",
                tag, us(t_begin, t_trap), us(t_trap, t_insert), us(t_insert, t_search), us(t_search, t_end), us(t_begin, t_end));
        };

        // A walk needs the full visibility graph. Lightweight (route-cost-only) maps
        // are upgraded here, transparently, on first walk. Done before taking
        // pathing_mutex so a slow first-time build doesn't stall other maps' searches.
        m_path.m_mp->EnsureFullBuild();

        std::lock_guard lock(pathing_mutex);

        // Bail early if the MilePath's worker thread aborted with std::bad_alloc; mImpl's data
        // (m_visGraph, points, portals, etc.) is partially built and unsafe to traverse.
        if (m_path.m_mp->build_failed()) return Error::MilePathBuildOOM;

        auto* mp = (Impl*)m_path.m_mp->GetImpl();

        BlockedPlaneBitset current_blocked_planes;
        const Error res = CopyPathingMapBlocks(&mp->m_mapData, &current_blocked_planes);

        if (res != Error::OK) return res;

        auto start_pos = _start_pos;
        auto goal_pos = _goal_pos;
        auto spt = FindClosestPositionOnTrapezoid(start_pos, &mp->m_mapData);
        auto gpt = FindClosestPositionOnTrapezoid(goal_pos, &mp->m_mapData);
        t_trap = clk::now();
        if (!spt) return Error::FailedToFindStartPathingTrapezoid;
        if (!gpt) return Error::FailedToFindGoalPathingTrapezoid;

        auto& sb = mp->m_searchBufs;
        const size_t n_real = mp->points.size();
        const PointId START_ID = static_cast<PointId>(n_real);
        const PointId GOAL_ID = static_cast<PointId>(n_real + 1);
        const size_t n = n_real + 2;
        sb.init_buffers(n);

        // Compute visibility edges without mutating m_visGraph.
        // Start's DFS also tests goal visibility so direct LOS is captured as an edge to GOAL_ID.
        sb.insert_visited.init(mp->portals.size());
        sb.insert_bp_pool.init(20004);
        mp->ComputeVisibleEdges(sb.insert_open, sb.insert_visited, sb.insert_bp_pool,
                                start_pos, spt, sb.start_edges,
                                &goal_pos, gpt, GOAL_ID);
        mp->ComputeVisibleEdges(sb.insert_open, sb.insert_visited, sb.insert_bp_pool,
                                goal_pos, gpt, sb.goal_edges,
                                nullptr, nullptr, 0);
        t_insert = clk::now();

        if (sb.start_edges.empty() || sb.goal_edges.empty()) {
            PATH_LOG_INFO("[AStar] insert: start=(%.0f,%.0f,z%d) trap=%d edges=%zu | goal=(%.0f,%.0f,z%d) trap=%d edges=%zu",
                start_pos.x, start_pos.y, (int)start_pos.zplane, (int)spt->id, sb.start_edges.size(),
                goal_pos.x, goal_pos.y, (int)goal_pos.zplane, (int)gpt->id, sb.goal_edges.size());
            m_path.insertPoint(start_pos);
            m_path.insertPoint(goal_pos);
            m_path.setCost(GW::GetDistance(start_pos, goal_pos));
            m_path.finalize();
            t_search = clk::now();
            log_timings("no-edges");
            return Error::FailedToFinializePath;
        }

        // LOS fast path: start has a direct edge to goal (emitted by ComputeVisibleEdges).
        for (const auto& vis : sb.start_edges) {
            if (vis.point_id != GOAL_ID) continue;
            if ((vis.blocked_planes & current_blocked_planes).none()) {
                m_path.insertPoint(start_pos);
                m_path.insertPoint(goal_pos);
                m_path.setCost(vis.distance);
                m_path.finalize();
                t_search = clk::now();
                log_timings("los");
                return Error::OK;
            }
        }

        // A* search using raw buffers
        float* cost_so_far = sb.cost_so_far;
        PointId* came_from = sb.came_from;
        int32_t* goal_edge_idx = sb.goal_edge_idx;
        sb.reset(n, START_ID);

        // Build O(1) lookup: for each point p, index into goal_edges giving edge p->goal (or -1).
        // Duplicates (different blocked_planes) collapse to the last one — acceptable since paths are
        // re-checked against current_blocked_planes in the relax step.
        for (size_t i = 0; i < sb.goal_edges.size(); ++i) {
            const auto pid = sb.goal_edges[i].point_id;
            if (pid < n) goal_edge_idx[pid] = static_cast<int32_t>(i);
        }

        came_from[START_ID] = START_ID;
        sb.pq_push(0.0f, START_ID);

        const bool teleports = !mp->m_teleports.empty();
        const bool has_blocked = current_blocked_planes.any(); // skip 2M+ bitset ANDs when no planes blocked
        PointId current = 0;
#ifdef DEBUG_PATHING
        int dbg_nodes_expanded = 0;
        int dbg_edges_examined = 0;
        int dbg_edges_blocked = 0;
        int dbg_edges_relaxed = 0;
        int dbg_pq_max_size = 0;
#endif
        while (!sb.pq_empty()) {
#ifdef DEBUG_PATHING
            if ((int)sb.pq_size > dbg_pq_max_size)
                dbg_pq_max_size = (int)sb.pq_size;
#endif
            auto top = sb.pq_pop();
            current = top.id;
            if (current == GOAL_ID) break;

            if (sb.closed[current]) continue;
            sb.closed[current] = true;

#ifdef DEBUG_PATHING
            dbg_nodes_expanded++;
#endif
            const float cost_current = cost_so_far[current];

            // Primary edges: start uses local start_edges, every other node uses m_visGraph.
            const auto& primary_edges = (current == START_ID) ? sb.start_edges : mp->m_visGraph[current];
            for (const auto& vis : primary_edges) {
#ifdef DEBUG_PATHING
                dbg_edges_examined++;
#endif
                if (has_blocked && (vis.blocked_planes & current_blocked_planes).any()) {
#ifdef DEBUG_PATHING
                    dbg_edges_blocked++;
#endif
                    continue;
                }
                const float new_cost = cost_current + vis.distance;
                if (new_cost >= cost_so_far[vis.point_id]) continue;
                cost_so_far[vis.point_id] = new_cost;
                came_from[vis.point_id] = current;
#ifdef DEBUG_PATHING
                dbg_edges_relaxed++;
#endif
                float h;
                if (vis.point_id == GOAL_ID) {
                    h = 0.0f;
                }
                else {
                    const auto& p = mp->points[vis.point_id];
                    h = GetDistance(p.m_pos, goal_pos);
                    if (teleports) {
                        const float tp_cost = TeleporterHeuristic(mp->m_teleports, mp->m_teleportGraph, p.m_pos, goal_pos);
                        h = std::min(h, tp_cost);
                    }
                }
                sb.pq_push(new_cost + h, vis.point_id);
            }

            // Implicit edge to goal via visibility (goal_edges is symmetric; O(1) lookup).
            if (current != START_ID) {
                const int32_t ge_idx = goal_edge_idx[current];
                if (ge_idx >= 0) {
                    const auto& vis = sb.goal_edges[ge_idx];
#ifdef DEBUG_PATHING
                    dbg_edges_examined++;
#endif
                    if (!has_blocked || (vis.blocked_planes & current_blocked_planes).none()) {
                        const float new_cost = cost_current + vis.distance;
                        if (new_cost < cost_so_far[GOAL_ID]) {
                            cost_so_far[GOAL_ID] = new_cost;
                            came_from[GOAL_ID] = current;
#ifdef DEBUG_PATHING
                            dbg_edges_relaxed++;
#endif
                            sb.pq_push(new_cost, GOAL_ID);
                        }
                    }
#ifdef DEBUG_PATHING
                    else dbg_edges_blocked++;
#endif
                }
            }
        }

        if (current == GOAL_ID) {
            m_path.setCost(cost_so_far[current]);
            if (BuildPath(*this, start_pos, START_ID, goal_pos, GOAL_ID, came_from) == Error::OK) {
                m_path.finalize();
            }
        }

#ifdef DEBUG_PATHING
        PATH_LOG_INFO("[AStar] %s s=(%.0f,%.0f,z%d)t%d e%zu g=(%.0f,%.0f,z%d)t%d e%zu cost=%.0f pts=%d expanded=%d",
            m_path.ready() ? "OK" : "FAIL",
            start_pos.x, start_pos.y, (int)start_pos.zplane, (int)spt->id, sb.start_edges.size(),
            goal_pos.x, goal_pos.y, (int)goal_pos.zplane, (int)gpt->id, sb.goal_edges.size(),
            m_path.cost(), (int)m_path.points().size(),
            dbg_nodes_expanded);
#endif
        t_search = clk::now();

        log_timings(m_path.ready() ? "ok" : "fail");
        return m_path.ready() ? Error::OK : Error::FailedToFinializePath;
    }
} // namespace Pathing
