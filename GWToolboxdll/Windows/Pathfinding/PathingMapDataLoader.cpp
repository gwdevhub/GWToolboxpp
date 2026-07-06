#include "stdafx.h"

#include "PathingMapDataLoader.h"

// Include actual GW headers
#include <GWCA/Context/MapContext.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Pathing.h>

#include <Utils/ArenaNetFileParser.h>

#include <mutex>
#include <unordered_map>

namespace Pathing {

    // =============================================================================
    // Helper: Allocate single buffer for a plane and set up pointers
    // =============================================================================

    static void AllocatePlaneBuffer(
        CopiedPathingMap& plane,
        uint32_t trap_count,
        uint32_t portal_count,
        uint32_t portal_trap_count,
        uint32_t x_node_count = 0,
        uint32_t y_node_count = 0,
        uint32_t sink_node_count = 0)
    {
        const size_t trap_bytes = sizeof(GW::PathingTrapezoid) * trap_count;
        const size_t portal_bytes = sizeof(GW::Portal) * portal_count;
        const size_t portal_trap_bytes = sizeof(GW::PathingTrapezoid*) * portal_trap_count;
        const size_t xnode_bytes = sizeof(GW::XNode) * x_node_count;
        const size_t ynode_bytes = sizeof(GW::YNode) * y_node_count;
        const size_t sinknode_bytes = sizeof(GW::SinkNode) * sink_node_count;
        const size_t buf_size = trap_bytes + portal_bytes + portal_trap_bytes
                              + xnode_bytes + ynode_bytes + sinknode_bytes;

        if (buf_size == 0) {
            plane.buffer = nullptr;
            plane.trapezoids = nullptr;
            plane.portals = nullptr;
            plane.portal_trapezoids = nullptr;
            plane.root_node = nullptr;
            plane.x_nodes = nullptr;
            plane.y_nodes = nullptr;
            plane.sink_nodes = nullptr;
            plane.trapezoid_count = 0;
            plane.portal_count = 0;
            plane.portal_trapezoids_count = 0;
            plane.x_node_count = 0;
            plane.y_node_count = 0;
            plane.sink_node_count = 0;
            return;
        }

        plane.buffer = new uint8_t[buf_size];
        memset(plane.buffer, 0, buf_size);

        uint8_t* ptr = plane.buffer;
        plane.trapezoids = trap_count ? reinterpret_cast<GW::PathingTrapezoid*>(ptr) : nullptr;
        ptr += trap_bytes;
        plane.portals = portal_count ? reinterpret_cast<GW::Portal*>(ptr) : nullptr;
        ptr += portal_bytes;
        plane.portal_trapezoids = portal_trap_count ? reinterpret_cast<GW::PathingTrapezoid**>(ptr) : nullptr;
        ptr += portal_trap_bytes;
        plane.x_nodes = x_node_count ? reinterpret_cast<GW::XNode*>(ptr) : nullptr;
        ptr += xnode_bytes;
        plane.y_nodes = y_node_count ? reinterpret_cast<GW::YNode*>(ptr) : nullptr;
        ptr += ynode_bytes;
        plane.sink_nodes = sink_node_count ? reinterpret_cast<GW::SinkNode*>(ptr) : nullptr;

        plane.trapezoid_count = trap_count;
        plane.portal_count = portal_count;
        plane.portal_trapezoids_count = portal_trap_count;
        plane.x_node_count = x_node_count;
        plane.y_node_count = y_node_count;
        plane.sink_node_count = sink_node_count;
    }

    // Travel-portal model file IDs (asura gates / ferry portals). Shared by the DAT
    // prop parser and the live MapContext extractor so both agree on what counts.
    static bool IsPortalModelFileId(uint32_t fid)
    {
        switch (fid) {
            case 0x4e6b2: case 0x3c5ac: case 0xa825: case 0xe723: case 0x858b: case 0x1c533: case 0x5e77a: return true;
            default: return false;
        }
    }

    // Decode a live prop's model file ID the way WorldMapWidget does:
    // h0034[4] points at a sub-struct whose [1] is the model's file hash.
    static uint32_t MapPropModelFileId(const GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4])) return 0;
        const auto* sub_deets = reinterpret_cast<const uint32_t*>(prop->h0034[4]);
        return ArenaNetFileParser::FileHashToFileId(reinterpret_cast<const wchar_t*>(sub_deets[1]));
    }

    // Extract travel-portal props from live map memory - the MapContext equivalent of
    // ParsePortalProps' DAT chunk walk.
    static void ParsePortalPropsFromMapContext(const GW::MapContext* map_context, std::vector<PortalProp>& out)
    {
        const auto props = map_context ? map_context->props : nullptr;
        if (!props) return;
        for (auto* prop : props->propArray) {
            const uint32_t fid = MapPropModelFileId(prop);
            if (IsPortalModelFileId(fid))
                out.push_back({{prop->position.x, prop->position.y}, fid});
        }
    }

    // =============================================================================
    // LoadFromMapContext - Copy from live game memory with pointer fixup
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
        result.pathNodeSize = path_ctx->pathNodes.size();

        // Build global ID->trapezoid map across all planes (pass 1: allocate + copy raw data)
        std::unordered_map<uint32_t, GW::PathingTrapezoid*> idToTrap;
        std::unordered_map<const GW::Portal*, GW::Portal*> oldToNew;
        std::unordered_map<const GW::Node*, GW::Node*> oldNodeToNew;

        for (uint32_t plane_idx = 0; plane_idx < plane_count; ++plane_idx) {
            const GW::PathingMap& src = src_maps[plane_idx];
            CopiedPathingMap& dst = result.planes[plane_idx];

            dst.zplane = src.zplane;

            AllocatePlaneBuffer(dst, src.trapezoid_count, src.portal_count, src.portal_trapezoids_count,
                               src.x_node_count, src.y_node_count, src.sink_node_count);

            // Copy trapezoids
            if (src.trapezoids && src.trapezoid_count > 0) {
                memcpy(dst.trapezoids, src.trapezoids, sizeof(GW::PathingTrapezoid) * src.trapezoid_count);

                // Clear adjacent pointers and build id map
                for (uint32_t i = 0; i < src.trapezoid_count; ++i) {
                    dst.trapezoids[i].adjacent[0] = nullptr;
                    dst.trapezoids[i].adjacent[1] = nullptr;
                    dst.trapezoids[i].adjacent[2] = nullptr;
                    dst.trapezoids[i].adjacent[3] = nullptr;

                    idToTrap[src.trapezoids[i].id] = &dst.trapezoids[i];
                }
            }

            // Copy portals
            if (src.portals && src.portal_count > 0) {
                memcpy(dst.portals, src.portals, sizeof(GW::Portal) * src.portal_count);

                // Clear pair and trapezoids pointers, build old->new map
                for (uint32_t i = 0; i < src.portal_count; ++i) {
                    dst.portals[i].pair = nullptr;
                    dst.portals[i].trapezoids = nullptr;

                    oldToNew[&src.portals[i]] = &dst.portals[i];
                }
            }

            // Copy BSP nodes (raw memcpy, pointer fixup in pass 2)
            if (src.x_nodes && src.x_node_count > 0) {
                memcpy(dst.x_nodes, src.x_nodes, sizeof(GW::XNode) * src.x_node_count);
            }
            if (src.y_nodes && src.y_node_count > 0) {
                memcpy(dst.y_nodes, src.y_nodes, sizeof(GW::YNode) * src.y_node_count);
            }
            if (src.sink_nodes && src.sink_node_count > 0) {
                memcpy(dst.sink_nodes, src.sink_nodes, sizeof(GW::SinkNode) * src.sink_node_count);
            }

            // Build old->new node pointer map for this plane
            for (uint32_t i = 0; i < src.x_node_count; ++i) {
                oldNodeToNew[reinterpret_cast<GW::Node*>(&src.x_nodes[i])] = reinterpret_cast<GW::Node*>(&dst.x_nodes[i]);
            }
            for (uint32_t i = 0; i < src.y_node_count; ++i) {
                oldNodeToNew[reinterpret_cast<GW::Node*>(&src.y_nodes[i])] = reinterpret_cast<GW::Node*>(&dst.y_nodes[i]);
            }
            for (uint32_t i = 0; i < src.sink_node_count; ++i) {
                oldNodeToNew[reinterpret_cast<GW::Node*>(&src.sink_nodes[i])] = reinterpret_cast<GW::Node*>(&dst.sink_nodes[i]);
            }
        }

        // Pass 2: Fix up all pointers
        for (uint32_t plane_idx = 0; plane_idx < plane_count; ++plane_idx) {
            const GW::PathingMap& src = src_maps[plane_idx];
            CopiedPathingMap& dst = result.planes[plane_idx];

            // Fix adjacent pointers in trapezoids
            if (src.trapezoids && src.trapezoid_count > 0) {
                for (uint32_t i = 0; i < src.trapezoid_count; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        if (src.trapezoids[i].adjacent[j]) {
                            uint32_t adj_id = src.trapezoids[i].adjacent[j]->id;
                            auto it = idToTrap.find(adj_id);
                            if (it != idToTrap.end()) {
                                dst.trapezoids[i].adjacent[j] = it->second;
                            }
                        }
                    }
                }
            }

            // Fix portal pointers
            if (src.portals && src.portal_count > 0) {
                // Track portal_trapezoid write offset within this plane's portal_trapezoids area
                uint32_t pt_offset = 0;

                for (uint32_t i = 0; i < src.portal_count; ++i) {
                    // Fix pair pointer
                    if (src.portals[i].pair) {
                        auto it = oldToNew.find(src.portals[i].pair);
                        if (it != oldToNew.end()) {
                            dst.portals[i].pair = it->second;
                        }
                    }

                    // Fix trapezoid pointers - point into portal_trapezoids area
                    if (src.portals[i].trapezoids && src.portals[i].count > 0) {
                        dst.portals[i].trapezoids = &dst.portal_trapezoids[pt_offset];
                        for (uint32_t j = 0; j < src.portals[i].count; ++j) {
                            if (src.portals[i].trapezoids[j]) {
                                uint32_t trap_id = src.portals[i].trapezoids[j]->id;
                                auto it = idToTrap.find(trap_id);
                                if (it != idToTrap.end()) {
                                    dst.portal_trapezoids[pt_offset + j] = it->second;
                                }
                                else {
                                    dst.portal_trapezoids[pt_offset + j] = nullptr;
                                }
                            }
                            else {
                                dst.portal_trapezoids[pt_offset + j] = nullptr;
                            }
                        }
                        pt_offset += src.portals[i].count;
                    }
                }
            }

            // Fix BSP node child pointers
            auto remap_node = [&](GW::Node* old_ptr) -> GW::Node* {
                if (!old_ptr) return nullptr;
                auto it = oldNodeToNew.find(old_ptr);
                return (it != oldNodeToNew.end()) ? it->second : nullptr;
            };

            for (uint32_t i = 0; i < dst.x_node_count; ++i) {
                dst.x_nodes[i].left = remap_node(dst.x_nodes[i].left);
                dst.x_nodes[i].right = remap_node(dst.x_nodes[i].right);
            }
            for (uint32_t i = 0; i < dst.y_node_count; ++i) {
                dst.y_nodes[i].above = remap_node(dst.y_nodes[i].above);
                dst.y_nodes[i].below = remap_node(dst.y_nodes[i].below);
            }
            for (uint32_t i = 0; i < dst.sink_node_count; ++i) {
                GW::SinkNode& sn = dst.sink_nodes[i];
                const GW::SinkNode& src_sn = src.sink_nodes[i];
                if (src_sn.trapezoid) {
                    auto it = idToTrap.find(src_sn.trapezoid->id);
                    sn.trapezoid = (it != idToTrap.end()) ? it->second : nullptr;
                } else {
                    sn.trapezoid = nullptr;
                }
            }

            // Remap root_node
            dst.root_node = remap_node(src.root_node);
        }

        *out = std::move(result);

        out->map_file_id = map_file_id;
        out->blockedPlanesPtr = &path_ctx->blockedPlanes;
        out->bounds_min = {map_context->start_pos.x, map_context->start_pos.y};
        out->bounds_max = {map_context->end_pos.x, map_context->end_pos.y};
        ParsePortalPropsFromMapContext(map_context, out->portal_props);
        return true;
    }

    // =============================================================================
    // LoadFromDAT - Parse FFNA file into same pointer-based layout
    // =============================================================================

    // Helper: convert a parsed plane into a CopiedPathingMap with single buffer
    static bool ConvertParsedPlaneToCopied(
        const detail::ParsedPlane& parsed,
        CopiedPathingMap& dst,
        uint32_t global_trap_id_start)
    {
        const uint32_t trap_count = static_cast<uint32_t>(parsed.trapezoids.size());
        const uint32_t portal_count = static_cast<uint32_t>(parsed.portals.size());
        const uint32_t portal_trap_count = static_cast<uint32_t>(parsed.portal_trapezoid_indices.size());
        const uint32_t x_count = static_cast<uint32_t>(parsed.x_nodes.size());
        const uint32_t y_count = static_cast<uint32_t>(parsed.y_nodes.size());
        const uint32_t sink_count = static_cast<uint32_t>(parsed.sink_nodes.size());

        dst.zplane = parsed.zplane;
        AllocatePlaneBuffer(dst, trap_count, portal_count, portal_trap_count,
                           x_count, y_count, sink_count);

        // Fill trapezoids
        for (uint32_t i = 0; i < trap_count; ++i) {
            const auto& pt = parsed.trapezoids[i];
            GW::PathingTrapezoid& dt = dst.trapezoids[i];

            dt.id = global_trap_id_start + i;
            dt.XTL = pt.XTL;
            dt.XTR = pt.XTR;
            dt.YT = pt.YT;
            dt.XBL = pt.XBL;
            dt.XBR = pt.XBR;
            dt.YB = pt.YB;
            dt.portal_left = pt.portal_left;
            dt.portal_right = pt.portal_right;

            // Convert neighbor indices to pointers within same plane
            for (int j = 0; j < 4; ++j) {
                if (pt.neighbors[j] != 0xFFFFFFFF && pt.neighbors[j] < trap_count) {
                    dt.adjacent[j] = &dst.trapezoids[pt.neighbors[j]];
                }
                else {
                    dt.adjacent[j] = nullptr;
                }
            }
        }

        // Fill portals (pair resolved later across planes)
        for (uint32_t i = 0; i < portal_count; ++i) {
            const auto& pp = parsed.portals[i];
            GW::Portal& dp = dst.portals[i];

            dp.portal_plane = 0;  // Will be set by caller
            dp.neighbor_plane = pp.neighbor_plane;
            dp.flags = static_cast<uint32_t>(pp.flags);
            dp.pair = nullptr;  // Will be resolved later
            dp.count = pp.trapezoid_count;

            // Point trapezoids into portal_trapezoids area
            if (pp.trapezoid_count > 0) {
                dp.trapezoids = &dst.portal_trapezoids[pp.trapezoid_index_start];
                for (uint32_t j = 0; j < pp.trapezoid_count; ++j) {
                    uint32_t idx = parsed.portal_trapezoid_indices[pp.trapezoid_index_start + j];
                    if (idx < trap_count) {
                        dst.portal_trapezoids[pp.trapezoid_index_start + j] = &dst.trapezoids[idx];
                    }
                    else {
                        dst.portal_trapezoids[pp.trapezoid_index_start + j] = nullptr;
                    }
                }
            }
            else {
                dp.trapezoids = nullptr;
            }
        }

        // Type-flagged child index → GW::Node* resolver
        // High 2 bits encode type: 0x00=XNode, 0x40=YNode, 0x80=SinkNode, 0xFF=null
        auto resolve_node = [&](uint32_t raw) -> GW::Node* {
            if (raw == 0xFFFFFFFF) return nullptr;
            uint32_t type_flag = raw & 0xC0000000;
            uint32_t idx = raw & 0x3FFFFFFF;
            switch (type_flag) {
                case 0x00000000: // XNode
                    return (idx < x_count) ? reinterpret_cast<GW::Node*>(&dst.x_nodes[idx]) : nullptr;
                case 0x40000000: // YNode
                    return (idx < y_count) ? reinterpret_cast<GW::Node*>(&dst.y_nodes[idx]) : nullptr;
                case 0x80000000: // SinkNode
                    return (idx < sink_count) ? reinterpret_cast<GW::Node*>(&dst.sink_nodes[idx]) : nullptr;
                default:
                    return nullptr;
            }
        };

        // Fill XNodes
        for (uint32_t i = 0; i < x_count; ++i) {
            const auto& src = parsed.x_nodes[i];
            GW::XNode& xn = dst.x_nodes[i];
            xn.type = 0;
            xn.id = i;
            xn.pos = (src.pos_vec_idx < parsed.vectors.size()) ? parsed.vectors[src.pos_vec_idx] : GW::Vec2f{};
            // DAT stores dir as absolute endpoint; convert to direction vector (endpoint - pos)
            if (src.dir_vec_idx < parsed.vectors.size()) {
                xn.dir = parsed.vectors[src.dir_vec_idx] - xn.pos;
            } else {
                xn.dir = {};
            }
            xn.left = resolve_node(src.left_idx);
            xn.right = resolve_node(src.right_idx);
        }

        // Fill YNodes
        for (uint32_t i = 0; i < y_count; ++i) {
            const auto& src = parsed.y_nodes[i];
            GW::YNode& yn = dst.y_nodes[i];
            yn.type = 1;
            yn.id = i;
            yn.pos = (src.pos_vec_idx < parsed.vectors.size()) ? parsed.vectors[src.pos_vec_idx] : GW::Vec2f{};
            yn.above = resolve_node(src.above_idx);
            yn.below = resolve_node(src.below_idx);
        }

        // Fill SinkNodes
        for (uint32_t i = 0; i < sink_count; ++i) {
            const auto& src = parsed.sink_nodes[i];
            GW::SinkNode& sn = dst.sink_nodes[i];
            sn.type = 2;
            sn.id = i;
            sn.trapezoid = (src.trapezoid_idx < trap_count) ? &dst.trapezoids[src.trapezoid_idx] : nullptr;
        }

        // Set root node: index 0 of the array indicated by root_node_type
        switch (parsed.root_node_type) {
            case 0: dst.root_node = x_count > 0 ? reinterpret_cast<GW::Node*>(&dst.x_nodes[0]) : nullptr; break;
            case 1: dst.root_node = y_count > 0 ? reinterpret_cast<GW::Node*>(&dst.y_nodes[0]) : nullptr; break;
            case 2: dst.root_node = sink_count > 0 ? reinterpret_cast<GW::Node*>(&dst.sink_nodes[0]) : nullptr; break;
            default: dst.root_node = nullptr; break;
        }

        return true;
    }

    // Shared helper: parse portal props from a loaded game asset
    static void ParsePortalProps(ArenaNetFileParser::ArenaNetFile& game_asset, std::vector<PortalProp>& out)
    {
        const auto prop_filenames_chunk = game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_PropFilenames);
        const auto prop_info_chunk = game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_PropInfo);
        if (!prop_filenames_chunk || !prop_info_chunk) return;

        auto* fn_data = reinterpret_cast<const uint8_t*>(prop_filenames_chunk) + sizeof(*prop_filenames_chunk);
        uint32_t fn_chunk_size = prop_filenames_chunk->chunk_size;
        if (fn_chunk_size < 5) return;

        uint32_t fn_count = (fn_chunk_size - 5) / 6;
        std::vector<uint32_t> prop_file_ids(fn_count);
        for (uint32_t i = 0; i < fn_count; i++) {
            size_t fn_off = 5 + i * 6;
            uint16_t id0, id1;
            memcpy(&id0, fn_data + fn_off, 2);
            memcpy(&id1, fn_data + fn_off + 2, 2);
            prop_file_ids[i] = (id0 > 0xff && id1 > 0xff)
                ? (id0 - 0xff00ff) + static_cast<uint32_t>(id1) * 0xff00
                : 0;
        }

        auto* pi_data = reinterpret_cast<const uint8_t*>(prop_info_chunk) + sizeof(*prop_info_chunk);
        uint32_t pi_chunk_size = prop_info_chunk->chunk_size;
        if (pi_chunk_size < 12) return;

        uint16_t num_props;
        memcpy(&num_props, pi_data + 10, 2);
        size_t pi_off = 12;
        for (uint16_t i = 0; i < num_props && pi_off + 48 <= pi_chunk_size; i++) {
            uint16_t filename_index;
            float px, pz;
            memcpy(&filename_index, pi_data + pi_off, 2);
            memcpy(&px, pi_data + pi_off + 2, 4);
            memcpy(&pz, pi_data + pi_off + 6, 4);
            uint8_t num_structs = 0;
            memcpy(&num_structs, pi_data + pi_off + 47, 1);

            if (filename_index < fn_count) {
                uint32_t fid = prop_file_ids[filename_index];
                if (IsPortalModelFileId(fid)) {
                    out.push_back({{px, pz}, fid});
                }
            }
            pi_off += 48 + num_structs * 8;
        }
    }

    bool LoadPortalPropsFromDAT(uint32_t map_file_id, std::vector<PortalProp>& out)
    {
        ArenaNetFileParser::ArenaNetFile game_asset;
        if (!game_asset.readFromDat(map_file_id, 1)) return false;
        ParsePortalProps(game_asset, out);
#if 0   // Diagnostic: dump prop file IDs when a map yields no portals. Noisy — disabled but kept.
        if (out.empty()) {
            // Dump all prop file IDs for debugging when no portals found
            const auto fn_chunk = game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_PropFilenames);
            if (fn_chunk) {
                auto* fn_data = reinterpret_cast<const uint8_t*>(fn_chunk) + sizeof(*fn_chunk);
                uint32_t fn_size = fn_chunk->chunk_size;
                uint32_t fn_count = (fn_size >= 5) ? (fn_size - 5) / 6 : 0;
                std::string ids;
                for (uint32_t i = 0; i < fn_count && i < 20; i++) {
                    uint16_t id0, id1;
                    memcpy(&id0, fn_data + 5 + i * 6, 2);
                    memcpy(&id1, fn_data + 5 + i * 6 + 2, 2);
                    uint32_t fid = (id0 > 0xff && id1 > 0xff)
                        ? (id0 - 0xff00ff) + static_cast<uint32_t>(id1) * 0xff00 : 0;
                    ids += std::format(" 0x{:X}", fid);
                }
                Log::Info("[PortalProps] fid=0x%X: 0 portals found. %d prop types:%s",
                    map_file_id, fn_count, ids.c_str());
            } else {
                Log::Info("[PortalProps] fid=0x%X: no PropFilenames chunk", map_file_id);
            }
        }
#endif
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

        // Parse into intermediate format first
        std::vector<detail::ParsedPlane> parsed_planes(plane_count);
        for (uint32_t i = 0; i < plane_count; ++i) {
            if (!detail::parse_plane(chunk_data, off, chunk_size, parsed_planes[i])) {
                return false;
            }
            // Set zplane (plane 0 = ground = UINT32_MAX, others = 0, 1, 2, ...)
            parsed_planes[i].zplane = (i == 0) ? UINT32_MAX : (i - 1);
        }

        // Convert to CopiedPathingMap planes
        PathingMapData result;
        result.planes.resize(plane_count);

        // Compute global trapezoid IDs (each plane gets consecutive IDs)
        uint32_t global_trap_id = 0;
        std::vector<uint32_t> plane_trap_id_start(plane_count);
        for (uint32_t i = 0; i < plane_count; ++i) {
            plane_trap_id_start[i] = global_trap_id;
            global_trap_id += static_cast<uint32_t>(parsed_planes[i].trapezoids.size());
        }
        result.pathNodeSize = global_trap_id;

        for (uint32_t i = 0; i < plane_count; ++i) {
            if (!ConvertParsedPlaneToCopied(parsed_planes[i], result.planes[i], plane_trap_id_start[i])) {
                return false;
            }
            // Set portal_plane for each portal
            for (uint32_t j = 0; j < result.planes[i].portal_count; ++j) {
                result.planes[i].portals[j].portal_plane = static_cast<uint16_t>(i);
            }
        }

        // Resolve portal pairs across planes using shared_id
        // Build shared_id -> portal map per plane
        for (uint32_t i = 0; i < plane_count; ++i) {
            for (uint32_t pi = 0; pi < result.planes[i].portal_count; ++pi) {
                GW::Portal& portal = result.planes[i].portals[pi];
                const auto& pp = parsed_planes[i].portals[pi];

                if (pp.shared_id == 0xFFFF || pp.neighbor_plane >= plane_count) continue;

                // Find matching portal on neighbor plane with same shared_id
                uint16_t neighbor_idx = pp.neighbor_plane;
                for (uint32_t npi = 0; npi < result.planes[neighbor_idx].portal_count; ++npi) {
                    const auto& neighbor_parsed = parsed_planes[neighbor_idx].portals[npi];
                    if (neighbor_parsed.shared_id == pp.shared_id &&
                        neighbor_parsed.neighbor_plane == i) {
                        portal.pair = &result.planes[neighbor_idx].portals[npi];
                        break;
                    }
                }
            }
        }

        *out = std::move(result);

        const auto map_info_chunk = (MapInfoChunk*)game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_Info);
        if (!map_info_chunk) return false;

        out->bounds_min = map_info_chunk->bounds[0];
        out->bounds_max = map_info_chunk->bounds[1];

        out->map_file_id = map_file_id;

        // Parse portal props
        ParsePortalProps(game_asset, out->portal_props);
        out->total_props_parsed = 0; // not tracked in shared helper
        out->total_prop_filenames = 0;

        return true;
    }

    bool GetMapGameBoundsFromDAT(uint32_t map_file_id, Vec2f& bounds_min, Vec2f& bounds_max)
    {
        if (!map_file_id) return false;

        static std::mutex mtx;
        static std::unordered_map<uint32_t, std::pair<Vec2f, Vec2f>> cache;
        {
            std::scoped_lock lock(mtx);
            if (const auto it = cache.find(map_file_id); it != cache.end()) {
                bounds_min = it->second.first;
                bounds_max = it->second.second;
                return true;
            }
        }

        PathingMapData data;
        if (!LoadPathingMapDataFromDAT(map_file_id, &data)) return false;

        std::scoped_lock lock(mtx);
        cache[map_file_id] = {data.bounds_min, data.bounds_max};
        bounds_min = data.bounds_min;
        bounds_max = data.bounds_max;
        return true;
    }

} // namespace Pathing
