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
#include <Utils/GuiUtils.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Modules/Resources.h>


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
    bool SetArmorItem(const GW::ItemData* item_data);

    
    bool IsBothHands(ItemType type)
    {
        switch (type) {
            case ItemType::Staff:
            case ItemType::Bow:
            case ItemType::Scythe:
            case ItemType::Hammer:
                return true;
            default:
                return false;
        }
    }
    bool IsWeapon(ItemType type)
    {
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
            case ItemType::Offhand:
                return true;
            default:
                return false;
        }
    }

    bool IsBodyArmor(ItemSlot slot)
    {
        switch (slot) {
            case ItemSlot::Leggings:
            case ItemSlot::Boots:
            case ItemSlot::Chestpiece:
            case ItemSlot::Gloves:
                return true;
        }
        return false;
    }
    bool IsArmor(ItemType type)
    {
        switch (type) {
            case ItemType::Headpiece:
            case ItemType::Gloves:
            case ItemType::Boots:
            case ItemType::Chestpiece:
            case ItemType::Leggings:
                return true;
            default:
                return false;
        }
    }
    GW::Constants::ProfessionByte GetItemProfession(GW::Item* item) {
        if ((item->interaction & 0x4) == 0) 
            return GW::Constants::ProfessionByte::None;
        const auto model_file_info = GW::Items::GetCompositeModelInfo(item->model_file_id);
        if (!model_file_info) 
            return GW::Constants::ProfessionByte::None;
        return (GW::Constants::ProfessionByte)((model_file_info->class_flags >> 0x12) & 0xf);
    }

    FestivalHatData* festival_hat_data_ptr = nullptr;

    // Returns pointer to the model_file_id for the profession and slot given that matches the costume for the costume_model_file_id
    // If nullptr, costume_model_file_id isn't a costume.
    const uint32_t* GetFileIdsForCostume(uint32_t costume_model_file_id, GW::Constants::Profession profession = GW::Constants::Profession::Warrior, ItemSlot slot = ItemSlot::Boots) {
        if (!(costume_data_ptr && costume_model_file_id))
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
        if (!(festival_hat_data_ptr && festival_hat_model_file_id))
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
            return true;
        case ItemSlot::Unknown:
            return false;
        }
        return true;
    }

    // Record of armor pieces actually drawn; this will differ from the Equipment because we spoof it.
    GW::ItemData original_pieces[_countof(GW::NPCEquipment::items)];
    GW::ItemData drawn_pieces[_countof(GW::NPCEquipment::items)];
    GW::ItemData imgui_armor_pieces[_countof(GW::NPCEquipment::items)];

    GW::NPCEquipment* equip_cached = 0;

    ComboListState combo_list_states[_countof(GW::NPCEquipment::items)];

    GW::Constants::Profession current_profession = GW::Constants::Profession::None;
    Campaign current_campaign = Campaign::BonusMissionPack;

    bool gwarmory_setitem = false;
    bool pending_reset_equipment = true;
    bool pending_initialise_equipment = true;

    // Fake item storage - static to keep it alive
    GW::ItemModifier empty_mod_struct[1] = {0xc0000000};

    void ItemDataToGWItem(const GW::ItemData* in, GW::Item* out)
    {
        memset(out, 0, sizeof(*out));
        out->mod_struct = empty_mod_struct;
        out->dye = in->dye;
        out->interaction = in->interaction;
        out->model_file_id = in->model_file_id;
        out->type = in->type;
        out->h0040[0] = 0x2;
    }
    void GwItemToItemData(const GW::Item* in, GW::ItemData* out)
    {
        memset(out, 0, sizeof(*out));
        if (!in) return;
        out->dye = in->dye;
        out->interaction = in->interaction;
        out->model_file_id = in->model_file_id;
        out->type = in->type;
    }

    bool Reset();
    bool ClearArmorItem(ItemSlot slot);

    GW::NPCEquipment* GetPlayerEquipment()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        return player && player->equip ? *player->equip : nullptr;
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
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Shield:
            return ItemSlot::LeftHand;
        }
        if (IsWeapon(type)) return ItemSlot::RightHand;
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
            if (TextUtils::ToLower(armor.label) == lower_name) return &armor;
        }
        for (size_t i = 0; i < _countof(costumes); i++) {
            const auto& armor = costumes[i];
            if (TextUtils::ToLower(armor.label) == lower_name) return &armor;
        }
        for (size_t i = 0; i < _countof(costume_heads); i++) {
            const auto& armor = costume_heads[i];
            if (TextUtils::ToLower(armor.label) == lower_name) return &armor;
        }
        for (size_t i = 0; i < _countof(weapons); i++) {
            const auto& weapon = weapons[i];
            if (TextUtils::ToLower(weapon.label) == lower_name) return &weapon;
        }
        for (size_t i = 0; i < _countof(unequipped_armors); i++) {
            const auto& armor = unequipped_armors[i];
            if (TextUtils::ToLower(armor.label) == lower_name) return &armor;
        }
        return nullptr;
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
                const auto piece = &drawn_pieces[slot];
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

        appendArmors(weapons, _countof(weapons));

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

        GW::ItemData data = {
            .model_file_id = found->model_file_id, 
            .type = found->type,
            .interaction = found->interaction,
        };

        const auto existing_item_data = GetSlotItemData(GetSlotFromItemType(found->type));
        if (existing_item_data) {
            data.dye = existing_item_data->dye;
        }
        data.dye.dye_tint = found->dye_tint;

        uint32_t col = 0;
        if (argc > 2) {
            if (!(TextUtils::ParseUInt(argv[2], &col, 10) && col <= std::to_underlying(GW::DyeColor::Pink))) {
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

        GW::GameThread::Enqueue([cpy = data]() {
            SetArmorItem(&cpy);
        });
    }

    bool Reset()
    {
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return false;

        current_campaign = Campaign::BonusMissionPack;
        current_profession = GetPlayerProfession();

        for (size_t slot = 0; slot < _countof(GW::NPCEquipment::items); slot++) {
            ClearArmorItem((ItemSlot)slot);
            SetArmorItem(&original_pieces[slot]);
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
        if (ImGui::Button("Guild Wars Wiki", size)) {
            ImGui::CloseCurrentPopup();
            GuiUtils::SearchWiki(TextUtils::StringToWString(armor_item->label));
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return false;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return true;
    }

    const std::map<ItemSlot, std::string_view> empty_slot_names = std::map<ItemSlot, std::string_view> {
        { ItemSlot::Headpiece, "NoHead" },
        { ItemSlot::Chestpiece, "NoChest" },
        { ItemSlot::Gloves, "NoGloves" },
        { ItemSlot::Leggings, "NoLegs" },
        { ItemSlot::Boots, "NoBoots" },
        { ItemSlot::CostumeHead, "NoCostumeHead" },
        { ItemSlot::CostumeBody, "NoCostume" },
        { ItemSlot::LeftHand, "NoLeftHand" },
        { ItemSlot::RightHand, "NoRightHand" }
    };

    bool DrawArmorPieceNew(ItemSlot slot) {
        ImGui::PushID(slot);
        const auto state = &combo_list_states[slot];
        const auto player_piece = &imgui_armor_pieces[slot];
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
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip([slot]() {
                ImGui::TextUnformatted("Empty Slot");
                ImGui::TextDisabled("/armory %s", empty_slot_names.at(slot).data());
            });
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

    enum SnapshotState { Idle, Pending, WaitingForDecode };
    SnapshotState state = SnapshotState::Idle;
    std::map<uint32_t, GuiUtils::EncString*> pending_decodes;
    std::map<uint32_t, GW::Item*> pending_items;

    void SnapshotToFile()
    {
        switch (state) {
            case SnapshotState::Idle:
                return;
            case SnapshotState::Pending: {
                for (auto& it : pending_decodes) {
                    delete it.second;
                }
                pending_decodes.clear();
                pending_items.clear();
                for (uint8_t i = (uint8_t)GW::Constants::Bag::Backpack; i < (uint8_t)GW::Constants::Bag::Max; i++) {
                    const auto bag = GW::Items::GetBag((GW::Constants::Bag)i);
                    if (!bag) continue;
                    for (auto item : bag->items) {
                        if (!(item && IsWeapon(item->type))) continue;
                        if (pending_items.contains(item->model_file_id)) continue;
                        pending_decodes[item->model_file_id] = new GuiUtils::EncString(item->name_enc);
                        pending_decodes[item->model_file_id]->string(); // Trigger decode
                        pending_items[item->model_file_id] = item;
                    }
                }
                state = SnapshotState::WaitingForDecode;
            } break;
            case SnapshotState::WaitingForDecode: {
                // Check if all strings are decoded
                for (auto& it : pending_decodes) {
                    if (it.second->IsDecoding()) return;
                }

                // Build JSON output
                nlohmann::json output_json = nlohmann::json::array();
                for (auto& it : pending_decodes) {
                    const auto item = pending_items[it.first];
                    const std::string& item_name_decoded = it.second->string();

                    nlohmann::json item_json = {
                        {"name", item_name_decoded},
                        {"model_file_id", item->model_file_id},
                        {"type", static_cast<uint8_t>(item->type)},
                        {"interaction", item->interaction},
                        {"dye_tint", item->dye.dye_tint}
                    };

                    output_json.push_back(item_json);
                }

                // Write to file
                const auto filename = Resources::GetPath("weapon_snapshot.json");
                std::ofstream file(filename);
                if (file.is_open()) {
                    file << output_json.dump(2); // Pretty print with 2 space indent
                    file.close();
                    Log::Info("Weapon snapshot saved to %s", filename.string().c_str());
                }
                else {
                    Log::Error("Failed to write weapon snapshot to %s", filename.string().c_str());
                }

                // Cleanup
                for (auto& it : pending_decodes) {
                    delete it.second;
                }
                pending_decodes.clear();
                pending_items.clear();
                state = SnapshotState::Idle;
            } break;
        }
    }

    using EquipmentSlotAction_pt = void(__fastcall*)(GW::NPCEquipment* equip, uint32_t edx, uint32_t equipment_slot);
    EquipmentSlotAction_pt EquipItem_Func = nullptr, EquipItem_Ret = nullptr;
    EquipmentSlotAction_pt ClearItem_Func = nullptr, ClearItem_Ret = nullptr;

    void __fastcall OnEquipItem(GW::NPCEquipment* equip, uint32_t edx, uint32_t slot) {
        GW::Hook::EnterHook();
        if(equip->items[slot].model_file_id)
            EquipItem_Ret(equip, edx, slot);
        GW::Hook::LeaveHook();
        if (equip != GetPlayerEquipment()) return;
        if (equip != equip_cached) {
            equip_cached = equip;
            memcpy(&original_pieces, &equip->items, sizeof(original_pieces));
            memcpy(&drawn_pieces, &equip->items, sizeof(drawn_pieces));
            memcpy(&imgui_armor_pieces, &equip->items, sizeof(drawn_pieces));
        }
        drawn_pieces[slot] = equip->items[slot];
        imgui_armor_pieces[slot] = drawn_pieces[slot];
        if (!gwarmory_setitem) {
            original_pieces[slot] = drawn_pieces[slot];
        }
        UpdateArmorsFilter();
    }
    void __fastcall OnClearItem(GW::NPCEquipment* equip, uint32_t edx, uint32_t slot)
    {
        GW::Hook::EnterHook();
        if (equip->items[slot].model_file_id)
            ClearItem_Ret(equip, edx, slot);
        GW::Hook::LeaveHook();
        if (equip != GetPlayerEquipment()) return;
        if (equip != equip_cached) {
            equip_cached = equip;
            memcpy(&original_pieces, &equip->items, sizeof(original_pieces));
            memcpy(&drawn_pieces, &equip->items, sizeof(drawn_pieces));
            memcpy(&imgui_armor_pieces, &equip->items, sizeof(imgui_armor_pieces));
        }
        drawn_pieces[slot] = {0};
        imgui_armor_pieces[slot] = drawn_pieces[slot];
        if (!gwarmory_setitem) {
            original_pieces[slot] = drawn_pieces[slot];
        }
        UpdateArmorsFilter();
    }

    bool HookEquipmentActions() {
        if (EquipItem_Func && ClearItem_Func) return true;
        const auto equip = GetPlayerEquipment();
        if (!equip)
            return false;
        equip_cached = equip;
        EquipItem_Func = equip->vtable->EquipItem;
        GW::Hook::CreateHook((void**)&EquipItem_Func, OnEquipItem, (void**)&EquipItem_Ret);
        GW::Hook::EnableHooks(EquipItem_Func);
        ClearItem_Func = equip->vtable->RemoveItem;
        GW::Hook::CreateHook((void**)&ClearItem_Func, OnClearItem, (void**)&ClearItem_Ret);
        GW::Hook::EnableHooks(ClearItem_Func);
        return true;
    }

    bool IsEquipped(GW::NPCEquipment* _equip, uint32_t _item_id)
    {
        for (auto item_id : _equip->item_ids) {
            if (item_id == _item_id) return true;
        }
        return false;
    }

    typedef void(__fastcall* UpdateWeaponAnimation_pt)(GW::AgentLiving* agent, uint32_t edx, uint32_t weapon_item_id, uint32_t offhand_item_id);
    UpdateWeaponAnimation_pt UpdateWeaponAnimation_Func = 0;

    bool UpdateWeaponDisplay() {
        if (!UpdateWeaponAnimation_Func) return false;
        const auto agent = GW::Agents::GetControlledCharacter();
        if(!(agent && agent->equip && *agent->equip)) 
            return false;
        const auto equip = *agent->equip;
        auto item_array = GW::Items::GetItemArray();
        if (!item_array) return false;

        GW::Item fake1;
        ItemDataToGWItem(&equip->items[ItemSlot::RightHand], &fake1);
        fake1.item_id = item_array->size() - 1;
        while (IsEquipped(equip, fake1.item_id)) {
            fake1.item_id--;
        }

        GW::Item fake2;
        ItemDataToGWItem(&equip->items[ItemSlot::LeftHand], &fake2);
        fake2.item_id = fake1.item_id - 1;
        while (IsEquipped(equip, fake2.item_id)) {
            fake2.item_id--;
        }

        GW::Item* original_weapon = (*item_array)[fake1.item_id];
        (*item_array)[fake1.item_id] = &fake1;

        GW::Item* original_offhand = (*item_array)[fake2.item_id];
        (*item_array)[fake2.item_id] = &fake2;

        gwarmory_setitem = true;
        UpdateWeaponAnimation_Func(agent, 0, fake1.model_file_id ? fake1.item_id : 0, fake2.model_file_id ? fake2.item_id : 0);
        (*item_array)[fake1.item_id] = original_weapon;
        (*item_array)[fake2.item_id] = original_offhand;
        gwarmory_setitem = false;
        return true;
    }

    bool ClearArmorItem(ItemSlot slot) {
        if (!HookEquipmentActions()) return false;
        const auto equip = GetPlayerEquipment();
        if (!equip) return false;
        if (!equip->items[slot].model_file_id) return true;
        gwarmory_setitem = true;
        equip->vtable->RemoveItem(equip, 0, slot);
        equip->items[slot] = {0};
        gwarmory_setitem = false;
        return true;
    }

    void RevertCostumePieces()
    {
        const auto equip = GetPlayerEquipment();
        if (!equip) return;
        gwarmory_setitem = true;
        const ItemSlot toCheck[] = {ItemSlot::Chestpiece, ItemSlot::Leggings, ItemSlot::Gloves, ItemSlot::Boots};
        for (auto slot : toCheck) {
            if (!IsCostumeFileId(drawn_pieces[slot].model_file_id)) 
                continue;
            if (equip->items[slot].model_file_id && !IsCostumeFileId(equip->items[slot].model_file_id)) {
                equip->vtable->EquipItem(equip, 0, slot);
                continue;
            }
            if (!equip->item_ids[slot]) {
                if (equip->items[slot].model_file_id) {
                    equip->vtable->RemoveItem(equip, 0, slot);
                    equip->items[slot] = {0};
                } 
                continue;
            }
            const auto item = GW::Items::GetItemById(equip->item_ids[slot]);
            if (item && item->model_file_id && !IsCostumeFileId(item->model_file_id)) {
                GwItemToItemData(item, &equip->items[slot]);
                equip->vtable->EquipItem(equip, 0, slot);
            }
        }
        gwarmory_setitem = false;
    }

    bool SetArmorItem(const GW::ItemData* item_data) {
        const auto slot = GetSlotFromItemType(item_data->type);
        if (slot == ItemSlot::Unknown) return false;
        if (!HookEquipmentActions()) return false;
        const auto equip = GetPlayerEquipment();
        if (!equip) return false;

        if (!item_data->model_file_id) {
            if (slot == ItemSlot::CostumeHead && GetFileIdForFestivalHat(drawn_pieces[slot].model_file_id, current_profession)) {
                ClearArmorItem(ItemSlot::Headpiece);
            }
            if (slot == ItemSlot::CostumeBody || slot == ItemSlot::Chestpiece || slot == ItemSlot::Gloves || slot == ItemSlot::Leggings || slot == ItemSlot::Boots) {
                RevertCostumePieces();
            }
            ClearArmorItem(slot);
            if (slot == ItemSlot::RightHand || slot == ItemSlot::LeftHand) 
                UpdateWeaponDisplay();
            return true;
        }

        const auto cpy = *item_data;
        
        if (IsWeapon(item_data->type)) {
            if (IsBothHands(item_data->type)) {
                ClearArmorItem(ItemSlot::LeftHand);
            }
            if (IsBothHands(equip->items[ItemSlot::RightHand].type)) {
                ClearArmorItem(ItemSlot::RightHand);
            }
        }
        if (slot == ItemSlot::Headpiece) {
            
            ClearArmorItem(ItemSlot::CostumeHead);
        }
        if (IsBodyArmor(slot) && !IsCostumeFileId(item_data->model_file_id)) {
            ClearArmorItem(ItemSlot::CostumeBody);
            RevertCostumePieces();
        }
        ClearArmorItem(slot);
        gwarmory_setitem = true;
        equip->items[slot] = cpy;
        equip->vtable->EquipItem(equip, 0, slot);
        
        if (slot == ItemSlot::CostumeHead) {
            // If we're a festival hat, set the correct model file id for this character's profession
            if (const auto hat_found = GetFileIdForFestivalHat(cpy.model_file_id, current_profession)) {
                equip->items[ItemSlot::Headpiece] = cpy;
                equip->items[ItemSlot::Headpiece].model_file_id = *hat_found;
                equip->vtable->EquipItem(equip, 0, ItemSlot::Headpiece);
            }
        }
        if (slot == ItemSlot::CostumeBody) {
            if (const auto costume_found = GetFileIdsForCostume(cpy.model_file_id, current_profession)) {
                // If we're a costume, set all of the other armor piece model file ids for this character's profession
                equip->items[ItemSlot::Boots] = cpy;
                equip->items[ItemSlot::Boots].model_file_id = costume_found[0];
                equip->vtable->EquipItem(equip, 0, ItemSlot::Boots);
                equip->items[ItemSlot::Leggings] = cpy;
                equip->items[ItemSlot::Leggings].model_file_id = costume_found[1];
                equip->vtable->EquipItem(equip, 0, ItemSlot::Leggings);
                equip->items[ItemSlot::Gloves] = cpy;
                equip->items[ItemSlot::Gloves].model_file_id = costume_found[2];
                equip->vtable->EquipItem(equip, 0, ItemSlot::Gloves);
                equip->items[ItemSlot::Chestpiece] = cpy;
                equip->items[ItemSlot::Chestpiece].model_file_id = costume_found[3];
                equip->vtable->EquipItem(equip, 0, ItemSlot::Chestpiece);
            }
        }
        gwarmory_setitem = false;
        if(slot == ItemSlot::RightHand || slot == ItemSlot::LeftHand)
            UpdateWeaponDisplay();
        return true;
    }
    
}

using namespace GWArmory;

bool ArmoryWindow::CanPreviewItem(GW::Item* item) {
    if (!item) return false;
    if (IsWeapon(item->type)) return true;
    if (!equip_cached) return false; // aka not initialised?
    if (IsArmor(item->type)) {
        const auto profession = GetItemProfession(item);
        return profession == GW::Constants::ProfessionByte::None || profession == (GW::Constants::ProfessionByte)GetPlayerProfession();
    }
    if (item->type == ItemType::Costume || item->type == ItemType::Costume_Headpiece) {
        return true;
    }
    return false;
}

void ArmoryWindow::PreviewItem(GW::Item* item) {
    
    GW::GameThread::Enqueue([item]() {
        if (!CanPreviewItem(item)) 
            return;
        GW::ItemData data;
        GwItemToItemData(item, &data);
        if(data.model_file_id)
            SetArmorItem(&data);
        });
}

void ArmoryWindow::Update(float) {
    if (pending_initialise_equipment) {
        const auto equip = GetPlayerEquipment();
        if (equip) {
            equip_cached = equip;
            memcpy(&original_pieces, &equip->items, sizeof(original_pieces));
            memcpy(&drawn_pieces, &equip->items, sizeof(drawn_pieces));
            memcpy(&imgui_armor_pieces, &equip->items, sizeof(imgui_armor_pieces));
            HookEquipmentActions();
            pending_initialise_equipment = false;
            pending_reset_equipment = true;
        }
    }
}

void ArmoryWindow::Draw(IDirect3DDevice9*)
{
    SnapshotToFile();
    if (!visible) {
        return;
    }

    if(pending_initialise_equipment)
        return;
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
                SetArmorItem(&imgui_armor_pieces[slot]);
            }
        }
        #ifdef _DEBUG
        if (ImGui::Button("Snapshot inventory weapons")) {
            state = SnapshotState::Pending;
        }
        #endif
    }
    ImGui::End();
}

void ArmoryWindow::Initialize()
{
    ToolboxWindow::Initialize();
    UpdateWeaponAnimation_Func = (UpdateWeaponAnimation_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\xc6\x86\xba\x01\x00\x00\x04", "xxxxxxx", 0), 0xfff);
    
    costume_data_ptr = (CostumeData*)GW::Scanner::Find("\xe5\x09\x00\x00\xf0\x09\x00\x00", "xxxxxxxx", -0xc, GW::ScannerSection::Section_RDATA);
    festival_hat_data_ptr = (FestivalHatData*)GW::Scanner::Find("\xe3\x09\x00\x00\xef\x09\x00\x00", "xxxxxxxx", 0, GW::ScannerSection::Section_RDATA);

#ifdef _DEBUG 
    ASSERT(UpdateWeaponAnimation_Func);
    ASSERT(costume_data_ptr);
    ASSERT(festival_hat_data_ptr);
#endif

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"armory", CmdArmory);
}

void ArmoryWindow::Terminate()
{
    Reset(); // NB: We're on the game thread, so this is ok
    if (EquipItem_Func) {
        GW::Hook::RemoveHook(EquipItem_Func);
        EquipItem_Func = nullptr;
    }
    if (ClearItem_Func) {
        GW::Hook::RemoveHook(ClearItem_Func);
        ClearItem_Func = nullptr;
    }
    pending_initialise_equipment = true;
    pending_reset_equipment = true;
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}
