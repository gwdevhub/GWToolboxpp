#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct AccountContext;
    GWCA_API AccountContext* GetAccountContext();

    struct AccountUnlockedCount {
        uint32_t id;
        uint32_t unk1;
        uint32_t unk2;
    };

    struct AccountUnlockedItemInfo {
        uint32_t name_id;
        uint32_t mod_struct_index; // Used to find mod struct in unlocked_pvp_items_mod_structs...
        uint32_t mod_struct_size;
    };

    struct AccountContext {
        /* +h0000 */ Array<AccountUnlockedCount> account_unlocked_counts; // e.g. number of unlocked storage panes
        /* +h0010 */ uint8_t h0010[0xA4];
        /* +h00b4 */ Array<uint32_t> unlocked_pvp_heros; // Unused, hero battles is no more :(
        /* +h00c4 */ Array<uint32_t> h00c4;// If an item is unlocked, the mod struct is stored here. Use unlocked_pvp_items_info to find the index. Idk why, chaos reigns I guess
        /* +h00e4 */ Array<AccountUnlockedItemInfo> unlocked_pvp_item_info; // If an item is unlocked, the details are stored here
        /* +h00f4 */ Array<uint32_t> unlocked_pvp_items; // Bitwise array of which pvp items are unlocked
        /* +h0104 */ uint8_t h0104[0x30]; // Some arrays, some linked lists, meh
        /* +h0124 */ Array<uint32_t> unlocked_account_skills; // List of skills unlocked (but not learnt) for this account, i.e. skills that heros can use, tomes can unlock
        /* +h0134 */ uint32_t account_flags;
    };
    static_assert(sizeof(AccountContext) == 0x138);
}
