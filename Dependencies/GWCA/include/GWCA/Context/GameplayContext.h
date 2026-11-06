#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Export.h>

namespace GW {
    struct GameplayContext;
    GameplayContext* GetGameplayContext();

    // Static stuff used at runtime
    struct GameplayContext {
        /* +h0000 */ uint32_t h0000[0x13];
        float mission_map_zoom;
        uint32_t unk[10];
    };
    static_assert(sizeof(GameplayContext) == 0x78);
}

namespace GW {
    extern "C" GWCA_API GameplayContext* __cdecl GW_GetGameplayContext() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline GameplayContext* GetGameplayContext()
    {
        return GWCA_CALL_C(GW_GetGameplayContext);
    }
}
#endif
