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
    namespace Constants {
        enum class MapID : uint32_t;
    }

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
        /* +h0028 */ uint32_t h0028;
        /* +h002C */ uint32_t h002C;
        /* +h0030 */ uint32_t h0030;
        /* +h0034 */ uint32_t h0034;
        /* +h0038 */ uint32_t h0038;
        /* +h003C */ uint32_t h003C;
        /* +h0040 */ uint32_t h0040;
        /* +h0044 */ uint32_t h0044;
        /* +h0048 */ uint32_t h0048;
        /* +h004C */ uint32_t h004C;
        /* +h0050 */ uint32_t h0050;
        /* +h0054 */ uint32_t h0054;
        /* +h0058 */ uint32_t h0058;
        /* +h005C */ uint32_t h005C;
        /* +h0060 */ uint32_t h0060;
        /* +h0064 */ uint32_t h0064;
        /* +h0068 */ uint32_t h0068;
        /* +h006C */ uint32_t h006C;
        /* +h0070 */ uint32_t h0070;
        /* +h0074 */ uint32_t h0074;
        /* +h0078 */ uint32_t h0078;
        /* +h007C */ uint32_t h007C;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ uint32_t nextTrapezoidId; // Starts at 0, increment everytime a trapezoid is created. It's used to assign a unique trapezoid id to every trapezoid. Used for path finding.
        /* +h0088 */ uint32_t h0088;
        /* +h008C */ GW::Constants::MapID map_id;
        /* +h0090 */ uint32_t h0090;
        /* +h0094 */ uint32_t h0094;
        /* +h0098 */ uint32_t h0098;
        /* +h009C */ uint32_t h009C;
    };
    static_assert(sizeof(MapStaticData) == 0xA0, "struct MapStaticData has incorrect size");

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
        /* +h0010 */ PathNodeArray pathNodes; // This array of pathNodes are indexed with the trapezoid id.
        /* +h001C */ NodeCache nodeCache;
        /* +h0030 */ PrioQ<PathNode> openList;
        /* +h0044 */ ObjectPool freeIPathNode;
        /* +h0050 */ PathNodeArray allocatedPathNodes; // This is just an array of all allocated path nodes used to cleanup. The order is the allocation order.
        /* +h005C */ uint32_t h005C;
        /* +h0060 */ uint32_t h0060;
        /* +h0064 */ Array<PathWaypoint> waypoints;
        /* +h0074 */ Array<struct Node*> nodeStack;
        /* +h0084 */ uint32_t h0084;
        /* +h0088 */ uint32_t h0088;
        /* +h008C */ uint32_t h008C;
        /* +h0090 */ uint32_t h0090;
    };
    static_assert(sizeof(PathContext) == 0x94, "struct PathContext has incorrect size");

    // The game can optionally load a DLL to do the path finding.
    // The DLL is named "PathEngine.dll", but not clear if it's a 3rd party or just their name
    // for development.
    struct PathEngineContext {
        /* +h0000 */ void **vtable;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ void *user_data;
        /* +h0010 */ HMODULE hDll;
        /* +h0014 */ uint32_t pfnCreateInterface;
    };
    static_assert(sizeof(PathEngineContext) == 0x18, "struct PathEngineContext has incorrect size");

    struct MapContext {
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ Vec2f start_pos;
        /* +h000c */ Vec2f end_pos;
        /* +h0014 */ uint32_t h0014[6];
        /* +h002C */ Array<void*> spawns1; // Seem to be arena spawns. struct is X,Y,unk 4 byte value,unk 4 byte value.
        /* +h003C */ Array<void*> spawns2; // Same as above
        /* +h004C */ Array<void*> spawns3; // Same as above
        /* +h005C */ float h005C[6]; // Some trapezoid i think.
        /* +h0074 */ PathContext* path;
        /* +h0078 */ PathEngineContext* path_engine;
        /* +h007C */ PropsContext* props;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ void* terrain;
        /* +h0088 */ uint32_t h0088;
        /* +h008C */ GW::Constants::MapID map_id;
        /* +h0090 */ uint32_t h0090;
        /* +h0094 */ uint32_t h0094;
        /* +h0098 */ uint32_t h0098;
        /* +h009C */ uint32_t h009C;
        /* +h00A0 */ uint32_t h00A0;
        /* +h00A4 */ uint32_t h00A4;
        /* +h00A8 */ uint32_t h00A8;
        /* +h00AC */ uint32_t h00AC;
        /* +h00B0 */ uint32_t h00B0;
        /* +h00B4 */ uint32_t h00B4;
        /* +h00B8 */ uint32_t h00B8;
        /* +h00BC */ uint32_t h00BC;
        /* +h00C0 */ uint32_t h00C0;
        /* +h00C4 */ uint32_t h00C4;
        /* +h00C8 */ uint32_t h00C8;
        /* +h00CC */ uint32_t h00CC;
        /* +h00D0 */ uint32_t h00D0;
        /* +h00D4 */ uint32_t h00D4;
        /* +h00D8 */ uint32_t h00D8;
        /* +h00DC */ uint32_t h00DC;
        /* +h00E0 */ uint32_t h00E0;
        /* +h00E4 */ uint32_t h00E4;
        /* +h00E8 */ uint32_t h00E8;
        /* +h00EC */ uint32_t h00EC;
        /* +h00F0 */ uint32_t h00F0;
        /* +h00F4 */ uint32_t h00F4;
        /* +h00F8 */ uint32_t h00F8;
        /* +h00FC */ uint32_t h00FC;
        /* +h0100 */ uint32_t h0100;
        /* +h0104 */ uint32_t h0104;
        /* +h0108 */ uint32_t h0108;
        /* +h010C */ uint32_t h010C;
        /* +h0110 */ uint32_t h0110;
        /* +h0114 */ uint32_t h0114;
        /* +h0118 */ uint32_t h0118;
        /* +h011C */ uint32_t h011C;
        /* +h0120 */ uint32_t h0120;
        /* +h0124 */ uint32_t h0124;
        /* +h0128 */ uint32_t h0128;
        /* +h012C */ uint32_t h012C;
        /* +h0130 */ void* zones;
        /* +h0134 */ uint32_t h0134;
    };
    static_assert(sizeof(MapContext) == 0x138, "struct MapContext has incorrect size");
    GWCA_API MapContext* GetMapContext();
}
