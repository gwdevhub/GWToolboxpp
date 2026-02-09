#pragma once

#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>

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

    inline void to_json(nlohmann::json& j, const Vec2f& v)
    {
        j = nlohmann::json::array({v.x, v.y});
    }

    inline void to_json(nlohmann::json& j, const Trapezoid& t)
    {
        j = nlohmann::json{
            {"geometry", nlohmann::json::array({
                             t.XTL, // Top left
                             t.XTR, // Top right
                             t.YT,  // Top Y
                             t.XBL, // Bottom left
                             t.XBR, // Bottom right,
                             t.YB   // Bottom y
                         })},
             {"neighbors", nlohmann::json::array({
                               t.neighbors[0] == INVALID_INDEX ? nullptr : nlohmann::json(t.neighbors[0]), // top left
                               t.neighbors[1] == INVALID_INDEX ? nullptr : nlohmann::json(t.neighbors[1]), // top right
                               t.neighbors[2] == INVALID_INDEX ? nullptr : nlohmann::json(t.neighbors[2]), // bottom left
                               t.neighbors[3] == INVALID_INDEX ? nullptr : nlohmann::json(t.neighbors[3])  // bottom right
                           })},
                     {"portals", nlohmann::json::array({t.portal_left == INVALID_INDEX16 ? nullptr : nlohmann::json(t.portal_left), t.portal_right == INVALID_INDEX16 ? nullptr : nlohmann::json(t.portal_right)})}
        };
    }

    inline void to_json(nlohmann::json& j, const Portal& p)
    {
        j = nlohmann::json{
            {"trap_index_start", p.trapezoid_index_start},
            {"trap_count", p.trapezoid_count},
            {"neighbor_plane", p.neighbor_plane == INVALID_INDEX16 ? nullptr : nlohmann::json(p.neighbor_plane)},
            {"shared_id", p.shared_id == INVALID_INDEX16 ? nullptr : nlohmann::json(p.shared_id)},
            {"flags", p.flags},
            {"blocked", (p.flags & 0x04) != 0}
        };
    }

    inline void to_json(nlohmann::json& j, const NavPlane& plane)
    {

        j = nlohmann::json{
            {"zplane", plane.zplane == UINT32_MAX ? "ground" : nlohmann::json(plane.zplane)},
            {"trapezoids", plane.trapezoids},
            {"portals", plane.portals},
            {"portal_trapezoid_indices", plane.portal_trapezoid_indices},
            {"stats", {{"trapezoid_count", plane.trapezoids.size()}, {"portal_count", plane.portals.size()}}}
        };
    }

    inline void to_json(nlohmann::json& j, const PathingMapData& data)
    {
        j = nlohmann::json{
            {"map_file_id", data.map_file_id},
            {"bounds", {{"min", data.bounds_min}, {"max", data.bounds_max}}},
            {"planes", data.planes},
            {"stats", {{"plane_count", data.planes.size()}, {"total_trapezoids", data.GetTotalTrapezoidCount()}, {"is_valid", data.IsValid()}}}
        };
    }
} // namespace Pathing
