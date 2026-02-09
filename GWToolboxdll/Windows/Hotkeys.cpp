#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <ImGuiAddons.h>
#include <Keys.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Windows/BuildsWindow.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/Hotkeys.h>
#include <Windows/PconsWindow.h>
#include <Utils/TextUtils.h>
#include "HotkeysWindow.h"

bool TBHotkey::show_active_in_header = true;
bool TBHotkey::show_run_in_header = true;
bool TBHotkey::hotkeys_changed = false;
unsigned int TBHotkey::cur_ui_id = 0;
std::unordered_map<WORD, HotkeyToggle*> HotkeyToggle::toggled;

typedef std::pair<GW::UI::ControlAction, GuiUtils::EncString*> ControlLabelPair;
std::vector<ControlLabelPair> HotkeyGWKey::control_labels;

namespace {
    // @Cleanup: when toolbox closes, this array isn't freed properly
    std::map<GW::Constants::Bag,std::vector<HotkeyEquipItemAttributes*>> available_items;

    constexpr std::array behaviors = {
        "Fight",
        "Guard",
        "Avoid Combat"
    };

    std::vector<const char*> HotkeyGWKey_labels = {};

    bool BuildHotkeyGWKeyLabels() {
        if (!HotkeyGWKey_labels.empty())
            return true;
        bool waiting = false;
        for (const auto& it : HotkeyGWKey::control_labels) {
            (it.second->string());
            if (it.second->IsDecoding()) {
                waiting = true;
                break;
            }
        }
        if (waiting) {
            //ImGui::Text("Waiting on endoded strings");
            return false;
        }
        // Reorder
        std::ranges::sort(HotkeyGWKey::control_labels, [](const ControlLabelPair& lhs, const ControlLabelPair& rhs) {
            return lhs.second->string().compare(rhs.second->string()) < 0;
            });
        for (const auto& it : HotkeyGWKey::control_labels) {
            HotkeyGWKey_labels.push_back(it.second->string().c_str());
        }
        return true;
    }

    int GetHotkeyActionIdx(GW::UI::ControlAction action) {
        BuildHotkeyGWKeyLabels();
        for (size_t i = 0; i < HotkeyGWKey::control_labels.size(); i++) {
            if (HotkeyGWKey::control_labels[i].first == action)
                return (int)i;
        }
        return 0;
    }


}


TBHotkey* TBHotkey::HotkeyFactory(ToolboxIni* ini, const char* section)
{
    std::string str(section);
    if (str.compare(0, 7, "hotkey-") != 0) {
        return nullptr;
    }
    constexpr size_t first_sep = 6;
    const size_t second_sep = str.find(L':', first_sep);
    std::string id = str.substr(first_sep + 1, second_sep - first_sep - 1);
    const std::string type = str.substr(second_sep + 1);

    hotkeys_changed = true;

    if (type == HotkeySendChat::IniSection()) {
        return new HotkeySendChat(ini, section);
    }
    if (type == HotkeyUseItem::IniSection()) {
        return new HotkeyUseItem(ini, section);
    }
    if (type == HotkeyDropUseBuff::IniSection()) {
        return new HotkeyDropUseBuff(ini, section);
    }
    if (type == HotkeyToggle::IniSection() &&
        HotkeyToggle::IsValid(ini, section)) {
        return new HotkeyToggle(ini, section);
    }
    if (type == HotkeyAction::IniSection()) {
        return new HotkeyAction(ini, section);
    }
    if (type == HotkeyTarget::IniSection()) {
        return new HotkeyTarget(ini, section);
    }
    if (type == HotkeyMove::IniSection()) {
        return new HotkeyMove(ini, section);
    }
    if (type == HotkeyDialog::IniSection()) {
        return new HotkeyDialog(ini, section);
    }
    if (type == HotkeyHeroTeamBuild::IniSection()) {
        return new HotkeyHeroTeamBuild(ini, section);
    }
    if (type == HotkeyEquipItem::IniSection()) {
        return new HotkeyEquipItem(ini, section);
    }
    if (type == HotkeyFlagHero::IniSection()) {
        return new HotkeyFlagHero(ini, section);
    }
    if (type == HotkeyGWKey::IniSection()) {
        return new HotkeyGWKey(ini, section);
    }
    if (type == HotkeyCommandPet::IniSection()) {
        return new HotkeyCommandPet(ini, section);
    }
    return nullptr;
}

TBHotkey::TBHotkey(const ToolboxIni* ini, const char* section)
    : ui_id(++cur_ui_id)
{
    memset(prof_ids, false, sizeof(prof_ids));
    if (ini) {
        const auto hotkey = ini->GetLongValue(section, VAR_NAME(hotkey), 0);
        auto modifier = ini->GetLongValue(section, VAR_NAME(modifier), 0);
        const auto key_combo_str = ini->GetValue(section, VAR_NAME(key_combo), "");
        GuiUtils::IniToBitset(key_combo_str, key_combo);
        if (hotkey)
            key_combo.set(hotkey);

        modifier >>= 16;  // Shift right to extract actual modifier bits (from higher bits)
        if (modifier & MOD_SHIFT) key_combo.set(VK_SHIFT);      // 0x10
        if (modifier & MOD_CONTROL) key_combo.set(VK_CONTROL);  // 0x11
        if (modifier & MOD_ALT) key_combo.set(VK_MENU);         // 0x12
        if (modifier & MOD_WIN) key_combo.set(VK_LWIN);         // 0x5B (Left Win Key)

        active = ini->GetBoolValue(section, VAR_NAME(active), active);

        std::string ini_str = ini->GetValue(section, VAR_NAME(map_ids), "");
        GuiUtils::IniToArray(ini_str, map_ids);
        if (map_ids.empty()) {
            // Legacy
            const int map_id = ini->GetLongValue(section, "map_id", 0);
            if (map_id > 0) {
                map_ids.push_back(map_id);
            }
        }

        ini_str = ini->GetValue(section, VAR_NAME(prof_ids), "");
        std::vector<uint32_t> prof_ids_tmp;

        GuiUtils::IniToArray(ini_str, prof_ids_tmp);
        if (!prof_ids_tmp.empty()) {
            for (const auto prof_id : prof_ids_tmp) {
                if (prof_id < _countof(prof_ids)) {
                    prof_ids[prof_id] = true;
                }
            }
        }
        else {
            // Legacy
            const int prof_id = ini->GetLongValue(section, "prof_id", 0);
            if (prof_id > 0 && prof_id < _countof(prof_ids)) {
                prof_ids[prof_id] = true;
            }
        }

        ini_str = ini->GetValue(section, VAR_NAME(group), group);
        memset(group, 0, sizeof(group));
        strncpy(group, ini_str.c_str(), _countof(group) - 1);

        instance_type = ini->GetLongValue(section, VAR_NAME(instance_type), instance_type);
        show_message_in_emote_channel =
            ini->GetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                              show_message_in_emote_channel);
        show_error_on_failure = ini->GetBoolValue(
            section, VAR_NAME(show_error_on_failure), show_error_on_failure);
        block_gw = ini->GetBoolValue(section, VAR_NAME(block_gw), block_gw);
        trigger_on_explorable = ini->GetBoolValue(section, VAR_NAME(trigger_on_explorable), trigger_on_explorable);
        trigger_on_outpost = ini->GetBoolValue(section, VAR_NAME(trigger_on_outpost), trigger_on_outpost);
        trigger_on_pvp_character = ini->GetBoolValue(section, VAR_NAME(trigger_on_pvp_character), trigger_on_pvp_character);
        trigger_on_lose_focus = ini->GetBoolValue(section, VAR_NAME(trigger_on_lose_focus), trigger_on_lose_focus);
        trigger_on_gain_focus = ini->GetBoolValue(section, VAR_NAME(trigger_on_gain_focus), trigger_on_gain_focus);
        trigger_on_key_up = ini->GetBoolValue(section, VAR_NAME(trigger_on_key_up), trigger_on_key_up);
        strict_key_combo = ini->GetBoolValue(section, VAR_NAME(strict_key_combo), strict_key_combo);

        trigger_in_controller_mode = ini->GetBoolValue(section, VAR_NAME(trigger_in_controller_mode), trigger_in_controller_mode);
        trigger_in_desktop_mode = ini->GetBoolValue(section, VAR_NAME(trigger_in_desktop_mode), trigger_in_desktop_mode);

        in_range_of_distance = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(in_range_of_distance), in_range_of_distance));
        in_range_of_npc_id = ini->GetLongValue(section, VAR_NAME(in_range_of_npc_id), in_range_of_npc_id);
        player_names = TextUtils::Split(ini->GetValue(section, VAR_NAME(player_names), ""), ",");
        // Legacy value
        const std::string player_name_s = ini->GetValue(section, VAR_NAME(player_name), "");
        if (!player_name_s.empty())
            player_names.push_back(player_name_s);
    }
}

size_t TBHotkey::HasProfession()
{
    size_t out = 0;
    for (size_t i = 1; i < _countof(prof_ids); i++) {
        if (prof_ids[i]) {
            out++;
        }
    }
    return out;
}

bool TBHotkey::IsValid(const char* _player_name, const GW::Constants::InstanceType _instance_type, GW::Constants::Profession _profession, GW::Constants::MapID _map_id, const bool is_pvp_character)
{
    return active
           && (!is_pvp_character || trigger_on_pvp_character)
           && (instance_type == -1 || static_cast<GW::Constants::InstanceType>(instance_type) == _instance_type)
           && (prof_ids[static_cast<size_t>(_profession)] || !HasProfession())
           && (map_ids.empty() || std::ranges::contains(map_ids, std::to_underlying(_map_id)))
           && (player_names.empty() || std::ranges::contains(player_names, _player_name));
}

bool TBHotkey::CanUse()
{
    return !isLoading() && !GW::Map::GetIsObserving() && GW::Agents::GetControlledCharacter() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow() && IsInRangeOfNPC();
}

void TBHotkey::Save(ToolboxIni* ini, const char* section) const
{
    if (key_combo.any()) {
        std::string key_combo_str;
        GuiUtils::BitsetToIni(key_combo, key_combo_str);
        ini->SetValue(section, VAR_NAME(key_combo), key_combo_str.c_str());
    }    
    ini->SetLongValue(section, VAR_NAME(instance_type), instance_type);
    ini->SetBoolValue(section, VAR_NAME(active), active);
    ini->SetBoolValue(section, VAR_NAME(block_gw), block_gw);
    ini->SetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                      show_message_in_emote_channel);
    ini->SetBoolValue(section, VAR_NAME(show_error_on_failure),
                      show_error_on_failure);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_explorable),
                      trigger_on_explorable);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_outpost),
                      trigger_on_outpost);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_pvp_character),
                      trigger_on_pvp_character);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_key_up),
        trigger_on_key_up);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_lose_focus), trigger_on_lose_focus);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_gain_focus), trigger_on_gain_focus);
    ini->SetBoolValue(section, VAR_NAME(strict_key_combo), strict_key_combo);


    if(player_names.size())
        ini->SetValue(section, VAR_NAME(player_names), TextUtils::Join(player_names, ",").c_str());
    if(*group)
        ini->SetValue(section, VAR_NAME(group), group);

    std::string out;
    std::vector<uint32_t> prof_ids_tmp;
    for (size_t i = 0; i < _countof(prof_ids); i++) {
        if (prof_ids[i]) {
            prof_ids_tmp.push_back(i);
        }
    }
    if (prof_ids_tmp.size()) {
        GuiUtils::ArrayToIni(prof_ids_tmp.data(), prof_ids_tmp.size(), &out);
        ini->SetValue(section, VAR_NAME(prof_ids), out.c_str());
    }

    if (map_ids.size()) {
        GuiUtils::ArrayToIni(map_ids.data(), map_ids.size(), &out);
        ini->SetValue(section, VAR_NAME(map_ids), out.c_str());
    }

    if (in_range_of_npc_id) {
        ini->SetDoubleValue(section, VAR_NAME(in_range_of_distance), in_range_of_distance);
        ini->SetLongValue(section, VAR_NAME(in_range_of_npc_id), in_range_of_npc_id);
    }
}

const char* TBHotkey::professions[] = {"Any", "Warrior", "Ranger",
                                       "Monk", "Necromancer", "Mesmer",
                                       "Elementalist", "Assassin", "Ritualist",
                                       "Paragon", "Dervish"};
const char* TBHotkey::instance_types[] = {"Any", "Outpost", "Explorable"};

bool TBHotkey::Draw(Op* op, bool first, bool last)
{
    bool hotkey_changed = false;
    const float scale = ImGui::GetIO().FontGlobalScale;
    const auto show_header_buttons = [&] {
        ImGui::PushID(static_cast<int>(ui_id));
        ImGui::PushID("header");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float btn_size = ImGui::GetFrameHeight();
        const float spacing = style.ItemSpacing.x;
        const float run_btn_width = 64.0f * scale;

        float offset = 0.0f;
        if (show_run_in_header) offset += run_btn_width + spacing;

        // Up/Down buttons
        offset += btn_size * 2 + spacing;

        // Active checkbox
        if (show_active_in_header) offset += btn_size + spacing;

        if (group[0] != '\0') {
            offset -= 21; // @Jon idk how to get this value from ImGuiStyle
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - offset);

        if (show_active_in_header) {
            hotkey_changed |= ImGui::Checkbox("##active", &active);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "The hotkey can trigger only when selected");
            }
            ImGui::SameLine();
        }

        if (first) {
            ImGui::Dummy(ImVec2(btn_size, btn_size));
        } else {
            if (ImGui::Button(ICON_FA_ARROW_UP, ImVec2(btn_size, btn_size))) {
                *op = Op_MoveUp;
                hotkey_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the hotkey up in the list");
            }
        }
        ImGui::SameLine();

        if (last) {
            ImGui::Dummy(ImVec2(btn_size, btn_size));
        } else {
            if (ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(btn_size, btn_size))) {
                *op = Op_MoveDown;
                hotkey_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the hotkey down in the list");
            }
        }

        if (show_run_in_header) {
            ImGui::SameLine();
            if (ImGui::Button(ongoing ? "Stop" : "Run", ImVec2(run_btn_width, 0.0f))) {
                Toggle();
            }
        }
        ImGui::PopID();
        ImGui::PopID();
    };

    // === Header ===
    char header[256]{};

    int written = 0;
    written += Description(&header[written], _countof(header) - written);
    switch (HasProfession()) {
        case 1:
            for (size_t i = 1; i < _countof(prof_ids); i++) {
                if (prof_ids[i]) {
                    written += snprintf(&header[written], _countof(header) - written, " [%s]", professions[i]);
                }
            }
            break;
        case 0:
            break;
        default: {
            char prof_ids_buf[128];
            int prof_ids_written = 0;
            for (size_t i = 1; i < _countof(prof_ids); i++) {
                if (!prof_ids[i]) {
                    continue;
                }
                auto format = ", %s";
                if (!prof_ids_written) {
                    format = "%s";
                }
                prof_ids_written += snprintf(&prof_ids_buf[prof_ids_written], _countof(prof_ids_buf) - prof_ids_written, format, GetProfessionAcronym(static_cast<GW::Constants::Profession>(i)));
            }
            written += snprintf(&header[written], _countof(header) - written, " [%s]", prof_ids_buf);
        }
        break;
    }
    switch (map_ids.size()) {
        case 1:
            written += snprintf(&header[written], _countof(header) - written, " [%s]", Resources::GetMapName((GW::Constants::MapID)map_ids[0])->string().c_str());
            break;
        case 0:
            break;
        default:
            written += snprintf(&header[written], _countof(header) - written, " [%d Maps]", map_ids.size());
            break;
    }
    switch (player_names.size()) {
    case 1:
        written += snprintf(&header[written], _countof(header) - written, " [%s]", player_names[0].c_str());
        break;
    case 0:
        break;
    default:
        written += snprintf(&header[written], _countof(header) - written, " [%d Chars]", player_names.size());
        break;
    }

    const auto keybuf_s = ModKeyName(key_combo);

    ASSERT(snprintf(&header[written], _countof(header) - written, " [%s]###header%u", keybuf_s.c_str(), ui_id) != -1);
    constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap;
    if (!ImGui::CollapsingHeader(header, flags)) {
        show_header_buttons();
    }
    else {
        show_header_buttons();
        ImGui::Indent();
        ImGui::PushID(static_cast<int>(ui_id));
        ImGui::PushItemWidth(-140.0f * scale);
        // === Specific section ===
        hotkey_changed |= Draw();

        // === Hotkey section ===
        const float indent_offset = ImGui::GetCurrentWindow()->DC.Indent.x;
        const float offset_sameline = indent_offset + ImGui::GetContentRegionAvail().x / 2;
        hotkey_changed |= ImGui::InputTextEx("Hotkey Group##hotkey_group", "No Hotkey Group", group, sizeof(group), ImVec2(0, 0), ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            hotkey_changed = true;
        }
        if (trigger_on_key_up) {
            ImGuiContext& g = *GImGui;
            ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
        }
        hotkey_changed |= ImGui::Checkbox("Block key in Guild Wars when triggered", &block_gw);
        if (trigger_on_key_up) {
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Unable to block key - this hotkey is set to trigger on key up!");
            }
            block_gw = false;
        }
        ImGui::ShowHelp("Will prevent Guild Wars from receiving the keypress event");
        ImGui::SameLine(offset_sameline);
        hotkey_changed |= ImGui::Checkbox("Trigger hotkey when playing on PvP character", &trigger_on_pvp_character);
        ImGui::ShowHelp("Unless enabled, this hotkey will not activate when playing on a PvP only character.");
        if (can_trigger_on_map_change) {
            hotkey_changed |= ImGui::Checkbox("Trigger hotkey when entering explorable area", &trigger_on_explorable);
            ImGui::SameLine(offset_sameline);
            hotkey_changed |= ImGui::Checkbox("Trigger hotkey when entering outpost", &trigger_on_outpost);
        }
        hotkey_changed |= ImGui::Checkbox("Trigger hotkey when Guild Wars loses focus", &trigger_on_lose_focus);
        ImGui::SameLine(offset_sameline);
        hotkey_changed |= ImGui::Checkbox("Trigger hotkey when Guild Wars gains focus", &trigger_on_gain_focus);
        hotkey_changed |= ImGui::Checkbox("Trigger hotkey when using keyboard/mouse", &trigger_in_desktop_mode);
        ImGui::SameLine(offset_sameline);
        hotkey_changed |= ImGui::Checkbox("Trigger hotkey when using a gamepad", &trigger_in_controller_mode);
        ImGui::Separator();
        ImGui::Text("Instance Type: ");
        ImGui::SameLine();
        if (ImGui::RadioButton("Any ##instance_type_any", instance_type == -1)) {
            instance_type = -1;
            hotkey_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Outpost ##instance_type_outpost", instance_type == 0)) {
            instance_type = 0;
            hotkey_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Explorable ##instance_type_explorable", instance_type == 1)) {
            instance_type = 1;
            hotkey_changed = true;
        }
        ImGui::PushItemWidth(60.0f * scale);
        ImGui::Text("Only use this within ");
        ImGui::SameLine(0, 0);
        hotkey_changed |= ImGui::InputFloat("##in_range_of_distance", &in_range_of_distance, 0.f, 0.f, "%.0f");
        ImGui::SameLine(0, 0);
        ImGui::Text(" gwinches of NPC Id: ");
        ImGui::SameLine(0, 0);
        hotkey_changed |= ImGui::InputInt("###in_range_of_npc_id", (int*)&in_range_of_npc_id, 0, 0);
        ImGui::PopItemWidth();
        ImGui::ShowHelp("Only trigger when in range of a certain NPC");

        const auto map_ids_header = std::format("Map IDs ({})###map_ids", map_ids.size());
        if (ImGui::CollapsingHeader(map_ids_header.c_str())) {
            ImGui::Indent();

            ImGui::TextDisabled("Only trigger in selected maps:");
            const float map_id_w = 200.f * scale;
            ImGui::Indent();
            for (auto it = map_ids.begin(); !hotkey_changed && it != map_ids.end(); it++) {
                ImGui::PushID(*it);
                ImGui::Text("%d: %s", *it, Resources::GetMapName((GW::Constants::MapID)*it)->string().c_str());
                ImGui::SameLine(indent_offset + map_id_w);
                if (ImGui::Button("X")) {
                    map_ids.erase(it);
                    hotkey_changed = true;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            static char map_id_input_buf[4];
            ImGui::Separator();
            bool add_map_id = ImGui::InputTextWithHint("###add_map_id", "Add Map ID", map_id_input_buf, _countof(map_id_input_buf), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
            if (add_map_id) {
                uint32_t map_id_out;
                if (*map_id_input_buf 
                    && TextUtils::ParseUInt(map_id_input_buf, &map_id_out)) {
                    if (!std::ranges::contains(map_ids, reinterpret_cast<uint32_t>(map_id_input_buf))) {
                        map_ids.push_back(map_id_out);
                        hotkey_changed = true;
                    }
                    memset(map_id_input_buf, 0, sizeof(map_id_input_buf));
                }
            }
            ImGui::Unindent();
        }
        const auto professions_header = std::format("Professions ({})###professions", std::count(&prof_ids[0], &prof_ids[_countof(prof_ids) - 1], true));
        if (ImGui::CollapsingHeader(professions_header.c_str())) {
            ImGui::Indent();
            const float prof_w = 140.f * scale;
            const int per_row = static_cast<int>(std::floor(ImGui::GetContentRegionAvail().x / prof_w));
            ImGui::TextDisabled("Only trigger when player is one of these professions");
            for (int i = 1; i < _countof(prof_ids); i++) {
                const int offset = (i - 1) % per_row;
                if (i > 1 && offset != 0) {
                    ImGui::SameLine(prof_w * offset + indent_offset);
                }
                hotkey_changed |= ImGui::Checkbox(professions[i], &prof_ids[i]);
            }
            ImGui::Unindent();
        }
        const auto character_names_header = std::format("Character Names ({})###character_names", player_names.size());
        if (ImGui::CollapsingHeader(character_names_header.c_str())) {
            ImGui::Indent();

            ImGui::TextDisabled("Only trigger for the following player names:");
            const float map_id_w = 200.f * scale;
            ImGui::Indent();
            for(auto it = player_names.begin();!hotkey_changed && it != player_names.end();it++) {
                ImGui::PushID(it->data());
                ImGui::TextUnformatted(it->c_str());
                ImGui::SameLine(indent_offset + map_id_w);
                if (ImGui::Button("X")) {
                    player_names.erase(it);
                    hotkey_changed = true;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::Unindent();
            static char player_name_input_buf[20];
            ImGui::Separator();
            bool add_player_name = ImGui::InputTextWithHint("###player_name_input_buf", "Add Player Name",player_name_input_buf, _countof(player_name_input_buf), ImGuiInputTextFlags_EnterReturnsTrue);
            if (add_player_name && *player_name_input_buf) {
                const auto sanitised = TextUtils::UcWords(player_name_input_buf);
                if (!std::ranges::contains(player_names, sanitised)) {
                    player_names.push_back(sanitised);
                    hotkey_changed = true;
                }
                memset(player_name_input_buf, 0, sizeof(player_name_input_buf));
            }
            ImGui::Unindent();
        }

        ImGui::Separator();
        if (!show_active_in_header) {
            hotkey_changed |= ImGui::Checkbox("###active", &active);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("The hotkey can trigger only when selected");
            }
            ImGui::SameLine();
        }
        const auto keybuf2 = std::format("Hotkey: {}", keybuf_s);
        const auto control_width = 140.0f * scale;
        if (ImGui::Button(keybuf2.c_str(), ImVec2(control_width * -3.f, 0))) {
            HotkeysWindow::ChooseKeyCombo(this);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click to change hotkey");
        }
        
        ImGui::SameLine();
        ImGui::PushItemWidth(control_width);
        hotkey_changed |= ImGui::Checkbox("Strict key combo?", &strict_key_combo);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Off: Your hotkey will trigger even if other keys are also held down\n\
e.g. A hotkey with Ctrl + H will trigger even if you've got the W key held aswell\n\n\
On: Your hotkey will only trigger when no other keys are held\n\
e.g. A hotkey with Ctrl + H will NOT trigger if you've got the W key held aswell");
        }
        ImGui::SameLine();
        hotkey_changed |= ImGui::Checkbox("Trigger on key up?", &trigger_on_key_up);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Hotkeys usually trigger as soon as the key combination is pressed.\nYou can change this to instead only trigger when the key is released");
        }
        ImGui::SameLine();
        if (!show_run_in_header) {
            if (ImGui::Button(ongoing ? "Stop" : "Run", ImVec2(control_width, 0.0f))) {
                Toggle();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Execute the hotkey now");
            }
        }
        ImGui::PopItemWidth();

        // === Move and delete buttons ===
        if (ImGui::Button("Delete", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            ImGui::OpenPopup("Delete Hotkey?");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete the hotkey");
        }
        if (ImGui::BeginPopupModal("Delete Hotkey?", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure?\nThis operation cannot be undone\n\n",
                        Name());
            if (ImGui::Button("OK", ImVec2(120.f * scale, 0))) {
                *op = Op_Delete;
                hotkey_changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.f * scale, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
        ImGui::Unindent();
    }
    return hotkey_changed;
}

bool TBHotkey::IsInRangeOfNPC() const
{
    if (!(in_range_of_npc_id && in_range_of_distance > 0.f)) {
        return true;
    }
    const auto agents = GW::Agents::GetAgentArray();
    const auto me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
    if (!me)
        return false;
    for (const auto agent : *agents) {
        if (!(agent && agent->type == 0xDB)) {
            continue;
        }
        const auto* living = agent->GetAsAgentLiving();
        if (living->login_number || living->player_number != static_cast<uint16_t>(in_range_of_npc_id)) {
            continue;
        }
        if (GetDistance(agent->pos, me->pos) < in_range_of_distance) {
            return true;
        }
    }
    return false;
}

HotkeySendChat::HotkeySendChat(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    strcpy_s(message, ini->GetValue(section, "msg", ""));
    channel = ini->GetValue(section, "channel", "/")[0];
}

void HotkeySendChat::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "msg", message);
    char buf[8];
    snprintf(buf, 8, "%c", channel);
    ini->SetValue(section, "channel", buf);
}

int HotkeySendChat::Description(char* buf, const size_t bufsz)
{
    return snprintf(buf, bufsz, "Send chat '%c%s'", channel, message);
}

bool HotkeySendChat::Draw()
{
    bool hotkey_changed = false;
    int index = 0;
    switch (channel) {
        case '/':
            index = 0;
            break;
        case '!':
            index = 1;
            break;
        case '@':
            index = 2;
            break;
        case '#':
            index = 3;
            break;
        case '$':
            index = 4;
            break;
        case '%':
            index = 5;
            break;
        case '"':
            index = 6;
            break;
    }
    static const char* channels[] = {"/ Commands", "! All", "@ Guild",
                                     "# Group", "$ Trade", "% Alliance",
                                     "\" Whisper"};
    if (ImGui::Combo("Channel", &index, channels, 7)) {
        switch (index) {
            case 0:
                channel = '/';
                break;
            case 1:
                channel = '!';
                break;
            case 2:
                channel = '@';
                break;
            case 3:
                channel = '#';
                break;
            case 4:
                channel = '$';
                break;
            case 5:
                channel = '%';
                break;
            case 6:
                channel = '"';
                break;
            default:
                channel = '/';
                break;
        }
        show_message_in_emote_channel = channel == '/' &&
                                        show_message_in_emote_channel;
        hotkey_changed = true;
    }
    hotkey_changed |= ImGui::InputText("Message", message, _countof(message));
    hotkey_changed |= channel == '/' && ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}

void HotkeySendChat::Execute()
{
    if (!CanUse()) {
        return;
    }
    if (show_message_in_emote_channel && channel == L'/') {
        Log::Flash("/%s", message);
    }
    GW::GameThread::Enqueue([&]() {
        GW::Chat::SendChat(channel, message);
    });
}

HotkeyUseItem::HotkeyUseItem(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    item_id = static_cast<size_t>(ini->GetLongValue(section, "ItemID", 0));
    strcpy_s(name, ini->GetValue(section, "ItemName", ""));
}

void HotkeyUseItem::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ItemID", item_id);
    ini->SetValue(section, "ItemName", name);
}

int HotkeyUseItem::Description(char* buf, const size_t bufsz)
{
    if (!name[0]) {
        return snprintf(buf, bufsz, "Use #%d", item_id);
    }
    return snprintf(buf, bufsz, "Use %s", name);
}

bool HotkeyUseItem::Draw()
{
    bool hotkey_changed = ImGui::InputInt("Item Model ID", (int*)&item_id);
    hotkey_changed |= ImGui::InputText("Item Name", name, _countof(name));
    hotkey_changed |= ImGui::Checkbox("Display error message on failure", &show_error_on_failure);
    return hotkey_changed;
}

void HotkeyUseItem::Execute()
{
    if (!CanUse()) {
        return;
    }
    if (item_id == 0) {
        return;
    }

    bool used = GW::Items::UseItemByModelId(item_id, 1, 4);
    if (!used &&
        GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        used = GW::Items::UseItemByModelId(item_id, 8, 16);
    }

    if (!used && show_error_on_failure) {
        if (name[0] == '\0') {
            Log::Error("Item #%d not found!", item_id);
        }
        else {
            Log::Error("%s not found!", name);
        }
    }
}

HotkeyEquipItemAttributes::HotkeyEquipItemAttributes(const GW::Item* item)
{
    if (!item) 
        set();
    else
        set(item->model_id, item->complete_name_enc ? item->complete_name_enc : item->name_enc, item->info_string, item->mod_struct, item->mod_struct_size);
}

HotkeyEquipItemAttributes::~HotkeyEquipItemAttributes()
{
    if (mod_struct) {
        delete mod_struct;
        mod_struct = nullptr;
        mod_struct_size = 0;
    }
}

HotkeyEquipItemAttributes::HotkeyEquipItemAttributes(const uint32_t _model_id, const wchar_t* _name_enc, const wchar_t* _info_string, const GW::ItemModifier* _mod_struct, const size_t _mod_struct_size)
{
    set(_model_id, _name_enc, _info_string, _mod_struct, _mod_struct_size);
}

HotkeyEquipItemAttributes* HotkeyEquipItemAttributes::set(const uint32_t _model_id, const wchar_t* _name_enc, const wchar_t* _info_string, const GW::ItemModifier* _mod_struct, const size_t _mod_struct_size)
{
    model_id = _model_id;
    enc_name.reset(_name_enc);
    enc_desc.reset(_info_string);
    if (mod_struct) {
        delete mod_struct;
        mod_struct = nullptr;
        mod_struct_size = 0;
    }
    mod_struct_size = _mod_struct_size;
    if (mod_struct_size) {
        ASSERT(_mod_struct);
        const size_t bytes = _mod_struct_size * sizeof(*_mod_struct);
        mod_struct = static_cast<GW::ItemModifier*>(malloc(bytes));
        memcpy(mod_struct, _mod_struct, bytes);
    }
    return this;
}

HotkeyEquipItemAttributes* HotkeyEquipItemAttributes::set(const HotkeyEquipItemAttributes& other)
{
    return set(other.model_id, other.enc_name.encoded().c_str(), other.enc_desc.encoded().c_str(), other.mod_struct, other.mod_struct_size);
}

bool HotkeyEquipItemAttributes::check(const GW::Item* item) const
{
    if (!item || item->model_id != model_id || item->mod_struct_size != mod_struct_size) {
        return false;
    }
    if (wcscmp(item->complete_name_enc ? item->complete_name_enc : item->name_enc, enc_name.encoded().c_str()) != 0) {
        return false;
    }
    if (mod_struct_size == item->mod_struct_size && memcmp(mod_struct, item->mod_struct, item->mod_struct_size * sizeof(*item->mod_struct)) != 0) {
        return false;
    }
    return true;
}

HotkeyEquipItem::HotkeyEquipItem(ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    // @Cleanup: Add error handling
    bag_idx = static_cast<size_t>(ini->GetLongValue(section, "Bag", bag_idx));
    slot_idx = static_cast<size_t>(ini->GetLongValue(section, "Slot", slot_idx));
    equip_by = static_cast<EquipBy>(ini->GetLongValue(section, "EquipBy", equip_by));
    if (equip_by == ITEM) {
        uint32_t model_id = ini->GetLongValue(section, "ModelId", 0);
        std::string in = ini->GetValue(section, "EncodedName", "");
        std::wstring enc_name;
        if (!in.empty()) {
            ASSERT(GuiUtils::IniToArray(in, enc_name));
        }
        in = ini->GetValue(section, "EncodedDesc", "");
        std::wstring enc_desc;
        if (!in.empty()) {
            ASSERT(GuiUtils::IniToArray(in, enc_desc));
        }
        in = ini->GetValue(section, "ModStruct", "");
        std::vector<uint32_t> mod_structs;
        if (!in.empty()) {
            ASSERT(GuiUtils::IniToArray(in, mod_structs));
        }
        item_attributes.set(model_id, enc_name.c_str(), enc_desc.c_str(), mod_structs.size() ? (GW::ItemModifier*)mod_structs.data() : nullptr, mod_structs.size());
    }
}

void HotkeyEquipItem::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "EquipBy", equip_by);
    ini->SetLongValue(section, "Bag", bag_idx);
    ini->SetLongValue(section, "Slot", slot_idx);
    if (equip_by == ITEM) {
        ini->SetLongValue(section, "ModelId", item_attributes.model_id);
        std::string out;
        GuiUtils::ArrayToIni(item_attributes.enc_name.encoded().c_str(), &out);
        ini->SetValue(section, "EncodedName", out.c_str());
        ASSERT(GuiUtils::ArrayToIni(item_attributes.enc_desc.encoded().c_str(), &out));
        ini->SetValue(section, "EncodedDesc", out.c_str());
        ASSERT(GuiUtils::ArrayToIni((uint32_t*)item_attributes.mod_struct, item_attributes.mod_struct_size, &out));
        ini->SetLongValue(section, "ModStructSize", item_attributes.mod_struct_size);
        ini->SetValue(section, "ModStruct", out.c_str());
    }
}

int HotkeyEquipItem::Description(char* buf, const size_t bufsz)
{
    if (equip_by == SLOT) {
        return snprintf(buf, bufsz, "Equip Item in bag %d slot %d", bag_idx, slot_idx);
    }
    return snprintf(buf, bufsz, "Equip %s", item_attributes.name().c_str());
}

bool HotkeyEquipItem::Draw()
{
    bool hotkey_changed = false;
    constexpr const char* bags[6] = {"None", "Backpack", "Belt Pouch", "Bag 1", "Bag 2", "Equipment Pack"};
    ImGui::TextUnformatted("Equip By: ");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Item", (int*)&equip_by, ITEM);
    ImGui::ShowHelp("Find and equip an item by its attributes, regardless of location in inventory.");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Slot", (int*)&equip_by, SLOT);
    ImGui::ShowHelp("Find and equip an item in a specific slot, regarless of what it is.\nUseful for using the same hotkey across characters.");
    if (equip_by == SLOT) {
        hotkey_changed |= ImGui::Combo("Bag", (int*)&bag_idx, bags, _countof(bags));
        hotkey_changed |= ImGui::InputInt("Slot (1-25)", (int*)&slot_idx);
    }
    else {
        static bool need_to_fetch_bag_items = false;
        if (!item_attributes.model_id) {
            ImGui::TextUnformatted("No item chosen");
        }
        else {
            ImGui::TextUnformatted(item_attributes.name().c_str());
            if (!item_attributes.desc().empty()) {
                ImGui::TextUnformatted(item_attributes.desc().c_str());
            }
        }
        if (ImGui::Button("Edit")) {
            need_to_fetch_bag_items = true;
            ImGui::OpenPopup("Choose Item to Equip");
        }
        if (ImGui::BeginPopupModal("Choose Item to Equip", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (need_to_fetch_bag_items) {
                // Free available_items ptrs
                for (auto& it : available_items) {
                    for (auto atts : it.second) {
                        delete atts;
                    }
                    it.second.clear();
                }
                available_items.clear();
                for (auto i = GW::Constants::Bag::Backpack; i <= GW::Constants::Bag::Equipment_Pack; i = (GW::Constants::Bag)((size_t)i + 1)) {
                    GW::Bag* bag = GW::Items::GetBag(i);
                    if (!bag) {
                        continue;
                    }
                    GW::ItemArray& items = bag->items;
                    for (size_t slot = 0, size = items.size(); slot < size; slot++) {
                        const GW::Item* cur_item = items[slot];
                        if (!IsEquippable(cur_item)) {
                            continue;
                        }
                        available_items[i].push_back(new HotkeyEquipItemAttributes(cur_item));
                    }
                }
                need_to_fetch_bag_items = false;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            for (auto& it : available_items) {
                auto& items = it.second;
                if (items.empty()) {
                    continue;
                }

                ImGui::TextUnformatted(bags[(uint32_t)it.first]);
                ImGui::Indent();
                for (auto& ai : it.second) {
                    ImGui::PushID(&ai);
                    if (ImGui::Button(ai->name().c_str())) {
                        item_attributes.set(*ai);
                        hotkey_changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(ai->desc().c_str());
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            if (ImGui::Button("Cancel###cancel_hotkey_equip_item")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    hotkey_changed |= ImGui::Checkbox("Display error message on failure", &show_error_on_failure);
    return hotkey_changed;
}

bool HotkeyEquipItem::IsEquippable(const GW::Item* _item)
{
    if (!_item) {
        return false;
    }
    switch (static_cast<GW::Constants::ItemType>(_item->type)) {
        case GW::Constants::ItemType::Axe:
        case GW::Constants::ItemType::Boots:
        case GW::Constants::ItemType::Bow:
        case GW::Constants::ItemType::Chestpiece:
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Gloves:
        case GW::Constants::ItemType::Hammer:
        case GW::Constants::ItemType::Headpiece:
        case GW::Constants::ItemType::Leggings:
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Shield:
        case GW::Constants::ItemType::Staff:
        case GW::Constants::ItemType::Sword:
        case GW::Constants::ItemType::Daggers:
        case GW::Constants::ItemType::Scythe:
        case GW::Constants::ItemType::Spear:
        case GW::Constants::ItemType::Costume_Headpiece:
        case GW::Constants::ItemType::Costume:
            break;
        default:
            return false;
    }
    return true;
    // 2021-05-02: Disabled customised check, conflicts with obfuscator module, not worth the hassle - the hotkey will fail with a message on timeout anyway - Jon
    /*
    if (!_item->customized)
        return true;
    GW::GameContext *g = GW::GetGameContext();
    GW::CharContext *c = g ? g->character : nullptr;
    return c && c->player_name &&
           wcscmp(c->player_name, _item->customized) == 0;*/
}

GW::Item* HotkeyEquipItem::FindMatchingItem(const GW::Constants::Bag _bag_idx, GW::Bag** bag) const
{
    *bag = GW::Items::GetBag(_bag_idx);
    if (!(*bag)) {
        return nullptr;
    }
    GW::ItemArray& items = (*bag)->items;
    for (const auto _item : items) {
        if (item_attributes.check(_item)) {
            return _item;
        }
    }
    return nullptr;
}

void HotkeyEquipItem::Execute()
{
    if (!CanUse()) {
        return;
    }
    GW::Bag* bag = nullptr;
    GW::Item* item = nullptr;
    if (!ongoing) {
        item_id = 0;
        if (equip_by == SLOT) {
            const auto bag_id = static_cast<GW::Constants::Bag>(bag_idx);
            if (bag_id < GW::Constants::Bag::Backpack || bag_id > GW::Constants::Bag::Equipment_Pack || slot_idx < 1 || slot_idx > 25) {
                show_error_on_failure && (Log::Error("Invalid bag slot %d/%d!", bag_id, slot_idx), true);
                return;
            }
            bag = GW::Items::GetBag(bag_id);
            if (!bag) {
                show_error_on_failure && (Log::Error("Bag #%d not found!", bag_id), true);
                return;
            }
            GW::ItemArray& items = bag->items;
            item = (items.valid() && slot_idx <= items.size()) ? items[slot_idx - 1] : nullptr;
            if (!(item && item->bag == bag)) {
                show_error_on_failure && (Log::Error("Invalid bag slot %d/%d!", bag_id, slot_idx), true);
                return;
            }
        }
        else {
            item = FindMatchingItem(GW::Constants::Bag::Equipped_Items, &bag);
            for (auto i = GW::Constants::Bag::Backpack; i <= GW::Constants::Bag::Equipment_Pack && !item; i = (GW::Constants::Bag)((size_t) i + 1)) {
                item = FindMatchingItem(i, &bag);
            }
            if (!(item && item->bag == bag)) {
                show_error_on_failure && (Log::Error("No equippable item matching your hotkey"), true);
                return;
            }
        }

        if (!(item && IsEquippable(item))) {
            show_error_on_failure && (Log::Error("No equippable item in bag %d slot %d", bag_idx, slot_idx), true);
            return;
        }
        item_id = item->item_id;
        ongoing = true;
        start_time = TIMER_INIT();
        last_try = 0;
    }
    ongoing = false;
    if (TIMER_DIFF(start_time) > 5000) {
        show_error_on_failure && (Log::Error("Failed to equip item"), true);
        return;
    }
    item = GW::Items::GetItemById(item_id);
    if (!(item && IsEquippable(item))) {
        show_error_on_failure && (Log::Error("Failed to equip item"), true);
        return;
    }
    if (item->bag && item->bag->bag_type == GW::Constants::BagType::Equipped) {
        return; // Success!
    }
    const GW::AgentLiving* p = GW::Agents::GetControlledCharacter();
    if (!(p && p->GetIsAlive())) {
        show_error_on_failure && (Log::Error("Failed to equip item in bag %d slot %d", bag_idx, slot_idx), true);
        return;
    }
    const GW::Skillbar* s = GW::SkillbarMgr::GetPlayerSkillbar();
    if (p->GetIsKnockedDown() || (s && s->cast_array.size())) {
        ongoing = true;
        // Player knocked down or casting; wait.
        return;
    }
    if (p->skill) {
        ongoing = true;
        // Casting atm
        return;
    }
    if (!p->model_state) {
        ongoing = true;
        return;
    }
    if (TIMER_DIFF(last_try) > 500) {
        last_try = TIMER_INIT();
        if (p->GetIsIdle() || p->GetIsMoving()) {
            GW::Items::EquipItem(item);
        }
        else {
            // Move to clear model state e.g. attacking, aftercast
            GW::Agents::Move(p->pos);
        }

    }
}

bool HotkeyDropUseBuff::GetText(void* data, int idx, const char** out_text)
{
    static char other_buf[64];
    switch (static_cast<SkillIndex>(idx)) {
        case Recall:
            *out_text = "Recall";
            break;
        case UA:
            *out_text = "UA";
            break;
        case HolyVeil:
            *out_text = "Holy Veil";
            break;
        default:
            snprintf(other_buf, 64, "Skill#%d", (int)data);
            *out_text = other_buf;
            break;
    }
    return true;
}

HotkeyDropUseBuff::SkillIndex HotkeyDropUseBuff::GetIndex() const
{
    switch (id) {
        case GW::Constants::SkillID::Recall:
            return Recall;
        case GW::Constants::SkillID::Unyielding_Aura:
            return UA;
        case GW::Constants::SkillID::Holy_Veil:
            return HolyVeil;
        default:
            return Other;
    }
}

HotkeyDropUseBuff::HotkeyDropUseBuff(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    id = static_cast<GW::Constants::SkillID>(ini->GetLongValue(
        section, "SkillID", static_cast<long>(GW::Constants::SkillID::Recall)));
}

void HotkeyDropUseBuff::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "SkillID", static_cast<long>(id));
}

int HotkeyDropUseBuff::Description(char* buf, const size_t bufsz)
{
    const char* skillname;
    GetText((void*)id, GetIndex(), &skillname);
    return snprintf(buf, bufsz, "Drop/Use %s", skillname);
}

bool HotkeyDropUseBuff::Draw()
{
    bool hotkey_changed = false;
    SkillIndex index = GetIndex();
    if (ImGui::Combo("Skill", (int*)&index,
                     "Recall\0Unyielding Aura\0Holy Veil\0Other", 4)) {
        switch (index) {
            case Recall:
                id = GW::Constants::SkillID::Recall;
                break;
            case UA:
                id = GW::Constants::SkillID::Unyielding_Aura;
                break;
            case HolyVeil:
                id = GW::Constants::SkillID::Holy_Veil;
                break;
            case Other:
                id = static_cast<GW::Constants::SkillID>(0);
                break;
            default:
                break;
        }
        hotkey_changed = true;
    }
    if (index == Other) {
        if (ImGui::InputInt("Skill ID", (int*)&id)) {
            hotkey_changed = true;
        }
    }
    return hotkey_changed;
}

void HotkeyDropUseBuff::Execute()
{
    if (!CanUse()) {
        return;
    }

    const GW::Buff* buff = GW::Effects::GetPlayerBuffBySkillId(id);
    if (buff) {
        GW::Effects::DropBuff(buff->buff_id);
    }
    else {
        const int islot = GW::SkillbarMgr::GetSkillSlot(id);
        if (islot >= 0) {
            uint32_t slot = static_cast<uint32_t>(islot);
            if (GW::SkillbarMgr::GetPlayerSkillbar()->skills[slot].recharge == 0) {
                GW::GameThread::Enqueue([slot] {
                    GW::SkillbarMgr::UseSkill(slot);
                });
            }
        }
    }
}

const char* HotkeyToggle::GetText(void*, int idx)
{
    switch (static_cast<ToggleTarget>(idx)) {
        case Clicker:
           return "Clicker";
        case Pcons:
            return "Pcons";
        case CoinDrop:
            return "Coin Drop";
        case Tick:
            return "Tick";
    }
    return nullptr;
}

bool HotkeyToggle::IsValid(const ToolboxIni* ini, const char* section)
{
    const long val = ini->GetLongValue(section, "ToggleID", Clicker);
    return val >= 0 && val < Count;
}

HotkeyToggle::HotkeyToggle(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    target = static_cast<ToggleTarget>(ini->GetLongValue(section, "ToggleID", target));
    static bool initialised = false;
    if (!initialised) {
        toggled.reserve(512);
    }
    initialised = true;
    switch (target) {
        case Clicker:
            interval = clicker_delay_ms;
            break;
        case CoinDrop:
            interval = 500;
            break;
    }
}

void HotkeyToggle::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ToggleID", target);
}

int HotkeyToggle::Description(char* buf, const size_t bufsz)
{
    const char* name = GetText(nullptr, target);
    return snprintf(buf, bufsz, "Toggle %s", name);
}

bool HotkeyToggle::Draw()
{
    bool hotkey_changed = false;
    if (ImGui::Combo("Toggle###combo", (int*)&target, GetText, nullptr, Count)) {
        if (target == Clicker) {
            togglekey = VK_LBUTTON;
        }
        hotkey_changed = true;
    }
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}

HotkeyToggle::~HotkeyToggle()
{
    if (IsToggled(true)) {
        toggled[togglekey] = nullptr;
    }
}

void HotkeyToggle::Toggle()
{
    if (!HasInterval()) {
        return Execute();
    }
    ongoing = !IsToggled(true);
    toggled[togglekey] = ongoing ? this : nullptr;
    last_use = 0;
    if (ongoing || (!ongoing && !toggled[togglekey])) {
        switch (target) {
            case Clicker:
                Log::Flash("Clicker is %s", ongoing ? "active" : "disabled");
                break;
            case CoinDrop:
                Log::Flash("Coindrop is %s", ongoing ? "active" : "disabled");
                break;
        }
    }
}

bool HotkeyToggle::IsToggled(const bool refresh)
{
    if (refresh) {
        const auto found = toggled.find(togglekey);
        ongoing = found != toggled.end() && found->second == this;
    }
    return ongoing;
}

void HotkeyToggle::Execute()
{
    if (HasInterval()) {
        if (GW::Chat::GetIsTyping()) {
            return;
        }
        if (!CanUse()) {
            ongoing = false;
        }
        if (!ongoing) {
            Toggle();
        }
        if (target == Clicker) {
            interval = clicker_delay_ms;
        }
        if (TIMER_DIFF(last_use) < interval) {
            return;
        }
        if (processing) {
            return;
        }
        if (!IsToggled(true)) {
            ongoing = false;
            last_use = 0;
            return;
        }
    }

    switch (target) {
        case Clicker:
            INPUT input;
            memset(&input, 0, sizeof(INPUT));
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
            processing = true;
            SendInput(1, &input, sizeof(INPUT));
            break;
        case Pcons:
            PconsWindow::Instance().ToggleEnable();
            ongoing = false;
        // writing to chat is done by ToggleActive if needed
            break;
        case CoinDrop:
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
                Toggle();
                return;
            }
            GW::Items::DropGold(1);
            break;
        case Tick:
            const auto ticked = GW::PartyMgr::GetIsPlayerTicked();
            GW::PartyMgr::Tick(!ticked);
            ongoing = false;
            break;
    }
    last_use = TIMER_INIT();
}

const char* HotkeyAction::GetText(void*, int idx)
{
    switch (static_cast<Action>(idx)) {
        case OpenXunlaiChest:
            return "Open Xunlai Chest";
        case DropGoldCoin:
            return "Drop Gold Coin";
        case ReapplyTitle:
            return "Reapply appropriate Title";
        case EnterChallenge:
            return "Enter Challenge";
    }
    return nullptr;
}

HotkeyAction::HotkeyAction(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    action = static_cast<Action>(ini->GetLongValue(section, "ActionID", OpenXunlaiChest));
}

void HotkeyAction::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ActionID", action);
}

int HotkeyAction::Description(char* buf, const size_t bufsz)
{
    const char* name = GetText(nullptr, action);
    return snprintf(buf, bufsz, "%s", name ? name : "Unknown");
}

bool HotkeyAction::Draw()
{
    if (ImGui::BeginCombo("Actions###actionscombo", GetText(nullptr, action))) {
        for (const auto hk_action : { OpenXunlaiChest, DropGoldCoin, ReapplyTitle, EnterChallenge }) {
            const bool selected = action == hk_action;
            if (ImGui::Selectable(GetText(nullptr, hk_action), selected)) {
                action = hk_action;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return true;
}

void HotkeyAction::Execute()
{
    if (!CanUse()) {
        return;
    }
    switch (action) {
        case OpenXunlaiChest:
            GW::GameThread::Enqueue([] {
                const auto frame = GW::UI::GetFrameByLabel(L"InvAccount");
                if (frame) {
                    GW::UI::DestroyUIComponent(frame);
                }
                else {
                    GW::Items::OpenXunlaiWindow();
                }
                
                });
            break;
        case DropGoldCoin:
            if (isExplorable()) {
                GW::Items::DropGold(1);
            }
            break;
        case ReapplyTitle: {
            GW::Chat::SendChat('/', L"title");
            break;
        }
        case EnterChallenge:
            GW::Chat::SendChat('/', L"enter");
            break;
    }
}

HotkeyTarget::HotkeyTarget(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    // don't print target hotkey to chat by default
    show_message_in_emote_channel = false;
    name[0] = 0;
    if (!ini) {
        return;
    }
    std::string ini_name = ini->GetValue(section, "TargetID", "");
    strcpy_s(id, ini_name.substr(0, sizeof(id) - 1).c_str());
    id[sizeof(id) - 1] = 0;
    long ini_type = ini->GetLongValue(section, "TargetType", -1);
    if (ini_type >= NPC && ini_type < Count) {
        type = static_cast<HotkeyTargetType>(ini_type);
    }
    ini_name = ini->GetValue(section, "TargetName", "");
    strcpy_s(name, ini_name.substr(0, sizeof(name) - 1).c_str());
    name[sizeof(name) - 1] = 0;

    ini->GetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                      show_message_in_emote_channel);
}

void HotkeyTarget::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "TargetID", id);
    ini->SetLongValue(section, "TargetType", type);
    ini->SetValue(section, "TargetName", name);
}

int HotkeyTarget::Description(char* buf, const size_t bufsz)
{
    if (!name[0]) {
        return snprintf(buf, bufsz, "Target %s %s", type_labels[type], id);
    }
    return snprintf(buf, bufsz, "Target %s", name);
}

bool HotkeyTarget::Draw()
{
    const float w = ImGui::GetContentRegionAvail().x / 1.5f;
    ImGui::PushItemWidth(w);
    bool hotkey_changed = ImGui::Combo("Target Type", (int*)&type, type_labels, 3);
    hotkey_changed |= ImGui::InputText(identifier_labels[type], id, _countof(id));
    ImGui::PopItemWidth();
    ImGui::ShowHelp("See Settings > Help > Chat Commands for /target options");
    ImGui::PushItemWidth(w);
    hotkey_changed |= ImGui::InputText("Hotkey label", name, _countof(name));
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(" (optional)");
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}

void HotkeyTarget::Execute()
{
    if (!CanUse()) {
        return;
    }

    constexpr size_t len = 122;
    auto message = new wchar_t[len];
    message[0] = 0;
    switch (type) {
        case Item:
            swprintf(message, len, L"target item %S", id);
            break;
        case NPC:
            swprintf(message, len, L"target npc %S", id);
            break;
        case Signpost:
            swprintf(message, len, L"target gadget %S", id);
            break;
        default:
            Log::ErrorW(L"Unknown target type %d", type);
            delete[] message;
            return;
    }
    GW::GameThread::Enqueue([message] {
        GW::Chat::SendChat('/', message);
        delete[] message;
    });

    if (show_message_in_emote_channel) {
        char buf[256];
        Description(buf, 256);
        Log::Flash("Triggered %s", buf);
    }
}

HotkeyMove::HotkeyMove(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    type = static_cast<MoveType>(ini->GetLongValue(section, "type", static_cast<int>(type)));
    x = static_cast<float>(ini->GetDoubleValue(section, "x", 0.0));
    y = static_cast<float>(ini->GetDoubleValue(section, "y", 0.0));
    range = static_cast<float>(ini->GetDoubleValue(section, "distance",
                                                   GW::Constants::Range::Compass));
    distance_from_location = static_cast<float>(ini->GetDoubleValue(section, "distance_from_location", distance_from_location));
    strcpy_s(name, ini->GetValue(section, "name", ""));
}

void HotkeyMove::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "type", static_cast<long>(type));
    ini->SetDoubleValue(section, "x", x);
    ini->SetDoubleValue(section, "y", y);
    ini->SetDoubleValue(section, "distance", range);
    ini->SetValue(section, "name", name);
    ini->SetDoubleValue(section, "distance_from_location", distance_from_location);
}

int HotkeyMove::Description(char* buf, const size_t bufsz)
{
    if (name[0]) {
        return snprintf(buf, bufsz, "Move to %s", name);
    }
    switch (type) {
        case MoveType::Target:
            return snprintf(buf, bufsz, "Move to current target");
        default:
            return snprintf(buf, bufsz, "Move to (%.0f, %.0f)", x, y);
    }
}

bool HotkeyMove::Draw()
{
    bool hotkey_changed = false;
    ImGui::TextUnformatted("Type: ");
    ImGui::SameLine();
    if (ImGui::RadioButton("Target", type == MoveType::Target) && type != MoveType::Target) {
        hotkey_changed = true;
        type = MoveType::Target;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Location", type == MoveType::Location) && type != MoveType::Location) {
        hotkey_changed = true;
        type = MoveType::Location;
    }
    if (type == MoveType::Location) {
        hotkey_changed |= ImGui::InputFloat("x", &x, 0.0f, 0.0f);
        hotkey_changed |= ImGui::InputFloat("y", &y, 0.0f, 0.0f);
    }
    hotkey_changed |= ImGui::InputFloat("Trigger within range", &range, 0.0f, 0.0f);
    ImGui::ShowHelp(
        "The hotkey will only trigger within this range.\nUse 0 for no limit.");
    hotkey_changed |= ImGui::InputFloat("Distance from location", &distance_from_location, 0.0f, 0.0f);
    ImGui::ShowHelp(
        "Calculate and move to a point this many gwinches away from the location.\nUse 0 to go to that exact location.");
    hotkey_changed |= ImGui::InputText("Name", name, 140);
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}

void HotkeyMove::Execute()
{
    if (!CanUse()) {
        return;
    }
    const auto me = GW::Agents::GetControlledCharacter();
    GW::Vec2f location(x, y);
    if (type == MoveType::Target) {
        const auto target = GW::Agents::GetTarget();
        if (!target) {
            return;
        }
        location = target->pos;
    }

    const auto dist = GetDistance(me->pos, location);
    if (range != 0 && dist > range) {
        return;
    }
    if (distance_from_location > 0.f) {
        const auto direction = location - me->pos;
        const auto unit = Normalize(direction);
        location = location - unit * distance_from_location;
    }
    GW::Agents::Move(location.x, location.y);
    if (name[0] == '\0') {
        if (show_message_in_emote_channel) {
            Log::Flash("Moving to (%.0f, %.0f)", x, y);
        }
    }
    else {
        if (show_message_in_emote_channel) {
            Log::Flash("Moving to %s", name);
        }
    }
}

HotkeyDialog::HotkeyDialog(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    id = static_cast<size_t>(ini->GetLongValue(section, "DialogID", 0));
    strcpy_s(name, ini->GetValue(section, "DialogName", ""));
}

void HotkeyDialog::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "DialogID", id);
    ini->SetValue(section, "DialogName", name);
}

int HotkeyDialog::Description(char* buf, const size_t bufsz)
{
    if (!name[0]) {
        return snprintf(buf, bufsz, "Dialog #%zu", id);
    }
    return snprintf(buf, bufsz, "Dialog %s", name);
}

bool HotkeyDialog::Draw()
{
    bool hotkey_changed = ImGui::InputInt("Dialog ID", reinterpret_cast<int*>(&id));
    ImGui::ShowHelp("If dialog is 0, accepts the first available quest dialog (either reward or accept quest).");
    hotkey_changed |= ImGui::InputText("Dialog Name", name, _countof(name));
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}

void HotkeyDialog::Execute()
{
    if (!CanUse()) {
        return;
    }
    char buf[32];
    if (id == 0) {
        snprintf(buf, _countof(buf), "dialog take");
    }
    else {
        snprintf(buf, _countof(buf), "dialog 0x%X", id);
    }

    GW::Chat::SendChat('/', buf);
    if (show_message_in_emote_channel) {
        Log::Flash("Sent dialog %s (%d)", name, id);
    }
}

const char* HotkeyHeroTeamBuild::GetText(void*, const int idx)
{
    if (idx >= static_cast<int>(HeroBuildsWindow::Instance().BuildCount())) {
        return nullptr;
    }
    return HeroBuildsWindow::Instance().BuildName(static_cast<size_t>(idx));
}

HotkeyHeroTeamBuild::HotkeyHeroTeamBuild(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    index = static_cast<size_t>(ini->GetLongValue(section, "BuildIndex", 0));
}

void HotkeyHeroTeamBuild::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "BuildIndex", index);
}

int HotkeyHeroTeamBuild::Description(char* buf, const size_t bufsz)
{
    const char* buildname = HeroBuildsWindow::Instance().BuildName(index);
    if (buildname == nullptr) {
        buildname = "<not found>";
    }
    return snprintf(buf, bufsz, "Load Hero Team Build '%s'", buildname);
}

bool HotkeyHeroTeamBuild::Draw()
{
    bool hotkey_changed = false;
    const int icount = static_cast<int>(HeroBuildsWindow::Instance().BuildCount());
    int iindex = static_cast<int>(index);
    if (ImGui::Combo("Build", &iindex, GetText, nullptr, icount)) {
        if (0 <= iindex) {
            index = static_cast<size_t>(iindex);
        }
        hotkey_changed = true;
    }
    return hotkey_changed;
}

void HotkeyHeroTeamBuild::Execute()
{
    if (!CanUse()) {
        return;
    }
    HeroBuildsWindow::Instance().Load(index);
}

HotkeyFlagHero::HotkeyFlagHero(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    degree = static_cast<float>(ini->GetDoubleValue(section, "degree", degree));
    distance = static_cast<float>(ini->GetDoubleValue(section, "distance", distance));
    hero = ini->GetLongValue(section, "hero", hero);
    if (hero < 0) {
        hero = 0;
    }
    if (hero > 11) {
        hero = 11;
    }
}

void HotkeyFlagHero::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetDoubleValue(section, "degree", degree);
    ini->SetDoubleValue(section, "distance", distance);
    ini->SetLongValue(section, "hero", hero);
}

int HotkeyFlagHero::Description(char* buf, const size_t bufsz)
{
    if (hero == 0) {
        return snprintf(buf, bufsz, "Flag All Heroes");
    }
    return snprintf(buf, bufsz, "Flag Hero %d", hero);
}

bool HotkeyFlagHero::Draw()
{
    bool hotkey_changed = false;
    hotkey_changed |= ImGui::DragFloat("Degree", &degree, 0.0f, -360.0f, 360.f);
    hotkey_changed |= ImGui::DragFloat("Distance", &distance, 0.0f, 0.0f, 10'000.f);
    if (hotkeys_changed && distance < 0.f) {
        distance = 0.f;
    }
    hotkey_changed |= ImGui::InputInt("Hero", &hero, 1);
    if (hotkey_changed && hero < 0) {
        hero = 0;
    }
    else if (hotkey_changed && hero > 11) {
        hero = 11;
    }
    ImGui::ShowHelp("The hero number that should be flagged (1-11).\nUse 0 to flag all");
    ImGui::Text("For a minimap flagging hotkey, please create a chat hotkey with:");
    ImGui::TextColored({1.f, 1.f, 0.f, 1.f}, "/flag %d toggle", hero);
    return hotkey_changed;
}

void HotkeyFlagHero::Execute()
{
    if (!isExplorable()) {
        return;
    }

    const GW::Vec3f allflag = GW::GetGameContext()->world->all_flag;

    if (hero < 0) {
        return;
    }
    if (hero == 0) {
        if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
            GW::PartyMgr::UnflagAll();
            return;
        }
    }
    else {
        const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
        if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
            return;
        }

        const GW::HeroFlag& flag = flags[hero - 1];
        if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
            GW::PartyMgr::UnflagHero(hero);
            return;
        }
    }

    const GW::AgentLiving* player = GW::Agents::GetControlledCharacter();
    if (!player) {
        return;
    }
    const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();

    float reference_radiant = player->rotation_angle;

    if (target && target != player) {
        const float dx = target->x - player->x;
        const float dy = target->y - player->y;

        reference_radiant = std::atan(dx == 0 ? dy : dy / dx);
        if (dx < 0) {
            reference_radiant += DirectX::XM_PI;
        }
        else if (dx > 0 && dy < 0) {
            reference_radiant += 2 * DirectX::XM_PI;
        }
    }

    const float radiant = degree * DirectX::XM_PI / 180.f;
    const float x = player->x + distance * std::cos(reference_radiant - radiant);
    const float y = player->y + distance * std::sin(reference_radiant - radiant);

    const auto pos = GW::GamePos(x, y, 0);

    if (hero == 0) {
        GW::PartyMgr::FlagAll(pos);
    }
    else {
        GW::PartyMgr::FlagHero(hero, pos);
    }
}

HotkeyGWKey::HotkeyGWKey(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    action = static_cast<GW::UI::ControlAction>(ini->GetLongValue(section, "ActionID", action));
}

void HotkeyGWKey::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ActionID", action);
}

int HotkeyGWKey::Description(char* buf, const size_t bufsz)
{
    action_idx = GetHotkeyActionIdx(action);
    if (action_idx < 0 || action_idx >= static_cast<int>(control_labels.size())) {
        return snprintf(buf, bufsz, "Guild Wars Key: Unknown Action %d", action_idx);
    }
    return snprintf(buf, bufsz, "Guild Wars Key: %s", control_labels[action_idx].second->string().c_str());
}

bool HotkeyGWKey::Draw()
{
    if (!BuildHotkeyGWKeyLabels()) {
        ImGui::Text("Waiting on endoded strings");
        return false;
    }
    action_idx = GetHotkeyActionIdx(action);

    if (ImGui::Combo("Action###combo", &action_idx, HotkeyGWKey_labels.data(), HotkeyGWKey_labels.size(), 10)) {
        action = control_labels[action_idx].first;
        return true;
    }
    return false;
}

void HotkeyGWKey::Execute()
{
    GW::GameThread::Enqueue([&] {
        const auto frame = GW::UI::GetFrameByLabel(L"Game");
        Keypress(action, GW::UI::GetChildFrame(frame, 6));
        Keypress(action, GW::UI::GetParentFrame(frame));
    });
}

namespace {
    const char* GetBehaviorDesc(GW::HeroBehavior behaviour)
    {
        if (std::to_underlying(behaviour) < behaviors.size()) {
            return behaviors[std::to_underlying(behaviour)];
        }
        return nullptr;
    }
}

HotkeyCommandPet::HotkeyCommandPet(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    behavior = ini ? static_cast<GW::HeroBehavior>(ini->GetLongValue(section, "behavior", static_cast<long>(behavior))) : behavior;
    if (!GetBehaviorDesc(behavior)) {
        behavior = GW::HeroBehavior::Fight;
    }
}

void HotkeyCommandPet::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "behavior", static_cast<long>(behavior));
}

int HotkeyCommandPet::Description(char* buf, const size_t bufsz)
{
    return snprintf(buf, bufsz, "Command Pet: %s", GetBehaviorDesc(behavior));
}

bool HotkeyCommandPet::Draw()
{
    const bool changed = ImGui::Combo("Behavior###combo", reinterpret_cast<int*>(&behavior), behaviors.data(), behaviors.size(), behaviors.size());
    if (changed && !GetBehaviorDesc(behavior)) {
        behavior = GW::HeroBehavior::Fight;
    }
    return changed;
}

void HotkeyCommandPet::Execute()
{
    GW::GameThread::Enqueue([&] {
        for (auto& pet : GW::GetWorldContext()->pets) {
            GW::PartyMgr::SetPetBehavior(pet.owner_agent_id, behavior);
        }
    });
}
