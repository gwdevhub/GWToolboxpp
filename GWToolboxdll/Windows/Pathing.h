#pragma once

#include <cstdint>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include "MapSpecificData.h"

namespace Pathing {
	//basdically a copy of Pathing trapezoid with additional layer and corner points.
	//  a----d   
	//   \    \
	//    b____c
    class SimplePT {
    public:
        enum class adjacentSide {
            none, aBottom_bTop, aTop_bBottom, aLeft_bRight, aRight_bLeft
        };

        SimplePT(const GW::PathingTrapezoid &pt, uint32_t layer);
        adjacentSide Touching(const SimplePT &rhs) const;
        adjacentSide TouchingHeight(const SimplePT &rhs, float max_height_diff = 200.0f) const;

        uint32_t id, layer;
        GW::Vec2f a, b, c, d;
    };

    class AABB {
    public:
        //https://noonat.github.io/intersect/
        typedef uint32_t boxId;

        AABB(const SimplePT &t);
        bool intersect(const GW::Vec2f& rhs) const;                               // point
        bool intersect(const GW::Vec2f& rhs, float radius) const;                 // circle
        bool intersect(const GW::Vec2f& a, const GW::Vec2f& b, GW::Vec2f padding = {}) const; // segment
        bool intersect(const AABB& rhs, GW::Vec2f padding = {}) const;                        // aabb
        
        boxId m_id;
        GW::Vec2f m_pos, m_half;
        const SimplePT *m_t;
    };

    class MilePath {
    private:
        MilePath();

        volatile bool m_processing = false;
        volatile bool m_done = false;
        volatile bool m_terminateThread = false;
        volatile int m_progress = 0;
        
    public:
        static MilePath *instance();

        void stopProcessing() { m_terminateThread = true; }

        int progress() {
            return m_progress;
        }

        MapSpecific::MapSpecificData m_msd;

        // Portal is a helper contruct between pathing trapezoids and it represents a line through which it
        // is possible to cross from one pathing trapezoid into another.
        class Portal {
        public:
            Portal(const GW::Vec2f &start, const GW::Vec2f &goal, const AABB *box1, const AABB *box2);
            bool intersect(const GW::Vec2f &p1, const GW::Vec2f &p2) const;

            const AABB *m_box1, *m_box2;
            GW::Vec2f m_start, m_goal;
        };

        class point {
        public:
            typedef int Id;
            Id id = 0;
            GW::Vec2f pos = {};
            const AABB *box = nullptr;
            const AABB *box2 = nullptr;
            const Portal *portal = nullptr;

            operator GW::GamePos() {
                return GW::GamePos(pos.x, pos.y, box->m_t->layer);
            }
        };

        typedef struct {
            point::Id point_id; //other point
            float distance;
            std::vector<uint32_t> blocking_ids; //Holds all layer changes; for checking if it's passable or blocked.
        } PointVisElement;
        
        float m_visibility_range = 5000;
        std::vector<AABB> m_aabbs;
        std::vector<SimplePT> m_trapezoids;
        std::vector<std::vector<PointVisElement>> m_visGraph; // [point.id]
        std::vector<std::vector<const AABB*>> m_AABBgraph; // [box.id]
        std::vector<Portal> m_portals; // [portal.id]
        std::vector<std::vector<const Portal*>> m_PTPortalGraph; // [simple_pt.id]
        std::vector<point> m_points; // [point.id]
        MapSpecific::Teleports m_teleports;
        std::vector<MapSpecific::teleport_node> m_teleportGraph;

        //Generate distance graph among teleports
        static void GenerateTeleportGraph(MilePath *mp);
        static MilePath::point CreatePoint(const MilePath *mp, const GW::GamePos &pos);

        static bool HasLineOfSight(const MilePath *mp, const point &start, const point &goal,
            std::vector<const AABB *> &open, std::vector<bool> &visited,
            std::vector<uint32_t> *blocking_ids = nullptr);

        static const AABB *FindAABB(const std::vector<AABB> &aabbs, const GW::GamePos &pos);
        static bool IsOnPathingTrapezoid(const std::vector<AABB> &aabbs, const GW::Vec2f &p, const SimplePT **pt = nullptr);

    private:
        void LoadMapSpecificData(MilePath *mp);

        //Generate Axis Aligned Bounding Boxes around trapezoids
        //This is used for quick intersection checks.
        void GenerateAABBs(MilePath *mp);

        bool CreatePortal(MilePath *mp, const AABB *box1, const AABB *box2, const SimplePT::adjacentSide &ts);

        //Connect trapezoid AABBS.
        void GenerateAABBGraph(MilePath *mp);

        void GeneratePoints(MilePath *mp);

        static bool IsOnPathingTrapezoid(const SimplePT *pt, const GW::Vec2f &p);

        static void GenerateVisibilityGraph(MilePath *mp);

        typedef enum { enter, exit, both } teleport_point_type;
        static void insertTeleportPointIntoVisGraph(MilePath *mp, MilePath::point &point, teleport_point_type type);
        static void InsertTeleportsIntoVisibilityGraph(MilePath *mp);
    };

    class AStar {
    public:
        class Path {
        public:
            Path() : m_points(), m_cost(0.0f) { 
                m_mp = MilePath::instance();
            };

            const std::vector<MilePath::point> points() const {
                if (m_mp != MilePath::instance())
                    return {};
                return m_points;
            }

            float cost() const {
                return m_cost;
            }

            void insertPoint(const MilePath::point &point) {
                m_points.emplace_back(point);
            }

            void setCost(float cost) {
                m_cost = cost;
            }

            void finalize() {
                if (!finalized)
                    std::reverse(m_points.begin(), m_points.end());
                finalized = true;
            }

        private:
            bool finalized = false;
            MilePath* m_mp;
            std::vector<MilePath::point> m_points;
            float m_cost; //distance
        };
		
        Path m_path;

        AStar(MilePath *mp);

        static void insertPointIntoVisGraph(const MilePath *mp, std::vector<std::vector<MilePath::PointVisElement>> &visGraph, MilePath::point &point);
        
        static Path buildPath(const MilePath *mp, const MilePath::point &start, const MilePath::point &goal, std::vector<MilePath::point::Id> &came_from);

        static inline float teleporterHeuristic(const MilePath *mp, const MilePath::point &start, const MilePath::point &goal);

        Path search(const GW::GamePos &start_pos, const GW::GamePos &goal_pos);

        GW::GamePos getClosestPoint(const GW::Vec2f &pos) const;
        static GW::GamePos getClosestPoint(const Path &path, const GW::Vec2f &pos);

    private:
        std::vector<std::vector<MilePath::PointVisElement>> m_visGraph;
        MilePath* m_mp;
    };
}
