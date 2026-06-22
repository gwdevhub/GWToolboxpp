#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <GWCA/GameContainers/GamePos.h>

#include "Pathing.h" // PATHING_MAX_PLANE_COUNT, PathingMapData, MapSpecificData

class dtNavMesh;

namespace Pathing {

    // Detour mesh from GW pathing trapezoids; drives the debug overlay only (pathing uses the visgraph A*).
    class NavMesh {
    public:
        NavMesh() = default;
        ~NavMesh();
        NavMesh(const NavMesh&) = delete;
        NavMesh& operator=(const NavMesh&) = delete;

        bool Build(const PathingMapData& data, const MapSpecific::Teleports& teleports);

        bool IsReady() const { return m_navmesh != nullptr; }

        struct DebugEdge { GW::GamePos a; GW::GamePos b; bool wall; };
        void DebugExtractEdges(std::vector<DebugEdge>& out) const;

    private:
        void DestroyMesh();
        int PlaneIndex(uint32_t zplane) const; // GW query zplane -> plane index (ground sentinel -> 0)
        float PlaneY(int plane) const;

        dtNavMesh* m_navmesh = nullptr;

        int          m_plane_count = 0;
        float        m_cs = 1.0f;
        float        m_ch = 1.0f;
        uint32_t     m_ground_poly_count = 0;

        std::vector<uint16_t> m_poly_plane; // ground poly index -> plane index
    };

} // namespace Pathing
