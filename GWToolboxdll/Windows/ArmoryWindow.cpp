#include "stdafx.h"


#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Modules/GwDatTextureModule.h>

#include <Windows/ArmoryWindow_Constants.h>
#include <Windows/ArmoryWindow.h>
#include <ImGuiAddons.h>
#include <ToolboxWindow.h>

#include <Utils/TextUtils.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>


namespace GWArmory {

    bool use_global_color;
    std::array<GW::DyeColor, 4> global_dyes = {};

    constexpr size_t costume_count = 0x17;
    struct CostumeData {
        // [costume_model_file_id][profession] = { chest file id, legs file id, hands file id, legs file id }
        uint32_t sets[costume_count][10][4];
    };

    CostumeData* costume_data_ptr = nullptr;

    constexpr size_t festival_hat_sets_count = 0x3c;
    struct FestivalHatData {
        // [festival_hat_set_id][profession] = { headpiece file id }
        uint32_t sets[festival_hat_sets_count][10][1];
    };

    FestivalHatData* festival_hat_data_ptr = nullptr;

    // Returns pointer to the model_file_id for the profession and slot given that matches the costume for the costume_model_file_id
    // If nullptr, costume_model_file_id isn't a costume.
    const uint32_t* GetFileIdsForCostume(uint32_t costume_model_file_id, GW::Constants::Profession profession = GW::Constants::Profession::Warrior, ItemSlot slot = ItemSlot::Boots) {
        if (!costume_data_ptr)
            return nullptr;
        uint32_t profession_idx = std::to_underlying(profession) - 1;
        uint32_t slot_idx = 0;
        switch (slot) {
        case ItemSlot::Boots:
            slot_idx = 0;
            break;
        case ItemSlot::Leggings:
            slot_idx = 1;
            break;
        case ItemSlot::Gloves:
            slot_idx = 2;
            break;
        case ItemSlot::Chestpiece:
            slot_idx = 3;
            break;
        default:
            return nullptr;
        }
        for (const auto& set : costume_data_ptr->sets) {
            for (const auto& profession_file_ids : set) {
                for (const auto& file_id : profession_file_ids) {
                    if (file_id == costume_model_file_id) {
                        return &set[profession_idx][slot_idx];
                    }
                }
            }
        }
        return nullptr;
    }
    // Returns pointer to the headpiece model_file_id for the profession given that matches the festival hat for the festival_hat_model_file_id
    // If nullptr, festival_hat_model_file_id isn't a festival hat.
    const uint32_t* GetFileIdForFestivalHat(uint32_t festival_hat_model_file_id, GW::Constants::Profession profession = GW::Constants::Profession::Warrior) {
        if (!festival_hat_data_ptr)
            return nullptr;
        uint32_t profession_idx = std::to_underlying(profession) - 1;
        for (const auto& set : festival_hat_data_ptr->sets) {
            for (const auto& profession_file_ids : set) {
                for (const auto& file_id : profession_file_ids) {
                    if (file_id == festival_hat_model_file_id) {
                        return &set[profession_idx][0];
                    }
                }
            }
        }
        return nullptr;
    }

    bool IsEquipmentSlotSupportedByArmory(ItemSlot slot) {
        switch (slot) {
        case ItemSlot::LeftHand:
        case ItemSlot::RightHand:
            // @Enhancement: atm weapons aren't being redrawn after being set - the hand will change, but the model of the weapon remains.

            // Set this to true to play around
            return false;
        case ItemSlot::Unknown:
            return false;
        }
        return true;
    }

    // Record of armor pieces actually drawn; this will differ from the Equipment because we spoof it.
    GW::ItemData drawn_pieces[_countof(GW::NPCEquipment::items)];

    GW::ItemData gwarmory_window_pieces[_countof(GW::NPCEquipment::items)];

    GW::ItemData original_armor_pieces[_countof(GW::NPCEquipment::items)];

    ComboListState combo_list_states[_countof(drawn_pieces)];

    GW::Constants::Profession current_profession = GW::Constants::Profession::None;
    Campaign current_campaign = Campaign::BonusMissionPack;

    using EquipmentSlotAction_pt = void(__fastcall *)(GW::NPCEquipment* equip, void* edx, ItemSlot equipment_slot);
    EquipmentSlotAction_pt RedrawAgentEquipment_Func = nullptr;
    EquipmentSlotAction_pt RedrawAgentEquipment_Ret = nullptr;

    EquipmentSlotAction_pt UndrawAgentEquipment_Func = nullptr;
    EquipmentSlotAction_pt UndrawAgentEquipment_Ret = nullptr;

    bool gwarmory_setitem = false;
    bool pending_reset_equipment = true;

    bool Reset();

    GW::NPCEquipment* GetPlayerEquipment()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        return player && player->equip && *player->equip ? *player->equip : nullptr;
    }

    GW::ItemData* GetSlotItemData(ItemSlot slot)
    {
        const auto equip = GetPlayerEquipment();
        if (!equip) return nullptr;
        return &equip->items[slot];
    }

    Armor* GetArmorsPerProfession(const GW::Constants::Profession prof, size_t* count)
    {
        switch (prof) {
            case GW::Constants::Profession::Warrior:
                *count = _countof(warrior_armors);
                return warrior_armors;
            case GW::Constants::Profession::Ranger:
                *count = _countof(ranger_armors);
                return ranger_armors;
            case GW::Constants::Profession::Monk:
                *count = _countof(monk_armors);
                return monk_armors;
            case GW::Constants::Profession::Necromancer:
                *count = _countof(necromancer_armors);
                return necromancer_armors;
            case GW::Constants::Profession::Mesmer:
                *count = _countof(mesmer_armors);
                return mesmer_armors;
            case GW::Constants::Profession::Elementalist:
                *count = _countof(elementalist_armors);
                return elementalist_armors;
            case GW::Constants::Profession::Assassin:
                *count = _countof(assassin_armors);
                return assassin_armors;
            case GW::Constants::Profession::Ritualist:
                *count = _countof(ritualist_armors);
                return ritualist_armors;
            case GW::Constants::Profession::Paragon:
                *count = _countof(paragon_armors);
                return paragon_armors;
            case GW::Constants::Profession::Dervish:
                *count = _countof(dervish_armors);
                return dervish_armors;
            case GW::Constants::Profession::None:
            default:
                *count = 0;
                return nullptr;
        }
    }

    GW::DyeColor DyeColorFromInt(size_t color)
    {
        const auto col = static_cast<GW::DyeColor>(color);
        switch (col) {
            case GW::DyeColor::Blue:
            case GW::DyeColor::Green:
            case GW::DyeColor::Purple:
            case GW::DyeColor::Red:
            case GW::DyeColor::Yellow:
            case GW::DyeColor::Brown:
            case GW::DyeColor::Orange:
            case GW::DyeColor::Silver:
            case GW::DyeColor::Black:
            case GW::DyeColor::Gray:
            case GW::DyeColor::White:
            case GW::DyeColor::Pink:
                return col;
            default:
                return GW::DyeColor::None;
        }
    }

    ImVec4 ImVec4FromDyeColor(GW::DyeColor color)
    {
        const uint32_t color_id = std::to_underlying(color) - std::to_underlying(GW::DyeColor::Blue);
        switch (color) {
            case GW::DyeColor::Blue:
            case GW::DyeColor::Green:
            case GW::DyeColor::Purple:
            case GW::DyeColor::Red:
            case GW::DyeColor::Yellow:
            case GW::DyeColor::Brown:
            case GW::DyeColor::Orange:
            case GW::DyeColor::Silver:
            case GW::DyeColor::Black:
            case GW::DyeColor::Gray:
            case GW::DyeColor::White:
            case GW::DyeColor::Pink: assert(color_id < _countof(palette));
                return palette[color_id];
            default:
                return {};
        }
    }

    const char* GetProfessionName(const GW::Constants::Profession prof)
    {
        switch (prof) {
            case GW::Constants::Profession::None:
                return "None";
            case GW::Constants::Profession::Warrior:
                return "Warrior";
            case GW::Constants::Profession::Ranger:
                return "Ranger";
            case GW::Constants::Profession::Monk:
                return "Monk";
            case GW::Constants::Profession::Necromancer:
                return "Necromancer";
            case GW::Constants::Profession::Mesmer:
                return "Mesmer";
            case GW::Constants::Profession::Elementalist:
                return "Elementalist";
            case GW::Constants::Profession::Assassin:
                return "Assassin";
            case GW::Constants::Profession::Ritualist:
                return "Ritualist";
            case GW::Constants::Profession::Paragon:
                return "Paragon";
            case GW::Constants::Profession::Dervish:
                return "Dervish";
            default:
                return "Unknown Profession";
        }
    }

    GW::Constants::Profession GetAgentProfession(GW::AgentLiving* agent)
    {
        if (!agent) {
            return GW::Constants::Profession::None;
        }
        const auto primary = static_cast<GW::Constants::Profession>(agent->primary);
        switch (primary) {
            case GW::Constants::Profession::None:
            case GW::Constants::Profession::Warrior:
            case GW::Constants::Profession::Ranger:
            case GW::Constants::Profession::Monk:
            case GW::Constants::Profession::Necromancer:
            case GW::Constants::Profession::Mesmer:
            case GW::Constants::Profession::Elementalist:
            case GW::Constants::Profession::Assassin:
            case GW::Constants::Profession::Ritualist:
            case GW::Constants::Profession::Paragon:
            case GW::Constants::Profession::Dervish:
                return primary;
            default:
                return GW::Constants::Profession::None;
        }
    }

    bool armor_filter_array_getter(void*, const int idx, const char** out_text)
    {
        switch (static_cast<GW::Constants::Campaign>(idx)) {
            case GW::Constants::Campaign::Core:
                *out_text = "Core";
                break;
            case GW::Constants::Campaign::Prophecies:
                *out_text = "Prophecies";
                break;
            case GW::Constants::Campaign::Factions:
                *out_text = "Factions";
                break;
            case GW::Constants::Campaign::Nightfall:
                *out_text = "Nightfall";
                break;
            case GW::Constants::Campaign::EyeOfTheNorth:
                *out_text = "Eye of the North";
                break;
            default:
                *out_text = "All";
                break;
        }
        return true;
    }

    ItemSlot GetSlotFromItemType(GW::Constants::ItemType type) {
        switch (type) {
        case GW::Constants::ItemType::Chestpiece:
            return ItemSlot::Chestpiece;
        case GW::Constants::ItemType::Leggings:
            return ItemSlot::Leggings;
        case GW::Constants::ItemType::Gloves:
            return ItemSlot::Gloves;
        case GW::Constants::ItemType::Boots:
            return ItemSlot::Boots;
        case GW::Constants::ItemType::Headpiece:
            return ItemSlot::Headpiece;
        case GW::Constants::ItemType::Costume_Headpiece:
            return ItemSlot::CostumeHead;
        case GW::Constants::ItemType::Costume:
            return ItemSlot::CostumeBody;
        case GW::Constants::ItemType::Sword:
        case GW::Constants::ItemType::Axe:
            return ItemSlot::RightHand;
        case GW::Constants::ItemType::Bow:
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Shield:
            return ItemSlot::LeftHand;
        }
        return ItemSlot::Unknown;
    }
    const char* GetSlotName(ItemSlot slot) {
        switch (slot) {
        case ItemSlot::RightHand:
            return "Right Hand";
        case ItemSlot::LeftHand:
            return "Left Hand";
        case ItemSlot::Chestpiece:
            return "Chest";
        case ItemSlot::Leggings:
            return "Legs";
        case ItemSlot::Boots:
            return "Boots";
        case ItemSlot::Gloves:
            return "Gloves";
        case ItemSlot::Headpiece:
            return "Head";
        case ItemSlot::CostumeBody:
            return "Costume";
        case ItemSlot::CostumeHead:
            return "Costume Head";
        }
        return "Unknown";
    }

    bool armor_pieces_array_getter(void* data, const int idx, const char** out_text)
    {
        const auto armors = static_cast<Armor**>(data);
        *out_text = armors[idx]->label;
        return true;
    }

    uint32_t CreateColor(GW::DyeColor col1, GW::DyeColor col2 = GW::DyeColor::None, GW::DyeColor col3 = GW::DyeColor::None, GW::DyeColor col4 = GW::DyeColor::None)
    {
        if (col1 == GW::DyeColor::None && col2 == GW::DyeColor::None && col3 == GW::DyeColor::None && col4 == GW::DyeColor::None) {
            col1 = GW::DyeColor::Gray;
        }
        const auto c1 = std::to_underlying(col1);
        const auto c2 = std::to_underlying(col2);
        const auto c3 = std::to_underlying(col3);
        const auto c4 = std::to_underlying(col4);
        const uint32_t composite = c1 | c2 << 4 | c3 << 8 | c4 << 12;
        return composite;
    }

    void ClearArmorItem(ItemSlot slot) {
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return;
        if (!IsEquipmentSlotSupportedByArmory(slot))
            return;
        if (drawn_pieces[slot].model_file_id && drawn_pieces[slot].interaction) {
            // Backup via copy
            equip->items[slot] = drawn_pieces[slot];

            // Clear the slot(s)
            gwarmory_setitem = true;
            equip->UndrawEquipmentSlot(slot);
            gwarmory_setitem = false;

            // Swap the data back
            equip->items[slot] = {};
        }
        
    }
 
    GW::Item* GetEquippedItem(ItemSlot slot) {
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return nullptr;
        const auto bag = GW::Items::GetBag(GW::Constants::Bag::Equipped_Items);
        return bag ? bag->items[slot] : nullptr;
    }

    const Armor* FindArmorItem(std::string_view item_name)
    {
        const auto me = GW::Agents::GetControlledCharacter();
        if (!me) return nullptr;
        size_t armor_cnt = 0;
        const auto armors = GetArmorsPerProfession((GW::Constants::Profession)me->primary, &armor_cnt);
        const auto lower_name = TextUtils::ToLower(item_name.data());
        for (size_t i = 0; i < armor_cnt && armors; i++) {
            const auto& armor = armors[i];
            if (TextUtils::ToLower(armor.label).starts_with(lower_name)) return &armor;
        }
        // TODO: Costumes
        return nullptr;
    }



    void SetArmorItem(ItemSlot slot, uint32_t model_file_id, GW::DyeInfo dye, uint32_t interaction = 0, GW::Constants::ItemType type = GW::Constants::ItemType::Unknown) {      
        if (slot == ItemSlot::Unknown)
            return;
        if (!IsEquipmentSlotSupportedByArmory(slot))
            return;
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return;

        /*if (model_file_id == drawn_pieces[slot].model_file_id) {
            return;

        }*/
        ClearArmorItem(slot);
        if (!model_file_id)
            return; // Clearing the slot.

        auto& gameItemData = equip->items[slot];

        // Backup via copy
        auto original = gameItemData;

        // Swap out model id and dye, keep interaction for now.
        gameItemData.model_file_id = model_file_id;
        if (type != GW::Constants::ItemType::Unknown) {
            gameItemData.type = type;
        }
        gameItemData.dye = dye;
        if (interaction)
            gameItemData.interaction = interaction;

        // Draw the slot again
        gwarmory_setitem = true;
        equip->RedrawEquipmentSlot(slot);
        gwarmory_setitem = false;

        // Swap the data back
        //gameItemData = original;

    }

    bool IsBothHands(ItemType type) {
        switch (type) {
        case ItemType::Staff:
        case ItemType::Bow:
        case ItemType::Hammer:
            return true;
        default:
            return false;
        }
    }
    bool IsWeapon(ItemType type) {
        switch (type) {
        case ItemType::Wand:
        case ItemType::Shield:
        case ItemType::Sword:
        case ItemType::Axe:
        case ItemType::Daggers:
        case ItemType::Scythe:
        case ItemType::Spear:
        case ItemType::Staff:
        case ItemType::Bow:
        case ItemType::Hammer:
            return true;
        default:
            return false;
        }
    }
    bool IsBodyArmor(ItemSlot slot) {
        switch (slot) {
        case ItemSlot::Leggings:
        case ItemSlot::Boots:
        case ItemSlot::Chestpiece:
        case ItemSlot::Gloves:
            return true;
        }
        return false;
    }

    GW::Constants::Profession GetPlayerProfession() {
        return GetAgentProfession(GW::Agents::GetControlledCharacter());
    }
    bool IsCostumeFileId(uint32_t model_file_id) {
        return model_file_id && GetFileIdsForCostume(model_file_id) != nullptr;
    }

    void UpdateArmorsFilter()
    {

        for (auto& state : combo_list_states) {
            state.pieces.clear();
            state.current_piece_index = -1;
            state.current_piece = nullptr;
        }
        const auto& c = current_campaign;
        auto appendArmors = [c](Armor* armor_arr, size_t length) {
            for (size_t i = 0; i < length; i++) {
                auto& armor = armor_arr[i];
                const auto slot = GetSlotFromItemType(armor.type);
                if (!IsEquipmentSlotSupportedByArmory(slot))
                    continue;
                if (c != Campaign::BonusMissionPack && armor.campaign != c)
                    continue;
                ASSERT(slot != ItemSlot::Unknown);
                const auto piece = &gwarmory_window_pieces[slot];
                const auto state = &combo_list_states[slot];

                if (piece->model_file_id == armor.model_file_id) {
                    state->current_piece = &armor;
                }
                state->pieces.push_back(&armor);
            }
        };
        size_t count = 0;
        const auto profession_armors = GetArmorsPerProfession(current_profession, &count);
        appendArmors(profession_armors, count);
        appendArmors(costume_heads, _countof(costume_heads));
        appendArmors(costumes, _countof(costumes));

        appendArmors(swords, _countof(swords));
        appendArmors(shields, _countof(shields));

    }
    // "Remove" any costume pieces that are drawn, reverting to the armor item worn.
    void RevertCostumePieces() {
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return;
        const ItemSlot toCheck[] = {
            ItemSlot::Chestpiece,
            ItemSlot::Leggings,
            ItemSlot::Gloves,
            ItemSlot::Boots
        };
        for (auto slot : toCheck) {
            if (IsCostumeFileId(drawn_pieces[slot].model_file_id)) {
                const auto& itemData = equip->items[slot];
                if (itemData.model_file_id && !IsCostumeFileId(itemData.model_file_id)) {
                    SetArmorItem(slot, itemData.model_file_id, itemData.dye, itemData.interaction, itemData.type);
                }
                else {
                    const auto item = GW::Items::GetItemById(equip->item_ids[slot]);
                    if (item && item->model_file_id && !IsCostumeFileId(item->model_file_id)) {
                        SetArmorItem(slot, item->model_file_id, item->dye, item->interaction, item->type);
                    }
                }
            }
        }
        UpdateArmorsFilter();
    }
    void SetArmorItem(const GW::ItemData* piece)
    {
        const auto slot = GetSlotFromItemType(piece->type);
        // If its a weapon, figure out if we need to clear left or right hand
        if (IsWeapon(piece->type)) {
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
                return;
            if (IsBothHands(piece->type)) {
                ClearArmorItem(ItemSlot::LeftHand);
                ClearArmorItem(ItemSlot::RightHand);
            }
            else {
                const auto& leftHand = drawn_pieces[ItemSlot::LeftHand];
                if (IsBothHands(leftHand.type)) {
                    ClearArmorItem(ItemSlot::LeftHand);
                    ClearArmorItem(ItemSlot::RightHand);
                }
            }
        }

        SetArmorItem(slot, piece->model_file_id, piece->dye, piece->interaction, piece->type);
        if (slot == ItemSlot::Headpiece) {
            ClearArmorItem(ItemSlot::CostumeHead);
        }
        if (slot == ItemSlot::CostumeHead) {
            // If we're a festival hat, set the correct model file id for this character's profession
            if(const auto hat_found = GetFileIdForFestivalHat(piece->model_file_id, current_profession))
                SetArmorItem(ItemSlot::Headpiece, *hat_found, piece->dye, piece->interaction, piece->type);
        }
        else if (slot == ItemSlot::CostumeBody) {
            if (const auto costume_found = GetFileIdsForCostume(piece->model_file_id, current_profession)) {
                // If we're a costume, set all of the other armor piece model file ids for this character's profession
                SetArmorItem(ItemSlot::Boots, costume_found[0], piece->dye, piece->interaction, piece->type);
                SetArmorItem(ItemSlot::Leggings, costume_found[1], piece->dye, piece->interaction, piece->type);
                SetArmorItem(ItemSlot::Gloves, costume_found[2], piece->dye, piece->interaction, piece->type);
                SetArmorItem(ItemSlot::Chestpiece, costume_found[3], piece->dye, piece->interaction, piece->type);
            }
        }
        else if (IsBodyArmor(slot) && !IsCostumeFileId(piece->model_file_id)) {
            RevertCostumePieces();
        }

    }

    GW::HookEntry ChatCmd_HookEntry;

    void CHAT_CMD_FUNC(CmdArmory)
    {
        const auto syntax = "Syntax: '/armory [armor_item_name] [dye1] [dye2] [dye3] [dye4]' (e.g. '/armory \"Elite Sunspear Raiment\"  2 1 10 4')";
        if (argc <= 1) {
            Log::Warning(syntax);
            return;
        }
        const auto item_name = TextUtils::WStringToString(argv[1]);
        const auto found = FindArmorItem(item_name);
        if (!found) {
            Log::Warning("Failed to find armor item: %s", item_name.c_str());
            return;
        }

        const auto existing_item_data = GetSlotItemData(GetSlotFromItemType(found->type));
        if (!existing_item_data) {
            Log::Warning("Failed to find item data for armor item: %s", item_name.c_str());
            return;
        }

        GW::ItemData data = *existing_item_data;
        data.model_file_id = found->model_file_id;
        data.type = found->type;
        data.interaction = found->interaction;

        uint32_t col = 0;
        if (argc > 2) {
            data.dye = {0};
            if (!(TextUtils::ParseUInt(argv[3], &col, 10) && col <= std::to_underlying(GW::DyeColor::Pink))) {
                Log::Warning(syntax);
                return;
            }
            data.dye.dye1 = (GW::DyeColor)(col & 0xff);
        }
        if (argc > 3) {
            if (!(TextUtils::ParseUInt(argv[3], &col, 10) && col <= std::to_underlying(GW::DyeColor::Pink))) {
                Log::Warning(syntax);
                return;
            }
            data.dye.dye2 = (GW::DyeColor)(col & 0xff);
        }
        if (argc > 4) {
            if (!(TextUtils::ParseUInt(argv[4], &col, 10) && col <= std::to_underlying(GW::DyeColor::Pink))) {
                Log::Warning(syntax);
                return;
            }
            data.dye.dye3 = (GW::DyeColor)(col & 0xff);
        }
        if (argc > 5) {
            if (!(TextUtils::ParseUInt(argv[5], &col, 10) && col <= std::to_underlying(GW::DyeColor::Pink))) {
                Log::Warning(syntax);
                return;
            }
            data.dye.dye4 = (GW::DyeColor)(col & 0xff);
        }

        data.dye.dye_tint = found->dye_tint;

        GW::GameThread::Enqueue([cpy = data]() {
            SetArmorItem(&cpy);
        });
    }


    void __fastcall OnRedrawAgentEquipment(GW::NPCEquipment* equip, void* edx, const ItemSlot equipment_slot)
    {
        GW::Hook::EnterHook();
        RedrawAgentEquipment_Ret(equip, edx, equipment_slot);
        GW::Hook::LeaveHook();

        const auto player_equip = GetPlayerEquipment();
        if (equip != player_equip) {
            return;
        }
        if (GetPlayerProfession() != current_profession) {
            memset(original_armor_pieces, 0, sizeof(original_armor_pieces));
            pending_reset_equipment = true;
        }

        drawn_pieces[equipment_slot] = equip->items[equipment_slot];
        gwarmory_window_pieces[equipment_slot] = drawn_pieces[equipment_slot];
        //Log::Info("Piece changed for slot %d: model_file_id 0x%08X, dye_tint %d", equipment_slot, equip->items[equipment_slot].model_file_id, equip->items[equipment_slot].dye.dye_tint);

        if (gwarmory_setitem)
            return;
        original_armor_pieces[equipment_slot] = drawn_pieces[equipment_slot];
        UpdateArmorsFilter();
    }

    void __fastcall OnUndrawAgentEquipment(GW::NPCEquipment* equip, void* edx, const ItemSlot equipment_slot)
    {
        GW::Hook::EnterHook();
        if (equip && equip->items[equipment_slot].model_file_id)
            UndrawAgentEquipment_Ret(equip, edx, equipment_slot);
        GW::Hook::LeaveHook();

        const auto player_equip = GetPlayerEquipment();
        if (equip != player_equip) {
            return;
        }

        drawn_pieces[equipment_slot] = { 0 };
        gwarmory_window_pieces[equipment_slot] = drawn_pieces[equipment_slot];
        //Log::Info("Piece cleared for slot %d", equipment_slot);

        if (gwarmory_setitem)
            return;
        original_armor_pieces[equipment_slot] = drawn_pieces[equipment_slot];
        UpdateArmorsFilter();
    }

    bool Reset()
    {
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return false;

        current_campaign = Campaign::BonusMissionPack;
        current_profession = GetPlayerProfession();

        for (size_t slot = 0; slot < _countof(original_armor_pieces); slot++) {
            SetArmorItem(&original_armor_pieces[slot]);
            gwarmory_window_pieces[slot] = drawn_pieces[slot];
        }

        UpdateArmorsFilter();
        return true;
    }

    bool DyePicker(const char* label, GW::DyeColor* color)
    {
        ImGui::PushID(label);

        const ImVec4 current_color = ImVec4FromDyeColor(*color);

        bool value_changed = false;
        const char* label_display_end = ImGui::FindRenderedTextEnd(label);

        if (ImGui::ColorButton("##ColorButton", current_color, *color == GW::DyeColor::None ? ImGuiColorEditFlags_AlphaPreview : 0)) {
            ImGui::OpenPopup("picker");
        }

        if (ImGui::BeginPopup("picker")) {
            if (label != label_display_end) {
                ImGui::TextUnformatted(label, label_display_end);
                ImGui::Separator();
            }
            size_t palette_index;
            if (ImGui::ColorPalette("##picker", &palette_index, palette, _countof(palette), 7, ImGuiColorEditFlags_AlphaPreview)) {
                if (palette_index < _countof(palette)) {
                    *color = DyeColorFromInt(palette_index + static_cast<size_t>(GW::DyeColor::Blue));
                }
                else {
                    *color = GW::DyeColor::None;
                }
                value_changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return value_changed;
    }

    bool GetIsFemale() {
        const auto player = GW::Agents::GetControlledCharacter();
        return player && player->GetIsFemale();
    }

    IDirect3DTexture9** GetArmorPieceImage(uint32_t model_file_id, uint32_t interaction) {
        const bool is_composite_item = (interaction & 4) != 0;

        uint32_t model_id_to_load = 0;

        if (is_composite_item) {
            // Armor/runes
            const auto model_file_info = GW::Items::GetCompositeModelInfo(model_file_id);
            if (model_file_info) {
                if(!model_id_to_load)
                    model_id_to_load = model_file_info->file_ids[0xa];
                if (!model_id_to_load)
                    model_id_to_load = GetIsFemale() ? model_file_info->file_ids[5] : model_file_info->file_ids[0];

            }
        }
        if (!model_id_to_load)
            model_id_to_load = model_file_id;

        return GwDatTextureModule::LoadTextureFromFileId(model_id_to_load);
    }
    
    
    std::string GetChatCommand(Armor* armor, GW::ItemData* data)
    {
        return std::format("/armory \"{}\" {} {} {} {}", armor->label, std::to_underlying(data->dye.dye1), std::to_underlying(data->dye.dye2), std::to_underlying(data->dye.dye3), std::to_underlying(data->dye.dye4));
    }

    GW::ItemData context_menu_piece;
    bool ArmorItemContextMenu(void* wparam)
    {
        const auto armor_item = (Armor*)wparam;
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
        ImGui::TextUnformatted(armor_item->label);
        const auto chat_cmd = GetChatCommand(armor_item, &context_menu_piece);
        ImGui::TextDisabled(chat_cmd.c_str());
        ImGui::Separator();
        if (ImGui::Button("Copy chat command", size)) {
            ImGui::CloseCurrentPopup();
            ImGui::SetClipboardText(chat_cmd.c_str());
            Log::Info("'%s' copied to clipboard", chat_cmd.c_str());
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return false;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return true;
    }




    bool DrawArmorPieceNew(ItemSlot slot) {
        ImGui::PushID(slot);
        const auto state = &combo_list_states[slot];
        const auto player_piece = &gwarmory_window_pieces[slot];
        bool value_changed = false;

        const float scale = ImGui::GetIO().FontGlobalScale;

        ImGui::Separator();
        ImGui::TextUnformatted(GetSlotName(slot));

        ImGui::PushID(slot);

        auto tmpDyeColor = player_piece->dye.dye1;
        ImGui::SameLine(128.f * scale);
        if (DyePicker("color1", &tmpDyeColor) || use_global_color && tmpDyeColor != global_dyes[0]) {
            value_changed = true;
            player_piece->dye.dye1 = use_global_color ? global_dyes[0] : tmpDyeColor;
        }

        tmpDyeColor = player_piece->dye.dye2;
        ImGui::SameLine();
        if (DyePicker("color2", &tmpDyeColor) || use_global_color && tmpDyeColor != global_dyes[1]) {
            value_changed = true;
            player_piece->dye.dye2 = use_global_color ? global_dyes[1] : tmpDyeColor;
        }

        tmpDyeColor = player_piece->dye.dye3;
        ImGui::SameLine();
        if (DyePicker("color3", &tmpDyeColor) || use_global_color && tmpDyeColor != global_dyes[2]) {
            value_changed = true;
            player_piece->dye.dye3 = use_global_color ? global_dyes[2] : tmpDyeColor;
        }

        tmpDyeColor = player_piece->dye.dye4;
        ImGui::SameLine();
        if (DyePicker("color4", &tmpDyeColor) || use_global_color && tmpDyeColor != global_dyes[3]) {
            value_changed = true;
            player_piece->dye.dye4 = use_global_color ? global_dyes[3] : tmpDyeColor;
        }

        ImGui::SameLine(280.f * scale);
        if (ImGui::SmallButton("None")) {
            player_piece->model_file_id = 0;
            value_changed = true;
        }
        ImGui::PopID();
        

        
        constexpr ImVec4 tint(1, 1, 1, 1);
        constexpr auto uv0 = ImVec2(0, 0);

        const ImVec2 icon_size = { 48.f, 48.f };

        ImVec2 scaled_size(icon_size.x * scale, icon_size.y * scale);

        const auto equipped_color = ImColor(IM_COL32(0, 0x99, 0, 192));
        const auto normal_bg = ImColor(IM_COL32(0, 0, 0, 0));


        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));

        ImGui::StartSpacedElements(icon_size.x);

#ifdef _DEBUG
        static Armor debug_piece("Debug Piece", 0, Profession::None, ItemType::Unknown, Campaign::Core, 0, 0);
        if (ImGui::CollapsingHeader("Debug Item")) {
            constexpr static std::array profession_names = {
                "None", "Warrior", "Ranger", "Monk", "Necromancer", "Mesmer", "Elementalist", "Assassin", "Ritualist", "Paragon", "Dervish"
            };
            constexpr static std::array campaign_names = {
                "Core", "Prophecies", "Factions", "Nightfall", "Eye of the North", "BonusMissionPack"
            };
            if (slot == Headpiece) {
                ImGui::InputInt("model_file_id", (int*)&debug_piece.model_file_id);
                ImGui::Combo("profession", (int*)&debug_piece.profession, profession_names.data(), profession_names.size());
                ImGui::InputInt("type", (int*)&debug_piece.type);
                ImGui::Combo("campaign", (int*)&debug_piece.campaign, campaign_names.data(), campaign_names.size());
                ImGui::InputInt("dye_tint", (int*)&debug_piece.dye_tint);
                ImGui::InputInt("interaction", (int*)&debug_piece.interaction);
            }
        }
        std::vector<Armor*> pieces{};
        if (slot == Headpiece && debug_piece.profession != Profession::None && debug_piece.interaction && debug_piece.model_file_id && debug_piece.type != ItemType::Unknown) {
            pieces.push_back(&debug_piece);
        }
        pieces.append_range(state->pieces);
        for (const auto& piece : pieces) {
#else
        for (const auto& piece : state->pieces) {
#endif
            ImGui::PushID(piece);

            if (0 <= state->current_piece_index && static_cast<size_t>(state->current_piece_index) < state->pieces.size()) {
                state->current_piece = state->pieces[state->current_piece_index];

                player_piece->model_file_id = state->current_piece->model_file_id;
                player_piece->interaction = state->current_piece->interaction;
                player_piece->type = state->current_piece->type;
                player_piece->dye.dye_tint = state->current_piece->dye_tint;
            }

            const auto texture = GetArmorPieceImage(piece->model_file_id, piece->interaction);
            if (!texture || !*texture) {
                ImGui::PopID();
                continue;
            }
    
            const auto uv1 = ImGui::CalculateUvCrop(*texture, scaled_size);
            const auto& bg = player_piece->model_file_id == piece->model_file_id ? equipped_color : normal_bg;
            ImGui::NextSpacedElement();
            if (ImGui::ImageButton(*texture, scaled_size, uv0, uv1, -1, bg, tint)) {
                player_piece->model_file_id = piece->model_file_id;
                player_piece->interaction = piece->interaction;
                player_piece->type = piece->type;
                player_piece->dye.dye_tint = piece->dye_tint;
                value_changed = true;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([piece, player_piece]() {
                    ImGui::TextUnformatted(piece->label);
                    ImGui::TextDisabled(GetChatCommand(piece, player_piece).c_str());
                });
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                context_menu_piece = *player_piece;
                ImGui::SetContextMenu(ArmorItemContextMenu, piece);
            }
            ImGui::PopID();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        ImGui::PopID();
        return value_changed;
    }

    bool DrawArmorPiece(ItemSlot slot)
    {
        const auto state = &combo_list_states[slot];
        const auto player_piece = &drawn_pieces[slot];

        ImGui::PushID(state);

        bool value_changed = false;
        const char* preview = state->current_piece ? state->current_piece->label : "";
        if (ImGui::MyCombo("##armors", preview, &state->current_piece_index, armor_pieces_array_getter, state->pieces.data(), state->pieces.size())) {
            if (0 <= state->current_piece_index && static_cast<size_t>(state->current_piece_index) < state->pieces.size()) {
                state->current_piece = state->pieces[state->current_piece_index];

                player_piece->model_file_id = state->current_piece->model_file_id;
                player_piece->interaction = state->current_piece->interaction;
                player_piece->type = state->current_piece->type;
                player_piece->dye.dye_tint = state->current_piece->dye_tint;
            }
            value_changed = true;
        }

        auto tmpDyeColor = player_piece->dye.dye1;
        ImGui::SameLine();
        if (DyePicker("color1", &tmpDyeColor)) {
            value_changed = true;
            player_piece->dye.dye1 = tmpDyeColor;
        }

        tmpDyeColor = player_piece->dye.dye2;
        ImGui::SameLine();
        if (DyePicker("color2", &tmpDyeColor)) {
            value_changed = true;
            player_piece->dye.dye2 = tmpDyeColor;
        }

        tmpDyeColor = player_piece->dye.dye3;
        ImGui::SameLine();
        if (DyePicker("color3", &tmpDyeColor)) {
            value_changed = true;
            player_piece->dye.dye3 = tmpDyeColor;
        }

        tmpDyeColor = player_piece->dye.dye4;
        ImGui::SameLine();
        if (DyePicker("color4", &tmpDyeColor)) {
            value_changed = true;
            player_piece->dye.dye4 = tmpDyeColor;
        }
        ImGui::PopID();
        return value_changed;
    }

}

using namespace GWArmory;

void ArmoryWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (pending_reset_equipment && Reset()) {
        pending_reset_equipment = false;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 208), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::Text("Profession: %s", GetProfessionName(current_profession));

        ImGui::SameLine();
        ImGui::Checkbox("Use same colour for all pieces", &use_global_color);
        ImGui::ShowHelp("When this is selected, all armour pieces will be coloured this way.");

        if (use_global_color) {
            ImGui::SameLine();
            DyePicker("globalcolor0", &global_dyes[0]);
            ImGui::SameLine();
            DyePicker("globalcolor1", &global_dyes[1]);
            ImGui::SameLine();
            DyePicker("globalcolor2", &global_dyes[2]);
            ImGui::SameLine();
            DyePicker("globalcolor3", &global_dyes[3]);
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 65.f);
        if (ImGui::Button("Reset")) {
            pending_reset_equipment = true;
        }

        if (ImGui::MyCombo("##filter", "All", reinterpret_cast<int*>(&current_campaign), armor_filter_array_getter, nullptr, 6)) {
            UpdateArmorsFilter();
        }
        const auto order = {Headpiece, Chestpiece, Gloves, Leggings, Boots, CostumeHead, CostumeBody, LeftHand, RightHand};
        for (const auto slot : order) {
            if (!IsEquipmentSlotSupportedByArmory(slot))
                continue;
            if (DrawArmorPieceNew(slot)) {
                SetArmorItem(&gwarmory_window_pieces[slot]);
            }
        }
    }
    ImGui::End();
}

void ArmoryWindow::Initialize()
{
    ToolboxWindow::Initialize();

    RedrawAgentEquipment_Func = (EquipmentSlotAction_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("CpsPlayer.cpp", "itemData.fileId",0,0),0xfff);
    if (RedrawAgentEquipment_Func) {
        GW::Hook::CreateHook((void**)&RedrawAgentEquipment_Func, OnRedrawAgentEquipment, (void**)&RedrawAgentEquipment_Ret);
        GW::Hook::EnableHooks(RedrawAgentEquipment_Func);
    }
    
    UndrawAgentEquipment_Func = (EquipmentSlotAction_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("CpsPlayer.cpp", "component < arrsize(m_componentItem)",0,0),0xfff);
    if (UndrawAgentEquipment_Func) {
        GW::Hook::CreateHook((void**)&UndrawAgentEquipment_Func, OnUndrawAgentEquipment, (void**)&UndrawAgentEquipment_Ret);
        GW::Hook::EnableHooks(UndrawAgentEquipment_Func);
    }

    uintptr_t address = GW::Scanner::Find("\x81\xc6\xa0\x00\x00\x00\x83\xf8\x17", "xxxxxxxxx", -0xb);
    if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address, GW::ScannerSection::Section_RDATA)) {
        address = *(uintptr_t*)address;
        address -= 0xC;
        costume_data_ptr = (CostumeData*)address;
    }
    address = GW::Scanner::Find("\x83\xc1\x28\x83\xf8\x3b", "xxxxxx", -0xf);
    if (address) {
        address = *(uintptr_t*)address;
        address -= 0x28;
        if (GW::Scanner::IsValidPtr(address, GW::ScannerSection::Section_RDATA)) {
            festival_hat_data_ptr = (FestivalHatData*)address;
        }
    }
    const auto equip = GetPlayerEquipment();
    if (equip) {
        memcpy(original_armor_pieces, equip->items, sizeof(original_armor_pieces));
    }
#ifdef _DEBUG 
    ASSERT(RedrawAgentEquipment_Func);
    ASSERT(UndrawAgentEquipment_Func);
    ASSERT(costume_data_ptr);
    ASSERT(festival_hat_data_ptr);
#endif

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"armory", CmdArmory);
}

void ArmoryWindow::Terminate()
{
    Reset(); // NB: We're on the game thread, so this is ok
    if (RedrawAgentEquipment_Func) {
        GW::Hook::RemoveHook(RedrawAgentEquipment_Func);
        RedrawAgentEquipment_Func = nullptr;
    }
    if (UndrawAgentEquipment_Func) {
        GW::Hook::RemoveHook(UndrawAgentEquipment_Func);
        UndrawAgentEquipment_Func = nullptr;
    }
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}
