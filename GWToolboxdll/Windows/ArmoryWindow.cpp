#include "stdafx.h"

#include "ToolboxWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <ImGuiAddons.h>

#include <Windows/ArmoryWindow_Constants.h>
#include <Windows/ArmoryWindow.h>

namespace GWArmory {

    struct PlayerArmor {
        PlayerArmorPiece head = ItemSlot::ItemSlot_Head;
        PlayerArmorPiece chest = ItemSlot::ItemSlot_Chest;
        PlayerArmorPiece hands = ItemSlot::ItemSlot_Hands;
        PlayerArmorPiece legs = ItemSlot::ItemSlot_Legs;
        PlayerArmorPiece feets = ItemSlot::ItemSlot_Feet;
    };

    PlayerArmor player_armor;

    typedef void(__fastcall * SetItem_pt)(GW::Equipment * equip, void* edx, uint32_t model_file_id, uint32_t color, uint32_t arg3, uint32_t agent_id);
    SetItem_pt SetItem_Func = nullptr;
    SetItem_pt SetItem_Ret = nullptr;

    bool gwarmory_setitem = false;
    bool pending_reset_equipment = false;

    struct ItemModelInfo {
        uint32_t class_flags;
        uint32_t file_id_1;
        uint32_t unk[4];
        uint32_t file_id_2;
        uint32_t unk2[5];
    };
    static_assert(sizeof(ItemModelInfo) == 0x30);

    GW::Array<ItemModelInfo>* item_model_info_array = nullptr;

    bool Reset();

    GW::Equipment* GetPlayerEquipment() {
        const auto player = GW::Agents::GetPlayerAsAgentLiving();
        return player && player->equip && *player->equip ? *player->equip : nullptr;
    }

    uint32_t GetEquipmentPieceItemId(ItemSlot slot) {
        const auto equip = GetPlayerEquipment();
        if (!equip) return 0;
        switch (slot) {
        case ItemSlot::ItemSlot_Chest: return equip->item_id_chest;
        case ItemSlot::ItemSlot_Feet: return equip->item_id_feet;
        case ItemSlot::ItemSlot_Hands: return equip->item_id_hands;
        case ItemSlot::ItemSlot_Head: return equip->item_id_head;
        case ItemSlot::ItemSlot_Legs: return equip->item_id_legs;
        default:
            return 0;
        }
    }


    uint32_t GetItemInteraction(ItemSlot slot) {
        const auto item = GW::Items::GetItemById(GetEquipmentPieceItemId(slot));
        return item ? item->interaction : 0;
    }

    ItemSlot GetItemSlot(uint32_t model_file_id);

    void __fastcall OnSetItem(GW::Equipment* equip, void* edx, uint32_t model_file_id, uint32_t color, uint32_t arg3, uint32_t agent_id) {
        GW::Hook::EnterHook();
        
        SetItem_Ret(equip, edx, model_file_id, color, arg3, agent_id);
        const auto player_equip = GetPlayerEquipment();
        if (!gwarmory_setitem && (!player_equip || equip == player_equip)) {
            // Reset controls - this could be done a little smarter to remember bits that haven't changed.
            pending_reset_equipment = true;
        }
        /*if (equip == player_equip) {
            const auto slot = GetItemSlot(model_file_id);
            Log::Info("Item in slot %d changed to model %d", slot, model_file_id);
        }*/
        GW::Hook::LeaveHook();
    }


    ComboListState head;
    ComboListState chest;
    ComboListState hands;
    ComboListState legs;
    ComboListState feets;

    GW::Constants::Profession current_profession = GW::Constants::Profession::None;
    Campaign current_campaign = Campaign_All;

    

    Armor* GetArmorsPerProfession(GW::Constants::Profession prof, size_t * count)
    {
        switch (prof) {
        case GW::Constants::Profession::Warrior: *count = _countof(warrior_armors); return warrior_armors;
        case GW::Constants::Profession::Ranger: *count = _countof(ranger_armors); return ranger_armors;
        case GW::Constants::Profession::Monk: *count = _countof(monk_armors); return monk_armors;
        case GW::Constants::Profession::Necromancer: *count = _countof(necromancer_armors); return necromancer_armors;
        case GW::Constants::Profession::Mesmer: *count = _countof(mesmer_armors); return mesmer_armors;
        case GW::Constants::Profession::Elementalist: *count = _countof(elementalist_armors); return elementalist_armors;
        case GW::Constants::Profession::Assassin: *count = _countof(assassin_armors); return assassin_armors;
        case GW::Constants::Profession::Ritualist: *count = _countof(ritualist_armors); return ritualist_armors;
        case GW::Constants::Profession::Paragon: *count = _countof(paragon_armors); return paragon_armors;
        case GW::Constants::Profession::Dervish: *count = _countof(dervish_armors); return dervish_armors;
        case GW::Constants::Profession::None:
        default: *count = 0; return nullptr;
        }
    }
    DyeColor DyeColorFromInt(size_t color)
    {
        const auto col = static_cast<DyeColor>(color);
        switch (col) {
        case DyeColor::Blue:
        case DyeColor::Green:
        case DyeColor::Purple:
        case DyeColor::Red:
        case DyeColor::Yellow:
        case DyeColor::Brown:
        case DyeColor::Orange:
        case DyeColor::Silver:
        case DyeColor::Black:
        case DyeColor::Gray:
        case DyeColor::White:
        case DyeColor::Pink: return col;
        default: return DyeColor::None;
        }
    }

    ImVec4 ImVec4FromDyeColor(DyeColor color)
    {
        const uint32_t color_id = static_cast<uint32_t>(color) - static_cast<uint32_t>(DyeColor::Blue);
        switch (color) {
        case DyeColor::Blue:
        case DyeColor::Green:
        case DyeColor::Purple:
        case DyeColor::Red:
        case DyeColor::Yellow:
        case DyeColor::Brown:
        case DyeColor::Orange:
        case DyeColor::Silver:
        case DyeColor::Black:
        case DyeColor::Gray:
        case DyeColor::White:
        case DyeColor::Pink: assert(color_id < _countof(palette)); return palette[color_id];
        default: return {};
        }
    }

    const char* GetProfessionName(GW::Constants::Profession prof)
    {
        switch (prof) {
        case GW::Constants::Profession::None: return "None";
        case GW::Constants::Profession::Warrior: return "Warrior";
        case GW::Constants::Profession::Ranger: return "Ranger";
        case GW::Constants::Profession::Monk: return "Monk";
        case GW::Constants::Profession::Necromancer: return "Necromancer";
        case GW::Constants::Profession::Mesmer: return "Mesmer";
        case GW::Constants::Profession::Elementalist: return "Elementalist";
        case GW::Constants::Profession::Assassin: return "Assassin";
        case GW::Constants::Profession::Ritualist: return "Ritualist";
        case GW::Constants::Profession::Paragon: return "Paragon";
        case GW::Constants::Profession::Dervish: return "Dervish";
        default: return "Unknown Profession";
        }
    }

    GW::Constants::Profession GetAgentProfession(GW::AgentLiving * agent)
    {
        if (!agent)
            return GW::Constants::Profession::None;
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
        case GW::Constants::Profession::Dervish: return primary;
        default: return GW::Constants::Profession::None;
        }
    }

    bool armor_filter_array_getter(void*, int idx, const char** out_text)
    {
        switch (idx) {
        case Campaign_All: *out_text = "All"; break;
        case Campaign_Core: *out_text = "Core"; break;
        case Campaign_Prophecies: *out_text = "Prophecies"; break;
        case Campaign_Factions: *out_text = "Factions"; break;
        case Campaign_Nightfall: *out_text = "Nightfall"; break;
        case Campaign_EotN: *out_text = "Eye of the North"; break;
        default: return false;
        }
        return true;
    }

    bool armor_pieces_array_getter(void* data, int idx, const char** out_text)
    {
        const auto armors = static_cast<Armor**>(data);
        *out_text = armors[idx]->label;
        return true;
    }

    uint32_t CreateColor(DyeColor col1, DyeColor col2 = DyeColor::None, DyeColor col3 = DyeColor::None, DyeColor col4 = DyeColor::None)
    {
        if (col1 == DyeColor::None && col2 == DyeColor::None && col3 == DyeColor::None && col4 == DyeColor::None)
            col1 = DyeColor::Gray;
        uint32_t c1 = static_cast<uint32_t>(col1);
        uint32_t c2 = static_cast<uint32_t>(col2);
        uint32_t c3 = static_cast<uint32_t>(col3);
        uint32_t c4 = static_cast<uint32_t>(col4);
        uint32_t composite = c1 | (c2 << 4) | (c3 << 8) | (c4 << 12);
        return composite;
    }

    ItemModelInfo* GetItemModelInfo(uint32_t model_file_id) {
        if (!(item_model_info_array && model_file_id && model_file_id < item_model_info_array->size()))
            return nullptr;
        return &item_model_info_array->m_buffer[model_file_id];
    }
    ItemSlot GetItemSlot(uint32_t model_file_id) {
        if (!model_file_id)
            return ItemSlot::ItemSlot_Unknown;
        const auto info = GetItemModelInfo(model_file_id);
        if (!info)
            return ItemSlot::ItemSlot_Unknown;
        switch (info->class_flags >> 0x16) {
        case 0:
        case 1:
        case 2:
            return ItemSlot::ItemSlot_Unknown; // 8
        case 3:
        case 15:
            return ItemSlot::ItemSlot_Chest;
        case 4:
        case 14:
            return ItemSlot::ItemSlot_Feet;
        case 5:
        case 16:
            return ItemSlot::ItemSlot_Hands;
        case 6:
        case 18:
            return ItemSlot::ItemSlot_Legs;
        case 7:
        case 8:
        case 9:
            return ItemSlot::ItemSlot_Unknown; // 7
        case 10:
        case 11:
            return ItemSlot::ItemSlot_Unknown; // 0
        case 12:
        case 13:
            return ItemSlot::ItemSlot_Unknown; // 2
        case 19:
            return ItemSlot::ItemSlot_Head;
        default:
            return ItemSlot::ItemSlot_Unknown;
        }

    }

    void SetArmorItem(const PlayerArmorPiece* piece)
    {
        const auto equip = GetPlayerEquipment();
        const uint32_t color = CreateColor(piece->color1, piece->color2, piece->color3, piece->color4);
        // 0x60111109
        uint32_t interaction = GetItemInteraction(piece->slot);
        if (GetItemModelInfo(piece->model_file_id) && interaction && SetItem_Func) {
            gwarmory_setitem = true;
            SetItem_Func(equip, nullptr, piece->model_file_id, color, interaction, piece->unknow1);
            gwarmory_setitem = false;
        }
    }


    bool IsEquipmentShowing(const GW::EquipmentType type) {
        const auto state = GW::Items::GetEquipmentVisibility(type);
        switch (state) {
        case GW::EquipmentStatus::AlwaysShow:
            return true;
        case GW::EquipmentStatus::AlwaysHide:
            return false;
        case GW::EquipmentStatus::HideInCombatAreas:
            return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable;
        case GW::EquipmentStatus::HideInTownsAndOutposts:
            return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost;
        }
        return false;
    }


    void UpdateArmorsFilter()
    {
        size_t count = 0;
        Armor* armors = GetArmorsPerProfession(current_profession, &count);

        head.pieces.clear();
        chest.pieces.clear();
        hands.pieces.clear();
        legs.pieces.clear();
        feets.pieces.clear();

        head.current_piece_index = -1;
        head.current_piece = nullptr;
        chest.current_piece_index = -1;
        chest.current_piece = nullptr;
        hands.current_piece_index = -1;
        hands.current_piece = nullptr;
        legs.current_piece_index = -1;
        legs.current_piece = nullptr;
        feets.current_piece_index = -1;
        feets.current_piece = nullptr;

        for (size_t i = 0; i < count; i++) {
            ComboListState* state = nullptr;
            PlayerArmorPiece* piece = nullptr;

            switch (armors[i].item_slot) {
            case ItemSlot_Head:
                state = &head;
                piece = &player_armor.head;
                break;
            case ItemSlot_Chest:
                state = &chest;
                piece = &player_armor.chest;
                break;
            case ItemSlot_Hands:
                state = &hands;
                piece = &player_armor.hands;
                break;
            case ItemSlot_Legs:
                state = &legs;
                piece = &player_armor.legs;
                break;
            case ItemSlot_Feet:
                state = &feets;
                piece = &player_armor.feets;
                break;
            }

            if (!(state && piece))
                continue;
            if (piece->model_file_id == armors[i].model_file_id)
                state->current_piece = &armors[i];
            if (current_campaign != Campaign_All && armors[i].campaign != current_campaign)
                continue;
            state->pieces.push_back(&armors[i]);

            if (piece && piece->model_file_id) {
                // Why is this here?
                SetArmorItem(piece);
            }
        }
    }

    void InitItemPiece(PlayerArmorPiece * piece, const GW::Equipment::ItemData* item_data)
    {
        piece->model_file_id = item_data->model_file_id;
        piece->unknow1 = item_data->dye.dye_id;
        piece->color1 = DyeColorFromInt(item_data->dye.dye1);
        piece->color2 = DyeColorFromInt(item_data->dye.dye2);
        piece->color3 = DyeColorFromInt(item_data->dye.dye3);
        piece->color4 = DyeColorFromInt(item_data->dye.dye4);
    }

    bool GetPlayerArmor(PlayerArmor& out) {
        const auto player = static_cast<GW::AgentLiving*>(GW::Agents::GetPlayer());
        if (!(player && player->equip && player->equip[0]))
            return false;
        const GW::Equipment* equip = player->equip[0];
        InitItemPiece(&out.head, &equip->head);
        InitItemPiece(&out.chest, &equip->chest);
        InitItemPiece(&out.hands, &equip->hands);
        InitItemPiece(&out.legs, &equip->legs);
        InitItemPiece(&out.feets, &equip->feet);

        return true;
    }

    bool Reset() {
        if (!GetPlayerArmor(player_armor))
            return false;
        SetArmorItem(&player_armor.head);
        SetArmorItem(&player_armor.chest);
        SetArmorItem(&player_armor.hands);
        SetArmorItem(&player_armor.legs);
        SetArmorItem(&player_armor.feets);

        current_campaign = Campaign_All;
        current_profession = GetAgentProfession(static_cast<GW::AgentLiving*>(GW::Agents::GetPlayer()));

        UpdateArmorsFilter();
        return true;
    }

    bool DyePicker(const char* label, DyeColor* color)
    {
        ImGui::PushID(label);

        const ImVec4 current_color = ImVec4FromDyeColor(*color);

        bool value_changed = false;
        const char* label_display_end = ImGui::FindRenderedTextEnd(label);

        if (ImGui::ColorButton("##ColorButton", current_color, *color == DyeColor::None ? ImGuiColorEditFlags_AlphaPreview : 0))
            ImGui::OpenPopup("picker");

        if (ImGui::BeginPopup("picker")) {
            if (label != label_display_end) {
                ImGui::TextUnformatted(label, label_display_end);
                ImGui::Separator();
            }
            size_t palette_index;
            if (ImGui::ColorPalette("##picker", &palette_index, palette, _countof(palette), 7, ImGuiColorEditFlags_AlphaPreview)) {
                if (palette_index < _countof(palette))
                    *color = DyeColorFromInt(palette_index + static_cast<size_t>(DyeColor::Blue));
                else
                    *color = DyeColor::None;
                value_changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return value_changed;
    }

    bool DrawArmorPiece(const char* label, PlayerArmorPiece* player_piece, ComboListState* state)
    {
        ImGui::PushID(label);

        bool value_changed = false;
        const char* preview = state->current_piece ? state->current_piece->label : "";
        if (ImGui::MyCombo("##armors", preview, &state->current_piece_index, armor_pieces_array_getter, state->pieces.data(), state->pieces.size())) {
            if (0 <= state->current_piece_index && static_cast<size_t>(state->current_piece_index) < state->pieces.size()) {
                state->current_piece = state->pieces[state->current_piece_index];
                player_piece->model_file_id = state->current_piece->model_file_id;
                player_piece->unknow1 = state->current_piece->unknow1;
            }
            value_changed = true;
        }

        ImGui::SameLine();
        value_changed |= DyePicker("color1", &player_piece->color1);
        ImGui::SameLine();
        value_changed |= DyePicker("color2", &player_piece->color2);
        ImGui::SameLine();
        value_changed |= DyePicker("color3", &player_piece->color3);
        ImGui::SameLine();
        value_changed |= DyePicker("color4", &player_piece->color4);

        ImGui::PopID();
        return value_changed;
    }
}

using namespace GWArmory;

void ArmoryWindow::Draw(IDirect3DDevice9*)
{
    if (!visible)
        return;
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    if (!player_agent)
        return;

    bool update_data = false;
    const GW::Constants::Profession prof = GetAgentProfession(player_agent);
    if (prof != current_profession)
        update_data = true;

    const auto equip = GetPlayerEquipment();
    if (!equip)
        return;

    if (pending_reset_equipment) {
        Reset();
        pending_reset_equipment = false;
    }  

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 208), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::Text("Profession: %s", GetProfessionName(prof));
        ImGui::SameLine(ImGui::GetWindowWidth() - 65.f);

        if (ImGui::Button("Reset"))
            pending_reset_equipment = true;

        if (ImGui::MyCombo("##filter", "All", (int*) & current_campaign, armor_filter_array_getter, nullptr, 5))
            UpdateArmorsFilter();
        bool showing_helm = !equip->costume_head.model_file_id && IsEquipmentShowing(GW::EquipmentType::Helm);
        bool showing_body = !equip->costume_body.model_file_id;

        if (showing_helm) {
            if (DrawArmorPiece("##head", &player_armor.head, &head))
                SetArmorItem(&player_armor.head);
        }
        if (showing_body) {
            if (DrawArmorPiece("##chest", &player_armor.chest, &chest))
                SetArmorItem(&player_armor.chest);
            if (DrawArmorPiece("##hands", &player_armor.hands, &hands))
                SetArmorItem(&player_armor.hands);
            if (DrawArmorPiece("##legs", &player_armor.legs, &legs))
                SetArmorItem(&player_armor.legs);
            if (DrawArmorPiece("##feets", &player_armor.feets, &feets))
                SetArmorItem(&player_armor.feets);
        }

        if (!showing_helm) {
            ImGui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE " Your helm is currently hidden ");
            ImGui::SameLine();
            if (ImGui::SmallButton("Show Helm")) {
                GW::GameThread::Enqueue([]() {
                    GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
                    GW::Items::SetEquipmentVisibility(GW::EquipmentType::CostumeHeadpiece, GW::EquipmentStatus::AlwaysHide);
                    });
            }
        }
        if (!showing_body) {
            ImGui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE " Your armor is currently hidden ");
            ImGui::SameLine();
            if (ImGui::SmallButton("Hide Costume")) {
                GW::GameThread::Enqueue([]() {
                    GW::Items::SetEquipmentVisibility(GW::EquipmentType::CostumeBody, GW::EquipmentStatus::AlwaysHide);
                    });
            }
        }
    }
    ImGui::End();
}

void ArmoryWindow::Initialize()
{
    ToolboxWindow::Initialize();


    uintptr_t address = GW::Scanner::Find("\x8b\x04\xc7\x5f\xc1\xe8\x16", "xxxxxxx", -0x2c);
    address = GW::Scanner::FunctionFromNearCall(address); // GetModelItemInfo
    
    if (address) {
        address += 0x27;
        item_model_info_array = *(GW::Array<ItemModelInfo>**)address;
    }
    

    SetItem_Func = (SetItem_pt)GW::Scanner::Find("\x83\xC4\x04\x8B\x08\x8B\xC1\xC1", "xxxxxxxx", -0x24);
    if (SetItem_Func) {
        GW::Hook::CreateHook(SetItem_Func, OnSetItem, (void**)&SetItem_Ret);
    }
    else {
        Log::Error("GWArmory failed to find the SetItem function");
    }
#ifdef _DEBUG
    char* buf = nullptr;
    for (auto& armor : warrior_armors) {
        buf = new char[100];
        snprintf(buf, 100, "%s %#04x", armor.label, armor.model_file_id);
        armor.label = buf;
    }
#endif
    pending_reset_equipment = true;
}
void ArmoryWindow::Terminate() {
    Reset(); // NB: We're on the game thread, so this is ok
    if (SetItem_Func) {
        GW::Hook::RemoveHook(SetItem_Func);
        SetItem_Func = 0;
    }
}
