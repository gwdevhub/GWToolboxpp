#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {
    typedef uint32_t PlayerID;

    struct Player { // total: 0x4C/76
        /* +h0000 */ uint32_t agent_id;
        /* +h0004 */ uint32_t h0004[3];
        /* +h0010 */ uint32_t appearance_bitmap;
        /* +h0014 */ uint32_t flags; // Bitwise field
        /* +h0018 */ uint32_t primary;
        /* +h001C */ uint32_t secondary;
        /* +h0020 */ uint32_t h0020;
        /* +h0024 */ wchar_t *name_enc;
        /* +h0028 */ wchar_t *name;
        /* +h002C */ uint32_t party_leader_player_number;
        /* +h0030 */ uint32_t active_title_tier;
        /* +h0034 */ uint32_t player_number;
        /* +h0038 */ uint32_t party_size;
        /* +h003C */ Array<void*> h003C;

        inline bool IsPvP() {
            return (flags & 0x800) != 0;
        }

    };
    static_assert(sizeof(Player) == 0x4c, "struct Player has incorrect size");

    typedef Array<Player> PlayerArray;
}
