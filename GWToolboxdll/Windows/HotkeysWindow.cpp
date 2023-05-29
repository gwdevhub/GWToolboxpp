#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <Utils/GuiUtils.h>
#include <Keys.h>

#include <Modules/Resources.h>
#include <Windows/HotkeysWindow.h>
#include <GWCA/Utilities/Scanner.h>


namespace {
    
    std::vector<TBHotkey*> hotkeys;             // list of hotkeys
    // Subset of hotkeys that are valid to current character/map combo
    std::vector<TBHotkey*> valid_hotkeys;

    // Ordered subsets
    enum class GroupBy : int {
        None,
        Profession,
        Map,
        PlayerName,
        Group
    };
    GroupBy group_by = GroupBy::Group;
    std::unordered_map<int, std::vector<TBHotkey*>> by_profession;
    std::unordered_map<int, std::vector<TBHotkey*>> by_map;
    std::unordered_map<int, std::vector<TBHotkey*>> by_instance_type;
    std::unordered_map<std::string, std::vector<TBHotkey*>> by_player_name;
    std::unordered_map<std::string, std::vector<TBHotkey*>> by_group;
    bool need_to_check_valid_hotkeys = true;
    

    long max_id_ = 0;
    bool block_hotkeys = false;

    bool clickerActive = false;             // clicker is active or not
    bool dropCoinsActive = false;           // coin dropper is active or not

    bool map_change_triggered = false;

    clock_t clickerTimer = 0;               // timer for clicker
    clock_t dropCoinsTimer = 0;             // timer for coin dropper

    float movementX = 0;                    // X coordinate of the destination of movement macro
    float movementY = 0;                    // Y coordinate of the destination of movement macro

    TBHotkey* current_hotkey = nullptr;
    bool is_window_active = true;

    bool loaded_action_labels = false;
    // NB: GetActionLabel_Func() must be called when we're in-game, because it relies on other gw modules being loaded internally.
    // Because we only draw this module when we're in-game, we just need to call this from the Draw() loop instead of on Initialise()
    void LoadActionLabels() {
        if (loaded_action_labels)
            return;
        loaded_action_labels = true;

        typedef wchar_t*(__cdecl* GetActionLabel_pt)(GW::UI::ControlAction action);
        GetActionLabel_pt GetActionLabel_Func = 0;
        GetActionLabel_Func = (GetActionLabel_pt)GW::Scanner::Find("\x83\xfe\x5b\x74\x27\x83\xfe\x5c\x74\x22\x83\xfe\x5d\x74\x1d", "xxxxxxxxxxxxxxx", -0x7);
        GWCA_INFO("[SCAN] GetActionLabel_Func = %p\n", (void*)GetActionLabel_Func);
        if (!GetActionLabel_Func)
            return;
        for (auto& label : HotkeyGWKey::control_labels) {
            label.second = new GuiUtils::EncString(GetActionLabel_Func(label.first));
        }
    }
    bool IsMapReady()
    {
        return GW::Map::GetIsMapLoaded() && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving();
    }
    // @Cleanup: Find the address for this without checking map garbage - how does inv window know to show "pvp equipment" button?
    bool IsPvPCharacter() {
        // This is the 3rd module that uses this function, should really be part of GWCA but leave here for now.
        return !(GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Ascalon_City_outpost)
            || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Shing_Jea_Monastery_outpost)
            || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost)
            || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Ascalon_City_pre_searing));
    }
    // Repopulates applicable_hotkeys based on current character/map context.
    // Used because its not necessary to check these vars on every keystroke, only when they change
    bool CheckSetValidHotkeys() {
        auto c = GW::GetCharContext();
        if (!c) return false;
        GW::Player* me = GW::PlayerMgr::GetPlayerByID(c->player_number);
        if (!me) return false;
        std::string player_name = GuiUtils::WStringToString(c->player_name);
        GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();
        GW::Constants::MapID map_id = GW::Map::GetMapID();
        GW::Constants::Profession primary = (GW::Constants::Profession)me->primary;
        bool is_pvp = IsPvPCharacter();
        valid_hotkeys.clear();
        by_profession.clear();
        by_map.clear();
        by_instance_type.clear();
        by_player_name.clear();
        by_group.clear();
        for (auto* hotkey : hotkeys) {
            if (hotkey->IsValid(player_name.c_str(), instance_type, primary, map_id, is_pvp))
                valid_hotkeys.push_back(hotkey);

            for (size_t i = 0; i < _countof(hotkey->prof_ids);i++) {
                if (!hotkey->prof_ids[i])
                    continue;
                if (!by_profession.contains(i))
                    by_profession[i] = std::vector<TBHotkey*>();
                by_profession[i].push_back(hotkey);
            }
            for (auto h_map_id : hotkey->map_ids) {
                if (!by_map.contains(h_map_id))
                    by_map[h_map_id] = std::vector<TBHotkey*>();
                by_map[h_map_id].push_back(hotkey);
            }

            if (!by_instance_type.contains(hotkey->instance_type))
                by_instance_type[hotkey->instance_type] = std::vector<TBHotkey*>();
            by_instance_type[hotkey->instance_type].push_back(hotkey);
            if (!by_player_name.contains(hotkey->player_name))
                by_player_name[hotkey->player_name] = std::vector<TBHotkey*>();
            by_player_name[hotkey->player_name].push_back(hotkey);
            if (!by_group.contains(hotkey->group))
                by_group[hotkey->group] = std::vector<TBHotkey*>();
            by_group[hotkey->group].push_back(hotkey);
        }

        return true;
    }
    bool OnMapChanged() {
        if (!IsMapReady())
            return false;
        if (!GW::Agents::GetPlayerAsAgentLiving())
            return false;
        GW::Constants::InstanceType mt = GW::Map::GetInstanceType();
        if (mt == GW::Constants::InstanceType::Loading)
            return false;
        if (!CheckSetValidHotkeys())
            return false;
        // NB: CheckSetValidHotkeys() has already checked validity of char/map etc
        for (TBHotkey* hk : valid_hotkeys) {
            if (!block_hotkeys
                && ((hk->trigger_on_explorable && mt == GW::Constants::InstanceType::Explorable)
                    || (hk->trigger_on_outpost && mt == GW::Constants::InstanceType::Outpost))
                && !hk->pressed) {

                hk->pressed = true;
                current_hotkey = hk;
                hk->Execute();
                current_hotkey = nullptr;
                hk->pressed = false;
            }
        }
        return true;
    }
    
    // Called in Update loop after WM_ACTIVATE has been received via WndProc
    bool OnWindowActivated(bool activated) {
        if (!IsMapReady())
            return false;
        if (!GW::Agents::GetPlayerAsAgentLiving())
            return false;
        if (!CheckSetValidHotkeys())
            return false;
        // NB: CheckSetValidHotkeys() has already checked validity of char/map etc
        for (TBHotkey* hk : valid_hotkeys) {
            if (!block_hotkeys && !hk->pressed 
                && ((activated && hk->trigger_on_gain_focus)
                || (!activated && hk->trigger_on_lose_focus))) {
                hk->pressed = true;
                current_hotkey = hk;
                hk->Execute();
                current_hotkey = nullptr;
                hk->pressed = false;
            }
        }
        return true;
    }
}



void HotkeysWindow::Initialize() {
    ToolboxWindow::Initialize();
    clickerTimer = TIMER_INIT();
    dropCoinsTimer = TIMER_INIT();
}

const TBHotkey* HotkeysWindow::CurrentHotkey() {
    return current_hotkey;
}

void HotkeysWindow::Terminate() {
    ToolboxWindow::Terminate();
    for (TBHotkey* hotkey : hotkeys) {
        delete hotkey;
    }
    hotkeys.clear();
    for (auto& it : HotkeyGWKey::control_labels) {
        if (it.second) delete it.second;
        it.second = nullptr;
    }
}

bool HotkeysWindow::ToggleClicker() { return clickerActive = !clickerActive; }
bool HotkeysWindow::ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }

void HotkeysWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    LoadActionLabels();
    bool hotkeys_changed = false;
    // === hotkey panel ===
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
    // Group By is proof of concept, but isn't really practical for use yet (move up/down, player name change boots you out of section etc)
#if 0
        ImGui::Combo("Group By", (int*)&group_by, "None\0Profession\0Map\0Player Name");
#endif
        if (ImGui::Button("Create Hotkey...", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            ImGui::OpenPopup("Create Hotkey");
        }
        if (ImGui::BeginPopup("Create Hotkey")) {
            TBHotkey* new_hotkey = 0;
            if (ImGui::Selectable("Send Chat")) {
                new_hotkey = new HotkeySendChat(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send a message or command to chat");
            if (ImGui::Selectable("Use Item")) {
                new_hotkey = new HotkeyUseItem(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use an item from your inventory");
            if (ImGui::Selectable("Drop or Use Buff")) {
                new_hotkey = new HotkeyDropUseBuff(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use or cancel a skill such as Recall or UA");
            if (ImGui::Selectable("Toggle...")) {
                new_hotkey = new HotkeyToggle(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle a GWToolbox++ functionality such as clicker");
            if (ImGui::Selectable("Execute...")) {
                new_hotkey = new HotkeyAction(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execute a single task such as opening chests\nor reapplying lightbringer title");
            if (ImGui::Selectable("Guild Wars Key")) {
                new_hotkey = new HotkeyGWKey(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Trigger an in-game hotkey via toolbox");
            if (ImGui::Selectable("Target")) {
                new_hotkey = new HotkeyTarget(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Target a game entity by its ID");
            if (ImGui::Selectable("Move to")) {
                new_hotkey = new HotkeyMove(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move to a specific (x,y) coordinate");
            if (ImGui::Selectable("Dialog")) {
                new_hotkey = new HotkeyDialog(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send a Dialog");
            if (ImGui::Selectable("Ping Build")) {
                new_hotkey = new HotkeyPingBuild(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ping a build from the Build Panel");
            if (ImGui::Selectable("Load Hero Team Build")) {
                new_hotkey = new HotkeyHeroTeamBuild(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load a team hero build from the Hero Build Panel");
            if (ImGui::Selectable("Equip Item")) {
                new_hotkey = new HotkeyEquipItem(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Equip an item from your inventory");
            if (ImGui::Selectable("Flag Hero")) {
                new_hotkey = new HotkeyFlagHero(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Flag a hero relative to your position");
            if (ImGui::Selectable("Command Pet")) {
                new_hotkey = new HotkeyCommandPet(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Change behavior of your pet");
            ImGui::EndPopup();
            if (new_hotkey) {
                hotkeys.push_back(new_hotkey);
                hotkeys_changed = true;
            }
        }

        // === each hotkey ===
        block_hotkeys = false;
        auto draw_hotkeys_vec = [&](std::vector<TBHotkey*>& in) {
            bool these_hotkeys_changed = false;
            for (unsigned int i = 0; i < in.size(); ++i) {
                TBHotkey::Op op = TBHotkey::Op_None;
                these_hotkeys_changed |= in[i]->Draw(&op);
                switch (op) {
                case TBHotkey::Op_None: break;
                case TBHotkey::Op_MoveUp: {
                    auto it = std::ranges::find(hotkeys, in[i]);
                    if (it != hotkeys.end() && it != hotkeys.begin()) {
                        std::swap(*it, *(it - 1));
                        these_hotkeys_changed = true;
                        return true;
                    }
                } break;
                case TBHotkey::Op_MoveDown: {
                    auto it = std::ranges::find(hotkeys, in[i]);
                    if (it != hotkeys.end() && it != hotkeys.end() - 1) {
                        std::swap(*it, *(it + 1));
                        these_hotkeys_changed = true;
                        return true;
                    }
                } break;
                case TBHotkey::Op_Delete: {
                    auto it = std::ranges::find(hotkeys, in[i]);
                    if (it != hotkeys.end()) {
                        hotkeys.erase(it);
                        delete in[i];
                        return true;
                    }

                } break;
                case TBHotkey::Op_BlockInput:
                    block_hotkeys = true;
                    break;
                default:
                    break;
                }
            }
            return these_hotkeys_changed;
        };
        switch (group_by) {
        case GroupBy::Group:
            for (auto& it : by_group) {
                if (it.first == "") {
                    // No collapsing header for hotkeys without a group.
                    if (draw_hotkeys_vec(it.second)) {
                        hotkeys_changed = true;
                        break;
                    }
                }
                else if (ImGui::CollapsingHeader(it.first.c_str())) {
                    ImGui::Indent();
                    if (draw_hotkeys_vec(it.second)) {
                        hotkeys_changed = true;
                        ImGui::Unindent();
                        break;
                    }
                    ImGui::Unindent();
                }
            }
            break;
        case GroupBy::Profession:
                for (auto& it : by_profession) {
                    if (ImGui::CollapsingHeader(TBHotkey::professions[it.first])) {
                        ImGui::Indent();
                        if (draw_hotkeys_vec(it.second)) {
                            hotkeys_changed = true;
                            ImGui::Unindent();
                            break;
                        }
                        ImGui::Unindent();
                    }
                }
                break;
            case GroupBy::Map: {
                const char* map_name;
                for (auto& it : by_map) {
                    if (it.first == 0)
                        map_name = "Any";
                    else if (it.first >= 0 && it.first < _countof(GW::Constants::NAME_FROM_ID))
                        map_name = GW::Constants::NAME_FROM_ID[it.first];
                    else
                        map_name = "Unknown";
                    if (ImGui::CollapsingHeader(map_name)) {
                        ImGui::Indent();
                        if (draw_hotkeys_vec(it.second)) {
                            hotkeys_changed = true;
                            ImGui::Unindent();
                            break;
                        }
                        ImGui::Unindent();
                    }
                }
            } break;
            case GroupBy::PlayerName: {
                const char* player_name;
                for (auto& it : by_player_name) {
                    if (it.first.empty())
                        player_name = "Any";
                    else
                        player_name = it.first.c_str();
                    if (ImGui::CollapsingHeader(player_name)) {
                        ImGui::Indent();
                        if (draw_hotkeys_vec(it.second)) {
                            hotkeys_changed = true;
                            ImGui::Unindent();
                            break;
                        }
                        ImGui::Unindent();
                    }
                }
            } break;
            default:
                hotkeys_changed |= draw_hotkeys_vec(hotkeys);
                break;


        }

    }
    if (hotkeys_changed) {
        CheckSetValidHotkeys();
        TBHotkey::hotkeys_changed = true;
    }

    ImGui::End();
}

void HotkeysWindow::DrawSettingInternal() {
    ToolboxWindow::DrawSettingInternal();
    ImGui::Checkbox("Show 'Active' checkbox in header", &TBHotkey::show_active_in_header);
    ImGui::Checkbox("Show 'Run' button in header", &TBHotkey::show_run_in_header);
}

void HotkeysWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

    TBHotkey::show_active_in_header = ini->GetBoolValue(Name(), "show_active_in_header", false);
    TBHotkey::show_run_in_header = ini->GetBoolValue(Name(), "show_run_in_header", false);

    // clear hotkeys from toolbox
    for (TBHotkey* hotkey : hotkeys) {
        delete hotkey;
    }
    hotkeys.clear();

    // then load again
    ToolboxIni::TNamesDepend entries;
    ini->GetAllSections(entries);
    for (ToolboxIni::Entry& entry : entries) {
        TBHotkey* hk = TBHotkey::HotkeyFactory(ini, entry.pItem);
        if (hk) hotkeys.push_back(hk);
    }
    CheckSetValidHotkeys();
    TBHotkey::hotkeys_changed = false;
}
void HotkeysWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), "show_active_in_header", TBHotkey::show_active_in_header);
    ini->SetBoolValue(Name(), "show_run_in_header", TBHotkey::show_run_in_header);

    if (TBHotkey::hotkeys_changed) {
        // clear hotkeys from ini
        ToolboxIni::TNamesDepend entries;
        ini->GetAllSections(entries);
        for (ToolboxIni::Entry& entry : entries) {
            if (strncmp(entry.pItem, "hotkey-", 7) == 0) {
                ini->Delete(entry.pItem, nullptr);
            }
        }

        // then save again
        char buf[256];
        for (unsigned int i = 0; i < hotkeys.size(); ++i) {
            snprintf(buf, 256, "hotkey-%03d:%s", i, hotkeys[i]->Name());
            hotkeys[i]->Save(ini, buf);
        }
    }
}

bool HotkeysWindow::WndProc(UINT Message, WPARAM wParam, LPARAM) {
    if (Message == WM_ACTIVATE) {
        OnWindowActivated(wParam != WA_INACTIVE);
        return false;
    }
    if (GW::Chat::GetIsTyping())
        return false;
    if (GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow())
        return false;
    long keyData = 0;
    switch (Message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        keyData = static_cast<int>(wParam);
        break;
    case WM_XBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (LOWORD(wParam) & MK_MBUTTON) keyData = VK_MBUTTON;
        if (LOWORD(wParam) & MK_XBUTTON1) keyData = VK_XBUTTON1;
        if (LOWORD(wParam) & MK_XBUTTON2) keyData = VK_XBUTTON2;
        break;
    case WM_XBUTTONUP:
    case WM_MBUTTONUP:
        // leave keydata to none, need to handle special case below
        break;
    default:
        break;
    }

    switch (Message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_XBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        if (block_hotkeys)
            return true;
        long modifier = 0;
        if (GetKeyState(VK_CONTROL) < 0)
            modifier |= ModKey_Control;
        if (GetKeyState(VK_SHIFT) < 0)
            modifier |= ModKey_Shift;
        if (GetKeyState(VK_MENU) < 0)
            modifier |= ModKey_Alt;

        bool triggered = false;
        for (TBHotkey* hk : valid_hotkeys) {
            if (!block_hotkeys
                && !hk->pressed
                && keyData == hk->hotkey
                && modifier == hk->modifier) {

                hk->pressed = true;
                current_hotkey = hk;
                hk->Toggle();
                current_hotkey = nullptr;
                if (hk->block_gw)
                    triggered = true;
            }
        }
        return triggered;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
        for (TBHotkey* hk : hotkeys) {
            if (hk->pressed && keyData == hk->hotkey) {
                hk->pressed = false;
            }
        }
        return false;

    case WM_XBUTTONUP:
        for (TBHotkey* hk : hotkeys) {
            if (hk->pressed && (hk->hotkey == VK_XBUTTON1 || hk->hotkey == VK_XBUTTON2)) {
                hk->pressed = false;
            }
        }
        return false;
    case WM_MBUTTONUP:
        for (TBHotkey* hk : hotkeys) {
            if (hk->pressed && hk->hotkey == VK_MBUTTON) {
                hk->pressed = false;
            }
        }
    default:
        return false;
    }
}

void HotkeysWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        if (map_change_triggered)
            map_change_triggered = false;
        return;
    }
    if (!map_change_triggered)
        map_change_triggered = OnMapChanged();

    for (unsigned int i = 0; i < hotkeys.size(); ++i) {
        if (hotkeys[i]->ongoing)
            hotkeys[i]->Execute();
    }
}
