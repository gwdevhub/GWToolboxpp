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

#include <Modules/DialogModule.h>
#include <Windows/BuildsWindow.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/Hotkeys.h>
#include <Windows/PconsWindow.h>
#include <Modules/Resources.h>


bool TBHotkey::show_active_in_header = true;
bool TBHotkey::show_run_in_header = true;
bool TBHotkey::hotkeys_changed = false;
unsigned int TBHotkey::cur_ui_id = 0;
WORD* TBHotkey::key_out = nullptr;
DWORD* TBHotkey::mod_out = nullptr;
std::unordered_map<WORD, HotkeyToggle*> HotkeyToggle::toggled;
std::vector<const char*> HotkeyGWKey::labels = {};
std::vector<std::pair<GW::UI::ControlAction, GuiUtils::EncString*>> HotkeyGWKey::control_labels = {
    { GW::UI::ControlAction::ControlAction_Interact, nullptr },
    { GW::UI::ControlAction::ControlAction_CancelAction, nullptr },
    { GW::UI::ControlAction::ControlAction_ClearPartyCommands, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero1, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero2, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero3, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero4, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero5, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero6, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandHero7, nullptr },
    { GW::UI::ControlAction::ControlAction_CommandParty, nullptr },

    { GW::UI::ControlAction::ControlAction_DropItem, nullptr },
    { GW::UI::ControlAction::ControlAction_Follow, nullptr },

    { GW::UI::ControlAction::ControlAction_OpenHero1PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHero2PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHero3PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHero4PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHero5PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHero6PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHero7PetCommander, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander1, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander2, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander3, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander4, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander5, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander6, nullptr },
    { GW::UI::ControlAction::ControlAction_OpenHeroCommander7, nullptr },

    { GW::UI::ControlAction::ControlAction_Hero1Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero1Skill8, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero2Skill8, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero3Skill8, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero4Skill8, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero5Skill8, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero6Skill8, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill1, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill2, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill3, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill4, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill5, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill6, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill7, nullptr },
    { GW::UI::ControlAction::ControlAction_Hero7Skill8, nullptr },

    { GW::UI::ControlAction::ControlAction_UseSkill1, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill2, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill3, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill4, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill5, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill6, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill7, nullptr },
    { GW::UI::ControlAction::ControlAction_UseSkill8, nullptr },

    { GW::UI::ControlAction::ControlAction_ActivateWeaponSet1, nullptr },
    { GW::UI::ControlAction::ControlAction_ActivateWeaponSet2, nullptr },
    { GW::UI::ControlAction::ControlAction_ActivateWeaponSet3, nullptr },
    { GW::UI::ControlAction::ControlAction_ActivateWeaponSet4, nullptr },

    { GW::UI::ControlAction::ControlAction_TargetAllyNearest, nullptr },
    { GW::UI::ControlAction::ControlAction_ClearTarget, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetNearestEnemy, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetNextEnemy, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPreviousEnemy, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetNearestItem, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetNextItem, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPreviousItem, nullptr },

    { GW::UI::ControlAction::ControlAction_TargetPartyMember1, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember2, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember3, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember4, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember5, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember6, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember7, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember8, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember9, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember10, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember11, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMember12, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMemberNext, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPartyMemberPrevious, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetPriorityTarget, nullptr },
    { GW::UI::ControlAction::ControlAction_TargetSelf, nullptr }
};
namespace {
    // @Cleanup: when toolbox closes, this array isn't freed properly
    std::vector<std::vector<HotkeyEquipItemAttributes*>> available_items;

    const char* behaviors[] = {
        "Fight",
        "Guard",
        "Avoid Combat"
    };
}


TBHotkey *TBHotkey::HotkeyFactory(ToolboxIni *ini, const char *section)
{
    std::string str(section);
    if (str.compare(0, 7, "hotkey-") != 0)
        return nullptr;
    size_t first_sep = 6;
    size_t second_sep = str.find(L':', first_sep);
    std::string id = str.substr(first_sep + 1, second_sep - first_sep - 1);
    std::string type = str.substr(second_sep + 1);

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
    if (type == HotkeyPingBuild::IniSection()) {
        return new HotkeyPingBuild(ini, section);
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

TBHotkey::TBHotkey(ToolboxIni *ini, const char *section)
    : ui_id(++cur_ui_id)
{
    memset(prof_ids, false, sizeof(prof_ids));
    if (ini) {
        hotkey = ini->GetLongValue(section, VAR_NAME(hotkey), hotkey);
        modifier = ini->GetLongValue(section, VAR_NAME(modifier), modifier);
        active = ini->GetBoolValue(section, VAR_NAME(active), active);

        std::string ini_str = ini->GetValue(section, VAR_NAME(map_ids), "");
        GuiUtils::IniToArray(ini_str, map_ids);
        if (map_ids.empty()) {
            // Legacy
            int map_id = ini->GetLongValue(section, "map_id",0);
            if (map_id > 0)
                map_ids.push_back(map_id);
        }

        ini_str = ini->GetValue(section, VAR_NAME(prof_ids), "");
        std::vector<uint32_t> prof_ids_tmp;

        GuiUtils::IniToArray(ini_str, prof_ids_tmp);
        if (!prof_ids_tmp.empty()) {
            for (const auto prof_id : prof_ids_tmp) {
                if (prof_id < _countof(prof_ids))
                    prof_ids[prof_id] = true;
            }
        }
        else {
            // Legacy
            int prof_id = ini->GetLongValue(section, "prof_id", 0);
            if (prof_id > 0 && prof_id < _countof(prof_ids))
                prof_ids[prof_id] = true;
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

        in_range_of_distance = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(in_range_of_distance), in_range_of_distance));
        in_range_of_npc_id = ini->GetLongValue(section, VAR_NAME(in_range_of_npc_id), in_range_of_npc_id);
        const std::string player_name_s = ini->GetValue(section, VAR_NAME(player_name), "");
        memset(player_name, 0, sizeof(player_name));
        if (!player_name_s.empty()) {
            strncpy(player_name, player_name_s.c_str(), _countof(player_name));
        }
    }
}
size_t TBHotkey::HasProfession() {
    size_t out = 0;
    for (size_t i = 1; i < _countof(prof_ids); i++) {
        if (prof_ids[i])
            out++;
    }
    return out;
}
bool TBHotkey::IsValid(const char* _player_name, GW::Constants::InstanceType _instance_type, GW::Constants::Profession _profession, GW::Constants::MapID _map_id, bool is_pvp_character) {
    return active
        && (!is_pvp_character || trigger_on_pvp_character)
        && (instance_type == -1 || static_cast<GW::Constants::InstanceType>(instance_type) == _instance_type)
        && (prof_ids[static_cast<size_t>(_profession)] || !HasProfession())
        && (map_ids.empty() || std::ranges::find(map_ids, static_cast<uint32_t>(_map_id)) != map_ids.end())
        && (!player_name[0] || strcmp(_player_name, player_name) == 0);
}
bool TBHotkey::CanUse()
{
    return !isLoading() && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow() && IsInRangeOfNPC();
}
void TBHotkey::Save(ToolboxIni *ini, const char *section) const
{
    ini->SetLongValue(section, VAR_NAME(hotkey), hotkey);
    ini->SetLongValue(section, VAR_NAME(modifier), modifier);
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
    ini->SetBoolValue(section, VAR_NAME(trigger_on_lose_focus), trigger_on_lose_focus);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_gain_focus), trigger_on_gain_focus);
    ini->SetValue(section, VAR_NAME(player_name), player_name);
    ini->SetValue(section, VAR_NAME(group), group);

    std::string out;
    std::vector<uint32_t> prof_ids_tmp;
    for (size_t i = 0; i < _countof(prof_ids); i++) {
        if(prof_ids[i])
            prof_ids_tmp.push_back(i);
    }
    GuiUtils::ArrayToIni(prof_ids_tmp.data(), prof_ids_tmp.size(), &out);
    ini->SetValue(section, VAR_NAME(prof_ids), out.c_str());

    GuiUtils::ArrayToIni(map_ids.data(), map_ids.size(), &out);
    ini->SetValue(section, VAR_NAME(map_ids), out.c_str());

    ini->SetDoubleValue(section, VAR_NAME(in_range_of_distance), in_range_of_distance);
    ini->SetLongValue(section, VAR_NAME(in_range_of_npc_id), in_range_of_npc_id);
}
const char* TBHotkey::professions[] = {"Any",          "Warrior",     "Ranger",
                                    "Monk",         "Necromancer", "Mesmer",
                                    "Elementalist", "Assassin",    "Ritualist",
                                    "Paragon",      "Dervish"};
const char* TBHotkey::instance_types[] = {"Any", "Outpost", "Explorable"};
void TBHotkey::HotkeySelector(WORD* key, DWORD* modifier) {
    key_out = key;
    mod_out = modifier;
    ImGui::OpenPopup("Select Hotkey");
}
bool TBHotkey::Draw(Op *op)
{
    bool hotkey_changed = false;
    const float scale = ImGui::GetIO().FontGlobalScale;
    auto ShowHeaderButtons = [&]() {
        if (show_active_in_header || show_run_in_header) {
            ImGui::PushID(static_cast<int>(ui_id));
            ImGui::PushID("header");
            ImGuiStyle &style = ImGui::GetStyle();
            const float btn_width = 64.0f * scale;
            if (show_active_in_header) {
                ImGui::SameLine(
                    ImGui::GetContentRegionAvail().x -
                    ImGui::GetTextLineHeight() - style.FramePadding.y * 2 -
                    (show_run_in_header
                         ? (btn_width + ImGui::GetStyle().ItemSpacing.x)
                         : 0));
                hotkey_changed |= ImGui::Checkbox("", &active);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "The hotkey can trigger only when selected");
            }
            if (show_run_in_header) {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                                btn_width);
                if (ImGui::Button(ongoing ? "Stop" : "Run", ImVec2(btn_width, 0.0f))) {
                    Toggle();
                }
            }
            ImGui::PopID();
            ImGui::PopID();
        }
    };

    // === Header ===
    char header[256]{};
    char keybuf[64]{};

    int written = 0;
    written += Description(&header[written], _countof(header) - written);
    switch (HasProfession()) {
    case 1:
        for (size_t i = 1; i < _countof(prof_ids);i++) {
            if (prof_ids[i])
                written += snprintf(&header[written], _countof(header) - written, " [%s]", professions[i]);
        }
        break;
    case 0:
        break;
    default: {
        char prof_ids_buf[128];
        int prof_ids_written = 0;
        for (size_t i = 1; i < _countof(prof_ids); i++) {
            if (!prof_ids[i])
                continue;
            const char* format = ", %s";
            if (!prof_ids_written)
                format = "%s";
            prof_ids_written += snprintf(&prof_ids_buf[prof_ids_written], _countof(prof_ids_buf) - prof_ids_written, format, GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(i)).c_str());
        }
        written += snprintf(&header[written], _countof(header) - written, " [%s]", prof_ids_buf);
    } break;

    }
    switch (map_ids.size()) {
    case 1:
        if (map_ids[0] < _countof(GW::Constants::NAME_FROM_ID))
            written += snprintf(&header[written], _countof(header) - written, " [%s]", GW::Constants::NAME_FROM_ID[map_ids[0]]);
        else
            written += snprintf(&header[written], _countof(header) - written, " [Map %d]", map_ids[0]);
        break;
    case 0:
        break;
    default:
        written += snprintf(&header[written], _countof(header) - written, " [%d Maps]", map_ids.size());
        break;
    }

    ASSERT(ModKeyName(keybuf, _countof(keybuf), modifier, hotkey, "<None>") != -1);
    ASSERT(snprintf(&header[written], _countof(header) - written, " [%s]###header%u", keybuf, ui_id) != -1);
    ImGuiTreeNodeFlags flags = (show_active_in_header || show_run_in_header)
                                   ? ImGuiTreeNodeFlags_AllowItemOverlap
                                   : 0;
    if (!ImGui::CollapsingHeader(header, flags)) {
        ShowHeaderButtons();
    } else {
        ShowHeaderButtons();
        ImGui::Indent();
        ImGui::PushID(static_cast<int>(ui_id));
        ImGui::PushItemWidth(-140.0f * scale);
        // === Specific section ===
        hotkey_changed |= Draw();

        // === Hotkey section ===
        const float indent_offset = ImGui::GetCurrentWindow()->DC.Indent.x;
        const float offset_sameline = indent_offset + (ImGui::GetContentRegionAvail().x / 2);
        hotkey_changed |= ImGui::InputTextEx("Hotkey Group##hotkey_group", "No Hotkey Group", group, sizeof(group), ImVec2(0, 0), ImGuiInputTextFlags_EnterReturnsTrue, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit())
            hotkey_changed = true;
        hotkey_changed |= ImGui::Checkbox("Block key in Guild Wars when triggered", &block_gw);
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
        if (ImGui::CollapsingHeader("Map IDs")) {
            ImGui::Indent();

            ImGui::TextDisabled("Only trigger in selected maps:");
            float map_id_w = (140.f * scale);
            if (map_ids.empty()) {
                ImGui::Text("    This hotkey will trigger in any map - add a map ID below to limit to a map");
            }
            for (int i = 0; i < static_cast<int>(map_ids.size()); i++) {
                ImGui::PushID(i);
                ImGui::Text("%d", map_ids[i]);
                ImGui::SameLine(indent_offset + map_id_w);
                if (ImGui::Button("X")) {
                    map_ids.erase(map_ids.begin() + i);
                    i--;
                    hotkey_changed = true;
                }
                ImGui::PopID();
            }

            static char map_id_input_buf[4];
            ImGui::Separator();
            ImGui::Text("Add Map ID:");
            ImGui::SameLine();
            ImGui::PushItemWidth(map_id_w);
            bool add_map_id = ImGui::InputText("##add_map_id", map_id_input_buf, _countof(map_id_input_buf), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            add_map_id |= ImGui::Button("Add##add_map_id_for_hotkey", { 64.f * scale,0.f });
            if (add_map_id) {
                int map_id_out = 0;
                if (strlen(map_id_input_buf)
                    && GuiUtils::ParseInt(map_id_input_buf, &map_id_out)
                    && map_id_out > 0
                    && std::ranges::find(map_ids, reinterpret_cast<uint32_t>(map_id_input_buf)) == map_ids.end()) {
                    map_ids.push_back(static_cast<uint32_t>(map_id_out));
                    memset(map_id_input_buf, 0, sizeof(map_id_input_buf));
                    hotkey_changed = true;
                }
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Professions")) {
            ImGui::Indent();
            float prof_w = 140.f * scale;
            int per_row = static_cast<int>(std::floor(ImGui::GetContentRegionAvail().x / prof_w));
            ImGui::TextDisabled("Only trigger when player is one of these professions");
            for (int i = 1; i < _countof(prof_ids); i++) {
                int offset = ((i-1) % per_row);
                if (i > 1 && offset != 0)
                    ImGui::SameLine(prof_w * offset + indent_offset);
                hotkey_changed |= ImGui::Checkbox(professions[i], &prof_ids[i]);
            }
            ImGui::Unindent();
        }

        ImGui::PushItemWidth(60.0f * scale);
        ImGui::Text("Only use this within ");
        ImGui::SameLine(0, 0);
        hotkey_changed |= ImGui::InputFloat("##in_range_of_distance", &in_range_of_distance, 0.f,0.f, "%.0f");
        ImGui::SameLine(0, 0);
        ImGui::Text(" gwinches of NPC Id: ");
        ImGui::SameLine(0, 0);
        hotkey_changed |= ImGui::InputInt("##in_range_of_npc_id",(int*) &in_range_of_npc_id, 0,0);
        ImGui::PopItemWidth();
        ImGui::ShowHelp("Only trigger when in range of a certain NPC");
        hotkey_changed |= ImGui::InputTextEx("Character Name##hotkey_player_name", "Any Character Name", player_name, sizeof(player_name), ImVec2(0, 0), 0, 0, 0);
        ImGui::ShowHelp("Only trigger for this character name (leave blank for any character name)");

        ImGui::Separator();
        hotkey_changed |= ImGui::Checkbox("###active", &active);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The hotkey can trigger only when selected");
        ImGui::SameLine();
        char keybuf2[_countof(keybuf) + 8];
        snprintf(keybuf2, _countof(keybuf2), "Hotkey: %s", keybuf);
        if (ImGui::Button(keybuf2, ImVec2(-140.0f * scale, 0))) {
            HotkeySelector((WORD*)&hotkey, (DWORD*)&modifier);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to change hotkey");
        if (ImGui::BeginPopup("Select Hotkey")) {
            static WORD newkey = 0;
            *op = Op_BlockInput;
            ImGui::Text("Press key");
            DWORD newmod = 0;
            if (mod_out) {
                if (ImGui::IsKeyDown(ImGuiKey_ModCtrl))
                    newmod |= ModKey_Control;
                if (ImGui::IsKeyDown(ImGuiKey_ModShift))
                    newmod |= ModKey_Shift;
                if (ImGui::IsKeyDown(ImGuiKey_ModAlt))
                    newmod |= ModKey_Alt;
            }


            if (newkey == 0) { // we are looking for the key
                for (WORD i = 0; i < 512; i++) {
                    switch (i) {
                        case VK_CONTROL:
                        case VK_LCONTROL:
                        case VK_RCONTROL:
                        case VK_SHIFT:
                        case VK_LSHIFT:
                        case VK_RSHIFT:
                        case VK_MENU:
                        case VK_LMENU:
                        case VK_RMENU:
                            continue;
                        default: {
                            if (::GetKeyState(i) & 0x8000)
                                newkey = i;
                        }
                    }
                }
            } else { // key was pressed, close if it's released
                if (!(::GetKeyState(newkey) & 0x8000)) {
                    *key_out = newkey;
                    if (mod_out)
                        *mod_out = newmod;
                    newkey = 0;
                    ImGui::CloseCurrentPopup();
                    hotkey_changed = true;
                }
            }

            // write the key
            char newkey_buf[256];
            ModKeyName(newkey_buf, _countof(newkey_buf), newmod, newkey);
            ImGui::Text("%s", newkey_buf);
            if (ImGui::Button("Clear")) {
                *key_out = 0;
                if (mod_out)
                    *mod_out = 0;
                newkey = 0;
                ImGui::CloseCurrentPopup();
                hotkey_changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                newkey = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(ongoing ? "Stop" : "Run", ImVec2(140.0f * scale, 0.0f))) {
            Toggle();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Execute the hotkey now");

        const auto btn_width = ImGui::GetContentRegionAvail().x / 3.0f;

        // === Move and delete buttons ===
        if (ImGui::Button("Move Up", ImVec2(btn_width, 0))) {
            *op = Op_MoveUp;
            hotkey_changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Move the hotkey up in the list");
        ImGui::SameLine();
        if (ImGui::Button("Move Down", ImVec2(btn_width, 0))) {
            *op = Op_MoveDown;
            hotkey_changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Move the hotkey down in the list");
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(btn_width, 0))) {
            ImGui::OpenPopup("Delete Hotkey?");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delete the hotkey");
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
bool TBHotkey::IsInRangeOfNPC() {
    if (!(in_range_of_npc_id && in_range_of_distance > 0.f))
        return true;
    auto* agents = GW::Agents::GetAgentArray();
    if (!agents)
        return false;
    auto* me = GW::Agents::GetPlayer();
    for (const auto agent : *agents) {
        if (!(agent && agent->type == 0xDB))
            continue;
        auto* living = agent->GetAsAgentLiving();
        if (living->login_number || living->player_number != static_cast<uint16_t>(in_range_of_npc_id))
            continue;
        if (GW::GetDistance(agent->pos, me->pos) < in_range_of_distance)
            return true;
    }
    return false;
}
HotkeySendChat::HotkeySendChat(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    strcpy_s(message, ini->GetValue(section, "msg", ""));
    channel = ini->GetValue(section, "channel", "/")[0];
}
void HotkeySendChat::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "msg", message);
    char buf[8];
    snprintf(buf, 8, "%c", channel);
    ini->SetValue(section, "channel", buf);
}
int HotkeySendChat::Description(char *buf, size_t bufsz)
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
    static const char *channels[] = {"/ Commands", "! All",   "@ Guild",
                                     "# Group",    "$ Trade", "% Alliance",
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
    hotkey_changed |= (channel == '/' && ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel));
    return hotkey_changed;
}
void HotkeySendChat::Execute()
{
    if (!CanUse())
        return;
    if (show_message_in_emote_channel && channel == L'/') {
        Log::Info("/%s", message);
    }
    GW::Chat::SendChat(channel, message);
}

HotkeyUseItem::HotkeyUseItem(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    item_id = static_cast<size_t>(ini->GetLongValue(section, "ItemID", 0));
    strcpy_s(name, ini->GetValue(section, "ItemName", ""));
}
void HotkeyUseItem::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ItemID", static_cast<long>(item_id));
    ini->SetValue(section, "ItemName", name);
}
int HotkeyUseItem::Description(char *buf, size_t bufsz)
{
    if (!name[0])
        return snprintf(buf, bufsz, "Use #%d", item_id);
    return snprintf(buf, bufsz, "Use %s", name);
}
bool HotkeyUseItem::Draw()
{
    bool hotkey_changed = ImGui::InputInt("Item ID", (int*)&item_id);
    hotkey_changed |= ImGui::InputText("Item Name", name, _countof(name));
    hotkey_changed |= ImGui::Checkbox("Display error message on failure", &show_error_on_failure);
    return hotkey_changed;
}
void HotkeyUseItem::Execute()
{
    if (!CanUse())
        return;
    if (item_id == 0)
        return;

    bool used = GW::Items::UseItemByModelId(item_id, 1, 4);
    if (!used &&
        GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        used = GW::Items::UseItemByModelId(item_id, 8, 16);
    }

    if (!used && show_error_on_failure) {
        if (name[0] == '\0') {
            Log::Error("Item #%d not found!", item_id);
        } else {
            Log::Error("%s not found!", name);
        }
    }
}

HotkeyEquipItemAttributes::HotkeyEquipItemAttributes(const GW::Item* item) {
    set(item->model_id, item->complete_name_enc ? item->complete_name_enc : item->name_enc, item->info_string, item->mod_struct, item->mod_struct_size);
}
HotkeyEquipItemAttributes::~HotkeyEquipItemAttributes() {
    if (mod_struct) {
        delete mod_struct;
        mod_struct = nullptr;
        mod_struct_size = 0;
    }
}
HotkeyEquipItemAttributes::HotkeyEquipItemAttributes(uint32_t _model_id, const wchar_t* _name_enc, const wchar_t* _info_string, const GW::ItemModifier* _mod_struct, size_t _mod_struct_size) {
    set(_model_id, _name_enc, _info_string, _mod_struct, _mod_struct_size);
}
HotkeyEquipItemAttributes* HotkeyEquipItemAttributes::set(uint32_t _model_id, const wchar_t* _name_enc, const wchar_t* _info_string, const GW::ItemModifier* _mod_struct, size_t _mod_struct_size) {
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
HotkeyEquipItemAttributes* HotkeyEquipItemAttributes::set(HotkeyEquipItemAttributes const& other) {
    return set(other.model_id, other.enc_name.encoded().c_str(), other.enc_desc.encoded().c_str(), other.mod_struct, other.mod_struct_size);
}

bool HotkeyEquipItemAttributes::check(GW::Item* item) {
    if (!item || item->model_id != model_id || item->mod_struct_size != mod_struct_size)
        return false;
    if (wcscmp(item->complete_name_enc ? item->complete_name_enc : item->name_enc, enc_name.encoded().c_str()) != 0)
        return false;
    if (mod_struct_size == item->mod_struct_size && memcmp(mod_struct, item->mod_struct, item->mod_struct_size * sizeof(*item->mod_struct)) != 0)
        return false;
    return true;
}

HotkeyEquipItem::HotkeyEquipItem(ToolboxIni *ini, const char *section)
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
        if (in.size()) {
            ASSERT(GuiUtils::IniToArray(in, enc_name));
        }
        in = ini->GetValue(section, "EncodedDesc", "");
        std::wstring enc_desc;
        if (in.size()) {
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
void HotkeyEquipItem::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "EquipBy", static_cast<long>(equip_by));
    ini->SetLongValue(section, "Bag", static_cast<long>(bag_idx));
    ini->SetLongValue(section, "Slot", static_cast<long>(slot_idx));
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
int HotkeyEquipItem::Description(char *buf, size_t bufsz)
{
    if (equip_by == SLOT)
        return snprintf(buf, bufsz, "Equip Item in bag %d slot %d", bag_idx, slot_idx);
    return snprintf(buf, bufsz, "Equip %s", item_attributes.name().c_str());
}
bool HotkeyEquipItem::Draw()
{
    bool hotkey_changed = false;
    constexpr const char* bags[6] = { "None", "Backpack", "Belt Pouch", "Bag 1", "Bag 2", "Equipment Pack" };
    ImGui::TextUnformatted("Equip By: "); ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Item", (int*)&equip_by, EquipBy::ITEM);
    ImGui::ShowHelp("Find and equip an item by its attributes, regardless of location in inventory.");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Slot", (int*)&equip_by, EquipBy::SLOT);
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
        constexpr size_t bags_size = _countof(bags);
        if (ImGui::BeginPopupModal("Choose Item to Equip",0, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (need_to_fetch_bag_items) {
                // Free available_items ptrs
                for (auto i = available_items.begin(); i != available_items.end(); i++) {
                    for (auto j = i->begin(); j != i->end(); j++) {
                        delete* j;
                    }
                }
                available_items.clear();
                available_items.resize(bags_size);
                for (size_t i = static_cast<size_t>(GW::Constants::Bag::Backpack); i < bags_size; i++) {
                    GW::Bag* bag = GW::Items::GetBag(i);
                    if (!bag)
                        continue;
                    GW::ItemArray& items = bag->items;
                    for (size_t slot = 0; slot < items.size(); slot++) {
                        const GW::Item* cur_item = items[slot];
                        if (!IsEquippable(cur_item)) continue;
                        available_items[i].push_back(new HotkeyEquipItemAttributes(cur_item));
                    }
                }
                need_to_fetch_bag_items = false;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            for (size_t i = static_cast<size_t>(GW::Constants::Bag::Backpack); i < bags_size; i++) {
                auto& items = available_items[i];
                if (items.empty())
                    continue;
                
                ImGui::TextUnformatted(bags[i]);
                ImGui::Indent();
                for (auto& ai : available_items[i]) {
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
bool HotkeyEquipItem::IsEquippable(const GW::Item *_item)
{
    if (!_item)
        return false;
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
            break;
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
GW::Item* HotkeyEquipItem::FindMatchingItem(GW::Constants::Bag _bag_idx) {
    GW::Bag* bag = GW::Items::GetBag(_bag_idx);
    if (!bag) return nullptr;
    GW::ItemArray& items = bag->items;
    for (const auto _item : items) {
        if (item_attributes.check(_item))
            return _item;
    }
    return nullptr;
}
void HotkeyEquipItem::Execute()
{
    if (!CanUse())
        return;
    if (!ongoing) {

        if (equip_by == SLOT) {
            if (bag_idx < 1 || bag_idx > 5 || slot_idx < 1 || slot_idx > 25) {
                if (show_error_on_failure)
                    Log::Error("Invalid bag slot %d/%d!", bag_idx, slot_idx);
                return;
            }
            GW::Bag* b = GW::Items::GetBag(bag_idx);
            if (!b) {
                if (show_error_on_failure)
                    Log::Error("Bag #%d not found!", bag_idx);
                return;
            }
            GW::ItemArray& items = b->items;
            if (!items.valid() || slot_idx > items.size()) {
                if (show_error_on_failure)
                    Log::Error("Invalid bag slot %d/%d!", bag_idx, slot_idx);
                return;
            }
            item = items.at(slot_idx - 1);
        }
        else {
            item = FindMatchingItem(GW::Constants::Bag::Equipped_Items);
            constexpr size_t bags_size = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack) + 1;
            for (size_t i = static_cast<size_t>(GW::Constants::Bag::Backpack); i < bags_size && !item; i++) {
                item = FindMatchingItem(static_cast<GW::Constants::Bag>(i));
            }
            if (!item) {
                if (show_error_on_failure)
                    Log::Error("No equippable item matching your hotkey");
                item = nullptr;
                return;
            }
        }

        if (!IsEquippable(item)) {
            if (show_error_on_failure)
                Log::Error("No equippable item in bag %d slot %d", bag_idx,
                           slot_idx);
            item = nullptr;
            return;
        }
        ongoing = true;
        start_time = std::chrono::steady_clock::now();
    } else {
        last_try = std::chrono::steady_clock::now();
        __int64 diff_mills =
            std::chrono::duration_cast<std::chrono::milliseconds>(last_try -
                                                                  start_time)
                .count();
        if (diff_mills < 500) {
            return; // Wait 250ms between tries.
        }
        if (diff_mills > 5000) {
            if (show_error_on_failure)
                Log::Error("Failed to equip item in bag %d slot %d", bag_idx,
                           slot_idx);
            ongoing = false;
            item = nullptr;
            return;
        }
    }

    if (!item || !item->item_id) {
        if (show_error_on_failure)
            Log::Error("Failed to equip item in bag %d slot %d", bag_idx,
                       slot_idx);
        ongoing = false;
        item = nullptr;
        return;
    }
    if (item->bag && item->bag->bag_type == GW::Constants::BagType::Equipped) {
        // Log::Info("Success!");
        ongoing = false;
        item = nullptr;
        return; // Success!
    }
    GW::AgentLiving *p = GW::Agents::GetCharacter();
    if (!p || p->GetIsDead()) {
        if (show_error_on_failure)
            Log::Error("Failed to equip item in bag %d slot %d", bag_idx,
                slot_idx);
        ongoing = false;
        item = nullptr;
        return;
    }
    const GW::Skillbar* s =  GW::SkillbarMgr::GetPlayerSkillbar();
    if (p->GetIsKnockedDown() || (s && s->casting)) {
        // Player knocked down or casting; wait.
        return;
    }
    if (p->skill) {
        // Casting atm
        return;
    }
    if (p->GetIsIdle() || p->GetIsMoving()) {
        GW::Items::EquipItem(item);
    } else {
        // Move to clear model state e.g. attacking, aftercast
        GW::Agents::Move(p->pos);
    }
}

bool HotkeyDropUseBuff::GetText(void *data, int idx, const char **out_text)
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
HotkeyDropUseBuff::HotkeyDropUseBuff(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    id = static_cast<GW::Constants::SkillID>(ini->GetLongValue(
        section, "SkillID", (long)GW::Constants::SkillID::Recall));
}
void HotkeyDropUseBuff::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "SkillID", static_cast<long>(id));
}
int HotkeyDropUseBuff::Description(char *buf, size_t bufsz)
{
    const char *skillname;
    GetText((void *)id, GetIndex(), &skillname);
    return snprintf(buf, bufsz, "Drop/Use %s", skillname);
}
bool HotkeyDropUseBuff::Draw()
{
    bool hotkey_changed = false;
    SkillIndex index = GetIndex();
    if (ImGui::Combo("Skill", (int *)&index,
                     "Recall\0Unyielding Aura\0Holy Veil\0Other", 4)) {
        switch (index) {
            case HotkeyDropUseBuff::Recall:
                id = GW::Constants::SkillID::Recall;
                break;
            case HotkeyDropUseBuff::UA:
                id = GW::Constants::SkillID::Unyielding_Aura;
                break;
            case HotkeyDropUseBuff::HolyVeil:
                id = GW::Constants::SkillID::Holy_Veil;
                break;
            case HotkeyDropUseBuff::Other:
                id = static_cast<GW::Constants::SkillID>(0);
                break;
            default:
                break;
        }
        hotkey_changed = true;
    }
    if (index == Other) {
        if (ImGui::InputInt("Skill ID", (int *)&id))
            hotkey_changed = true;
    }
    return hotkey_changed;
}
void HotkeyDropUseBuff::Execute()
{
    if (!CanUse())
        return;

    GW::Buff *buff = GW::Effects::GetPlayerBuffBySkillId(id);
    if (buff) {
        GW::Effects::DropBuff(buff->buff_id);
    } else {
        int islot = GW::SkillbarMgr::GetSkillSlot(id);
        if (islot >= 0) {
            uint32_t slot = static_cast<uint32_t>(islot);
            if (GW::SkillbarMgr::GetPlayerSkillbar()->skills[slot].recharge == 0) {
                GW::GameThread::Enqueue([slot] () -> void {
                    GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId(),
                        static_cast<uint32_t>(ImGui::IsKeyDown(ImGuiKey_ModCtrl)));
                });
            }
        }
    }
}

bool HotkeyToggle::GetText(void *, int idx, const char **out_text)
{
    switch (static_cast<ToggleTarget>(idx)) {
        case Clicker:
            *out_text = "Clicker";
            return true;
        case Pcons:
            *out_text = "Pcons";
            return true;
        case CoinDrop:
            *out_text = "Coin Drop";
            return true;
        case Tick:
            *out_text = "Tick";
            return true;
        default:
            return false;
    }
}

bool HotkeyToggle::IsValid(ToolboxIni *ini, const char *section)
{
    long val = ini->GetLongValue(section, "ToggleID", static_cast<long>(Clicker));
    return val >= 0 && val < Count;
}
HotkeyToggle::HotkeyToggle(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    target = static_cast<ToggleTarget>(ini->GetLongValue(section, "ToggleID", target));
    static bool initialised = false;
    if (!initialised)
        toggled.reserve(512);
    initialised = true;
    switch (target) {
    case Clicker:
        interval = 7;
        break;
    case CoinDrop:
        interval = 500;
        break;
    }
}
void HotkeyToggle::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ToggleID", (long)target);
}
int HotkeyToggle::Description(char *buf, size_t bufsz)
{
    const char* name{};
    GetText(nullptr, (int)target, &name);
    return snprintf(buf, bufsz, "Toggle %s", name);
}
bool HotkeyToggle::Draw()
{
    bool hotkey_changed = false;
    if (ImGui::Combo("Toggle###combo", (int*)&target, GetText, nullptr, Count)) {
        if (target == Clicker)
            togglekey = VK_LBUTTON;
        hotkey_changed = true;
    }
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}
HotkeyToggle::~HotkeyToggle() {
    if (IsToggled(true))
        toggled[togglekey] = 0;
}
void HotkeyToggle::Toggle() {
    if (!HasInterval())
        return Execute();
    ongoing = !IsToggled(true);
    toggled[togglekey] = ongoing ? this : 0;
    last_use = 0;
    if (ongoing || (!ongoing && !toggled[togglekey])) {
        switch (target) {
        case Clicker:
            Log::Info("Clicker is %s", ongoing ? "active" : "disabled");
            break;
        case CoinDrop:
            Log::Info("Coindrop is %s", ongoing ? "active" : "disabled");
            break;
        }
    }
}
bool HotkeyToggle::IsToggled(bool force) {
    if (force) {
        auto found = toggled.find(togglekey);
        ongoing = (found != toggled.end() && found->second == this);
    }
    return ongoing;
}
void HotkeyToggle::Execute()
{
    if (HasInterval()) {
        if (GW::Chat::GetIsTyping())
            return;
        if (!CanUse())
            ongoing = false;
        if (!ongoing)
            Toggle();
        if (TIMER_DIFF(last_use) < interval)
            return;
        if(!IsToggled(true)) {
            ongoing = false;
            last_use = 0;
            return;
        }
    }

    switch (target) {
        case HotkeyToggle::Clicker:
            INPUT input;
            memset(&input, 0, sizeof(INPUT));
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
            break;
        case HotkeyToggle::Pcons:
            PconsWindow::Instance().ToggleEnable();
            ongoing = false;
            // writing to chat is done by ToggleActive if needed
            break;
        case HotkeyToggle::CoinDrop:
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
                Toggle();
                return;
            }
            GW::Items::DropGold(1);
            break;
        case HotkeyToggle::Tick:
            const auto ticked = GW::PartyMgr::GetIsPlayerTicked();
            GW::PartyMgr::Tick(!ticked);
            ongoing = false;
            break;
    }
    last_use = TIMER_INIT();
}

bool HotkeyAction::GetText(void *, int idx, const char **out_text)
{
    switch (static_cast<Action>(idx)) {
        case OpenXunlaiChest:
            *out_text = "Open Xunlai Chest";
            return true;
        case OpenLockedChest:
            *out_text = "Open Locked Chest";
            return true;
        case DropGoldCoin:
            *out_text = "Drop Gold Coin";
            return true;
        case ReapplyTitle:
            *out_text = "Reapply appropriate Title";
            return true;
        case EnterChallenge:
            *out_text = "Enter Challenge";
            return true;
        default:
            return false;
    }
}
HotkeyAction::HotkeyAction(ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    action = static_cast<Action>(ini->GetLongValue(section, "ActionID", OpenXunlaiChest));
}
void HotkeyAction::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ActionID", action);
}
int HotkeyAction::Description(char *buf, size_t bufsz)
{
    const char* name{};
    GetText(nullptr, (int)action, &name);
    return snprintf(buf, bufsz, "%s", name);
}
bool HotkeyAction::Draw()
{
    ImGui::Combo("Action###actioncombo", (int*)&action, GetText, nullptr, n_actions);
    return true;
}
void HotkeyAction::Execute()
{
    if (!CanUse())
        return;
    switch (action) {
        case HotkeyAction::OpenXunlaiChest:
            if (isOutpost()) {
                GW::GameThread::Enqueue([]() {
                    GW::Items::OpenXunlaiWindow();
                    });
            }
            break;
        case HotkeyAction::OpenLockedChest: {
            if (isExplorable()) {
                GW::Agent* target = GW::Agents::GetTarget();
                if (target && target->GetIsGadgetType()) {
                    GW::Agents::GoSignpost(target);
                    GW::Items::OpenLockedChest();
                }
            }
            break;
        }
        case HotkeyAction::DropGoldCoin:
            if (isExplorable()) {
                GW::Items::DropGold(1);
            }
            break;
        case HotkeyAction::ReapplyTitle: {
            GW::Chat::SendChat('/', L"title");
            break;
        }
        case HotkeyAction::EnterChallenge:
            GW::Chat::SendChat('/',L"enter");
            break;
    }
}

HotkeyTarget::HotkeyTarget(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    // don't print target hotkey to chat by default
    show_message_in_emote_channel = false;
    name[0] = 0;
    if (!ini)
        return;
    std::string ini_name = ini->GetValue(section, "TargetID", "");
    strcpy_s(id, ini_name.substr(0, sizeof(id) - 1).c_str());
    id[sizeof(id) - 1] = 0;
    long ini_type = ini->GetLongValue(section, "TargetType", -1);
    if (ini_type >= HotkeyTargetType::NPC && ini_type < HotkeyTargetType::Count)
        type = static_cast<HotkeyTargetType>(ini_type);
    ini_name = ini->GetValue(section, "TargetName", "");
    strcpy_s(name, ini_name.substr(0, sizeof(name)-1).c_str());
    name[sizeof(name)-1] = 0;

    ini->GetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                        show_message_in_emote_channel);
}
void HotkeyTarget::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "TargetID", id);
    ini->SetLongValue(section, "TargetType", static_cast<long>(type));
    ini->SetValue(section, "TargetName", name);
}
int HotkeyTarget::Description(char *buf, size_t bufsz)
{
    if (!name[0])
        return snprintf(buf, bufsz, "Target %s %s", type_labels[type], id);
    return snprintf(buf, bufsz, "Target %s", name);
}
bool HotkeyTarget::Draw()
{
    const float w = ImGui::GetContentRegionAvail().x / 1.5f;
    ImGui::PushItemWidth(w);
    bool hotkey_changed = ImGui::Combo("Target Type", (int *)&type, type_labels, 3);
    hotkey_changed |= ImGui::InputText(identifier_labels[type], id, _countof(id));
    ImGui::PopItemWidth();
    ImGui::ShowHelp("See Settings > Help > Chat Commands for /target options");
    ImGui::PushItemWidth(w);
    hotkey_changed |= ImGui::InputText("Hotkey label", name, _countof(name));
    ImGui::PopItemWidth();
    ImGui::SameLine(0,0);    ImGui::TextDisabled(" (optional)");
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}
void HotkeyTarget::Execute()
{
    if (!CanUse())
        return;

    constexpr size_t len = 122;
    wchar_t* message = new wchar_t[len];
    message[0] = 0;
    switch (type) {
    case HotkeyTargetType::Item:
        swprintf(message, len, L"target item %S", id);
        break;
    case HotkeyTargetType::NPC:
        swprintf(message, len, L"target npc %S", id);
        break;
    case HotkeyTargetType::Signpost:
        swprintf(message, len, L"target gadget %S", id);
        break;
    default:
        Log::ErrorW(L"Unknown target type %d", type);
        delete[] message;
        return;
    }
    GW::GameThread::Enqueue([message]() {
        GW::Chat::SendChat('/', message);
        delete[] message;
    });

    if (show_message_in_emote_channel) {
        char buf[256];
        Description(buf, 256);
        Log::Info("Triggered %s", buf);
    }
}

HotkeyMove::HotkeyMove(ToolboxIni *ini, const char *section)
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
void HotkeyMove::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "type", static_cast<long>(type));
    ini->SetDoubleValue(section, "x", x);
    ini->SetDoubleValue(section, "y", y);
    ini->SetDoubleValue(section, "distance", range);
    ini->SetValue(section, "name", name);
    ini->SetDoubleValue(section, "distance_from_location", distance_from_location);
}
int HotkeyMove::Description(char *buf, size_t bufsz)
{
    if (name[0])
        return snprintf(buf, bufsz, "Move to %s", name);
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
    if (!CanUse())
        return;
    const auto me = GW::Agents::GetPlayer();
    GW::Vec2f location(x, y);
    if (type == MoveType::Target) {
        const auto target = GW::Agents::GetTarget();
        if (!target) return;
        location = target->pos;
    }

    const auto dist = GW::GetDistance(me->pos, location);
    if (range != 0 && dist > range)
        return;
    if (distance_from_location > 0.f) {
        const auto direction = location - me->pos;
        const auto unit = Normalize(direction);
        location = location - unit * distance_from_location;
    }
    GW::Agents::Move(location.x, location.y);
    if (name[0] == '\0') {
        if (show_message_in_emote_channel)
            Log::Info("Moving to (%.0f, %.0f)", x, y);
    } else {
        if (show_message_in_emote_channel)
            Log::Info("Moving to %s", name);
    }
}

HotkeyDialog::HotkeyDialog(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    id = static_cast<size_t>(ini->GetLongValue(section, "DialogID", 0));
    strcpy_s(name, ini->GetValue(section, "DialogName", ""));
}
void HotkeyDialog::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "DialogID", id);
    ini->SetValue(section, "DialogName", name);
}
int HotkeyDialog::Description(char *buf, size_t bufsz)
{
    if (!name[0])
        return snprintf(buf, bufsz, "Dialog #%zu", id);
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
    if (!CanUse())
        return;
    char buf[32];
    if (id == 0) {
        snprintf(buf, _countof(buf), "dialog take");
    }
    else {
        snprintf(buf, _countof(buf), "dialog 0x%X", id);
    }

    GW::Chat::SendChat('/', buf);
    if (show_message_in_emote_channel)
        Log::Info("Sent dialog %s (%d)", name, id);
}

bool HotkeyPingBuild::GetText(void *, int idx, const char **out_text)
{
    if (idx >= static_cast<int>(BuildsWindow::Instance().BuildCount()))
        return false;
    *out_text = BuildsWindow::Instance().BuildName(static_cast<size_t>(idx));
    return true;
}
HotkeyPingBuild::HotkeyPingBuild(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    index = static_cast<size_t>(ini->GetLongValue(section, "BuildIndex", 0));
}
void HotkeyPingBuild::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "BuildIndex", static_cast<long>(index));
}
int HotkeyPingBuild::Description(char *buf, size_t bufsz)
{
    const char *buildname = BuildsWindow::Instance().BuildName(index);
    if (buildname == nullptr)
        buildname = "<not found>";
    return snprintf(buf, bufsz, "Ping build '%s'", buildname);
}
bool HotkeyPingBuild::Draw()
{
    bool hotkey_changed = false;
    int icount = static_cast<int>(BuildsWindow::Instance().BuildCount());
    int iindex = static_cast<int>(index);
    if (ImGui::Combo("Build", &iindex, GetText, nullptr, icount)) {
        if (0 <= iindex)
            index = static_cast<size_t>(iindex);
        hotkey_changed = true;
    }
    return hotkey_changed;
}
void HotkeyPingBuild::Execute()
{
    if (!CanUse())
        return;

    BuildsWindow::Instance().Send(index);
}

bool HotkeyHeroTeamBuild::GetText(void *, int idx, const char **out_text)
{
    if (idx >= static_cast<int>(HeroBuildsWindow::Instance().BuildCount()))
        return false;
    size_t index = static_cast<size_t>(idx);
    *out_text = HeroBuildsWindow::Instance().BuildName(index);
    return true;
}
HotkeyHeroTeamBuild::HotkeyHeroTeamBuild(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    index = static_cast<size_t>(ini->GetLongValue(section, "BuildIndex", 0));
}
void HotkeyHeroTeamBuild::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "BuildIndex", static_cast<long>(index));
}
int HotkeyHeroTeamBuild::Description(char *buf, size_t bufsz)
{
    const char *buildname = HeroBuildsWindow::Instance().BuildName(index);
    if (buildname == nullptr)
        buildname = "<not found>";
    return snprintf(buf, bufsz, "Load Hero Team Build '%s'", buildname);
}
bool HotkeyHeroTeamBuild::Draw()
{
    bool hotkey_changed = false;
    int icount = static_cast<int>(HeroBuildsWindow::Instance().BuildCount());
    int iindex = static_cast<int>(index);
    if (ImGui::Combo("Build", &iindex, GetText, nullptr, icount)) {
        if (0 <= iindex)
            index = static_cast<size_t>(iindex);
        hotkey_changed = true;
    }
    return hotkey_changed;
}
void HotkeyHeroTeamBuild::Execute()
{
    if (!CanUse())
        return;
    HeroBuildsWindow::Instance().Load(index);
}

HotkeyFlagHero::HotkeyFlagHero(ToolboxIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    degree = static_cast<float>(ini->GetDoubleValue(section, "degree", degree));
    distance = static_cast<float>(ini->GetDoubleValue(section, "distance", distance));
    hero = ini->GetLongValue(section, "hero", hero);
    if (hero < 0) hero = 0;
    if (hero > 11) hero = 11;
}
void HotkeyFlagHero::Save(ToolboxIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetDoubleValue(section, "degree", degree);
    ini->SetDoubleValue(section, "distance", distance);
    ini->SetLongValue(section, "hero", hero);
}
int HotkeyFlagHero::Description(char *buf, size_t bufsz)
{
    if (hero == 0)
        return snprintf(buf, bufsz, "Flag All Heroes");
    return snprintf(buf, bufsz, "Flag Hero %d", hero);
}
bool HotkeyFlagHero::Draw()
{
    bool hotkey_changed = false;
    hotkey_changed |= ImGui::DragFloat("Degree", &degree, 0.0f, -360.0f, 360.f);
    hotkey_changed |= ImGui::DragFloat("Distance", &distance, 0.0f, 0.0f, 10'000.f);
    if (hotkeys_changed && distance < 0.f)
        distance = 0.f;
    hotkey_changed |= ImGui::InputInt("Hero", &hero, 1);
    if (hotkey_changed && hero < 0)
        hero = 0;
    else if (hotkey_changed && hero > 11)
        hero = 11;
    ImGui::ShowHelp("The hero number that should be flagged (1-11).\nUse 0 to flag all");
    ImGui::Text("For a minimap flagging hotkey, please create a chat hotkey with:");
    ImGui::TextColored({1.f, 1.f, 0.f, 1.f}, "/flag %d toggle", hero);
    return hotkey_changed;
}
void HotkeyFlagHero::Execute()
{
    if (!isExplorable())
        return;

    const GW::Vec3f allflag = GW::GetGameContext()->world->all_flag;

    if (hero < 0)
        return;
    if (hero == 0) {
        if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
            GW::PartyMgr::UnflagAll();
            return;
        }
    } else {
        const GW::HeroFlagArray &flags = GW::GetGameContext()->world->hero_flags;
        if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size())
            return;

        const GW::HeroFlag &flag = flags[hero - 1];
        if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
            GW::PartyMgr::UnflagHero(hero);
            return;
        }
    }

    const GW::AgentLiving *player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player)
        return;
    const GW::AgentLiving *target = GW::Agents::GetTargetAsAgentLiving();

    float reference_radiant = player->rotation_angle;

    if (target && target != player) {
        float dx = target->x - player->x;
        float dy = target->y - player->y;

        reference_radiant = std::atan(dx == 0 ? dy : dy / dx);
        if (dx < 0) {
            reference_radiant += DirectX::XM_PI;
        } else if (dx > 0 && dy < 0) {
            reference_radiant += 2 * DirectX::XM_PI;
        }
    }

    const float radiant = degree * DirectX::XM_PI / 180.f;
    const float x = player->x + distance * std::cos(reference_radiant - radiant);
    const float y = player->y + distance * std::sin(reference_radiant - radiant);

    GW::GamePos pos = GW::GamePos(x, y, 0);

    if (hero == 0) {
        GW::PartyMgr::FlagAll(pos);
    } else {
        GW::PartyMgr::FlagHero(hero, pos);
    }
}

HotkeyGWKey::HotkeyGWKey(ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    can_trigger_on_map_change = trigger_on_explorable = trigger_on_outpost = false;
    action = static_cast<GW::UI::ControlAction>(ini->GetLongValue(section, "ActionID", (long)action));
    const auto found = std::ranges::find_if(control_labels, [&](std::pair<GW::UI::ControlAction, GuiUtils::EncString*> in) {
        return action == in.first;
        });
    if (found != control_labels.end()) {
        action_idx = std::distance(control_labels.begin(), found);
    }
}
void HotkeyGWKey::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ActionID", (long)action);
}
int HotkeyGWKey::Description(char* buf, size_t bufsz)
{
    if (action_idx < 0 || action_idx >= static_cast<int>(control_labels.size())) {
        return snprintf(buf, bufsz, "Guild Wars Key: Unknown Action %d", action_idx);
    }
    return snprintf(buf, bufsz, "Guild Wars Key: %s", control_labels[action_idx].second->string().c_str());
}
bool HotkeyGWKey::Draw()
{
    if (labels.empty()) {
        bool waiting = false;
        for (const auto& it : control_labels) {
            if (it.second->string().empty()) {
                waiting = true;
                break;
            }
            labels.push_back(it.second->string().c_str());
        }
        if (waiting) {
            labels.clear();
        }
    }
    if (labels.empty()) {
        ImGui::Text("Waiting on endoded strings");
        return false;
    }

    if (ImGui::Combo("Action###combo", (int*)&action_idx, labels.data(), labels.size(), 10)) {
        action = control_labels[action_idx].first;
        return true;
    }
    return false;
}
void HotkeyGWKey::Execute()
{
    GW::GameThread::Enqueue([&]() {
        GW::UI::Keypress(action);
        });
}

namespace {

    const char* GetBehaviorDesc(GW::HeroBehavior behaviour) {
        if ((uint32_t)behaviour < _countof(behaviors))
            return behaviors[(uint32_t)behaviour];
        return nullptr;
    }
}

HotkeyCommandPet::HotkeyCommandPet(ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    behavior = ini ? (GW::HeroBehavior)ini->GetLongValue(section, "behavior", (long)behavior) : behavior;
    if (!GetBehaviorDesc(behavior))
        behavior = GW::HeroBehavior::Fight;
}
void HotkeyCommandPet::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "behavior", (long)behavior);
}
int HotkeyCommandPet::Description(char* buf, size_t bufsz)
{
    return snprintf(buf, bufsz, "Command Pet: %s", GetBehaviorDesc(behavior));
}
bool HotkeyCommandPet::Draw()
{
    bool changed = ImGui::Combo("Behavior###combo", (int*)&behavior, behaviors, _countof(behaviors), _countof(behaviors));
    if(changed && !GetBehaviorDesc(behavior))
        behavior = GW::HeroBehavior::Fight;
    return changed;
}
void HotkeyCommandPet::Execute()
{
    GW::GameThread::Enqueue([&]() {
        GW::PartyMgr::SetPetBehavior(behavior);
        });
}
