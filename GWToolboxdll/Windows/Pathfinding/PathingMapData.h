#pragma once

#include <cstdint>
#include <vector>
#include <glaze/glaze.hpp>

// =============================================================================
// PathingMapData
//
// A self-contained, pointer-free representation of map pathfinding data that
// can be constructed from either:
//   1. GW::PathContext (live game memory)
//   2. FFNA Type-3 file chunk 0x20000008 (raw file data)
//
// Design principles:
//   - No runtime pointers - all references use indices
//   - All data is stored in flat vectors for easy serialization
//   - Neighbor/portal relationships stored as indices, not pointers
//   - BSP tree stored as index-based nodes
//   - Can be used for pathfinding without any external dependencies
//
// This struct owns all its data and is fully self-contained.
// =============================================================================

namespace Pathing {
    constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
    constexpr uint16_t INVALID_INDEX16 = 0xFFFF;

    struct Vec2f {
        float x, y;
    };

    // Simplified trapezoid - only geometry and neighbor connectivity
    struct Trapezoid {
        // Geometry (6 floats defining the trapezoid shape)
        float XTL, XTR; // X coords of top edge (left, right)
        float YT;       // Y coord of top edge
        float XBL, XBR; // X coords of bottom edge (left, right)
        float YB;       // Y coord of bottom edge

        // Neighbor connectivity (indices into same plane's trapezoid array)
        // Order: [top, right, bottom, left]
        uint32_t neighbors[4];

        // Portal indices for cross-plane connections
        uint16_t portal_left;
        uint16_t portal_right;
    };

    // Portal for cross-plane connectivity
    struct Portal {
        uint16_t trapezoid_count;       // Number of trapezoids in this portal
        uint16_t trapezoid_index_start; // Start index in portal_trapezoid_indices array
        uint16_t neighbor_plane;        // Index of connected plane
        uint16_t shared_id;             // ID shared between portal pairs
        uint8_t flags;                  // Portal flags (0x1 = blocked)

        bool IsBlocked() const { return (flags & 0x1) != 0; }
    };

    // Navigation plane (one height layer)
    struct NavPlane {
        std::vector<Trapezoid> trapezoids;
        std::vector<Portal> portals;
        std::vector<uint32_t> portal_trapezoid_indices;

        // Z-plane identifier (for multi-level maps)
        // Can be: 0 (ground), or positive integer for elevated planes
        uint32_t zplane;
    };

    // Complete pathing map data
    struct PathingMapData {
        uint32_t map_file_id;
        Vec2f bounds_min;
        Vec2f bounds_max; 
        std::vector<NavPlane> planes;

        bool IsValid() const { return !planes.empty(); }

        size_t GetPlaneCount() const { return planes.size(); }

        size_t GetTotalTrapezoidCount() const
        {
            size_t total = 0;
            for (const auto& plane : planes) {
                total += plane.trapezoids.size();
            }
            return total;
        }
    };

    // =========================================================================
    // JSON Serialization for PathingMapData
    //
    // Converts the complete pathing data structure to JSON format.
    // Useful for:
    //   - Debugging and visualization
    //   - Exporting to external tools
    //   - Comparing different map versions
    //   - Web-based map viewers
    // =========================================================================

    inline glz::json_t ToJson(const Vec2f& v)
    {
        glz::json_t::array_t arr;
        arr.emplace_back(v.x);
        arr.emplace_back(v.y);
        return arr;
    }

    inline glz::json_t IndexOrNull(uint32_t idx)
    {
        return idx == INVALID_INDEX ? glz::json_t{nullptr} : glz::json_t{static_cast<double>(idx)};
    }

    inline glz::json_t Index16OrNull(uint16_t idx)
    {
        return idx == INVALID_INDEX16 ? glz::json_t{nullptr} : glz::json_t{static_cast<double>(idx)};
    }

    inline glz::json_t ToJson(const Trapezoid& t)
    {
        glz::json_t::array_t geometry;
        geometry.emplace_back(t.XTL);
        geometry.emplace_back(t.XTR);
        geometry.emplace_back(t.YT);
        geometry.emplace_back(t.XBL);
        geometry.emplace_back(t.XBR);
        geometry.emplace_back(t.YB);

        glz::json_t::array_t neighbors;
        neighbors.emplace_back(IndexOrNull(t.neighbors[0]));
        neighbors.emplace_back(IndexOrNull(t.neighbors[1]));
        neighbors.emplace_back(IndexOrNull(t.neighbors[2]));
        neighbors.emplace_back(IndexOrNull(t.neighbors[3]));

        glz::json_t::array_t portals;
        portals.emplace_back(Index16OrNull(t.portal_left));
        portals.emplace_back(Index16OrNull(t.portal_right));

        glz::json_t j;
        j["geometry"] = std::move(geometry);
        j["neighbors"] = std::move(neighbors);
        j["portals"] = std::move(portals);
        return j;
    }

    inline glz::json_t ToJson(const Portal& p)
    {
        glz::json_t j;
        j["trap_index_start"] = static_cast<double>(p.trapezoid_index_start);
        j["trap_count"] = static_cast<double>(p.trapezoid_count);
        j["neighbor_plane"] = Index16OrNull(p.neighbor_plane);
        j["shared_id"] = Index16OrNull(p.shared_id);
        j["flags"] = static_cast<double>(p.flags);
        j["blocked"] = (p.flags & 0x04) != 0;
        return j;
    }

    inline glz::json_t ToJson(const NavPlane& plane)
    {
        glz::json_t::array_t trapezoids;
        trapezoids.reserve(plane.trapezoids.size());
        for (const auto& t : plane.trapezoids) trapezoids.emplace_back(ToJson(t));

        glz::json_t::array_t portals;
        portals.reserve(plane.portals.size());
        for (const auto& p : plane.portals) portals.emplace_back(ToJson(p));

        glz::json_t::array_t portal_trapezoid_indices;
        portal_trapezoid_indices.reserve(plane.portal_trapezoid_indices.size());
        for (auto idx : plane.portal_trapezoid_indices)
            portal_trapezoid_indices.emplace_back(static_cast<double>(idx));

        glz::json_t stats;
        stats["trapezoid_count"] = static_cast<double>(plane.trapezoids.size());
        stats["portal_count"] = static_cast<double>(plane.portals.size());

        glz::json_t j;
        j["zplane"] = plane.zplane == UINT32_MAX
            ? glz::json_t{std::string{"ground"}}
            : glz::json_t{static_cast<double>(plane.zplane)};
        j["trapezoids"] = std::move(trapezoids);
        j["portals"] = std::move(portals);
        j["portal_trapezoid_indices"] = std::move(portal_trapezoid_indices);
        j["stats"] = std::move(stats);
        return j;
    }

    inline glz::json_t ToJson(const PathingMapData& data)
    {
        glz::json_t::array_t planes;
        planes.reserve(data.planes.size());
        for (const auto& p : data.planes) planes.emplace_back(ToJson(p));

        glz::json_t bounds;
        bounds["min"] = ToJson(data.bounds_min);
        bounds["max"] = ToJson(data.bounds_max);

        glz::json_t stats;
        stats["plane_count"] = static_cast<double>(data.planes.size());
        stats["total_trapezoids"] = static_cast<double>(data.GetTotalTrapezoidCount());
        stats["is_valid"] = data.IsValid();

        glz::json_t j;
        j["map_file_id"] = static_cast<double>(data.map_file_id);
        j["bounds"] = std::move(bounds);
        j["planes"] = std::move(planes);
        j["stats"] = std::move(stats);
        return j;
    }
} // namespace Pathing
