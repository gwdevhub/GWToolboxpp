#include "stdafx.h"

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <ImGuiAddons.h>
#include <Keys.h>

#include <Windows/Hotkeys/HotkeyGWKey.h>

typedef std::pair<GW::UI::ControlAction, std::unique_ptr<GuiUtils::EncString>> ControlLabelPair;
std::vector<ControlLabelPair> HotkeyGWKey::control_labels;

namespace {
    std::vector<const char*> HotkeyGWKey_labels = {};

    bool BuildHotkeyGWKeyLabels()
    {
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

    int GetHotkeyActionIdx(GW::UI::ControlAction action)
    {
        BuildHotkeyGWKeyLabels();
        for (size_t i = 0; i < HotkeyGWKey::control_labels.size(); i++) {
            if (HotkeyGWKey::control_labels[i].first == action)
                return (int)i;
        }
        return 0;
    }
}

HotkeyGWKey::HotkeyGWKey(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (ini) {
        action = static_cast<GW::UI::ControlAction>(ini->GetLongValue(section, "ActionID", action));
    }
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

bool HotkeyGWKey::DrawSettings()
{
    bool hotkey_changed = false;
    if (!BuildHotkeyGWKeyLabels()) {
        ImGui::Text("Waiting on encoded strings");
        return false;
    }
    action_idx = GetHotkeyActionIdx(action);

    if (ImGui::Combo("Action###combo", &action_idx, HotkeyGWKey_labels.data(), HotkeyGWKey_labels.size(), 10)) {
        action = control_labels[action_idx].first;
        return true;
    }
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
    return hotkey_changed;
}

void HotkeyGWKey::Execute()
{
    if (!CanUse()) {
        return;
    }
    GW::GameThread::Enqueue([&] {
        const auto frame = GW::UI::GetFrameByLabel(L"Game");
        Keypress(action, GW::UI::GetChildFrame(frame, 6));
        Keypress(action, GW::UI::GetParentFrame(frame));
    });
}
