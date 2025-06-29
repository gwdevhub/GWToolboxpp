#pragma once

#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/ObjectPool.h>
#include <GWCA/GameContainers/PrioQ.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct PathingMap;
    struct MapProp;
    struct PropByType;
    struct PropModelInfo;

    typedef Array<PathingMap> PathingMapArray;

    struct PropsContext {
        /* +h0000 */ uint32_t pad1[0x1b];
        /* +h006C */ Array<TList<PropByType>> propsByType;
        /* +h007C */ uint32_t h007C[0xa];
        /* +h00A4 */ Array<PropModelInfo> propModels;
        /* +h00B4 */ uint32_t h00B4[0x38];
        /* +h0194 */ Array<MapProp*> propArray;
    };
    static_assert(sizeof(PropsContext) == 0x1A4, "struct PropsContext has incorrect size");

    struct MapStaticData {
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ uint32_t trapezoidCount;
        /* +h0018 */ PathingMapArray map;
    };

    // Those are planes that are blocked and can be unblocked at runtime. e.g., the gates in foundry
    // Those aren't in the dat file, but sent from the server
    typedef BaseArray<uint32_t> BlockedPlaneArray;
    static_assert(sizeof(BlockedPlaneArray) == 0xC, "struct BlockedPlaneArray has incorrect size");

    // The game leak this type name and it's `IPath::PathNode`
    struct PathNode {
        /* +h0000 */ uint32_t closed;
        /* +h0004 */ float costToNode;
        /* +h0008 */ PrioQLink<PathNode*> priority;
        /* +h0018 */ GamePos nodePos; // This position is a guess on the best position to pass through. It's usually on one of the 4 edges of a trapezoid.
        /* +h0024 */ struct PathingTrapezoid *currentTrapezoid;
        /* +h0028 */ PathNode *parentPathMap;
    };
    static_assert(sizeof(PathNode) == 0x2C, "struct PathNode has incorrect size");

    typedef BaseArray<PathNode*> PathNodeArray;
    static_assert(sizeof(PathNodeArray) == 0xC, "struct PathNodeArray has incorrect size");

    struct NodeCache {
        /* +h0000 */ uint32_t* cachedCount;
        /* +h0004 */ uint32_t m_mask;
        /* +h0008 */ BaseArray<uint32_t> buffer;
    };
    static_assert(sizeof(NodeCache) == 0x14, "struct NodeCache has incorrect size");

    struct PathWaypoint {
        /* +h0000 */ float x;
        /* +h0004 */ float y;
        /* +h0008 */ float width;
        /* +h000C */ float height;
        /* +h0010 */ uint32_t plane;
        /* +h0014 */ PathingTrapezoid *nextTrap;
    };
    static_assert(sizeof(PathWaypoint) == 0x18, "struct PathWaypoint has incorrect size");

    struct PathContext {
        /* +h0000 */ MapStaticData* staticData;
        /* +h0004 */ BlockedPlaneArray blockedPlanes;
        /* +h0010 */ PathNodeArray pathMaps;
        /* +h001C */ NodeCache nodeCache;
        /* +h0030 */ PrioQ<PathNode> openList;
        /* +h0044 */ ObjectPool freeIPathNode;
        /* +h0050 */ PathNodeArray pathNodes;
        /* +h005C */ uint32_t h005C;
        /* +h0060 */ uint32_t h0060;
        /* +h0064 */ Array<PathWaypoint> waypoints;
        /* +h0074 */ Array<struct Node*> nodeStack;
    };

    struct MapContext {
        /* +h0000 */ float map_boundaries[5];
        /* +h0014 */ uint32_t h0014[6];
        /* +h002C */ Array<void *> spawns1; // Seem to be arena spawns. struct is X,Y,unk 4 byte value,unk 4 byte value.
        /* +h003C */ Array<void *> spawns2; // Same as above
        /* +h004C */ Array<void *> spawns3; // Same as above
        /* +h005C */ float h005C[6]; // Some trapezoid i think.
        /* +h0074 */ PathContext *path;
        /* +h0078 */ uint8_t pad1[4];
        /* +h007C */ PropsContext *props;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ void* terrain;
        /* +h0088 */ uint32_t h0088[42];
        /* +h0130 */ void* zones;
        //... Player coords and shit beyond this point if they are desirable :p
    };

    GWCA_API MapContext* GetMapContext();
}
