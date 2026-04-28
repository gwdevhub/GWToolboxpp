#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct ItemContext;
    GWCA_API ItemContext* GetItemContext();

    struct Bag;
    struct Item;
    struct Inventory;

    struct InventoryTableEntry {
        uint32_t stride;
        uint32_t end;
        GW::Inventory* start;
    };

    struct ItemContext { // total: 0x10C/268 BYTEs
        /* +h0000 */ Array<void*> h0000;
        /* +h0010 */ Array<void*> h0010;
        /* +h0020 */ DWORD h0020;
        /* +h0024 */ Array<Bag*> bags_array;
        /* +h0034 */ uint32_t h0034;
        /* +h0038 */ uint32_t h0038;
        /* +h003C */ uint32_t h003C;
        /* +h0040 */ Array<void*> h0040;
        /* +h0050 */ Array<void*> h0050;
        /* +h0060 */ uint32_t h0060;
        /* +h0064 */ uint32_t h0064;
        /* +h0068 */ uint32_t h0068;
        /* +h006C */ uint32_t h006C;
        /* +h0070 */ uint32_t h0070;
        /* +h0074 */ uint32_t h0074;
        /* +h0078 */ uint32_t h0078;
        /* +h007C */ uint32_t h007C;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ uint32_t h0084;
        /* +h0088 */ uint32_t h0088;
        /* +h008C */ uint32_t h008C;
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
        /* +h00B8 */ Array<Item*> item_array;
        /* +h00C8 */ uint32_t h00C8;
        /* +h00CC */ uint32_t h00CC;
        /* +h00D0 */ uint32_t h00D0;
        /* +h00D4 */ uint32_t h00D4;
        /* +h00D8 */ uint32_t h00D8;
        /* +h00DC */ uint32_t h00DC;
        /* +h00E0 */ uint32_t h00E0;
        /* +h00E4 */ Array<InventoryTableEntry> inventory_table;
        /* +h00F4 */ uint32_t h00F4;
        /* +h00F8 */ Inventory* inventory;
        /* +h00FC */ Array<void*> h00FC;
    };
    static_assert(sizeof(ItemContext) == 0x10c);
}
