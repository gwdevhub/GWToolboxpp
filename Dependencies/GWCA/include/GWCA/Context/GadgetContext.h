#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {
    struct GadgetInfo {
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ wchar_t *name_enc;
    };

    struct GadgetContext {
        /* +h0000 */ Array<GadgetInfo> GadgetInfo;
        // ...
    };
}
