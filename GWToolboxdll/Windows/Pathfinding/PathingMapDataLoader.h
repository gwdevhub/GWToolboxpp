#pragma once

#include "PathingMapData.h"
#include <cstring>

// Forward declarations for GW types (include actual headers in .cpp)
namespace GW {
    struct MapContext;
    struct PathingMap;
    struct PathingTrapezoid;
    struct Portal;
    struct XNode;
    struct YNode;
    struct SinkNode;
    struct Node;
}

namespace Pathing {

    // =========================================================================
    // LoadFromPathContext
    //
    // Extracts navigation data from live game memory (GW::PathContext).
    // Use this when the game is running and map is loaded.
    // =========================================================================

    bool LoadFromMapContext(const GW::MapContext* path_ctx, uint32_t map_file_id, PathingMapData* out);

    bool LoadPathingMapDataFromDAT(uint32_t map_file_id, PathingMapData* out);

} // namespace Pathing


// =============================================================================
// IMPLEMENTATION
// =============================================================================

namespace Pathing {
    namespace detail {

        // -------------------------------------------------------------------------
        // Read helpers (little-endian, unaligned)
        // -------------------------------------------------------------------------

        inline bool read_u8(const uint8_t* p, size_t off, size_t len, uint8_t& out) {
            if (off + 1 > len) return false;
            out = p[off];
            return true;
        }

        inline bool read_u16(const uint8_t* p, size_t off, size_t len, uint16_t& out) {
            if (off + 2 > len) return false;
            std::memcpy(&out, p + off, 2);
            return true;
        }

        inline bool read_u32(const uint8_t* p, size_t off, size_t len, uint32_t& out) {
            if (off + 4 > len) return false;
            std::memcpy(&out, p + off, 4);
            return true;
        }

        inline bool read_f32(const uint8_t* p, size_t off, size_t len, float& out) {
            if (off + 4 > len) return false;
            std::memcpy(&out, p + off, 4);
            return true;
        }

        // -------------------------------------------------------------------------
        // Tag header reading
        // -------------------------------------------------------------------------

        inline bool read_tag(const uint8_t* p, size_t off, size_t len, uint8_t& tag, uint32_t& size) {
            if (!read_u8(p, off, len, tag)) return false;
            if (!read_u32(p, off + 1, len, size)) return false;
            return true;
        }

        // -------------------------------------------------------------------------
        // Find pathfinding chunk (0x20000008) in FFNA file
        // -------------------------------------------------------------------------

        inline bool find_pathing_chunk(
            const uint8_t* data, 
            size_t length, 
            size_t& payload_offset, 
            uint32_t& payload_size)
        {
            constexpr size_t FFNA_HEADER = 5;  // 4-byte sig + 1-byte type
            constexpr uint32_t CHUNK_HDR = 8;   // chunk_id(4) + chunk_size(4)
            constexpr uint32_t TARGET = 0x20000008;

            size_t off = FFNA_HEADER;
            while (off + CHUNK_HDR <= length) {
                uint32_t id, sz;
                if (!read_u32(data, off, length, id)) return false;
                if (!read_u32(data, off + 4, length, sz)) return false;

                if (id == TARGET) {
                    payload_offset = off + CHUNK_HDR;
                    payload_size = sz;
                    return (payload_offset + sz) <= length;
                }

                size_t next = off + CHUNK_HDR + sz;
                if (next <= off) return false;  // Overflow guard
                off = next;
            }
            return false;
        }

        // -------------------------------------------------------------------------
        // Parse a single plane from the tag stream
        // -------------------------------------------------------------------------

        inline bool parse_plane(
            const uint8_t* data, 
            size_t& off, 
            size_t end, 
            NavPlane& plane)
        {
            auto consume_tag = [&](uint8_t expected_tag, uint32_t& tag_size) -> bool {
                uint8_t tag;
                uint32_t sz;
                if (!read_tag(data, off, end, tag, sz)) return false;
                if (tag != expected_tag) return false;
                tag_size = sz;
                off += 5;  // 1 byte tag + 4 bytes size
                return true;
            };

            uint32_t tag_size;

            // --- Tag 0: Header (32 bytes = 8 × uint32) ---
            if (!consume_tag(0, tag_size)) return false;
            if (tag_size < 32) return false;

            uint32_t poly_count, vectors_count, trapezoids_count;
            uint32_t xnodes_count, ynodes_count, sinknodes_count;
            uint32_t portals_count, portal_traps_count;

            size_t base = off;
            if (!read_u32(data, base + 0, end, poly_count)) return false;
            if (!read_u32(data, base + 4, end, vectors_count)) return false;
            if (!read_u32(data, base + 8, end, trapezoids_count)) return false;
            if (!read_u32(data, base + 12, end, xnodes_count)) return false;
            if (!read_u32(data, base + 16, end, ynodes_count)) return false;
            if (!read_u32(data, base + 20, end, sinknodes_count)) return false;
            if (!read_u32(data, base + 24, end, portals_count)) return false;
            if (!read_u32(data, base + 28, end, portal_traps_count)) return false;
            off += tag_size;

            // --- Tag 11: Poly boundary data (skip - not needed) ---
            if (!consume_tag(11, tag_size)) return false;
            off += poly_count * 8;  // Game reads poly_count × 8 bytes

            // --- Tag 1: Edge vectors ---
            if (!consume_tag(1, tag_size)) return false;
            off += tag_size;

            // --- Tag 2: Trapezoids (44 bytes each) ---
            // File layout (different from our struct):
            //   4×u32 neighbors, 2×u16 portals, then 6×f32 coords
            // Our struct stores coords first for cache locality during queries
            if (!consume_tag(2, tag_size)) return false;
            plane.trapezoids.resize(trapezoids_count);
            base = off;
            for (uint32_t i = 0; i < trapezoids_count; ++i) {
                size_t t = base + i * 44;
                auto& trap = plane.trapezoids[i];

                // Read neighbors
                if (!read_u32(data, t + 0, end, trap.neighbors[0])) return false;
                if (!read_u32(data, t + 4, end, trap.neighbors[1])) return false;
                if (!read_u32(data, t + 8, end, trap.neighbors[2])) return false;
                if (!read_u32(data, t + 12, end, trap.neighbors[3])) return false;

                // Read portal indices
                if (!read_u16(data, t + 16, end, trap.portal_left)) return false;
                if (!read_u16(data, t + 18, end, trap.portal_right)) return false;

                // Read coordinates (file order: YT, YB, XTL, XTR, XBL, XBR)
                if (!read_f32(data, t + 20, end, trap.YT)) return false;
                if (!read_f32(data, t + 24, end, trap.YB)) return false;
                if (!read_f32(data, t + 28, end, trap.XTL)) return false;
                if (!read_f32(data, t + 32, end, trap.XTR)) return false;
                if (!read_f32(data, t + 36, end, trap.XBL)) return false;
                if (!read_f32(data, t + 40, end, trap.XBR)) return false;
            }
            off += tag_size;

            // --- Tag 3: Root node type (1 byte) ---
            if (!consume_tag(3, tag_size)) return false;
            off += tag_size;

            // --- Tag 4: XNodes (16 bytes each) ---
            if (!consume_tag(4, tag_size)) return false;
            off += tag_size;

            // --- Tag 5: YNodes (12 bytes each) ---
            if (!consume_tag(5, tag_size)) return false;
            off += tag_size;

            // --- Tag 6: SinkNodes (4 bytes each) ---
            if (!consume_tag(6, tag_size)) return false;
            off += tag_size;

            // --- Tag 10: Portal trapezoid indices (uint32 each) ---
            if (!consume_tag(10, tag_size)) return false;
            plane.portal_trapezoid_indices.resize(portal_traps_count);
            base = off;
            for (uint32_t i = 0; i < portal_traps_count; ++i) {
                if (!read_u32(data, base + i * 4, end, plane.portal_trapezoid_indices[i])) return false;
            }
            off += tag_size;


            // --- Tag 9: Portals (9 bytes each) ---
            if (!consume_tag(9, tag_size)) return false;
            plane.portals.resize(portals_count);
            base = off;
            for (uint32_t i = 0; i < portals_count; ++i) {
                size_t p = base + i * 9;
                auto& portal = plane.portals[i];
                if (!read_u16(data, p + 0, end, portal.trapezoid_count)) return false;
                if (!read_u16(data, p + 2, end, portal.trapezoid_index_start)) return false;
                if (!read_u16(data, p + 4, end, portal.neighbor_plane)) return false;

                uint16_t file_shared_id;
                if (!read_u16(data, p + 6, end, file_shared_id)) return false;
                portal.shared_id = file_shared_id - 1; // File uses 1-indexed, convert to 0-indexed

                if (!read_u8(data, p + 8, end, portal.flags)) return false;
            }
            off += tag_size;

            return true;
        }

    } // namespace detail
} // namespace Pathing
