#include "Helpers.h"
#include "stl.h"

#include "ImGuiAddons.h"
#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

#include "ArmorsDatabase.h"

SetItem_pt SetItem_Func;

PlayerArmor player_armor;
GW::Constants::Profession current_profession = GW::Constants::Profession::None;

ComboListState head;
ComboListState chest;
ComboListState hands;
ComboListState legs;
ComboListState feets;

// Arg3:
//  - Costume = 0x20000006
//  - Armor =   0x20110007 (3)
//  - Chaos glove = 0x20110407 (38)

ImVec4 palette[] = {
    {0.f, 0.f, 1.f, 0.f},       // Blue
    {0.f, 0.75f, 0.f, 0.f},     // Green
    {0.5f, 0.f, 0.5f, 0.f},     // Purple
    {1.f, 0.f, 0.f, 0.f},       // Red
    {1.f, 1.f, 0.f, 0.f},       // Yellow
    {0.5f, 0.25f, 0.f, 0.f},    // Brown
    {1.f, 0.65f, 0.f, 0.f},     // Orange
    {0.75f, 0.75f, 0.75f, 0.f}, // Silver
    {0.f, 0.f, 0.f, 0.f},       // Black
    {0.5f, 0.5f, 0.5f, 0.f},    // Gray
    {1.f, 1.f, 1.f, 0.f},       // White
    {0.95f, 0.5f, 0.95f, 0.f},  // Pink
};

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

GW::Constants::Profession GetAgentProfession(GW::AgentLiving* agent)
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

void InitItemPiece(PlayerArmorPiece* piece, const GW::Equipment::ItemData* item_data)
{
    piece->model_file_id = item_data->model_file_id;
    piece->unknow1 = item_data->dye.dye_id;
    piece->color1 = DyeColorFromInt(item_data->dye.dye1);
    piece->color2 = DyeColorFromInt(item_data->dye.dye2);
    piece->color3 = DyeColorFromInt(item_data->dye.dye3);
    piece->color4 = DyeColorFromInt(item_data->dye.dye4);
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
    if (piece->model_file_id) {
        SetItem_Func(*player->equip, nullptr, piece->model_file_id, color, 0x20110007, piece->unknow1);
    }
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
