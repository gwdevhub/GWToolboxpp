#pragma once
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {
    struct PathingTrapezoid { // total: 0x30/48
        /* +h0000 */ uint32_t id;
        /* +h0004 */ PathingTrapezoid* adjacent[4];
        /* +h0014 */ uint16_t portal_left;
        /* +h0016 */ uint16_t portal_right;
        /* +h0018 */ float XTL;
        /* +h001C */ float XTR;
        /* +h0020 */ float YT;
        /* +h0024 */ float XBL;
        /* +h0028 */ float XBR;
        /* +h002C */ float YB;
    };
    static_assert(sizeof(PathingTrapezoid) == 48, "struct PathingTrapezoid has incorrect size");

    struct Node {
        /* +h0000 */ uint32_t type;   //XNode = 0, YNode = 1, SinkNode = 2
        /* +h0004 */ uint32_t id;
    };

    struct XNode : Node { // type = 0
        /* +h0008 */ Vec2f    pos;
        /* +h0010 */ Vec2f    dir;
        /* +h0018 */ Node     *left;
        /* +h001C */ Node     *right;
    };
    static_assert(sizeof(XNode) == 32, "struct XNode has incorrect size");

    struct YNode : Node { // type = 1
        /* +h0008 */ Vec2f    pos;
        /* +h0010 */ Node     *left;
        /* +h0014 */ Node     *right;
    };
    static_assert(sizeof(YNode) == 24, "struct YNode has incorrect size");

    struct SinkNode : Node { // type = 2
        /* +h0008 */ PathingTrapezoid **trapezoid;
    };
    static_assert(sizeof(SinkNode) == 12, "struct SinkNode has incorrect size");

    struct Portal { // total: 0x14/20
        /* +h0000 */ uint16_t left_layer_id;
        /* +h0002 */ uint16_t right_layer_id;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ Portal   *pair;
        /* +h000C */ uint32_t count;
        /* +h0010 */ PathingTrapezoid **trapezoids;
    };
    static_assert(sizeof(Portal) == 20, "struct Portal has incorrect size");

    struct PathingMap { // total: 0x54/84
        /* +h0000 */ uint32_t zplane; // ground plane = UINT_MAX, rest 0 based index
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ uint32_t trapezoid_count;
        /* +h0018 */ PathingTrapezoid* trapezoids;
        /* +h001C */ uint32_t sink_node_count;
        /* +h0020 */ SinkNode *sink_nodes;
        /* +h0024 */ uint32_t x_node_count;
        /* +h0028 */ XNode    *x_nodes;
        /* +h002C */ uint32_t y_node_count;
        /* +h0030 */ YNode    *y_nodes;
        /* +h0034 */ uint32_t h0034;
        /* +h0038 */ uint32_t h0038;
        /* +h003C */ uint32_t portal_count;
        /* +h0040 */ Portal   *portals;
        /* +h0044 */ Node     *root_node;
        /* +h0048 */ uint32_t *h0048;
        /* +h004C */ uint32_t *h004C;
        /* +h0050 */ uint32_t *h0050;
    };
    static_assert(sizeof(PathingMap) == 84, "struct PathingMap has incorrect size");

    struct PropByType {
        uint32_t object_id;
        uint32_t prop_index;
    };

    struct PropModelInfo {
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ uint32_t h0014;
    };
    static_assert(sizeof(PropModelInfo) == 0x18, "struct PropModelInfo has incorrect size");

    struct RecObject {
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t accessKey; // This is used by the game to make sure the data from the DAT matches the data in game
        // ...
    };
    static_assert(sizeof(RecObject) == 0xC, "struct RecObject has incorrect size");

    struct MapProp { // total: 0x54/84
        /* +h0000 */ uint32_t h0000[5];
        /* +h0014 */ uint32_t uptime_seconds; // time since spawned
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ uint32_t prop_index;
        /* +h0020 */ Vec3f position;
        /* +h002C */ uint32_t model_file_id;
        /* +h0030 */ uint32_t h0030[2];
        /* +h0038 */ float rotation_angle;
        /* +h003C */ float rotation_cos;
        /* +h003C */ float rotation_sin;
        /* +h0040 */ uint32_t h0034[5];
        /* +h0058 */ RecObject* interactive_model;
        /* +h005C */ uint32_t h005C[4];
        /* +h006C */ uint32_t appearance_bitmap; // Modified when animation changes
        /* +h0070 */ uint32_t animation_bits;
        /* +h0064 */ uint32_t h0064[5];
        /* +h0088 */ PropByType* prop_object_info;
        /* +h008C */ uint32_t h008C;
    };

    static_assert(sizeof(MapProp) == 0x90, "struct MapProp has incorrect size");

    typedef Array<PathingMap> PathingMapArray;
}
