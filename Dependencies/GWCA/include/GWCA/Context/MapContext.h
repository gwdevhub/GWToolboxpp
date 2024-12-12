#pragma once

#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>
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


    struct MapContext {
        /* +h0000 */ float map_boundaries[5];
        /* +h0014 */ uint32_t h0014[6];
        /* +h002C */ Array<void *> spawns1; // Seem to be arena spawns. struct is X,Y,unk 4 byte value,unk 4 byte value.
        /* +h003C */ Array<void *> spawns2; // Same as above
        /* +h004C */ Array<void *> spawns3; // Same as above
        /* +h005C */ float h005C[6]; // Some trapezoid i think.
        /* +h0074 */ struct sub1 {
            struct sub2 {
                uint32_t pad1[6];
                PathingMapArray pmaps;
            } *sub2;
            /* +h0004 */ Array<uint32_t> pathing_map_block;
            /* +h0018 */ uint32_t total_trapezoid_count;
            /* +h0018 */ uint32_t h0014[0x12];
            /* +h0060 */ Array<TList<void*>> something_else_for_props;
            //... Bunch of arrays and shit
        } *sub1;
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
