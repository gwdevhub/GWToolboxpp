#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <ImGuiAddons.h>
#include <Keys.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/HotkeysWindow.h>

#include <Windows/Hotkeys/TBHotkey.h>
#include <Windows/Hotkeys/HotkeyAction.h>
#include <Windows/Hotkeys/HotkeyCommandPet.h>
#include <Windows/Hotkeys/HotkeyDialog.h>
#include <Windows/Hotkeys/HotkeyDropUseBuff.h>
#include <Windows/Hotkeys/HotkeyEquipItem.h>
#include <Windows/Hotkeys/HotkeyFlagHero.h>
#include <Windows/Hotkeys/HotkeyGroup.h>
#include <Windows/Hotkeys/HotkeyGWKey.h>
#include <Windows/Hotkeys/HotkeyMove.h>
#include <Windows/Hotkeys/HotkeySendChat.h>
#include <Windows/Hotkeys/HotkeyTarget.h>
#include <Windows/Hotkeys/HotkeyToggle.h>
#include <Windows/Hotkeys/HotkeyUseItem.h>

bool TBHotkey::show_active_in_header = true;
bool TBHotkey::show_run_in_header = true;
bool TBHotkey::hotkeys_changed = false;
unsigned int TBHotkey::cur_ui_id = 0;

TBHotkey* TBHotkey::HotkeyFactory(ToolboxIni* ini, const char* section)
{
    std::string str(section);
    if (str.compare(0, 7, "hotkey-") != 0) {
        return nullptr;
    }
    constexpr size_t first_sep = 6;
    const size_t second_sep = str.find(':', first_sep);
    if (second_sep == std::string::npos) {
        return nullptr;
    }
    std::string id = str.substr(first_sep + 1, second_sep - first_sep - 1);
    const std::string type = str.substr(second_sep + 1);

    // Skip child sections (e.g. "hotkey-000-001:SendChat") – loaded by HotkeyGroup
    if (id.find('-') != std::string::npos) {
        return nullptr;
    }

    hotkeys_changed = true;

    if (type == HotkeyGroup::IniSection()) {
        return new HotkeyGroup(ini, section);
    }
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
    if (type == "HeroTeamBuild") {
        // Migrate legacy index-based hero team build hotkeys to SendChat hotkeys
        const size_t build_index = static_cast<size_t>(ini->GetLongValue(section, "BuildIndex", 0));
        auto& hbw = HeroBuildsWindow::Instance();
        if (hbw.BuildCount() == 0) {
            hbw.LoadFromFile();
        }
        const char* build_name = hbw.BuildName(build_index);
        char msg[139] = "heroteam";
        if (build_name && *build_name) {
            snprintf(msg, sizeof(msg), "heroteam %s", build_name);
        }
        ini->SetValue(section, "msg", msg);
        ini->SetValue(section, "channel", "/");
        hotkeys_changed = true;
        return new HotkeySendChat(ini, section);
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

        ini_str = ini->GetValue(section, VAR_NAME(label), "");
        strncpy(label, ini_str.c_str(), _countof(label) - 1);

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
        block_other_hotkeys_on_trigger = ini->GetBoolValue(section, VAR_NAME(block_other_hotkeys_on_trigger), block_other_hotkeys_on_trigger);

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
    ini->SetBoolValue(section, VAR_NAME(block_other_hotkeys_on_trigger), block_other_hotkeys_on_trigger);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_lose_focus), trigger_on_lose_focus);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_gain_focus), trigger_on_gain_focus);
    ini->SetBoolValue(section, VAR_NAME(strict_key_combo), strict_key_combo);


    if(player_names.size())
        ini->SetValue(section, VAR_NAME(player_names), TextUtils::Join(player_names, ",").c_str());
    if(*group)
        ini->SetValue(section, VAR_NAME(group), group);
    if(*label)
        ini->SetValue(section, VAR_NAME(label), label);

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
    const float scale = ImGui::FontScale();
    const auto show_header_buttons = [&] {
        // If no buttons will be drawn, avoid calling SameLine() — doing so without
        // drawing anything after leaves ImGui's cursor at the header's Y, causing
        // subsequent items (next header or expanded content) to render at the wrong position.
        if (!show_run_in_header && first && last && !show_active_in_header) {
            return;
        }

        ImGui::PushID(static_cast<int>(ui_id));
        ImGui::PushID("header");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float btn_size = ImGui::GetFrameHeight();
        const float spacing = style.ItemSpacing.x;
        const float run_btn_width = 64.0f * scale;

        // Draw buttons from right to left

        auto current_pos = ImVec2(
            ImGui::GetContentRegionAvail().x + ImGui::GetIndent(),
            0.f
        );
        ImGui::SameLine();
        current_pos.y = ImGui::GetCursorPosY();

        if (show_run_in_header) {
            current_pos.x -= run_btn_width;
            ImGui::SetCursorPos(current_pos);
            if (ImGui::Button(ongoing ? "Stop" : "Run", ImVec2(run_btn_width, 0.0f))) {
                Toggle();
            }
            current_pos.x -= spacing;
        }

        current_pos.x -= btn_size;
        if (!last) {
            ImGui::SetCursorPos(current_pos);
            if (ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(btn_size, btn_size))) {
                *op = Op_MoveDown;
                hotkey_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the hotkey down in the list");
            }

        }
        current_pos.x -= spacing;

        current_pos.x -= btn_size;
        if(!first) {
            ImGui::SetCursorPos(current_pos);
            if (ImGui::Button(ICON_FA_ARROW_UP, ImVec2(btn_size, btn_size))) {
                *op = Op_MoveUp;
                hotkey_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the hotkey up in the list");
            }
        }
        current_pos.x -= spacing;

        if (show_active_in_header) {
            current_pos.x -= btn_size;
            ImGui::SetCursorPos(current_pos);
            hotkey_changed |= ImGui::Checkbox("##active", &active);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("The hotkey can trigger only when selected");
            }
            current_pos.x -= spacing;
        }

        ImGui::PopID();
        ImGui::PopID();
    };

    // === Header ===
    char header[256]{};

    int written = 0;
    if (*label) {
        written += snprintf(&header[written], _countof(header) - written, "%s", label);
    } else {
        written += Description(&header[written], _countof(header) - written);
    }
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
                prof_ids_written += snprintf(&prof_ids_buf[prof_ids_written], _countof(prof_ids_buf) - prof_ids_written, format, ToolboxUtils::GetProfessionAcronym(static_cast<GW::Constants::Profession>(i))->string().c_str());
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
    if (open_state_override >= 0) {
        ImGui::SetNextItemOpen(open_state_override == 1, ImGuiCond_Always);
        open_state_override = -1;
    }
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
        hotkey_changed |= ImGui::InputTextEx("Label##hotkey_label", "Use description as label", label, sizeof(label), ImVec2(0, 0), ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr);
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
        hotkey_changed |= ImGui::CheckboxWithHelp("Trigger hotkey when playing on PvP character", &trigger_on_pvp_character, "Unless enabled, this hotkey will not activate when playing on a PvP only character.");
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
        hotkey_changed |= ImGui::CheckboxWithHelp("Block other hotkeys when triggered", &block_other_hotkeys_on_trigger, "If this hotkey is triggered, don't check any hotkeys that come after this one in the list");

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

        // === Duplicate and delete buttons ===
        const float half_btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.f;
        if (ImGui::Button("Duplicate", ImVec2(half_btn_w, 0))) {
            *op = Op_Duplicate;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a copy of this hotkey below");
        }
        ImGui::SameLine();
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
