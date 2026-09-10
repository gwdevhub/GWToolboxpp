#pragma once

#include <GWCA/GameContainers/GamePos.h>
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

    // Planes blocked but unblockable at runtime (e.g. the Foundry gates); sent by the server, not in the dat file.
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

    // The game can optionally load "PathEngine.dll" for path finding; unclear if third party or their own dev name.
    struct PathEngineContext {
        /* +h0000 */ void **vtable;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ void *user_data;
        /* +h0010 */ HMODULE hDll;
        /* +h0014 */ uint32_t pfnCreateInterface;
    };
    static_assert(sizeof(PathEngineContext) == 0x18, "struct PathEngineContext has incorrect size");

    // Point lights baked into the map, parsed from its "LITE" chunk by Engine\Map\MapLight.cpp.
    // Torches, braziers and campfires are these, not props with a glow texture.
    struct MapLightDescriptor {
        /* +h0000 */ Vec3f position;
        /* +h000C */ uint8_t red;
        /* +h000D */ uint8_t green;
        /* +h000E */ uint8_t blue;
        /* +h000F */ uint8_t h000F;
        /* +h0010 */ float intensity; // colour bytes are premultiplied by intensity / 255 when the light is built
        /* +h0014 */ float inner_range;
        /* +h0018 */ float outer_range;
    };
    static_assert(sizeof(MapLightDescriptor) == 0x1C, "struct MapLightDescriptor has incorrect size");

    // Field layout taken from MapLightApplyDescriptor, which hands each field straight to a GrLight setter:
    //   GrLightSetColour(light, &desc->red, desc->intensity), GrLightSetPosition(light, &desc->position, 0),
    //   GrLightSetRange(light, desc->inner_range, desc->outer_range)
    struct MapLightContext {
        /* +h0000 */ Array<MapLightDescriptor> descriptors;
        /* +h0010 */ Array<uint32_t> lights; // HGrLight handles, parallel to descriptors (same count)
    };
    static_assert(sizeof(MapLightContext) == 0x20, "struct MapLightContext has incorrect size");

    // Only the fields below are confirmed; the real object is at least 0x1A0 bytes, so no size assert.
    // Only present when MapContext::flags bit 1 is set.
    struct MapWaterContext {
        /* +h0000 */ uint32_t h0000[0x2b];
        /* +h00AC */ uint32_t scene_program; // the single handle water contributes to the scene program list
        /* +h00B0 */ uint32_t h00B0[0xc];
        /* +h00E0 */ uint32_t shader_programs[2]; // two of the four per-map shader programs, chosen by quality
        /* +h00E8 */ uint32_t h00E8;
        /* +h00EC */ float plane_z; // world height of the water plane
        /* +h00F0 */ uint32_t h00F0[0x2a];
        /* +h0198 */ uint32_t shader_programs2[2]; // the other two quality variants
    };

    // === terrain textures / baked terrain shadows ===
    //
    // GW bakes the map's static terrain shadows into the terrain textures, and streams them in
    // per tile. The data lives in two layers, both reachable from the terrain texture context:
    //   1. a per-texel bitmask, entropy coded per tile and decoded on demand into a scratch
    //      buffer (soft edges, detail) - TrnTexShadowDecompressTile;
    //   2. a per-block "this whole block is in shadow" bitmask, held in the map's own data and
    //      tested first - a set bit fills the block flat with the shadow colour without ever
    //      consulting layer 1 - TrnTexComposeTileLighting.
    // Clearing only layer 1 leaves the solid interiors of large shadows behind, which is what
    // makes layer 2 worth documenting.

    // 32x32 blocks, one bit each, MSB first, 4 bytes per row; 0x80 bytes in total.
    // Quality 0 walks it as two 16-row halves (the second at +0x40), every other quality walks
    // all 32 rows in one pass - both reach exactly 0x80 bytes, so that is the whole mask.
    typedef uint8_t TerrainBlockShadowMask[0x80];

    // One per terrain tile, indexed by `shadow_grid_width * tile_y + tile_x`.
    struct TerrainShadowRecord {
        /* +h0000 */ void* compressed_stream; // layer 1, entropy coded, decoded per tile
        /* +h0004 */ void* compressed_stream_end;
        /* +h0008 */ TerrainBlockShadowMask* block_mask; // layer 2, the map's own data - not scratch
    };
    static_assert(sizeof(TerrainShadowRecord) == 0xC, "struct TerrainShadowRecord has incorrect size");

    struct TerrainTexTile {
        /* +h0000 */ uint32_t h0000[3];
        /* +h000C */ uint32_t tile_x;
        /* +h0010 */ uint32_t tile_y;
        /* +h0014 */ uint32_t state; // set to 2 once the tile's shadow layer has been decoded
        /* +h0018 */ uint32_t h0018; // zeroed alongside the state above
    };

    // A single streamed terrain texture. Only the two fields below are confirmed, so no size assert.
    struct TerrainTexture {
        /* +h0000 */ uint32_t h0000[0x27];
        /* +h009C */ uint32_t stream_flags; // bit 0x800: decompress + upload still pending
        /* +h00A0 */ uint32_t h00A0[0x37];
        /* +h017C */ TerrainTexTile* tile; // tagged pointer: null, or bit 0 set, means no tile
    };

    // The map-wide terrain texture context (`TrnTex`). Partially mapped, so no size assert.
    struct TerrainTexContext {
        /* +h0000 */ uint32_t h0000[0x2a];
        // The bound tested by the game itself before indexing, so a tile outside the grid is
        // simply "no baked shadow" rather than a read off the end.
        /* +h00A8 */ BaseArray<TerrainShadowRecord> shadow_records;
        // Row stride for the index above. The three fields at +0xA8/+0xB0/+0xB4 read equally well
        // as an Array<TerrainShadowRecord> whose m_param happens to be the width; naming it is
        // the more useful of two identical layouts.
        /* +h00B4 */ uint32_t shadow_grid_width;
        /* +h00B8 */ uint32_t h00B8[0x10b];
        // Scratch, shared by every tile this context streams: layer 1 is decoded here and
        // uploaded, then the buffer is reused for the next tile. 0x110 rows of 0x22 bytes =
        // 272x272 bits, one per terrain texel. A SET bit is shadow, a clear bit is lit.
        /* +h04E4 */ uint8_t decoded_shadow_tile[0x110][0x22];
    };
    // Self-check on the padding above; the real object is at least this big, not exactly.
    static_assert(sizeof(TerrainTexContext) == 0x2904, "struct TerrainTexContext has incorrect layout");

    struct MapContext {
        /* +h0000 */ uint32_t map_type; // less than 4
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
        /* +h0084 */ void* terrain; // relationship to TerrainTexContext is unconfirmed
        /* +h0088 */ void* collision; // "Collision" map chunk (Engine\Map\Collision\CollApi.cpp)
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
        /* +h00B4 */ Vec3f view_eye;    // cached by GmWorldUpdateView
        /* +h00C0 */ Vec3f view_target;
        /* +h00CC */ Vec3f view_up;
        /* +h00D8 */ uint32_t h00D8;
        /* +h00DC */ uint32_t view_flags; // bit 0: view has been updated this frame
        /* +h00E0 */ uint32_t h00E0;
        /* +h00E4 */ uint32_t h00E4;
        /* +h00E8 */ uint32_t h00E8;
        /* +h00EC */ MapLightContext* lights; // "Light" map chunk
        /* +h00F0 */ uint32_t h00F0;
        /* +h00F4 */ uint32_t h00F4;
        /* +h00F8 */ uint32_t h00F8;
        /* +h00FC */ uint32_t h00FC;
        /* +h0100 */ uint32_t h0100;
        /* +h0104 */ uint32_t h0104;
        /* +h0108 */ uint32_t flags; // bit 1: map has water
        /* +h010C */ uint32_t h010C;
        /* +h0110 */ uint32_t h0110;
        /* +h0114 */ uint32_t h0114;
        /* +h0118 */ uint32_t h0118;
        /* +h011C */ uint32_t h011C;
        /* +h0120 */ void* shore; // "Shore" map chunk, only loaded when flags bit 1 is set
        /* +h0124 */ uint32_t h0124;
        /* +h0128 */ uint32_t h0128;
        /* +h012C */ MapWaterContext* water;
        /* +h0130 */ void* zones;
        /* +h0134 */ uint32_t h0134;
    };
    static_assert(sizeof(MapContext) == 0x138, "struct MapContext has incorrect size");
    GWCA_API MapContext* GetMapContext();
}
