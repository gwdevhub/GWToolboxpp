#include <thread>
#include <algorithm>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/GWCA.h>
#include <Logger.h>

#include "MathUtility.h"
#include "Pathing.h"

namespace Pathing {
    using namespace GW;
    using namespace MathUtil;

    SimplePT::SimplePT(const GW::PathingTrapezoid& pt, uint32_t layer) :
        id(pt.id),
        a(pt.XTL, pt.YT), b(pt.XBL, pt.YB),
        c(pt.XBR, pt.YB), d(pt.XTR, pt.YT),
        layer(layer) { }

    SimplePT::adjacentSide SimplePT::Touching(const SimplePT& rhs) const {
        if (a.x != d.x && rhs.b.x != rhs.c.x && a.y == b.y) {
            //a bot, b top
            if (collinear(a, d, rhs.b, rhs.c)) {
                return adjacentSide::aBottom_bTop;
            }
        }
        if (b.x != c.x && rhs.a.x != rhs.d.x && b.y == rhs.a.y) {
            //a top, b bot
            if (collinear(c, b, rhs.d, rhs.a)) {
                return adjacentSide::aTop_bBottom;
            }
        }

        //a right, b left
        if (collinear(a, b, rhs.c, rhs.d)) {
            return adjacentSide::aRight_bLeft;
        }
        //a left, b right
        if (collinear(d, c, rhs.b, rhs.a)) {
            return adjacentSide::aLeft_bRight;
        }
        return adjacentSide::none;
    }

	//Gets height of map at current position and layer
    static float height (const Vec2f &p, int layer){
        float height;
        Map::QueryAltitude(GamePos(p.x, p.y, layer), 5, height);
        return height;
    }

    SimplePT::adjacentSide SimplePT::TouchingHeight(const SimplePT &rhs, float max_height_diff) const {
        if (a.x != d.x && rhs.b.x != rhs.c.x && a.y == rhs.b.y) {
            //a bot, b top
            if (collinear(a, d, rhs.b, rhs.c)) {
                float dh = (fabsf(height(a, layer) - height(rhs.b, rhs.layer)) + fabsf(height(d, layer) - height(rhs.c, rhs.layer))) / 2.0f;
                if (dh > max_height_diff)
                    return adjacentSide::none;
                return adjacentSide::aBottom_bTop;
            }
        }
        if (b.x != c.x && rhs.a.x != rhs.d.x && b.y == rhs.a.y) {
            //a top, b bot
            if (collinear(c, b, rhs.d, rhs.a)) {
                float dh = (fabsf(height(b, layer) - height(rhs.a, rhs.layer)) + fabsf(height(c, layer) - height(rhs.d, rhs.layer))) / 2.0f;
                if (dh > max_height_diff)
                    return adjacentSide::none;
                return adjacentSide::aTop_bBottom;
            }
        }

        //a right, b left
        if (collinear(a, b, rhs.c, rhs.d)) {
            float dh = (fabsf(height(a, layer) - height(rhs.d, rhs.layer)) + fabsf(height(b, layer) - height(rhs.c, rhs.layer))) / 2.0f;
            if (dh > max_height_diff)
                return adjacentSide::none;
            return adjacentSide::aRight_bLeft;
        }
        //a left, b right
        if (collinear(d, c, rhs.b, rhs.a)) {
            float dh = (fabsf(height(c, layer) - height(rhs.b, rhs.layer)) + fabsf(height(d, layer) - height(rhs.a, rhs.layer))) / 2.0f;
            if (dh > max_height_diff)
                return adjacentSide::none;
            return adjacentSide::aLeft_bRight;
        }
        return adjacentSide::none;
    }


    AABB::AABB(const SimplePT& t) :
        m_id(0), m_t(&t) {
        float min_x = std::min(t.b.x, t.a.x);
        float max_x = std::max(t.c.x, t.d.x);
        m_pos = { (min_x + max_x) / 2, (t.a.y + t.b.y) / 2 };
        m_half = { (max_x - min_x) / 2, (t.a.y - t.b.y) / 2 };
    }

    //point
    bool AABB::intersect(const Vec2f& rhs) const {
        auto d = rhs - m_pos;
        float px = m_half.x - fabsf(d.x);
        if (px <= 0.0f) return false;

        float py = m_half.y - fabsf(d.y);
        if (py <= 0.0f) return false;
        return true;
    }

    //circle
    bool AABB::intersect(const Vec2f& rhs, float radius) const {
        auto d = rhs - m_pos;
        float px = m_half.x + radius - fabsf(d.x);
        if (px <= 0.0f) return false;

        float py = m_half.y + radius - fabsf(d.y);
        if (py <= 0.0f) return false;
        return true;
    }

    //segment
    bool AABB::intersect(const Vec2f& a, const Vec2f& b, Vec2f padding) const {
        Vec2f dist = m_half + padding;
        if (a.x > m_pos.x + dist.x && b.x > m_pos.x + dist.x) return false;
        if (a.x < m_pos.x - dist.x && b.x < m_pos.x - dist.x) return false;
        if (a.y > m_pos.y + dist.y && b.y > m_pos.y + dist.y) return false;
        if (a.y < m_pos.y - dist.y && b.y < m_pos.y - dist.y) return false;

        auto delta = b - a;
        auto scale = 1.0f / delta;
        auto sig = sign(scale);
        auto sq = Hadamard(sig, dist);
        auto nearTime = Hadamard(sq, scale);
        auto farTime = Hadamard(sq, scale);

        if (nearTime.x > farTime.y || nearTime.y > farTime.x) {
            return false;
        }
        if (std::max(nearTime.x, nearTime.y) >= 1.0f || std::min(farTime.x, farTime.y) <= 0.0f) {
            return false;
        }
        return true;
    }

    bool AABB::intersect(const AABB& rhs, Vec2f padding) const {
        auto d = rhs.m_pos - m_pos;
        float px = (rhs.m_half.x + m_half.x + padding.x) - fabsf(d.x);
        if (px <= 0.0f) return false;

        float py = (rhs.m_half.y + m_half.y + padding.y) - fabsf(d.y);
        if (py <= 0.0f) return false;
        return true;
    }

    void MilePath::LoadMapSpecificData(MilePath *mp) {
        m_msd = MapSpecific::MapSpecificData(Map::GetMapID());
        mp->m_teleports = m_msd.m_teleports;
    }

    MilePath::MilePath() {
        m_processing = true;
        static volatile clock_t start = clock();
        start = clock();
        LoadMapSpecificData(this);
        GenerateAABBs(this);
        GenerateAABBGraph(this); //not threaded because it relies on gw client Query altitude.
        std::thread([&]() {
            GeneratePoints(this);
            GenerateVisibilityGraph(this);
            GenerateTeleportGraph(this);
            InsertTeleportsIntoVisibilityGraph(this);
            volatile clock_t stop = clock();
            m_processing = false;
            m_done = true;
            m_progress = 100;

            Log::Info("Processing %s\n", m_terminateThread ? "terminated." : "done.");
            Log::Info("processing time: %d ms\n", stop - start);
        }).detach();
    }
        
    MilePath* MilePath::instance() {
        static std::vector<MilePath *> instances;
        static GW::Constants::MapID currentMapId = GW::Constants::MapID::None;
        static GW::Constants::InstanceType currentInstanceType = GW::Constants::InstanceType::Loading;
            
        auto terminateInstance = [](auto *instance) {
            instance->m_terminateThread = true;
            Log::Warning("Terminating current instance.\n");
        };
        auto deleteOldInstances = [](auto *i) {
            if (i && i->m_done) {
                delete i;
                i = nullptr;
                Log::Info("Removing old instances.\n");
                return true;
            }
            return false;
        };

        auto type = GW::Map::GetInstanceType();
        if (type == GW::Constants::InstanceType::Loading && currentInstanceType != type) {
            currentInstanceType = type;
            if (instances.size()) {
                std::for_each(instances.begin(), instances.end(), terminateInstance);
                instances.erase(std::remove_if(instances.begin(), instances.end(), deleteOldInstances), instances.end());
            }
            return nullptr;
        }

        if (type == GW::Constants::InstanceType::Loading)
            return nullptr;

        if (currentMapId != GW::Map::GetMapID()) {
            currentMapId = GW::Map::GetMapID();
            std::for_each(instances.begin(), instances.end(), terminateInstance);
            instances.erase(std::remove_if(instances.begin(), instances.end(), deleteOldInstances), instances.end());

            instances.push_back(new MilePath());
            Log::Info("Created new instance.\n");
        }
        if (instances.empty())
            return nullptr;
        return instances.back();
    }

    MilePath::Portal::Portal(const Vec2f& start, const Vec2f& goal, const AABB* box1, const AABB* box2) :
        m_start(start), m_goal(goal), m_box1(box1), m_box2(box2) {
        Vec2f diff(goal - start);
    }

    bool MilePath::Portal::intersect(const Vec2f& p1, const Vec2f& p2) const {
        return Intersect(m_start, m_goal, p1, p2);
    }

    //Generate distance graph among teleports
    void MilePath::GenerateTeleportGraph(MilePath* mp) {
        if (mp->m_terminateThread) return;

        using namespace MapSpecific;

        mp->m_teleportGraph.clear();
       
        for (size_t i = 0; i < mp->m_teleports.size(); ++i) {
            auto& p1 = mp->m_teleports[i];
            for (size_t j = i; j < mp->m_teleports.size(); ++j) {
                auto& p2 = mp->m_teleports[j];

                if (p1.m_directionality == Teleport::direction::both_ways &&
                    p2.m_directionality == Teleport::direction::both_ways) {
                    float dist = std::min(
                        std::min(GetDistance(p1.m_enter, p2.m_exit), GetDistance(p1.m_exit, p2.m_enter)), 
                        std::min(GetDistance(p1.m_exit, p2.m_exit), GetDistance(p1.m_enter, p2.m_enter)));
                    mp->m_teleportGraph.push_back({ &p1, &p2, dist });
                    if (&p1 == &p2) continue;
                    mp->m_teleportGraph.push_back({ &p2, &p1, dist });
                } else if (p1.m_directionality == Teleport::direction::both_ways) {
                    float dist = std::min(GetDistance(p1.m_enter, p2.m_enter), GetDistance(p1.m_exit, p2.m_enter));
                    mp->m_teleportGraph.push_back({ &p1, &p2, dist });
                    dist = std::min(GetDistance(p2.m_exit, p1.m_enter), GetDistance(p2.m_exit, p1.m_exit));
                    mp->m_teleportGraph.push_back({ &p2, &p1, dist });
                } else if (p2.m_directionality == Teleport::direction::both_ways) {
                    float dist = std::min(GetDistance(p2.m_enter, p1.m_enter), GetDistance(p2.m_exit, p1.m_enter));
                    mp->m_teleportGraph.push_back({ &p2, &p1, dist });
                    dist = std::min(GetDistance(p1.m_exit, p2.m_enter), GetDistance(p1.m_exit, p2.m_exit));
                    mp->m_teleportGraph.push_back({ &p1, &p2, dist });
                } else {
                    float dist = GetDistance(p1.m_exit, p2.m_enter);
                    mp->m_teleportGraph.push_back({ &p1, &p2, dist });
                    if (&p1 == &p2) continue;
                    dist = GetDistance(p2.m_exit, p1.m_enter);
                    mp->m_teleportGraph.push_back({ &p2, &p1, dist });
                }
            }
        }
    }

    MilePath::point MilePath::CreatePoint(const MilePath *mp, const GamePos& pos) {
        MilePath::point point;
        point.pos = pos;
        point.box = MilePath::FindAABB(mp->m_aabbs, pos);
        if (!point.box) {
            return point;
        }
        point.id = 0;
        return point;
    }

    //Generate Axis Aligned Bounding Boxes around trapezoids
    //This is used for quick intersection checks.
    //AABB related stuff could be entirely omitted.
    void MilePath::GenerateAABBs(MilePath* mp) {
        PathingMapArray* map = Map::GetPathingMap();
        MapContext* mapContex = GW::GetMapContext();
        if (!map || !mapContex) return;

        mp->m_aabbs.clear();
        mp->m_aabbs.reserve(mapContex->sub1->h0014[0]); //h0014[0] == total trapezoid count
        mp->m_trapezoids.clear();
        mp->m_trapezoids.reserve(mapContex->sub1->h0014[0]);
            
        for (uint32_t i = 0; i < map->size(); ++i) {
            auto& m = (*map)[i];
            for (uint32_t j = 0; j < m.trapezoid_count; j++) {
                const PathingTrapezoid* t = &m.trapezoids[j];
                if (t->YB == t->YT) continue;
                mp->m_trapezoids.emplace_back( *t, i );
                mp->m_aabbs.emplace_back(mp->m_trapezoids.back());
            }
        }
        std::sort(mp->m_aabbs.begin(), mp->m_aabbs.end(), [](AABB& a, AABB& b) { return a.m_pos.y - a.m_half.y > b.m_pos.y - b.m_half.y; });
        AABB::boxId id = 0;
        for (auto& box : mp->m_aabbs) {
            box.m_id = id++;
        }
    }

    bool MilePath::CreatePortal(MilePath* mp, const AABB* box1, const AABB* box2, const SimplePT::adjacentSide& ts)
    {
        const SimplePT* pt1 = box1->m_t;
        const SimplePT* pt2 = box2->m_t;
        
        //ignore portals smaller than tolerance.
        constexpr float tolerance = 0.001f;
        constexpr float square_tolerance = tolerance * tolerance;

        switch (ts) {
        case SimplePT::adjacentSide::aBottom_bTop: {
            auto a = std::max(pt1->a.x, pt2->b.x);
            auto b = std::min(pt1->d.x, pt2->c.x);
            if (fabsf(a - b) < tolerance) 
                return false;
            Vec2f p1{ a, pt1->a.y };
            Vec2f p2{ b, pt1->a.y };
            mp->m_portals.emplace_back( p1, p2, box1, box2 );
        } break;
        case SimplePT::adjacentSide::aTop_bBottom: {
            auto a = std::max(pt1->b.x, pt2->a.x);
            auto b = std::min(pt1->c.x, pt2->d.x);
            if (fabsf(a - b) < tolerance) 
                return false;
            Vec2f p1{ a, pt1->b.y };
            Vec2f p2{ b, pt1->b.y };
            mp->m_portals.emplace_back( p1, p2, box1, box2 );
        } break;
        case SimplePT::adjacentSide::aLeft_bRight: {
            bool o1 = onSegment(pt1->c, pt2->a, pt1->d);
            bool o2 = onSegment(pt1->c, pt2->b, pt1->d);
            if (o1 && o2) {
                if (GetSquareDistance(pt2->a, pt2->b) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt2->a, pt2->b, box1, box2 );
            }
            else if (o1) {
                if (GetSquareDistance(pt2->a, pt1->c) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt2->a, pt1->c, box1, box2 );
            }
            else if (o2) {
                if (GetSquareDistance(pt1->d, pt2->b) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt1->d, pt2->b, box1, box2 );
            }
            else {
                if (GetSquareDistance(pt1->c, pt1->d) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt1->c, pt1->d, box1, box2 );
            }
        } break;
        case SimplePT::adjacentSide::aRight_bLeft: {
            bool o1 = onSegment(pt1->a, pt2->c, pt1->b);
            bool o2 = onSegment(pt1->a, pt2->d, pt1->b);
            if (o1 && o2) {
                if (GetSquareDistance(pt2->c, pt2->d) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt2->c, pt2->d, box1, box2 );
            }
            else if (o1) {
                if (GetSquareDistance(pt2->c, pt1->a) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt2->c, pt1->a, box1, box2 );
            }
            else if (o2) {
                if (GetSquareDistance(pt1->b, pt2->d) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt1->b, pt2->d, box1, box2 );
            }
            else {
                if (GetSquareDistance(pt1->a, pt1->b) < square_tolerance)
                    return false;
                mp->m_portals.emplace_back( pt1->a, pt1->b, box1, box2 );
            }
        } break;
        default: return false;
        }

        mp->m_PTPortalGraph[pt1->id].emplace_back(&mp->m_portals.back());
        mp->m_PTPortalGraph[pt2->id].emplace_back(&mp->m_portals.back());
        return true;
    }

    //Connect trapezoid AABBS.
    void MilePath::GenerateAABBGraph(MilePath* mp) {
        if (m_terminateThread) return;

        mp->m_AABBgraph.clear();
        mp->m_AABBgraph.resize(mp->m_aabbs.size());

        mp->m_portals.clear();
        mp->m_portals.reserve(mp->m_aabbs.size() * 3);
        mp->m_PTPortalGraph.clear();
        mp->m_PTPortalGraph.resize(mp->m_aabbs.size() * 2, {});

        for (size_t i = 0; i < mp->m_aabbs.size(); ++i) {
            for (size_t j = i + 1; j < mp->m_aabbs.size(); ++j) {
                //coarse intersection
                if (!mp->m_aabbs[i].intersect(mp->m_aabbs[j], { 1.0f, 1.0f })) continue;
                auto *a = &mp->m_aabbs[i], *b = &mp->m_aabbs[j];

                //fine intersection
                SimplePT::adjacentSide ts;
                if (a->m_t->layer == b->m_t->layer)
                    ts = a->m_t->Touching(*b->m_t);
                else
                    ts = a->m_t->TouchingHeight(*b->m_t);
                if (ts == SimplePT::adjacentSide::none) continue;
                if (CreatePortal(mp, a, b, ts)) {
                    mp->m_AABBgraph[a->m_id].emplace_back(b);
                    mp->m_AABBgraph[b->m_id].emplace_back(a);
                }
            }
        }
        Log::Info("Portal count: %d\n", mp->m_portals.size());
    }

    void MilePath::GeneratePoints(MilePath* mp) {
        if (m_terminateThread) return;

        mp->m_points.clear();
        mp->m_points.reserve(mp->m_portals.size() * 2 + mp->m_teleports.size() * 2);
            
        for (const auto& portal : mp->m_portals) {
            mp->m_points.emplace_back( 0, portal.m_start, portal.m_box1, portal.m_box2, &portal );
            mp->m_points.emplace_back( 0, portal.m_goal, portal.m_box1, portal.m_box2, &portal );
        }

        //Not needed. But if they are sorted binary search can be used.
        std::sort(mp->m_points.begin(), mp->m_points.end(), [](point& a, point& b) { return a.pos.y > b.pos.y; });
        for (size_t i = 0; i < mp->m_points.size(); ++i) { 
            mp->m_points[i].id = i; 
        };

        Log::Info("Number of points: %d\n", mp->m_points.size());
    }

    bool MilePath::IsOnPathingTrapezoid(const std::vector<AABB> &aabbs, const Vec2f &p, const SimplePT **ppt) {
        //note: maybe use GW client O(logn) algorithm?
        //constexpr float tolerance = 2.0f;
        //auto lower = std::lower_bound(aabbs.begin(), aabbs.end(), p.y,
        //    [](const AABB &box, const float &val) { return box.m_pos.y - box.m_half.y - tolerance < val; });
        //auto upper = std::upper_bound(lower, aabbs.end(), p.y,
        //    [](const float &val, const AABB &box) { return val < box.m_pos.y + box.m_half.y + tolerance; });

        //for (auto it = lower; it < aabbs.end()/*upper*/; ++it) {
        //    const SimplePT *pt = (*it).m_t;
        //    if (IsOnPathingTrapezoid(pt, p)) {
        //        if (ppt) *ppt = pt;
        //        return true;
        //    }
        //}
        
        for (auto &box : aabbs) { 
            const SimplePT *pt = box.m_t;            
            if (IsOnPathingTrapezoid(pt, p)) {
                if (ppt) *ppt = pt;
                return true;
            }
        }
        if (ppt) *ppt = nullptr;
        return false;
    }

    bool MilePath::IsOnPathingTrapezoid(const SimplePT* pt, const Vec2f& p) {
        constexpr float tolerance = 2.0f;
        if (pt->a.y < p.y || pt->b.y > p.y) return false;
        if (pt->b.x > p.x && pt->a.x > p.x) return false;
        if (pt->c.x < p.x && pt->d.x < p.x) return false;
        Vec2f ab = pt->b - pt->a, bc = pt->c - pt->b, cd = pt->d - pt->c, da = pt->a - pt->d;
        Vec2f pa = pt->a - p, pb = pt->b - p, pc = pt->c - p, pd = pt->d - p;
        if (Cross(ab, pa) > tolerance) return false;
        //if (Cross(bc, pb) > tolerance) return false;
        if (Cross(cd, pc) > tolerance) return false;
        //if (Cross(da, pd) > tolerance) return false;
        return true;
    }

    bool IntersectPt(const SimplePT& pt, const Vec2f& start, const Vec2f& goal) {
        if (Intersect(pt.a, pt.b, start, goal)) return true;
        if (Intersect(pt.b, pt.c, start, goal)) return true;
        if (Intersect(pt.c, pt.d, start, goal)) return true;
        if (Intersect(pt.d, pt.a, start, goal)) return true;
        return false;
    }

    const AABB *MilePath::FindAABB(const std::vector<AABB> &aabbs, const GamePos &pos) {
        for (auto& a : aabbs) {
            if (pos.zplane == a.m_t->layer && IsOnPathingTrapezoid(a.m_t, pos))
                return &a;
        }
        return nullptr;
    }

    inline void addBlockingId(std::vector<uint32_t> *blocking_ids, const AABB *box) {
        if (box && box->m_t && box->m_t->layer) {
            blocking_ids->push_back(box->m_t->layer);
        }
    }

    
    bool MilePath::HasLineOfSight(const MilePath* mp, const point& start, const point& goal,
            std::vector<const AABB *> &open, std::vector<bool> &visited,
            std::vector<uint32_t>* blocking_ids)
    {
        if ((start.box && goal.box && start.box->m_id == goal.box->m_id)
            || (start.box && goal.box2 && start.box->m_id == goal.box2->m_id)
            || (start.box2 && goal.box && start.box2->m_id == goal.box->m_id)
            || (start.box2 && goal.box2 && start.box2->m_id == goal.box2->m_id)) {
            if (blocking_ids) {
                addBlockingId(blocking_ids, start.box);
                addBlockingId(blocking_ids, start.box2);
                addBlockingId(blocking_ids, goal.box);
                addBlockingId(blocking_ids, goal.box2);
            }
            return true; //internal point are visible
        }

        open.clear();
        open.reserve(mp->m_aabbs.size());

        if (start.box) open.emplace_back(start.box);
        if (start.box2) open.emplace_back(start.box2);
        uint32_t last_layer = 0;

        const AABB *current; //current open box
        visited.clear();
        visited.resize(mp->m_aabbs.size());

        while (open.size()) {
            current = open.back();
            open.pop_back();
            if (visited[current->m_id]) continue;
            visited[current->m_id] = true; //close box

            //get portals of the current box
            auto &portals = mp->m_PTPortalGraph[current->m_t->id];
            for (const auto *portal : portals) {
                if (start.portal == portal) //self intersection is always true
                    continue;

                //continue if there is no direct line of sight going through the current portal from the start to the goal points;
                if (!portal->intersect(start.pos, goal.pos))
                    continue;
                
                if (blocking_ids && last_layer != current->m_t->layer) {
                    last_layer = current->m_t->layer;
                    if (last_layer)
                        blocking_ids->push_back(last_layer);
                }
                //if (blocking_ids && last_layer != portal->m_box2->m_t->layer) {
                //    last_layer = portal->m_box2->m_t->layer;
                //    if (last_layer)
                //        blocking_ids->push_back(last_layer);
                //}

                //goal is reached when the current box id is the same as one of the goal boxes
                if ((goal.box && current->m_id == goal.box->m_id) || (goal.box2 && current->m_id == goal.box2->m_id)) {
                    return true;
                }

                //here at this point the line crosses one of portals
                //add boxes that the point is not currently in to explore
                if (current != portal->m_box1)
                    open.push_back(portal->m_box1);
                if (current != portal->m_box2)
                    open.push_back(portal->m_box2);  
            }
        }
        return false;
    }
   
    void MilePath::GenerateVisibilityGraph(MilePath* mp) {
        if (mp->m_terminateThread) return;

        //note: naive VG generation is O(n^3)
        //TODO: great speedup if only checking visibility of convex points.

        mp->m_visGraph.clear();
        mp->m_visGraph.resize(mp->m_portals.size() * 2 + mp->m_teleports.size() * 2 + 2);

        float range = mp->m_visibility_range;
        float sqrange = range * range;

        std::vector<const AABB *> open;
        std::vector<bool> visited;
        size_t size = mp->m_points.size();
        for (size_t i = 0; i < size; ++i) {
            auto& p1 = mp->m_points[i];
            float min_range = p1.pos.y - range;
            float max_range = p1.pos.y + range;

            for (size_t j = i + 1; j < size; ++j) {
                auto& p2 = mp->m_points[j];

                if (min_range > p2.pos.y || max_range < p2.pos.y)
                    continue;

                float sqdist = GetSquareDistance(p1.pos, p2.pos);
                if (sqdist > sqrange) 
                    continue;

                if (std::any_of(
                    std::begin(mp->m_visGraph[p1.id]), 
                    std::end(mp->m_visGraph[p1.id]), 
                    [&p2](const PointVisElement &a) { return a.point_id == p2.id; })) continue;

                std::vector<uint32_t> blocking_ids;
                if (HasLineOfSight(mp, p1, p2, open, visited, &blocking_ids)) {
                    float dist = sqrtf(sqdist);
                    mp->m_visGraph[p1.id].emplace_back( p2.id, dist, blocking_ids );
                    mp->m_visGraph[p2.id].emplace_back( p1.id, dist, std::move(blocking_ids) );
                }
            }
            mp->m_progress = (i * 100) / size;
            if (mp->m_terminateThread) return;
        }
    }

    void MilePath::insertTeleportPointIntoVisGraph(MilePath *mp, MilePath::point& point, teleport_point_type type) {
        std::vector<const AABB*> open;
        std::vector<bool> visited;
        for (const auto &p : mp->m_points) {
            std::vector<uint32_t> blocking_ids;
            if (!MilePath::HasLineOfSight(mp, p, point, open, visited, &blocking_ids)) continue;

            float distance = GetDistance(point.pos, p.pos);
            if (type == both) {
                mp->m_visGraph[p.id].emplace_back( point.id, distance, blocking_ids );
                mp->m_visGraph[point.id].emplace_back( p.id, distance, blocking_ids );
            } else if (type == enter) {
                mp->m_visGraph[p.id].emplace_back( point.id, distance, blocking_ids );
            } else if(type == exit) {
                mp->m_visGraph[point.id].emplace_back( p.id, distance, blocking_ids );
            }
        }
    }

    void MilePath::InsertTeleportsIntoVisibilityGraph(MilePath* mp) {
        if (mp->m_terminateThread) return;

        using namespace MapSpecific;

        for (const auto& teleport : mp->m_teleports) {
            bool bidir = teleport.m_directionality == Teleport::direction::both_ways;

            auto point_enter = CreatePoint(mp, teleport.m_enter);
            point_enter.id = mp->m_points.size();
            mp->m_points.emplace_back(point_enter);
            insertTeleportPointIntoVisGraph(mp, mp->m_points.back(), bidir ? both : enter);

            auto point_exit = CreatePoint(mp, teleport.m_exit);
            point_exit.id = mp->m_points.size();
            mp->m_points.emplace_back(point_exit);
            insertTeleportPointIntoVisGraph(mp, mp->m_points.back(), bidir ? both : exit);

            //although the distance between teleports is 0, a tiny value is used as a penalty for various reasons.
            float dist = GetDistance(teleport.m_enter, teleport.m_exit) * 0.01f;
            mp->m_visGraph[point_enter.id].emplace_back( mp->m_points[point_exit.id].id, dist , std::initializer_list<uint32_t>() );
            if (bidir)
                mp->m_visGraph[point_exit.id].emplace_back( mp->m_points[point_enter.id].id, dist * 0.01f, std::initializer_list<uint32_t>() );
        }
    }


    typedef std::pair<float, MilePath::point::Id> PQElement;
    class MyPQueue : public std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>>
    {
    public:
        MyPQueue(size_t reserve_size)
        {
            this->c.reserve(reserve_size);
        }
    };

    AStar::AStar(MilePath* mp) : m_mp(mp), m_path() {
        //Visibility graph challenge: integrating start and goal points requires careful
        //handling to prevent continuous graph expansion and search slowdown. 
        //Previous method involved copying the entire graph for each search.
        //There's a slight improvement in performance by directly inserting and 
        //subsequently removing start and stop points in the visibility graph.

        //create temporary vis graph to hold start and goal points
        m_visGraph.clear();
        //m_visGraph.resize(m_mp->m_points.size() + 2);
        //for (size_t i = 0; i < m_mp->m_points.size(); ++i) {
        //    m_visGraph[i] = m_mp->m_visGraph[i];
        //}
    };

    class Path {
    public:
        Path() : points(), cost(0.0f), visited_index(0) {};
        std::vector<MilePath::point> points;
        float cost; //distance
        int visited_index;
    };
    Path m_path;

    void AStar::insertPointIntoVisGraph(const MilePath* mp, std::vector<std::vector<MilePath::PointVisElement>>& visGraph, MilePath::point& point)
    {
        float sqrange = mp->m_visibility_range * mp->m_visibility_range;
        std::vector<const AABB *> open;
        std::vector<bool> visited;
        for (auto it = mp->m_points.cbegin(); it != mp->m_points.cend(); ++it) {
            float sqdistance = GetSquareDistance((*it).pos, point.pos);
            if (sqdistance > sqrange)
                continue;

            std::vector<uint32_t> blocking_ids;
            if (!MilePath::HasLineOfSight(mp, (*it), point, open, visited, &blocking_ids)) 
                continue;

            float distance = sqrtf(sqdistance);
            visGraph[point.id].emplace_back( (*it).id, distance, blocking_ids );
            visGraph[(*it).id].emplace_back( point.id, distance, std::move(blocking_ids) );
        }
    }

    //https://github.com/Rikora/A-star/blob/master/src/AStar.cpp
    AStar::Path AStar::buildPath(const MilePath* mp, const MilePath::point& start, const MilePath::point& goal,
        std::vector<MilePath::point::Id>& came_from) {
        Path path{};
        MilePath::point current(goal);

        int count = 0;
        while (current.id != start.id) {
            if (count++ > 64) {
                Log::Error("build path failed\n");
                break;
            }
            if (current.id < 0) {
                break;
            }
            path.insertPoint(current);
            auto& id = came_from[current.id];
            if (id == start.id)
                break;
            current = mp->m_points[id];
        }
        path.insertPoint(start);
        path.finalize();
        return path;
    }

    float AStar::teleporterHeuristic(const MilePath* mp, const MilePath::point& start, const MilePath::point& goal) {
        if (!mp->m_teleports.size())
            return 0.0f;
        
        using namespace MapSpecific;

        float cost = INFINITY;
        const Teleport *ts = nullptr, *tg = nullptr;
        float dist_start = cost, dist_goal = cost;
        for (const auto &tp : mp->m_teleports) {
            float dist = GetSquareDistance(start.pos, tp.m_enter);
            if (tp.m_directionality == Teleport::direction::both_ways)
                dist = std::min(dist, GetSquareDistance(start.pos, tp.m_exit));
                
            if (dist_start > dist) {
                dist_start = dist;
                ts = &tp;
            }

            dist = GetSquareDistance(goal.pos, tp.m_exit);
            if (tp.m_directionality == Teleport::direction::both_ways)
                dist = std::min(dist, GetSquareDistance(goal.pos, tp.m_enter));
                
            if (dist_goal > dist) {
                dist_goal = dist;
                tg = &tp;
            }
        }

        for (const auto &ttd : mp->m_teleportGraph) {
            if (ttd.tp1 == ts && ttd.tp2 == tg) {
                //cost = sqrtf(dist_start) + ttd.distance + sqrtf(dist_goal);
                cost = sqrtf(dist_start);
                break;
            }
        }
        return cost;
    }

    AStar::Path AStar::search(const GamePos &start_pos, const GamePos &goal_pos) {
        MilePath::point::Id point_id = m_mp->m_points.size();
        MilePath::point start;
        bool new_start = false;
        //if (m_mp->m_pointLookup.contains(start_pos)) {
        //    start = *m_mp->m_pointLookup.at(start_pos);
        //} else 
        {
            start = m_mp->CreatePoint(m_mp, start_pos);
            if (!start.box)
                return m_path;
            start.id = point_id++;
            new_start = true;
        }

        MilePath::point goal;
        bool new_goal = false;
        //if (m_mp->m_pointLookup.contains(goal_pos)) {
        //    goal = *m_mp->m_pointLookup.at(goal_pos);
        //} else 
        {
            goal = m_mp->CreatePoint(m_mp, goal_pos);
            if (!goal.box)
                return m_path;
            goal.id = point_id++;
            new_goal = true;
        }

        GW::MapContext* mapContext = GW::GetMapContext();
        if (!mapContext) return m_path;
        GW::Array<uint32_t>& block = mapContext->sub1->pathing_map_block;

        {
            std::vector<const AABB *> open;
            std::vector<bool> visited;
            std::vector<uint32_t> blocking_ids;
            if (MilePath::HasLineOfSight(m_mp, start, goal, open, visited, &blocking_ids)) {
                if (!std::ranges::any_of(blocking_ids, [&block](auto &id) { return block[id]; })) {
                    m_path.insertPoint(start);
                    m_path.insertPoint(goal);
                    m_path.setCost(GetDistance(start_pos, goal_pos));
                    return m_path;
                }
            }
        }

        volatile clock_t start_timestamp = clock();

        if (new_start) {
            m_mp->m_points.push_back(start);
            insertPointIntoVisGraph(m_mp, m_mp->m_visGraph, start);
        }
        if (new_goal) {
            m_mp->m_points.push_back(goal);
            insertPointIntoVisGraph(m_mp, m_mp->m_visGraph, goal);
        }

        std::vector<float> cost_so_far;
        std::vector<MilePath::point::Id> came_from;
        MyPQueue open(m_mp->m_points.size() + 2);

        cost_so_far.resize(m_mp->m_points.size() + 2, -INFINITY);
        cost_so_far[start.id] = 0.0f;
        
        came_from.resize(m_mp->m_points.size() + 2);
        came_from[start.id] = start.id;
        open.emplace(0.0f, start.id);

        bool teleports = m_mp->m_teleports.size();
        MilePath::point::Id current = 0; //-1
        while (!open.empty()) {
            current = open.top().second;
            open.pop();
            if (current == goal.id)
                break;

            for (auto &vis : m_mp->m_visGraph[current]) {
                if (std::ranges::any_of(vis.blocking_ids, [&block](auto &id) { return block[id]; }))
                    continue;

                float new_cost = cost_so_far[current] + vis.distance;
                if (cost_so_far[vis.point_id] == -INFINITY || new_cost < cost_so_far[vis.point_id]) {
                    cost_so_far[vis.point_id] = new_cost;
                    came_from[vis.point_id] = current;

                    float priority = new_cost;
                    if (teleports) {
                        auto &point = m_mp->m_points[vis.point_id];
                        float tp_cost = teleporterHeuristic(m_mp, point, goal);
                        priority += std::min(GetDistance(point.pos, goal.pos), tp_cost);
                    }
                    open.emplace(priority, vis.point_id);
                }
            }
        }

        if (current == goal.id) {
            m_path = buildPath(m_mp, start, goal, came_from);
            m_path.setCost(cost_so_far[current]);
        }

        //TODO: Implement a pop list so there is no need to search every node?
        if (new_goal) {
            auto &other_elements = m_mp->m_visGraph[goal.id];
            
            for (auto &elem : other_elements) {
                auto &points = m_mp->m_visGraph[elem.point_id];
                std::erase_if(points, [&goal](auto &elem) { return elem.point_id == goal.id; });
            }
            m_mp->m_points.pop_back();
            m_mp->m_visGraph[goal.id].clear();
        }
        if (new_start) {
            auto &other_elements = m_mp->m_visGraph[start.id];
            
            for (auto &elem : other_elements) {
                auto &points = m_mp->m_visGraph[elem.point_id];
                std::erase_if(points, [&start](auto &elem) { return elem.point_id == start.id; });
            }
            m_mp->m_points.pop_back();
            m_mp->m_visGraph[start.id].clear();
        }

        volatile clock_t stop_timestamp = clock();
        Log::Info("Find path: %d ms\n", stop_timestamp - start_timestamp);

        return m_path;
    }

    GamePos AStar::getClosestPoint(const Vec2f& pos) const {
        return getClosestPoint(m_path, pos);
    }

    GamePos AStar::getClosestPoint(const Path& path, const Vec2f& pos) {
        auto& points = path.points();
        if (!points.size()) return {};
        if (points.size() < 2)
            return { points.front().pos.x, points.front().pos.y, points.front().box->m_t->layer };

        typedef struct pqdist {
            float distance = 0.0f;
            MilePath::point point = {};
            size_t index = 0;
        } pqdist;

        std::vector<pqdist> pq;
        for (size_t i = 1; i < points.size(); ++i) {
            auto AP = pos - points[i - 1].pos;
            auto AB = points[i].pos - points[i - 1].pos;

            float mag = GetSquaredNorm(AB);
            float ABAP = Dot(AP, AB);
            float distance = ABAP / mag;

            distance = std::clamp(distance, 0.0f, 1.0f);

            pqdist e;
            e.point.box = points[i - 1].box;
            e.point.pos = points[i - 1].pos + AB * distance;
            e.distance = GetDistance(pos, e.point.pos);
            pq.emplace_back(std::move(e));
        }

        if (!pq.size()) return pos; // points.back().pos;
        float min_distance = std::numeric_limits<float>::infinity();
        size_t min_idx = 0;
        for (size_t i = 0; i < pq.size(); ++i) {
            if (pq[i].distance < min_distance) {
                min_distance = pq[i].distance;
                min_idx = i;
            }
        }
        return { pq[min_idx].point.pos.x, pq[min_idx].point.pos.y, pq[min_idx].point.box->m_t->layer };
    }
}
