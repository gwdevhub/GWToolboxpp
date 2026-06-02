#include "stdafx.h"

#include <ImGuiAddons.h>

#include <Windows/Hotkeys/HotkeyGroup.h>
#include <Windows/Hotkeys/HotkeyAction.h>
#include <Windows/Hotkeys/HotkeyCommandPet.h>
#include <Windows/Hotkeys/HotkeyDialog.h>
#include <Windows/Hotkeys/HotkeyDropUseBuff.h>
#include <Windows/Hotkeys/HotkeyEquipItem.h>
#include <Windows/Hotkeys/HotkeyFlagHero.h>
#include <Windows/Hotkeys/HotkeyGWKey.h>
#include <Windows/Hotkeys/HotkeyMove.h>
#include <Windows/Hotkeys/HotkeySendChat.h>
#include <Windows/Hotkeys/HotkeyTarget.h>
#include <Windows/Hotkeys/HotkeyToggle.h>
#include <Windows/Hotkeys/HotkeyUseItem.h>
#include <Defines.h>

HotkeyGroup::HotkeyGroup(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (*label) {
        auto found = hotkey_groups.find(label);
        if (found != hotkey_groups.end()) {
            hotkeys = found->second->hotkeys;
            found->second->hotkeys.clear();
            delete found->second;
        }
        hotkey_groups[label] = this;
    }
    for (auto hotkey : hotkeys) {
        hotkey->group = this;
    }
}
HotkeyGroup::HotkeyGroup(const char* _label) : TBHotkey(0, 0)
{
    strncpy(label, _label, _countof(label));
    if (*label) {
        auto found = hotkey_groups.find(label);
        if (found != hotkey_groups.end()) {
            hotkeys = found->second->hotkeys;
            found->second->hotkeys.clear();
            delete found->second;
        }
        hotkey_groups[label] = this;
    }
    for (auto hotkey : hotkeys) {
        hotkey->group = this;
    }
}

HotkeyGroup::~HotkeyGroup()
{
    // Promote children to this group's parent scope before unlinking.
    // Without this, deleted groups leave children in all_hotkeys but not in
    // top_level_hotkeys or any group's hotkeys list, causing SortHotkeys() to assert.
    for (TBHotkey* hk : hotkeys) {
        if (hk->group != this) continue;
        hk->group = group;
        if (group) {
            group->hotkeys.push_back(hk);
        } else {
            top_level_hotkeys.push_back(hk);
        }
    }
    auto found = hotkey_groups.find(label);
    if (found != hotkey_groups.end() && found->second == this) hotkey_groups.erase(label);
}
void HotkeyGroup::Toggle()
{
    if (ongoing) {
        for (auto hk : hotkeys) {
            if (hk->ongoing) 
                hk->Toggle();
        }
        ongoing = false;
    }
    else {
        for (auto hk : hotkeys) {
            if (hk->active) 
                hk->Toggle();
            ongoing |= hk->ongoing;
        }
    }

}
int HotkeyGroup::Description(char* buf, size_t bufsz)
{
    return snprintf(buf, bufsz, "Group: %s", *label ? label : "(unnamed)");
}

bool HotkeyGroup::DrawSettings()
{
    bool hotkey_changed = false;
    // Draw child hotkeys
    ImGui::Spacing();
    ImGui::TextDisabled("Group hotkeys:");
    ImGui::Separator();
    if (hotkeys.empty()) {
        ImGui::Indent();
        ImGui::TextDisabled("No hotkeys assigned to this group");
        ImGui::Unindent();
    }
    for (unsigned int i = 0; i < hotkeys.size() && !hotkey_changed; ++i) {
        hotkey_changed |= hotkeys[i]->Draw(); // re-render next frame after list mutation
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Group settings:");
    ImGui::Separator();
    char old_label[sizeof(label)];
    memcpy(old_label, label, sizeof(label));
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
    if (strcmp(old_label, label) != 0) {
        // Keep hotkey_groups in sync when label changes
        auto it = hotkey_groups.find(old_label);
        if (it != hotkey_groups.end() && it->second == this)
            hotkey_groups.erase(it);
        if (*label)
            hotkey_groups[label] = this;
    }
    return hotkey_changed;
}
