#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {
    struct GameplayContext;
    GWCA_API GameplayContext* GetGameplayContext();

    // Static stuff used at runtime
    struct GameplayContext {
        /* +h0000 */ uint32_t h0000[0x13];
        float mission_map_zoom;
        uint32_t unk[10];
    };
    static_assert(sizeof(GameplayContext) == 0x78);
}
