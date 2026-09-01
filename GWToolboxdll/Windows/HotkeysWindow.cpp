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
#include <Defines.h>
#include <Keys.h>

#include <Windows/HotkeysWindow.h>
#include <GWCA/Utilities/Scanner.h>
#include <Timer.h>
#include <GWToolbox.h>
#include <Utils/TextUtils.h>
#include <Modules/Resources.h>

namespace {
    constexpr char hotkey_section_prefix[] = "hotkey-";
    constexpr auto HotkeysIniFilename = L"Hotkeys.ini";

    typedef std::bitset<256> KeysHeldBitset;

    std::vector<TBHotkey*> valid_hotkeys;

    KeysHeldBitset keys_currently_held;
    KeysHeldBitset wndproc_keys_held;


    bool clickerActive = false;   // 点击器是否激活
    bool dropCoinsActive = false; // 金币投掷器是否激活
    bool map_change_triggered = false;

    clock_t clickerTimer = 0;   // 点击器计时器
    clock_t dropCoinsTimer = 0; // 金币投掷计时器

    TBHotkey* current_hotkey = nullptr;

    std::deque<TBHotkey*> pending_hotkeys;
    std::recursive_mutex pending_mutex;

    bool IsHotkeySection(const char* section)
    {
        return strncmp(section, hotkey_section_prefix, sizeof(hotkey_section_prefix) - 1) == 0;
    }

    void DeleteHotkeySections(ToolboxIni* ini)
    {
        TNamesDepend sections;
        ini->GetAllSections(sections);
        for (const auto& section : sections) {
            if (IsHotkeySection(section.pItem)) {
                ini->Delete(section.pItem, nullptr);
            }
        }
    }

    bool MigrateLegacyHotkeys(const ToolboxIni* src, ToolboxIni* dst)
    {
        TNamesDepend dst_sections;
        dst->GetAllSections(dst_sections);
        for (const auto& s : dst_sections) {
            if (IsHotkeySection(s.pItem)) return false;
        }
        TNamesDepend src_sections;
        src->GetAllSections(src_sections);
        bool copied = false;
        for (const auto& section : src_sections) {
            if (!IsHotkeySection(section.pItem)) continue;
            TNamesDepend keys;
            src->GetAllKeys(section.pItem, keys);
            for (const auto& key : keys) {
                TNamesDepend values;
                src->GetAllValues(section.pItem, key.pItem, values);
                for (const auto& value : values) {
                    dst->SetValue(section.pItem, key.pItem, value.pItem);
                }
            }
            copied = true;
        }
        if (copied) Log::LogW(L"已将快捷键迁移到 %s", dst->location_on_disk.wstring().c_str());
        return copied;
    }

    void PushPendingHotkey(TBHotkey* hk) {
        pending_mutex.lock();
        if (std::ranges::find(pending_hotkeys, hk) == pending_hotkeys.end()) {
            pending_hotkeys.push_back(hk);
        }
        pending_mutex.unlock();
    }
    TBHotkey* PopPendingHotkey() {
        TBHotkey* hk = nullptr;
        pending_mutex.lock();
        if (pending_hotkeys.size()) {
            hk = pending_hotkeys.front();
            pending_hotkeys.pop_front();
        }
        pending_mutex.unlock();
        return hk;
    }

    bool loaded_action_labels = false;
    // 注意：GetActionLabel_Func() 必须在游戏内调用，因为它内部依赖其他 GW 模块加载。
    // 因为我们只在游戏内绘制此模块，所以在 Draw() 循环中调用，而不是在 Initialize() 中。
    void LoadActionLabels()
    {
        if (loaded_action_labels) {
            return;
        }
        loaded_action_labels = true;

        using GetActionLabel_pt = wchar_t*(__cdecl*)(GW::UI::ControlAction action);
        const auto GetActionLabel_Func = reinterpret_cast<GetActionLabel_pt>(GW::Scanner::Find("\x83\xfe\x5b\x74\x27\x83\xfe\x5c\x74\x22\x83\xfe\x5d\x74\x1d", "xxxxxxxxxxxxxxx", -0x7));
        DEBUG_ASSERT(GetActionLabel_Func);
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
        for (auto* hotkey : TBHotkey::top_level_hotkeys) {
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
        auto inherited_trigger = [&mt](const TBHotkey* hk) -> bool {
            for (const TBHotkey* cur = hk; cur; cur = cur->group) {
                if (cur->trigger_on_explorable && mt == GW::Constants::InstanceType::Explorable) return true;
                if (cur->trigger_on_outpost && mt == GW::Constants::InstanceType::Outpost) return true;
            }
            return false;
        };
        // 注意：CheckSetValidHotkeys() 已检查角色/地图等的有效性
        for (TBHotkey* hk : valid_hotkeys) {
            if (inherited_trigger(hk)
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

    // 在 Update 循环中收到 WM_ACTIVATE 后调用
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
        // 注意：CheckSetValidHotkeys() 已检查角色/地图等的有效性
        for (TBHotkey* hk : valid_hotkeys) {
            if (((activated && hk->trigger_on_gain_focus)
                    || (!activated && hk->trigger_on_lose_focus))) {
                // 这里本来可以用 PushPendingHotkey，但失去/获得焦点是特殊情况
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
        keysHeld.reset(); // 清除先前按键状态
        BYTE keyState[256];

        if (GetKeyboardState(keyState)) {
            for (uint32_t vkey = 0; vkey < 256; ++vkey) {
                // 检查高位是否置位（按键被按下）
                if (keyState[vkey] & 0x80) {
                    keysHeld.set(vkey); // 标记按键被按下
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
            ImGui::OpenPopup("选择快捷键");
            pending_being_assigned = nullptr;
            return;
        }
        if (!keys_being_assigned) {
            return;
        }
        if (!ImGui::BeginPopup("选择快捷键")) {
            keys_selected.reset();
            hotkey_popup_first_draw = true;
            keys_being_assigned = nullptr;
            return;
        }
        if (hotkey_popup_first_draw) {
            keys_selected = keys_being_assigned->key_combo;
            hotkey_popup_first_draw = false;
        }

        keys_selected |= wndproc_keys_held;

        std::string keys_held_buf = ModKeyName(keys_selected);

        ImGui::TextUnformatted(keys_held_buf.c_str());
        if (ImGui::Button("清除")) {
            keys_selected.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("保存")) {
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
    SettingsRegistry::Register(this, settings);
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
    // === 快捷键面板 ===
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (ImGui::Button("创建快捷键...", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            ImGui::OpenPopup("创建快捷键");
        }
        if (ImGui::BeginPopup("创建快捷键")) {
            TBHotkey* new_hotkey = nullptr;
            if (ImGui::Selectable("发送聊天消息")) {
                new_hotkey = new HotkeySendChat(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("向聊天发送消息或命令");
            }
            if (ImGui::Selectable("使用物品")) {
                new_hotkey = new HotkeyUseItem(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("从背包中使用一个物品");
            }
            if (ImGui::Selectable("丢弃或使用增益")) {
                new_hotkey = new HotkeyDropUseBuff(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("使用或取消一个技能，如召回或虔诚姿态");
            }
            if (ImGui::Selectable("切换...")) {
                new_hotkey = new HotkeyToggle(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("切换 GWToolbox++ 功能，如自动点击器");
            }
            if (ImGui::Selectable("执行...")) {
                new_hotkey = new HotkeyAction(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("执行单个任务，如打开宝箱或重新应用光辉称号");
            }
            if (ImGui::Selectable("激战游戏按键")) {
                new_hotkey = new HotkeyGWKey(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("通过工具箱触发游戏内快捷键");
            }
            if (ImGui::Selectable("目标定位")) {
                new_hotkey = new HotkeyTarget(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("根据 ID 定位一个游戏实体");
            }
            if (ImGui::Selectable("移动到")) {
                new_hotkey = new HotkeyMove(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("移动到指定坐标 (x, y)");
            }
            if (ImGui::Selectable("对话")) {
                new_hotkey = new HotkeyDialog(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("发送对话");
            }
            if (ImGui::Selectable("装备物品")) {
                new_hotkey = new HotkeyEquipItem(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("从背包中装备一个物品");
            }
            if (ImGui::Selectable("标记英雄")) {
                new_hotkey = new HotkeyFlagHero(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("相对你的位置标记英雄");
            }
            if (ImGui::Selectable("指挥宠物")) {
                new_hotkey = new HotkeyCommandPet(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("改变你宠物的行为");
            }
            ImGui::Separator();
            if (ImGui::Selectable("快捷键组")) {
                new_hotkey = new HotkeyGroup(nullptr, nullptr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("创建一个命名的组来组织和排序快捷键");
            }
            ImGui::EndPopup();
            hotkeys_changed = new_hotkey != 0;
        }

        for (auto hotkey : TBHotkey::top_level_hotkeys) {
            if (hotkey->Draw()) break; // 列表变异后下一帧重新绘制
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
    ImGui::Checkbox("在标题中显示“启用”复选框", &settings.show_active_in_header);
    ImGui::Checkbox("在标题中显示“运行”按钮", &settings.show_run_in_header);
    ImGui::SliderInt("自动点击器延迟（毫秒）", &settings.clicker_delay_ms, 1, 1'000);
}

void HotkeysWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    while (!TBHotkey::all_hotkeys.empty())
        delete TBHotkey::all_hotkeys[0]; // 在析构中移除第一个元素

    std::vector<HotkeyEntry> entries;
    if (doc.Get(Name(), "hotkeys", entries)) {
        ToolboxIni tmp_ini;
        char buf[256];
        int sec_idx = 0;
        for (const auto& [type, fields] : entries) {
            snprintf(buf, sizeof(buf), "hotkey-%04d:%s", sec_idx++, type.c_str());
            for (const auto& [key, value] : fields) {
                tmp_ini.SetValue(buf, key.c_str(), value.c_str());
            }
            TBHotkey::HotkeyFactory(&tmp_ini, buf);
        }
    }
    else {
        ToolboxIni hotkeys_ini;
        const auto hotkeys_ini_path = Resources::GetLegacySettingFile(HotkeysIniFilename);
        ASSERT(hotkeys_ini.LoadIfExists(hotkeys_ini_path) == SI_OK);
        hotkeys_ini.location_on_disk = hotkeys_ini_path;

        // 迁移的快捷键仅在内存中保留，直到 JSON 保存；永远不会将任何 .ini 写回磁盘
        if (legacy && MigrateLegacyHotkeys(legacy, &hotkeys_ini)) {
            DeleteHotkeySections(legacy);
        }

        TNamesDepend ini_sections;
        hotkeys_ini.GetAllSections(ini_sections);

        for (const auto& section : ini_sections) {
            TBHotkey::HotkeyFactory(&hotkeys_ini, section.pItem);
        }
    }

    TBHotkey::SortHotkeys();
    CheckSetValidHotkeys();
}

void HotkeysWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    std::vector<HotkeyEntry> entries;
    entries.reserve(TBHotkey::all_hotkeys.size());
    ToolboxIni tmp_ini;
    int sec_idx = 0;
    char buf[256];
    for (const auto hotkey : TBHotkey::all_hotkeys) {
        snprintf(buf, sizeof(buf), "hotkey-%04d:%s", sec_idx++, hotkey->Name());
        hotkey->Save(&tmp_ini, buf);
        auto& entry = entries.emplace_back();
        entry.type = hotkey->Name();
        TNamesDepend keys;
        tmp_ini.GetAllKeys(buf, keys);
        for (const auto& key : keys) {
            entry.fields[key.pItem] = tmp_ini.GetValue(buf, key.pItem, "");
        }
    }
    doc.Set(Name(), "hotkeys", entries);
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
        if (!hk->key_combo.test(keyData)) return false; // 触发键不包含在此快捷键组合中
        if (hk->strict_key_combo) return hk->key_combo == wndproc_keys_held;
        if (is_in_controller_mode && !hk->trigger_in_controller_mode) return false;
        if (!is_in_controller_mode && !hk->trigger_in_desktop_mode) return false;
        return (hk->key_combo & wndproc_keys_held) == hk->key_combo;
    };

    size_t hotkeys_triggered = 0;

    auto check_triggers = [check_trigger, &hotkeys_triggered](bool is_key_up, uint32_t keyData) {
        static std::vector<TBHotkey*> matching_hotkeys;
        matching_hotkeys.clear();
        size_t max_modifier_count = 0;

        bool is_in_controller_mode = GW::UI::IsInControllerMode();

        // 步骤1：查找与当前按下的按键匹配的所有快捷键
        for (TBHotkey* hk : valid_hotkeys) {
            if (is_key_up) hk->pressed = false;

            if (check_trigger(hk, is_key_up, keyData, is_in_controller_mode)) {
                size_t modifier_count = hk->key_combo.count();
                matching_hotkeys.push_back(hk);

                max_modifier_count = std::max(max_modifier_count, modifier_count);
            }
        }

        bool triggered = false;

        // 步骤2：仅触发最具体的快捷键
        for (TBHotkey* hk : matching_hotkeys) {
            if (hk->key_combo.count() == max_modifier_count) {
                PushPendingHotkey(hk);

                if (!is_key_up && hk->block_gw) {
                    triggered = true;
                }
                if (hk->block_other_hotkeys_on_trigger) break;
            }
        }

        return triggered; // 如果有任何快捷键阻止了输入，则返回 true 以防止按键事件到达 GW
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
            while (PopPendingHotkey()) {} // 清空上一个地图的待处理快捷键
            for (auto hk : TBHotkey::all_hotkeys) {
                hk->pressed = false;
            }
        }
        return;
    }
    if (!map_change_triggered) {
        static clock_t last_map_check = 0;
        if (!last_map_check || TIMER_DIFF(last_map_check) > 500) {
            last_map_check = TIMER_INIT();
            map_change_triggered = OnMapChanged();
        }
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
