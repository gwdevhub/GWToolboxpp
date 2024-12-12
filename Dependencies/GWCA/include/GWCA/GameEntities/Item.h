#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Constants/ItemIDs.h>

namespace GW {
    typedef uint32_t ItemID;
    namespace Constants {
        enum class Bag : uint8_t;
        enum class ItemType : uint8_t;

        enum class MaterialSlot : uint32_t;

        enum class BagType {
            None,
            Inventory,
            Equipped,
            NotCollected,
            Storage,
            MaterialStorage
        };
    }

    struct Item;
    typedef Array<Item *> ItemArray;

    enum class DyeColor : uint8_t {
        None   = 0,
        Blue   = 2,
        Green  = 3,
        Purple = 4,
        Red    = 5,
        Yellow = 6,
        Brown  = 7,
        Orange = 8,
        Silver = 9,
        Black  = 10,
        Gray   = 11,
        White  = 12,
        Pink   = 13
    };
    struct DyeInfo {
        uint8_t dye_tint;
        DyeColor dye1 : 4;
        DyeColor dye2 : 4;
        DyeColor dye3 : 4;
        DyeColor dye4 : 4;
    };
    static_assert(sizeof(DyeInfo) == 3, "struct DyeInfo has incorrect size");

    struct ItemData {
        uint32_t model_file_id = 0;
        GW::Constants::ItemType type = (GW::Constants::ItemType)0xff;
        GW::DyeInfo dye = {};
        uint32_t value = 0;
        uint32_t interaction = 0;
    };
    static_assert(sizeof(ItemData) == 0x10, "struct ItemData has incorrect size");

    struct MaterialCost {
        GW::Constants::MaterialSlot material;
        uint32_t amount;
        uint32_t h0008;
        uint32_t h000c;
    };
    static_assert(sizeof(MaterialCost) == 0x10);

    struct ItemFormula {
        uint32_t h0000;
        uint32_t gold_cost;
        uint32_t skill_point_cost;
        uint32_t material_cost_count;
        MaterialCost* material_cost_buffer; // NB: The game stores a cached array of material amounts that the player has in inventory; we don't care about it though!
    };
    static_assert(sizeof(ItemFormula) == 0x14);

    struct Bag { // total: 0x28/40
        /* +h0000 */ Constants::BagType bag_type;
        /* +h0004 */ uint32_t index;
        /* +h0008 */ uint32_t _unknown0;
        /* +h000C */ uint32_t container_item;
        /* +h0010 */ uint32_t items_count;
        /* +h0014 */ Bag      *bag_array;
        /* +h0018 */ ItemArray items;

        bool IsInventoryBag()       const { return bag_type == Constants::BagType::Inventory; }
        bool IsStorageBag()         const { return bag_type == Constants::BagType::Storage; }
        bool IsMaterialStorage()    const { return bag_type == Constants::BagType::MaterialStorage; }

        static const size_t npos = (size_t)-1;

        size_t find_dye(uint32_t model_id, DyeInfo ExtraId, size_t pos = 0) const;

        size_t find1(uint32_t model_id, size_t pos = 0) const;
        size_t find2(const Item *item, size_t pos = 0) const;

        [[nodiscard]] Constants::Bag bag_id() const
        {
            return static_cast<Constants::Bag>(index + 1);
        }
    };
    static_assert(sizeof(Bag) == 40, "struct Bag has incorrect size");

    struct ItemModifier {
        uint32_t mod = 0;

        uint32_t identifier() const { return mod >> 16; }
        uint32_t arg1() const { return (mod & 0x0000FF00) >> 8; }
        uint32_t arg2() const { return (mod & 0x000000FF); }
        uint32_t arg() const { return (mod & 0x0000FFFF); }
        operator bool() const { return mod != 0; }
    };

    struct Item { // total: 0x54/84
        /* +h0000 */ uint32_t       item_id;
        /* +h0004 */ uint32_t       agent_id;
        /* +h0008 */ Bag           *bag_equipped; // Only valid if Item is a equipped Bag
        /* +h000C */ Bag           *bag;
        /* +h0010 */ ItemModifier  *mod_struct; // Pointer to an array of mods.
        /* +h0014 */ uint32_t       mod_struct_size; // Size of this array.
        /* +h0018 */ wchar_t       *customized;
        /* +h001C */ uint32_t       model_file_id;
        /* +h0020 */ GW::Constants::ItemType        type;
        /* +h0021 */ DyeInfo        dye;
        /* +h0024 */ uint16_t       value;
        /* +h0026 */ uint16_t       h0026;
        /* +h0028 */ uint32_t       interaction;
        /* +h002C */ uint32_t       model_id;
        /* +h0030 */ wchar_t       *info_string;
        /* +h0034 */ wchar_t       *name_enc;
        /* +h0038 */ wchar_t       *complete_name_enc; // with color, quantity, etc.
        /* +h003C */ wchar_t       *single_item_name; // with color, w/o quantity, named as single item
        /* +h0040 */ uint32_t       h0040[2];
        /* +h0048 */ uint16_t       item_formula;
        /* +h004A */ uint8_t        is_material_salvageable; // Only valid for type 11 (Materials)
        /* +h004B */ uint8_t        h004B; // probably used for quantity extension for new material storage
        /* +h004C */ uint16_t       quantity;
        /* +h004E */ uint8_t        equipped;
        /* +h004F */ uint8_t        profession;
        /* +h0050 */ uint8_t        slot;

        inline bool GetIsStackable() const {
            return (interaction & 0x80000) != 0;
        }

        inline bool GetIsInscribable() const {
            return (interaction & 0x08000000) != 0;
        }

        GWCA_API bool GetIsMaterial() const;
        GWCA_API bool GetIsZcoin() const;
        GW::ItemModifier* GetModifier(const uint32_t identifier) const;
    };
    static_assert(sizeof(Item) == 84, "struct Item has incorrect size");

    struct WeaponSet { // total: 0x8/8
        /* +h0000 */ Item *weapon;
        /* +h0004 */ Item *offhand;
    };
    static_assert(sizeof(WeaponSet) == 8, "struct WeaponSet has incorrect size");

    struct Inventory { // total: 0x98/152
        union {
        /* +h0000 */ Bag *bags[23];
            struct {
        /* +h0000 */ Bag *unused_bag;
        /* +h0004 */ Bag *backpack;
        /* +h0008 */ Bag *belt_pouch;
        /* +h000C */ Bag *bag1;
        /* +h0010 */ Bag *bag2;
        /* +h0014 */ Bag *equipment_pack;
        /* +h0018 */ Bag *material_storage;
        /* +h001C */ Bag *unclaimed_items;
        /* +h0020 */ Bag *storage1;
        /* +h0024 */ Bag *storage2;
        /* +h0028 */ Bag *storage3;
        /* +h002C */ Bag *storage4;
        /* +h0030 */ Bag *storage5;
        /* +h0034 */ Bag *storage6;
        /* +h0038 */ Bag *storage7;
        /* +h003C */ Bag *storage8;
        /* +h0040 */ Bag *storage9;
        /* +h0044 */ Bag *storage10;
        /* +h0048 */ Bag *storage11;
        /* +h004C */ Bag *storage12;
        /* +h0050 */ Bag *storage13;
        /* +h0054 */ Bag *storage14;
        /* +h0058 */ Bag *equipped_items;
            };
        };
        /* +h005C */ Item *bundle;
        /* +h0060 */ uint32_t h0060;
        union {
        /* +h0064 */ WeaponSet weapon_sets[4];
            struct {
        /* +h0064 */ Item *weapon_set0;
        /* +h0068 */ Item *offhand_set0;
        /* +h006C */ Item *weapon_set1;
        /* +h0070 */ Item *offhand_set1;
        /* +h0074 */ Item *weapon_set2;
        /* +h0078 */ Item *offhand_set2;
        /* +h007C */ Item *weapon_set3;
        /* +h0080 */ Item *offhand_set3;
            };
        };
        /* +h0084 */ uint32_t active_weapon_set;
        /* +h0088 */ uint32_t h0088[2];
        /* +h0090 */ uint32_t gold_character;
        /* +h0094 */ uint32_t gold_storage;
    };
    static_assert(sizeof(Inventory) == 152, "struct Inventory has incorrect size");

    // Static struct for info about available item upgrade info, used for PvP Equipment window
    struct PvPItemUpgradeInfo {
        uint32_t file_id;
        uint32_t name_id;
        uint32_t upgrade_type; // Axe, Bow, Inscription
        uint32_t campaign_id;
        uint32_t interaction;
        uint32_t is_dev; // boolean; if 1, then don't use in-game
        uint32_t profession; // if 0xb then is for all professions
        uint32_t h0018;
        uint32_t mod_struct_size;
        uint32_t* mod_struct;
    };
    static_assert(sizeof(PvPItemUpgradeInfo) == 0x28);

    struct PvPItemInfo {
        uint32_t unk[9];
    };
    static_assert(sizeof(PvPItemInfo) == 0x24);

    struct CompositeModelInfo {
        uint32_t class_flags;
        uint32_t file_ids[11];
    };

    static_assert(sizeof(CompositeModelInfo) == 0x30);

    struct SalvageSessionInfo {
        void* vtable;
        uint32_t frame_id;
        uint32_t item_id;
        uint32_t salvagable_1; // Prefix
        uint32_t salvagable_2; // Suffix
        uint32_t salvagable_3; // Inscription
        uint32_t chosen_salvagable; // 3 for materials
        uint32_t h001c;
        uint32_t kit_id;
    };
    static_assert(sizeof(SalvageSessionInfo) == 0x24);

    typedef Array<ItemID> MerchItemArray;

    inline size_t Bag::find1(uint32_t model_id, size_t pos) const {
        for (size_t i = pos; i < items.size(); i++) {
            Item *item = items[i];
            if (!item && model_id == 0) return i;
            if (!item) continue;
            if (item->model_id == model_id)
                return i;
        }
        return npos;
    }

    inline size_t Bag::find_dye(uint32_t model_id, DyeInfo extra_id, size_t pos) const {
        for (size_t i = pos; i < items.size(); i++) {
            Item *item = items[i];
            if (!item && model_id == 0) return i;
            if (!item) continue;
            if (item->model_id == model_id && memcmp(&item->dye,&extra_id,sizeof(item->dye)) == 0)
                return i;
        }
        return npos;
    }

    // Find a similar item
    inline size_t Bag::find2(const Item *item, size_t pos) const {
        if (item->model_id == Constants::ItemID::Dye)
            return find_dye(item->model_id, item->dye, pos);
        else
            return find1(item->model_id, pos);
    }
}
