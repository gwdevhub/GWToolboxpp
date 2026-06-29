#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Pathing.h>

// =============================================================================
// PathingMapData
//
// A self-contained representation of map pathfinding data using GW's actual
// pointer-based types (GW::PathingTrapezoid*, GW::Portal*) with a single
// continuous buffer per plane.
//
// Can be constructed from either:
//   1. GW::PathContext (live game memory)
//   2. FFNA Type-3 file chunk 0x20000008 (raw file data)
//
// Design principles:
//   - Uses GW's native pointer-based types directly
//   - Single buffer allocation per plane (mirrors GW::PathingMap::allocatedBuffer)
//   - All internal pointers resolved within the buffer
//   - No external pointer dependencies - fully self-contained deep copy
//
// This struct owns all its data and is fully self-contained.
// =============================================================================

namespace Pathing {

    struct Vec2f {
        float x, y;
    };

    // One per plane - mirrors GW::PathingMap but we own the memory
    struct CopiedPathingMap {
        uint32_t zplane = 0;
        uint32_t trapezoid_count = 0;
        GW::PathingTrapezoid* trapezoids = nullptr;
        uint32_t portal_count = 0;
        GW::Portal* portals = nullptr;
        uint32_t portal_trapezoids_count = 0;
        GW::PathingTrapezoid** portal_trapezoids = nullptr;

        // BSP tree nodes
        GW::Node* root_node = nullptr;
        uint32_t x_node_count = 0;
        GW::XNode* x_nodes = nullptr;
        uint32_t y_node_count = 0;
        GW::YNode* y_nodes = nullptr;
        uint32_t sink_node_count = 0;
        GW::SinkNode* sink_nodes = nullptr;

        uint8_t* buffer = nullptr;  // single allocation owns all above

        CopiedPathingMap() = default;
        ~CopiedPathingMap() { delete[] buffer; }
        CopiedPathingMap(const CopiedPathingMap&) = delete;
        CopiedPathingMap& operator=(const CopiedPathingMap&) = delete;
        CopiedPathingMap(CopiedPathingMap&& o) noexcept
            : zplane(o.zplane)
            , trapezoid_count(o.trapezoid_count)
            , trapezoids(o.trapezoids)
            , portal_count(o.portal_count)
            , portals(o.portals)
            , portal_trapezoids_count(o.portal_trapezoids_count)
            , portal_trapezoids(o.portal_trapezoids)
            , root_node(o.root_node)
            , x_node_count(o.x_node_count)
            , x_nodes(o.x_nodes)
            , y_node_count(o.y_node_count)
            , y_nodes(o.y_nodes)
            , sink_node_count(o.sink_node_count)
            , sink_nodes(o.sink_nodes)
            , buffer(o.buffer)
        {
            o.buffer = nullptr;
            o.trapezoids = nullptr;
            o.portals = nullptr;
            o.portal_trapezoids = nullptr;
            o.root_node = nullptr;
            o.x_nodes = nullptr;
            o.y_nodes = nullptr;
            o.sink_nodes = nullptr;
        }
        CopiedPathingMap& operator=(CopiedPathingMap&& o) noexcept {
            if (this != &o) {
                delete[] buffer;
                zplane = o.zplane;
                trapezoid_count = o.trapezoid_count;
                trapezoids = o.trapezoids;
                portal_count = o.portal_count;
                portals = o.portals;
                portal_trapezoids_count = o.portal_trapezoids_count;
                portal_trapezoids = o.portal_trapezoids;
                root_node = o.root_node;
                x_node_count = o.x_node_count;
                x_nodes = o.x_nodes;
                y_node_count = o.y_node_count;
                y_nodes = o.y_nodes;
                sink_node_count = o.sink_node_count;
                sink_nodes = o.sink_nodes;
                buffer = o.buffer;
                o.buffer = nullptr;
                o.trapezoids = nullptr;
                o.portals = nullptr;
                o.portal_trapezoids = nullptr;
                o.root_node = nullptr;
                o.x_nodes = nullptr;
                o.y_nodes = nullptr;
                o.sink_nodes = nullptr;
            }
            return *this;
        }
    };

    struct PortalProp {
        Vec2f pos;              // game coordinates (x, y)
        uint32_t model_file_id; // model file ID for portal type identification
    };

    // Complete pathing map data
    struct PathingMapData {
        uint32_t map_file_id = 0;
        uint32_t pathNodeSize = 0;
        Vec2f bounds_min{}, bounds_max{};
        std::vector<CopiedPathingMap> planes;
        std::vector<PortalProp> portal_props; // travel portal props from DAT
        uint32_t total_props_parsed = 0;   // debug: total props in DAT
        uint32_t total_prop_filenames = 0; // debug: total prop filenames in DAT

        // Points to live game blockedPlanes array (MapContext-loaded),
        // or nullptr for DAT-loaded (treat all planes as unblocked)
        const GW::BaseArray<uint32_t>* blockedPlanesPtr = nullptr;

        bool IsValid() const { return !planes.empty(); }
        size_t GetPlaneCount() const { return planes.size(); }
        size_t GetTotalTrapezoidCount() const {
            size_t t = 0;
            for (const auto& p : planes) t += p.trapezoid_count;
            return t;
        }
    };

} // namespace Pathing
