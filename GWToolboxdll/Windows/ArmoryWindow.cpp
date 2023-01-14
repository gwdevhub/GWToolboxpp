#include "stdafx.h"

#include "ToolboxWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Timer.h>
#include <ImGuiAddons.h>

#include <Windows/ArmoryWindow_Constants.h>
#include <Windows/ArmoryWindow.h>

namespace GWArmory {

    bool reset_helm_visibility = false;
    bool head_item_set = false;

    typedef void(__fastcall * SetItem_pt)(GW::Equipment * equip, void* edx, uint32_t model_file_id, uint32_t color, uint32_t arg3, uint32_t agent_id);
    SetItem_pt SetItem_Func;
    ComboListState head;
    ComboListState chest;
    ComboListState hands;
    ComboListState legs;
    ComboListState feets;

    PlayerArmor player_armor;
    GW::Constants::Profession current_profession = GW::Constants::Profession::None;

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

    void SetArmorItem(const PlayerArmorPiece* piece)
    {
        const auto player = GW::Agents::GetPlayerAsAgentLiving();
        if (!(player && player->equip && *player->equip))
            return;
        const uint32_t color = CreateColor(piece->color1, piece->color2, piece->color3, piece->color4);
        // 0x60111109
        if (piece->model_file_id && SetItem_Func) {
            SetItem_Func(*player->equip, nullptr, piece->model_file_id, color, 0x20110007, piece->unknow1);
        }
    }


    void UpdateArmorsFilter(GW::Constants::Profession prof, Campaign campaign)
    {
        size_t count = 0;
        Armor* armors = GetArmorsPerProfession(prof, &count);

        head.pieces.clear();
        chest.pieces.clear();
        hands.pieces.clear();
        legs.pieces.clear();
        feets.pieces.clear();

        head.current_piece_index = -1;
        chest.current_piece_index = -1;
        hands.current_piece_index = -1;
        legs.current_piece_index = -1;
        feets.current_piece_index = -1;

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
            case ItemSlot_Feets:
                state = &feets;
                piece = &player_armor.feets;
                break;
            }

            if (!(state && piece))
                continue;
            if (piece->model_file_id == armors[i].model_file_id)
                state->current_piece = &armors[i];
            if (campaign != Campaign_All && armors[i].campaign != campaign)
                continue;
            state->pieces.push_back(&armors[i]);

            if (piece && piece->model_file_id) {
                SetArmorItem(piece);
            }
        }

        current_profession = prof;
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

void ArmoryWindow::Update(const float delta)
{
    ToolboxWindow::Update(delta);
    if (!head_item_set) {
        reset_helm_visibility = false;
        return;
    }
    static clock_t timer = 0;

    const auto is_visible = GW::Items::GetEquipmentVisibility(GW::EquipmentType::Helm);
    static auto wanted_visibility = is_visible;

    const auto toggle_visibility = [is_visible] {
        if (is_visible == GW::EquipmentStatus::AlwaysHide) {
            GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
        }
        else if (is_visible == GW::EquipmentStatus::HideInCombatAreas) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
            }
        }
        else if (is_visible == GW::EquipmentStatus::HideInTownsAndOutposts) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
                GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, GW::EquipmentStatus::AlwaysShow);
            }
        }
    };

    if (reset_helm_visibility) {
        timer = TIMER_INIT();
        wanted_visibility = is_visible;
        reset_helm_visibility = false;
        toggle_visibility();
    }

    if (timer != 0 && TIMER_DIFF(timer) > 300) {
        GW::Items::SetEquipmentVisibility(GW::EquipmentType::Helm, wanted_visibility);
        timer = 0;
        head_item_set = false;
    }
}

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

    static int filter_index = Campaign_All;

    // constexpr ImVec2 window_size(330.f, 208.f);
    // ImGui::SetNextWindowSize(window_size);
    if (ImGui::Begin("Armory", &visible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Profession: %s", GetProfessionName(prof));
        ImGui::SameLine(ImGui::GetWindowWidth() - 65.f);

        if (ImGui::Button("Refresh")) {
            reset_helm_visibility = true;
            head.current_piece = nullptr;
            if (player_agent->equip && player_agent->equip[0]) {
                GW::Equipment* equip = player_agent->equip[0];
                InitItemPiece(&player_armor.head, &equip->head);
                InitItemPiece(&player_armor.chest, &equip->chest);
                InitItemPiece(&player_armor.hands, &equip->hands);
                InitItemPiece(&player_armor.legs, &equip->legs);
                InitItemPiece(&player_armor.feets, &equip->feet);
            }

            update_data = true;
        }

        if (ImGui::MyCombo("##filter", "All", &filter_index, armor_filter_array_getter, nullptr, 5))
            update_data = true;
        if (update_data) {
            UpdateArmorsFilter(prof, static_cast<Campaign>(filter_index));
        }

        if (DrawArmorPiece("##head", &player_armor.head, &head)) {
            SetArmorItem(&player_armor.head);
            head_item_set = true;
        }
        if (DrawArmorPiece("##chest", &player_armor.chest, &chest))
            SetArmorItem(&player_armor.chest);
        if (DrawArmorPiece("##hands", &player_armor.hands, &hands))
            SetArmorItem(&player_armor.hands);
        if (DrawArmorPiece("##legs", &player_armor.legs, &legs))
            SetArmorItem(&player_armor.legs);
        if (DrawArmorPiece("##feets", &player_armor.feets, &feets))
            SetArmorItem(&player_armor.feets);
    }
    ImGui::End();
}

void ArmoryWindow::Initialize()
{
    ToolboxWindow::Initialize();

    SetItem_Func = (SetItem_pt)GW::Scanner::Find("\x83\xC4\x04\x8B\x08\x8B\xC1\xC1", "xxxxxxxx", -0x24);
    if (!SetItem_Func) {
        Log::Error("GWArmory failed to find the SetItem function");
    }

    const auto player_agent = GW::Agents::GetPlayerAsAgentLiving();
    if (player_agent && player_agent->equip && player_agent->equip[0]) {
        GW::Equipment* equip = player_agent->equip[0];
        InitItemPiece(&player_armor.head, &equip->head);
        InitItemPiece(&player_armor.chest, &equip->chest);
        InitItemPiece(&player_armor.hands, &equip->hands);
        InitItemPiece(&player_armor.legs, &equip->legs);
        InitItemPiece(&player_armor.feets, &equip->feet);
    }
}

void ArmoryWindow::Terminate()
{
    ToolboxWindow::Terminate();
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    const GW::Constants::Profession prof = GetAgentProfession(player_agent);

    UpdateArmorsFilter(prof, Campaign_All);
}
