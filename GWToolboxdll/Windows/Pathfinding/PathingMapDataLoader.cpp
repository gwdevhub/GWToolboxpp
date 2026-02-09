#include "stdafx.h"

#include "PathingMapDataLoader.h"

// Include actual GW headers
#include <GWCA/Context/MapContext.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Pathing.h>

#include <Utils/ArenaNetFileParser.h>

namespace Pathing {

    // =============================================================================
    // LoadFromPathContext - Extract navigation data from live game memory
    // =============================================================================

    bool LoadFromMapContext(const GW::MapContext* map_context, uint32_t map_file_id, PathingMapData* out)
    {
        if (!map_context) return false;
        const auto path_ctx = map_context->path;

        if (!path_ctx || !path_ctx->staticData || !out) { 
            return false;
        }

        const auto& src_maps = path_ctx->staticData->map;
        if (src_maps.size() == 0) {
            return false;
        }

        PathingMapData result;
        const uint32_t plane_count = src_maps.size();
        result.planes.resize(static_cast<size_t>(plane_count));

        // Process each plane
        for (uint32_t plane_idx = 0; plane_idx < plane_count; ++plane_idx) {
            const GW::PathingMap& src = src_maps[plane_idx];
            NavPlane& dst = result.planes[plane_idx];

            dst.zplane = src.zplane;

            const uint32_t trapezoid_count = src.trapezoid_count;
            const uint32_t portal_count = src.portal_count;
            const uint32_t x_node_count = src.x_node_count;
            const uint32_t y_node_count = src.y_node_count;
            const uint32_t sink_node_count = src.sink_node_count;

            // ---------------------------------------------------------------------
            // Copy trapezoids
            // ---------------------------------------------------------------------
            if (src.trapezoids && trapezoid_count > 0) {
                dst.trapezoids.resize(static_cast<size_t>(trapezoid_count));

                // Build pointer-to-index map for this plane's trapezoids
                std::unordered_map<const GW::PathingTrapezoid*, uint32_t> trap_ptr_to_idx;
                for (uint32_t i = 0; i < trapezoid_count; ++i) {
                    trap_ptr_to_idx[&src.trapezoids[i]] = i;
                }

                for (uint32_t i = 0; i < trapezoid_count; ++i) {
                    const GW::PathingTrapezoid& st = src.trapezoids[i];
                    Trapezoid& dt = dst.trapezoids[i];

                    // Copy geometry
                    dt.XTL = st.XTL;
                    dt.XTR = st.XTR;
                    dt.YT = st.YT;
                    dt.XBL = st.XBL;
                    dt.XBR = st.XBR;
                    dt.YB = st.YB;

                    // Copy portal indices
                    dt.portal_left = st.portal_left;
                    dt.portal_right = st.portal_right;

                    // Convert neighbor pointers to indices
                    for (int n = 0; n < 4; ++n) {
                        if (st.adjacent[n]) {
                            auto it = trap_ptr_to_idx.find(st.adjacent[n]);
                            if (it != trap_ptr_to_idx.end()) {
                                dt.neighbors[n] = it->second;
                            }
                            else {
                                // Neighbor is on a different plane - mark as invalid
                                // (cross-plane connections go through portals)
                                dt.neighbors[n] = INVALID_INDEX;
                            }
                        }
                        else {
                            dt.neighbors[n] = INVALID_INDEX;
                        }
                    }
                }
            }

            // ---------------------------------------------------------------------
            // Copy portals
            // ---------------------------------------------------------------------
            if (src.portals && portal_count > 0) {
                dst.portals.resize(static_cast<size_t>(portal_count));

                // First, gather all portal trapezoid indices
                std::vector<uint32_t> all_trap_indices;
                all_trap_indices.reserve(static_cast<size_t>(src.portal_trapezoids_count));

                // Build pointer-to-index map for trapezoids
                std::unordered_map<const GW::PathingTrapezoid*, uint32_t> trap_ptr_to_idx;
                for (uint32_t i = 0; i < trapezoid_count; ++i) {
                    trap_ptr_to_idx[&src.trapezoids[i]] = i;
                }

                for (uint32_t i = 0; i < portal_count; ++i) {
                    const GW::Portal& sp = src.portals[i];
                    Portal& dp = dst.portals[i];

                    dp.trapezoid_index_start = static_cast<uint16_t>(all_trap_indices.size());
                    dp.trapezoid_count = static_cast<uint16_t>(sp.count);
                    dp.neighbor_plane = sp.neighbor_plane;
                    dp.flags = static_cast<uint8_t>(sp.flags);

                    // Find shared_id by examining the pair portal
                    // The pair portal on the other plane has the same shared_id
                    dp.shared_id = INVALID_INDEX16;
                    if (sp.pair) {
                        // We need to figure out which portal index sp.pair is
                        // For now, use portal_plane as a proxy
                        // A more accurate approach would be to match by geometry
                        dp.shared_id = static_cast<uint16_t>(i); // Simplified
                    }

                    // Convert trapezoid pointers to indices
                    const uint32_t sp_count = sp.count;
                    if (sp.trapezoids && sp_count > 0) {
                        for (uint32_t j = 0; j < sp_count; ++j) {
                            if (sp.trapezoids[j]) {
                                auto it = trap_ptr_to_idx.find(sp.trapezoids[j]);
                                if (it != trap_ptr_to_idx.end()) {
                                    all_trap_indices.push_back(it->second);
                                }
                                else {
                                    all_trap_indices.push_back(INVALID_INDEX);
                                }
                            }
                            else {
                                all_trap_indices.push_back(INVALID_INDEX);
                            }
                        }
                    }
                }

                dst.portal_trapezoid_indices = std::move(all_trap_indices);
            }

            // ---------------------------------------------------------------------
            // Copy BSP tree nodes
            // ---------------------------------------------------------------------

            // Build pointer-to-index maps for nodes
            std::unordered_map<const GW::Node*, uint32_t> xnode_ptr_to_idx;
            std::unordered_map<const GW::Node*, uint32_t> ynode_ptr_to_idx;
            std::unordered_map<const GW::Node*, uint32_t> sinknode_ptr_to_idx;

            if (src.x_nodes) {
                for (uint32_t i = 0; i < x_node_count; ++i) {
                    xnode_ptr_to_idx[reinterpret_cast<const GW::Node*>(&src.x_nodes[i])] = i;
                }
            }
            if (src.y_nodes) {
                for (uint32_t i = 0; i < y_node_count; ++i) {
                    ynode_ptr_to_idx[reinterpret_cast<const GW::Node*>(&src.y_nodes[i])] = i;
                }
            }
            if (src.sink_nodes) {
                for (uint32_t i = 0; i < sink_node_count; ++i) {
                    sinknode_ptr_to_idx[reinterpret_cast<const GW::Node*>(&src.sink_nodes[i])] = i;
                }
            }

            // Helper to encode node pointer to unified index
            auto encode_node = [&](const GW::Node* node) -> uint32_t {
                if (!node) return INVALID_INDEX;

                auto xit = xnode_ptr_to_idx.find(node);
                if (xit != xnode_ptr_to_idx.end()) {
                    return xit->second; // XNode: [0, x_count)
                }

                auto yit = ynode_ptr_to_idx.find(node);
                if (yit != ynode_ptr_to_idx.end()) {
                    return x_node_count + yit->second; // YNode: [x_count, x_count + y_count)
                }

                auto sit = sinknode_ptr_to_idx.find(node);
                if (sit != sinknode_ptr_to_idx.end()) {
                    return x_node_count + y_node_count + sit->second; // SinkNode
                }

                return INVALID_INDEX;
            };
        }

        *out = std::move(result);

        out->map_file_id = map_file_id;
        out->bounds_min = {map_context->start_pos.x, map_context->start_pos.y};
        out->bounds_max = {map_context->end_pos.x, map_context->end_pos.y};
        return true;
    }

    bool LoadPathingMapDataFromDAT(uint32_t map_file_id, PathingMapData* out)
    {
        ArenaNetFileParser::ArenaNetFile game_asset;
        if (!game_asset.readFromDat(map_file_id, 1))
            return false;

#pragma pack(push, 1)
        struct MapInfoChunk : ArenaNetFileParser::Chunk {
            uint32_t signature;
            uint8_t version;
            Vec2f bounds[2];
            // ...
        };
#pragma pack(pop)



        const auto pathfinding_chunk = game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_Pathfinding);
        if (!pathfinding_chunk) return false;

        auto chunk_data = (uint8_t*)pathfinding_chunk;
        chunk_data += sizeof(*pathfinding_chunk);

        auto chunk_size = pathfinding_chunk->chunk_size;

        // Header: signature(4) + version(4) + sequence(4)
        uint32_t signature, version, sequence;
        if (!detail::read_u32(chunk_data, 0, chunk_size, signature)) return false;
        if (!detail::read_u32(chunk_data, 4, chunk_size, version)) return false;
        if (!detail::read_u32(chunk_data, 8, chunk_size, sequence)) return false;

        // Verify signature
        if (signature != 0xEEFE704C) return false;

        size_t off = 12;

        // --- Tag 7: Preamble (skip) ---
        uint8_t tag;
        uint32_t tag_size;
        if (!detail::read_tag(chunk_data, off, chunk_size, tag, tag_size)) return false;
        if (tag != 7) return false;
        off += 5 + tag_size;

        // --- Tag 8: All planes ---
        if (!detail::read_tag(chunk_data, off, chunk_size, tag, tag_size)) return false;
        if (tag != 8) return false;
        off += 5;

        uint32_t plane_count;
        if (!detail::read_u32(chunk_data, off, chunk_size, plane_count)) return false;
        off += 4;

        // Sanity check
        if (plane_count > 256) return false;

        PathingMapData result;
        result.planes.resize(plane_count);

        // Parse each plane
        for (uint32_t i = 0; i < plane_count; ++i) {
            if (!detail::parse_plane(chunk_data, off, chunk_size, result.planes[i])) {
                return false;
            }

            // Set zplane (plane 0 = ground = UINT32_MAX, others = 0, 1, 2, ...)
            result.planes[i].zplane = (i == 0) ? UINT32_MAX : (i - 1);
        }

        // Skip remaining tags (12, 13, 14, terminator) - we don't need them for pathfinding
        // They contain obstacle data that can be processed separately if needed

        *out = std::move(result);

        const auto map_info_chunk = (MapInfoChunk*)game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_Info);
        if (!map_info_chunk) return false;

        out->bounds_min = map_info_chunk->bounds[0];
        out->bounds_max = map_info_chunk->bounds[1];

        out->map_file_id = map_file_id;
        
        return true;
    }

} // namespace Pathing
