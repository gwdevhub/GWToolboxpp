#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/HotkeysWindow.h>
#include <Windows/PconsWindow.h>
#include <Windows/Hotkeys/HotkeyToggle.h>

std::unordered_map<WORD, HotkeyToggle*> HotkeyToggle::toggled;

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
    target = ini ? static_cast<ToggleTarget>(ini->GetLongValue(section, "ToggleID", target)) : target;
    static bool initialised = false;
    if (!initialised) {
        toggled.reserve(512);
    }
    initialised = true;
    switch (target) {
        case Clicker:
            interval = HotkeysWindow::Instance().settings.clicker_delay_ms;
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

bool HotkeyToggle::DrawSettings()
{
    bool hotkey_changed = false;

    if (ImGui::Combo("Toggle###combo", (int*)&target, GetText, nullptr, Count)) {
        if (target == Clicker) {
            togglekey = VK_LBUTTON;
        }
        hotkey_changed = true;
    }
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
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
            interval = HotkeysWindow::Instance().settings.clicker_delay_ms;
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
