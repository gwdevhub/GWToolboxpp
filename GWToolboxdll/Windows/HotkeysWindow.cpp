#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <Utils/GuiUtils.h>
#include <Keys.h>

#include <Windows/HotkeysWindow.h>
#include <GWCA/Utilities/Scanner.h>
#include <Timer.h>
#include <GWToolbox.h>
#include <Utils/TextUtils.h>
#include <Modules/Resources.h>

namespace {
    typedef std::bitset<256> KeysHeldBitset;

    std::vector<TBHotkey*> valid_hotkeys;

    KeysHeldBitset keys_currently_held;
    KeysHeldBitset wndproc_keys_held;


    bool clickerActive = false;   // clicker is active or not
    bool dropCoinsActive = false; // coin dropper is active or not
    bool map_change_triggered = false;

    clock_t clickerTimer = 0;   // timer for clicker
    clock_t dropCoinsTimer = 0; // timer for coin dropper

    TBHotkey* current_hotkey = nullptr;

    std::queue<TBHotkey*> pending_hotkeys;
    std::recursive_mutex pending_mutex;
    void PushPendingHotkey(TBHotkey* hk) {
        pending_mutex.lock();
        pending_hotkeys.push(hk);
        pending_mutex.unlock();
    }
    TBHotkey* PopPendingHotkey() {
        TBHotkey* hk = nullptr;
        pending_mutex.lock();
        if (pending_hotkeys.size()) {
            hk = pending_hotkeys.front();
            pending_hotkeys.pop();
        }
        pending_mutex.unlock();
        return hk;
    }

    bool loaded_action_labels = false;
    // NB: GetActionLabel_Func() must be called when we're in-game, because it relies on other gw modules being loaded internally.
    // Because we only draw this module when we're in-game, we just need to call this from the Draw() loop instead of on Initialise()
    void LoadActionLabels()
    {
        if (loaded_action_labels) {
            return;
        }
        loaded_action_labels = true;

        using GetActionLabel_pt = wchar_t*(__cdecl*)(GW::UI::ControlAction action);
        const auto GetActionLabel_Func = reinterpret_cast<GetActionLabel_pt>(GW::Scanner::Find("\x83\xfe\x5b\x74\x27\x83\xfe\x5c\x74\x22\x83\xfe\x5d\x74\x1d", "xxxxxxxxxxxxxxx", -0x7));
        GWCA_INFO("[SCAN] GetActionLabel_Func = %p\n", reinterpret_cast<void*>(GetActionLabel_Func));
        if (!GetActionLabel_Func) {
            return;
        }
        HotkeyGWKey::control_labels.clear();
        for (size_t i = 0x80; i < 0x12a; i++) {
            HotkeyGWKey::control_labels.push_back({ (GW::UI::ControlAction)i, nullptr });
        }
        for (auto& [action, label] : HotkeyGWKey::control_labels) {
            label = std::make_unique<GuiUtils::EncString>(GetActionLabel_Func(action));
        }
    }

    bool IsFrameCreated(GW::UI::Frame* frame) {
        return frame && frame->IsCreated();
    }
    
    
    bool IsPlayerEquipmentReady()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        if(!(player && player->equip && *player->equip))
            return false;
        const auto equip = *(GW::PlayerEquipment**)player->equip;
        return equip->equipment_flags == 0;
    }
    bool IsMapReady()
    {
        return GW::Map::GetIsMapLoaded() 
            && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading 
            && !GW::Map::GetIsObserving() && IsPlayerEquipmentReady()
            && IsFrameCreated(GW::UI::GetFrameByLabel(L"Skillbar"));
    }

    void AddHotkeyIfValid(TBHotkey* hotkey, const char* player_name, GW::Constants::InstanceType instance_type, GW::Constants::Profession primary, GW::Constants::MapID map_id, bool is_pvp, std::vector<TBHotkey*>& valid)
    {
        if (const auto* grp = dynamic_cast<HotkeyGroup*>(hotkey)) {
            for (auto* child : grp->hotkeys) {
                AddHotkeyIfValid(child, player_name,instance_type,primary,map_id,is_pvp,valid);
            }
        }
        if (hotkey && hotkey->IsValid(player_name, instance_type, primary, map_id, is_pvp)) {
            valid.push_back(hotkey);
        }
    }

    // Repopulates applicable_hotkeys based on current character/map context.
    // Used because its not necessary to check these vars on every keystroke, only when they change
    bool CheckSetValidHotkeys()
    {
        const auto c = GW::GetCharContext();
        if (!c) {
            return false;
        }
        GW::Player* me = GW::PlayerMgr::GetPlayerByID(c->player_number);
        if (!me) {
            return false;
        }
        const std::string player_name = TextUtils::WStringToString(c->player_name);
        const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();
        const GW::Constants::MapID map_id = GW::Map::GetMapID();
        const auto primary = static_cast<GW::Constants::Profession>(me->primary);
        const bool is_pvp = me->IsPvP();
        valid_hotkeys.clear();
        for (auto* hotkey : TBHotkey::all_hotkeys) {
            AddHotkeyIfValid(hotkey, player_name.c_str(), instance_type, primary, map_id, is_pvp, valid_hotkeys);
        }

        return true;
    }

    bool OnMapChanged()
    {
        if (!IsMapReady()) {
            return false;
        }
        if (!GW::Agents::GetControlledCharacter()) {
            return false;
        }
        const GW::Constants::InstanceType mt = GW::Map::GetInstanceType();
        if (mt == GW::Constants::InstanceType::Loading) {
            return false;
        }
        if (!CheckSetValidHotkeys()) {
            return false;
        }
        bool is_in_controller_mode = GW::UI::IsInControllerMode();
        // NB: CheckSetValidHotkeys() has already checked validity of char/map etc
        for (TBHotkey* hk : valid_hotkeys) {
            if (((hk->trigger_on_explorable && mt == GW::Constants::InstanceType::Explorable)
                    || (hk->trigger_on_outpost && mt == GW::Constants::InstanceType::Outpost))
                && !hk->pressed 
                &&
                ((is_in_controller_mode && hk->trigger_in_controller_mode) || (!is_in_controller_mode && hk->trigger_in_desktop_mode))) {
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
    bool OnWindowActivated(const bool activated)
    {
        if (!IsMapReady()) {
            return false;
        }
        if (!GW::Agents::GetControlledCharacter()) {
            return false;
        }
        if (!CheckSetValidHotkeys()) {
            return false;
        }
        // NB: CheckSetValidHotkeys() has already checked validity of char/map etc
        for (TBHotkey* hk : valid_hotkeys) {
            if (((activated && hk->trigger_on_gain_focus)
                    || (!activated && hk->trigger_on_lose_focus))) {
                // Would be nice to use PushPendingHotkey here, but losing/gaining focus is a special case
                hk->pressed = true;
                current_hotkey = hk;
                hk->Execute();
                current_hotkey = nullptr;
                hk->pressed = false;
            }
        }
        return true;
    }

    inline void GetKeysHeld(KeysHeldBitset& keysHeld) {
        keysHeld.reset(); // Clear previous key states
        BYTE keyState[256];

        // Get the current keyboard state
        if (GetKeyboardState(keyState)) {
            for (uint32_t vkey = 0; vkey < 256; ++vkey) {
                // Check if the high-order bit is set (key is pressed)
                if (keyState[vkey] & 0x80) {
                    keysHeld.set(vkey); // Mark key as pressed
                }
            }
        }
    }

    TBHotkey* pending_being_assigned = nullptr;
    TBHotkey* keys_being_assigned = nullptr;
    KeysHeldBitset keys_selected;
    bool hotkey_popup_first_draw = true;
    void DrawSelectHotkeyPopup() {
        if (pending_being_assigned) {
            keys_being_assigned = pending_being_assigned;
            ImGui::OpenPopup("Select Hotkey");
            pending_being_assigned = nullptr;
            return;
        }
        if (!keys_being_assigned) {
            return;
        }
        if (!ImGui::BeginPopup("Select Hotkey")) {
            keys_selected.reset();
            hotkey_popup_first_draw = true;
            keys_being_assigned = nullptr;
            return;
        }
        if (hotkey_popup_first_draw) {
            keys_selected = keys_being_assigned->key_combo;
            hotkey_popup_first_draw = false;
        }

        // Record any new key presses
        keys_selected |= wndproc_keys_held;

        std::string keys_held_buf = ModKeyName(keys_selected);

        ImGui::TextUnformatted(keys_held_buf.c_str());
        if (ImGui::Button("Clear")) {
            keys_selected.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            keys_being_assigned->key_combo = keys_selected;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    size_t KeyDataFromWndProc(const UINT Message, const WPARAM wParam) {
        switch (Message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            return static_cast<size_t>(wParam);
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return VK_MBUTTON;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP: {
            WORD xButton = GET_XBUTTON_WPARAM(wParam);
            return (xButton == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
        }
        }
        return 0;
    }
}

void HotkeysWindow::Initialize()
{
    ToolboxWindow::Initialize();
    clickerTimer = TIMER_INIT();
    dropCoinsTimer = TIMER_INIT();
}

const TBHotkey* HotkeysWindow::CurrentHotkey()
{
    return current_hotkey;
}

void HotkeysWindow::Terminate()
{
    ToolboxWindow::Terminate();
    while (TBHotkey::all_hotkeys.size())
        delete TBHotkey::all_hotkeys[0];
    HotkeyGWKey::control_labels.clear();
}

bool HotkeysWindow::ToggleClicker() { return clickerActive = !clickerActive; }
bool HotkeysWindow::ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }

void HotkeysWindow::ChooseKeyCombo(TBHotkey* hotkey)
{
    pending_being_assigned = hotkey;
}

void HotkeysWindow::Draw(IDirect3DDevice9*)
{
    DrawSelectHotkeyPopup();

    if (!visible) {
        return;
    }
    LoadActionLabels();
    bool hotkeys_changed = false;
    // === hotkey panel ===
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (ImGui::Button("Create Hotkey...", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            ImGui::OpenPopup("Create Hotkey");
        }
        if (ImGui::BeginPopup("Create Hotkey")) {
            TBHotkey* new_hotkey = nullptr;
            if (ImGui::Selectable("Send Chat")) {
                new_hotkey = new HotkeySendChat(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Send a message or command to chat");
            }
            if (ImGui::Selectable("Use Item")) {
                new_hotkey = new HotkeyUseItem(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Use an item from your inventory");
            }
            if (ImGui::Selectable("Drop or Use Buff")) {
                new_hotkey = new HotkeyDropUseBuff(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Use or cancel a skill such as Recall or UA");
            }
            if (ImGui::Selectable("Toggle...")) {
                new_hotkey = new HotkeyToggle(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Toggle a GWToolbox++ functionality such as clicker");
            }
            if (ImGui::Selectable("Execute...")) {
                new_hotkey = new HotkeyAction(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Execute a single task such as opening chests\nor reapplying lightbringer title");
            }
            if (ImGui::Selectable("Guild Wars Key")) {
                new_hotkey = new HotkeyGWKey(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Trigger an in-game hotkey via toolbox");
            }
            if (ImGui::Selectable("Target")) {
                new_hotkey = new HotkeyTarget(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Target a game entity by its ID");
            }
            if (ImGui::Selectable("Move to")) {
                new_hotkey = new HotkeyMove(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move to a specific (x,y) coordinate");
            }
            if (ImGui::Selectable("Dialog")) {
                new_hotkey = new HotkeyDialog(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Send a Dialog");
            }
if (ImGui::Selectable("Equip Item")) {
                new_hotkey = new HotkeyEquipItem(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Equip an item from your inventory");
            }
            if (ImGui::Selectable("Flag Hero")) {
                new_hotkey = new HotkeyFlagHero(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Flag a hero relative to your position");
            }
            if (ImGui::Selectable("Command Pet")) {
                new_hotkey = new HotkeyCommandPet(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Change behavior of your pet");
            }
            ImGui::Separator();
            if (ImGui::Selectable("Hotkey Group")) {
                new_hotkey = new HotkeyGroup(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Create a named group to organise and reorder hotkeys");
            }
            ImGui::EndPopup();
            hotkeys_changed = new_hotkey != 0;
        }

        // === each hotkey / group ===
        // Groups are first-class items in `hotkeys`; HotkeyGroup::Draw handles its children.
        // All moves at the top level are simple swaps — no rotation needed.
        for (auto hotkey : TBHotkey::top_level_hotkeys) {
            if (hotkey->Draw()) break; // re-render next frame after list mutation
        }
    }
    if (hotkeys_changed) {
        TBHotkey::SortHotkeys();
        CheckSetValidHotkeys();
    }

    ImGui::End();
}

void HotkeysWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::Checkbox("Show 'Active' checkbox in header", &TBHotkey::show_active_in_header);
    ImGui::Checkbox("Show 'Run' button in header", &TBHotkey::show_run_in_header);
    ImGui::SliderInt("Autoclicker delay (ms)", &HotkeyToggle::clicker_delay_ms, 1, 1'000);
}

void HotkeysWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);

    TBHotkey::show_active_in_header = ini->GetBoolValue(Name(), "show_active_in_header", false);
    TBHotkey::show_run_in_header = ini->GetBoolValue(Name(), "show_run_in_header", false);
    HotkeyToggle::clicker_delay_ms = ini->GetLongValue(Name(), "clicker_delay_ms", HotkeyToggle::clicker_delay_ms);

    while (TBHotkey::all_hotkeys.size())
        delete TBHotkey::all_hotkeys[0];

    TNamesDepend entries;
    ini->GetAllSections(entries);

    for (const auto& entry : entries) {
        const char* sec = entry.pItem;
        TBHotkey::HotkeyFactory(ini, sec);
    }

    TBHotkey::SortHotkeys();
    CheckSetValidHotkeys();
}

void HotkeysWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), "show_active_in_header", TBHotkey::show_active_in_header);
    ini->SetBoolValue(Name(), "show_run_in_header", TBHotkey::show_run_in_header);
    ini->SetLongValue(Name(), "clicker_delay_ms", HotkeyToggle::clicker_delay_ms);

    // Delete all existing hotkey sections before re-saving.
    TNamesDepend sections;
    ini->GetAllSections(sections);
    for (const auto& s : sections) {
        if (strncmp(s.pItem, "hotkey-", 7) == 0)
            ini->Delete(s.pItem, nullptr);
    }

    // Save all hotkeys as flat sections. Every section gets a sort_order field
    // that determines display order on load. Children also get a group field
    // containing their parent group's label.
    int sec_idx = 0;
    char buf[256];
    for (auto hotkey : TBHotkey::all_hotkeys) {
        snprintf(buf, sizeof(buf), "hotkey-%04d:%s", sec_idx++, hotkey->Name());
        hotkey->Save(ini, buf);
    }
}

bool HotkeysWindow::WndProc(const UINT Message, const WPARAM wParam, LPARAM)
{
    if (Message == WM_LBUTTONUP && HotkeyToggle::processing) {
        HotkeyToggle::processing = false;
    }
    if (Message == WM_ACTIVATE) {
        wndproc_keys_held.reset();
        OnWindowActivated(LOWORD(wParam) != WA_INACTIVE);
        return false;
    }
    if (GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow() || GW::Chat::GetIsTyping()) {
        wndproc_keys_held.reset();
        return false;
    }
    auto check_trigger = [](TBHotkey* hk, bool is_key_up, uint32_t keyData, bool is_in_controller_mode) {
        if (hk->pressed) return false;
        if (hk->trigger_on_key_up != is_key_up) return false;
        if (!hk->key_combo.test(keyData)) return false; // The triggering key isn't included in this hotkey's combo
        if (hk->strict_key_combo) return hk->key_combo == wndproc_keys_held;
        if (is_in_controller_mode && !hk->trigger_in_controller_mode) return false;
        if (!is_in_controller_mode && !hk->trigger_in_desktop_mode) return false;
        return (hk->key_combo & wndproc_keys_held) == hk->key_combo;
    };

    size_t hotkeys_triggered = 0;

    auto check_triggers = [check_trigger, &hotkeys_triggered](bool is_key_up, uint32_t keyData) {
        std::vector<TBHotkey*> matching_hotkeys;
        size_t max_modifier_count = 0;

        bool is_in_controller_mode = GW::UI::IsInControllerMode();

        // Step 1: Find all hotkeys that match the currently pressed keys
        for (TBHotkey* hk : valid_hotkeys) {
            if (is_key_up) hk->pressed = false;

            // A hotkey is considered "matching" if:
            // - It hasn't already been triggered (`hk->pressed == false`)
            // - It should trigger on key-up (if we're processing a key-up event)
            // - All its required keys are currently held (`hk->key_combo & wndproc_keys_held == hk->key_combo`)
            // - The key that was just pressed/released is part of this hotkey (`hk->key_combo.test(keyData)`)
            if (check_trigger(hk, is_key_up, keyData, is_in_controller_mode)) {
                // Count how many keys (modifiers + main key) are required for this hotkey
                size_t modifier_count = hk->key_combo.count();
                matching_hotkeys.push_back(hk);

                // Track the highest number of required keys (most specific hotkey)
                max_modifier_count = std::max(max_modifier_count, modifier_count);
            }
        }

        bool triggered = false;

        // Step 2: Trigger only the most specific hotkeys
        for (TBHotkey* hk : matching_hotkeys) {
            if (hk->key_combo.count() == max_modifier_count) {
                PushPendingHotkey(hk);

                // If this hotkey is set to block Guild Wars input, mark it as triggered
                if (!is_key_up && hk->block_gw) {
                    triggered = true;
                }
                if (hk->block_other_hotkeys_on_trigger) break;
            }
        }

        return triggered; // If any hotkey blocked input, return true to prevent the key event from reaching GW
    };

    switch (Message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_XBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            const auto keyData = KeyDataFromWndProc(Message, wParam);
            if (!keyData || keyData >= wndproc_keys_held.size())
                return false;
            wndproc_keys_held.set(keyData);
            return keys_being_assigned || check_triggers(false, keyData);
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_MBUTTONUP:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_XBUTTONUP: {
            const auto keyData = KeyDataFromWndProc(Message, wParam);
            if (!keyData || keyData >= wndproc_keys_held.size())
                return false;
            if (!keys_being_assigned)
                check_triggers(true, keyData);
            wndproc_keys_held.reset(keyData);
            return keys_being_assigned;
        }
        default:
            return false;
    }
}

void HotkeysWindow::Update(const float)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        if (map_change_triggered) {
            map_change_triggered = false;
            while (PopPendingHotkey()) {} // Clear any pending hotkeys from the last map
            for (auto hk : TBHotkey::all_hotkeys) {
                hk->pressed = false;
            }
        }
        return;
    }
    if (!map_change_triggered) {
        map_change_triggered = OnMapChanged();
    }
    for (auto hotkey : TBHotkey::all_hotkeys) {
        if (hotkey->ongoing) hotkey->Execute();
    }
    while (const auto hk = PopPendingHotkey()) {
        hk->pressed = true;
        current_hotkey = hk;
        hk->Toggle();
        current_hotkey = nullptr;
    }
}
