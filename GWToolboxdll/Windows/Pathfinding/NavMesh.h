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

        // Diagnostic: log every ground poly within `radius` (game units) of `center` — source trapezoid
        // fields + each ring edge's wall/connection classification and neighbour poly/plane. Writes to log.txt.
        void DebugDumpNear(const GW::GamePos& center, float radius) const;

        // Terrain height to drape an overlay/path point on, resolved against the navmesh: among the planes that
        // actually have a walkable polygon at (x,y), return the QueryAltitude surface closest to `prev_z`
        // (continuity). This keeps a path on the surface it walks — e.g. up onto a monument plane between two
        // ground hops, instead of sinking to the ground-under-monument that a blind all-planes query returns.
        // Returns FLT_MAX if the navmesh isn't ready or no walkable plane covers (x,y) — caller should fall back.
        // Must run on the game/render thread (calls GW::Map::QueryAltitude).
        float DrapeHeightAt(float x, float y, float prev_z) const;

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
        std::vector<const GW::PathingTrapezoid*> m_poly_trap; // ground poly index -> source trapezoid (point-location for DrapeHeightAt)
    };

} // namespace Pathing
