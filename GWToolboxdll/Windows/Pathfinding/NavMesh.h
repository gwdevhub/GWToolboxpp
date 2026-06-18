#pragma once

#include <cstdint>
#include <vector>

#include <GWCA/GameContainers/GamePos.h>

#include "Pathing.h" // BlockedPlaneBitset, PATHING_MAX_PLANE_COUNT, PathingMapData, MapSpecificData

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

namespace Pathing {

    class NavMesh {
    public:
        enum class Result { OK, NoMesh, StartNotFound, GoalNotFound, NoPath, Partial };

        NavMesh() = default;
        ~NavMesh();
        NavMesh(const NavMesh&) = delete;
        NavMesh& operator=(const NavMesh&) = delete;

        bool Build(const PathingMapData& data, const MapSpecific::Teleports& teleports);

        bool BuildFromRecast(const PathingMapData& data, const MapSpecific::Teleports& teleports);

        bool IsReady() const { return m_navmesh != nullptr; }

        void ApplyBlockedPlanes(const BlockedPlaneBitset& blocked);

        Result FindPath(const GW::GamePos& start, const GW::GamePos& goal,
                        std::vector<GW::GamePos>& out_points, float& out_cost);

        struct DebugEdge { GW::GamePos a; GW::GamePos b; bool wall; };
        void DebugExtractEdges(std::vector<DebugEdge>& out) const;

        static bool s_smooth_paths;

        static bool s_use_recast_builder;

    private:
        void DestroyMesh();
        int PlaneIndex(uint32_t zplane) const; // GW query zplane -> plane index (ground sentinel -> 0)
        float PlaneY(int plane) const;
        int PlaneOfPolyRef(unsigned int ref) const; // ground poly -> plane index; off-mesh -> -1
        unsigned int SnapToPoly(const GW::Vec2f& xy, int preferred_plane, float* snapped, int& out_plane,
                                const dtQueryFilter& filter) const;
        bool SegmentWalkable(const GW::Vec2f& a, const GW::Vec2f& b, int plane, const dtQueryFilter& filter) const;
        void ShortcutPath(std::vector<GW::GamePos>& pts, const dtQueryFilter& filter) const;

        dtNavMesh*      m_navmesh = nullptr;
        dtNavMeshQuery* m_query   = nullptr;

        int          m_plane_count = 0;
        float        m_cs = 1.0f;
        float        m_ch = 1.0f;
        unsigned int m_poly_base = 0;
        uint32_t     m_ground_poly_count = 0;

        std::vector<uint16_t>              m_poly_plane;  // ground poly index -> plane index
        std::vector<std::vector<uint32_t>> m_plane_polys; // plane index -> ground poly indices

        struct OffMeshPlanes { uint32_t poly_index; uint16_t plane_a; uint16_t plane_b; };
        std::vector<OffMeshPlanes> m_offmesh_planes; // disabled when either plane is blocked

        BlockedPlaneBitset m_applied_blocked;
        bool               m_applied_valid = false;
    };

} // namespace Pathing
