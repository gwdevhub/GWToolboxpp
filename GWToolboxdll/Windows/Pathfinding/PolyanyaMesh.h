#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <GWCA/GameContainers/GamePos.h>      // GW::Vec2f, GW::GamePos
#include <GWCA/GameEntities/Pathing.h>        // GW::PathingTrapezoid

#include "MapSpecificData.h"                  // MapSpecific::Teleports
#include "PathingMapData.h"                   // Pathing::PathingMapData

namespace Pathing {

    // =========================================================================
    // PortalAdjacency
    //
    // Polyanya migration, phase A. A read-only capture of the visgraph's own
    // portal adjacency, recorded once per walkable connection by
    // Impl::createPortalPair. This is the load-bearing input to
    // PolyanyaMesh::Build — we consume these captured connections directly
    // instead of re-deriving neighbours from raw GW trapezoid pointers.
    //
    // The shared edge lies on trap_a's edge `edge_a` and trap_b's edge
    // `(edge_a + 2) % 4`, over the exact interval [a, b] (identical GW::Vec2f
    // on both traps — createPortalPair's max/min endpoints, unrounded).
    // =========================================================================
    struct PortalAdjacency {
        const GW::PathingTrapezoid* trap_a;
        const GW::PathingTrapezoid* trap_b;
        uint8_t layer_a, layer_b;
        GW::Vec2f a, b;       // exact shared-edge interval endpoints (no rounding)
        bool a_viable, b_viable;
        uint8_t edge_a;       // neighbour location relative to trap_a: 0=top,1=right,2=bottom,3=left
    };

    // One ring vertex. Exact float coords are stored; dedup keys round only to a
    // tiny epsilon so identical shared endpoints from two traps collapse to one.
    struct PolyVertex {
        float x, y;
        uint16_t plane;
        bool is_viable;
    };

    // One convex polygon (one non-degenerate GW trapezoid, conformed). The two
    // vectors are parallel: edge i runs vertex_ids[i] -> vertex_ids[(i+1)%n] and
    // its neighbour poly is edge_neighbour[i] (-1 = wall). CCW winding.
    struct ConvexPoly {
        uint16_t plane;
        std::vector<uint32_t> vertex_ids;
        std::vector<int32_t> edge_neighbour;
    };

    // A connection between polys on different planes (cross-plane portal or
    // teleport). Not represented as a shared ring edge.
    struct InterPlaneLink {
        uint32_t poly_a, poly_b;
        GW::Vec2f a, b;
        uint16_t plane_a, plane_b;
        float cost;
        bool directional;
        bool is_teleport;
    };

    class PolyanyaMesh {
    public:
        PolyanyaMesh() = default;
        ~PolyanyaMesh() = default;
        PolyanyaMesh(const PolyanyaMesh&) = delete;
        PolyanyaMesh& operator=(const PolyanyaMesh&) = delete;

        bool Build(const PathingMapData& data, const MapSpecific::Teleports& teleports,
                   const std::vector<PortalAdjacency>& adjacency);

        bool IsReady() const { return m_ready; }

        struct DebugEdge { GW::GamePos a, b; bool wall; };
        void DebugExtractEdges(std::vector<DebugEdge>& out) const;

        int32_t PolyOfTrapezoid(const GW::PathingTrapezoid*) const;

        // Ghost-wall audit: true if trapezoids a and b are connected in the mesh (shared ring edge,
        // or an inter-plane link). Used to confirm the conforming build dropped no visgraph connection.
        bool AreConnected(const GW::PathingTrapezoid* a, const GW::PathingTrapezoid* b) const;

    private:
        std::vector<PolyVertex>   m_verts;
        std::vector<ConvexPoly>   m_polys;
        std::vector<InterPlaneLink> m_links;
        std::vector<std::vector<uint32_t>> m_plane_polys;                       // plane index -> poly indices
        std::unordered_map<const GW::PathingTrapezoid*, uint32_t> m_trap_to_poly;
        bool m_ready = false;
    };

} // namespace Pathing
