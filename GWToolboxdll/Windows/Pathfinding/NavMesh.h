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

        float DrapeHeightAt(float x, float y, float prev_z) const;

    private:
        void DestroyMesh();
        int PlaneIndex(uint32_t zplane) const; // GW query zplane -> plane index (ground sentinel -> 0)
        float PlaneY(int plane) const;
        // Bucket ground trapezoids into Y rows so DrapeHeightAt point-locates in O(few), not O(all ~10k-32k).
        void BuildDrapeIndex(float min_y, float max_y);

        dtNavMesh* m_navmesh = nullptr;

        int          m_plane_count = 0;
        float        m_cs = 1.0f;
        float        m_ch = 1.0f;
        uint32_t     m_ground_poly_count = 0;

        std::vector<uint16_t> m_poly_plane; // ground poly index -> plane index
        std::vector<const GW::PathingTrapezoid*> m_poly_trap; // ground poly index -> source trapezoid (point-location for DrapeHeightAt)

        // === Y-row point-location index (DrapeHeightAt) ===
        // 1D rows suffice: GW trapezoids are thin horizontal bands, so a query touches exactly one row.
        float                 m_row_min_y = 0.f;
        float                 m_row_inv_h = 0.f;  // rows per unit Y; 0 => not built, fall back to the full scan
        uint32_t              m_row_count = 0;
        std::vector<uint32_t> m_row_start;        // CSR offsets into m_row_items, size m_row_count + 1
        std::vector<uint32_t> m_row_items;
        std::vector<uint32_t> m_tall_items;       // spans too many rows to bucket; always scanned
    };

} // namespace Pathing
