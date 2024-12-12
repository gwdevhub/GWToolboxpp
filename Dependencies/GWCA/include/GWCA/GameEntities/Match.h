#pragma once

namespace GW {
    struct ObserverMatch { // total: 0x4C/76
        /* +h0000 */ uint32_t match_id;
        /* +h0004 */ uint32_t match_id_dup;
        /* +h0008 */ uint32_t map_id;
        /* +h000C */ uint32_t age;
        /* +h0010 */ uint32_t h0010[14];
        /* +h0048 */ wchar_t *team_names;
    };
    static_assert(sizeof(ObserverMatch) == 76, "struct ObserverMatch has incorrect size");
}
