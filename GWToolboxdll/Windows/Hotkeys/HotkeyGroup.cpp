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

HotkeyGroup::HotkeyGroup(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    key_combo.reset(); // groups don't have a key combo
}

HotkeyGroup::~HotkeyGroup()
{
    for (const TBHotkey* hk : hotkeys) {
        delete hk;
    }
}

int HotkeyGroup::Description(char* buf, size_t bufsz)
{
    return snprintf(buf, bufsz, "Group: %s", *label ? label : "(unnamed)");
}

void HotkeyGroup::Execute()
{
    // Groups don't execute themselves
}

void HotkeyGroup::Save(ToolboxIni* ini, const char* section) const
{
    // Only save label and active state for the group itself; skip key-combo fields
    if (*label)
        ini->SetValue(section, VAR_NAME(label), label);
    ini->SetBoolValue(section, VAR_NAME(active), active);
}

bool HotkeyGroup::Draw()
{
    bool changed = false;
    const float scale = ImGui::FontScale();
    ImGui::PushItemWidth(-140.0f * scale);
    changed |= ImGui::InputTextEx("Group Name##group_label", "Group Name", label, sizeof(label),
                                   ImVec2(0, 0), ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        changed = true;
    }
    ImGui::PopItemWidth();
    return changed;
}

bool HotkeyGroup::Draw(Op* op, bool first, bool last)
{
    bool changed = false;
    const float btn_size = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushID(static_cast<int>(ui_id));

    const auto header_label = std::format("[Group: {}]###grphdr{}", *label ? label : "(unnamed)", ui_id);
    if (open_state_override >= 0) {
        ImGui::SetNextItemOpen(open_state_override == 1, ImGuiCond_Always);
        open_state_override = -1;
    }
    const bool open = ImGui::CollapsingHeader(header_label.c_str(), ImGuiTreeNodeFlags_AllowOverlap);

    // Overlay buttons (right-aligned, drawn after the header)
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - (btn_size * 3 + spacing * 2));

    if (ImGui::Checkbox("##grp_active", &active)) {
        for (auto* hk : hotkeys) hk->active = active;
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle all hotkeys in this group");

    ImGui::SameLine();
    if (!first) {
        if (ImGui::Button(ICON_FA_ARROW_UP "##grpup", ImVec2(btn_size, btn_size))) {
            *op = Op_MoveUp;
            changed = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move group up");
    } else {
        ImGui::Dummy(ImVec2(btn_size, btn_size));
    }

    ImGui::SameLine();
    if (!last) {
        if (ImGui::Button(ICON_FA_ARROW_DOWN "##grpdown", ImVec2(btn_size, btn_size))) {
            *op = Op_MoveDown;
            changed = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move group down");
    } else {
        ImGui::Dummy(ImVec2(btn_size, btn_size));
    }

    if (open) {
        ImGui::Indent();

        // Group rename field
        changed |= Draw();

        // Draw child hotkeys
        for (unsigned int i = 0; i < hotkeys.size(); ++i) {
            TBHotkey::Op child_op = TBHotkey::Op_None;
            changed |= hotkeys[i]->Draw(&child_op, i == 0, i == hotkeys.size() - 1);
            switch (child_op) {
                case TBHotkey::Op_MoveUp:
                    if (i > 0) {
                        std::swap(hotkeys[i], hotkeys[i - 1]);
                        changed = true;
                    }
                    break;
                case TBHotkey::Op_MoveDown:
                    if (i < hotkeys.size() - 1) {
                        std::swap(hotkeys[i], hotkeys[i + 1]);
                        changed = true;
                    }
                    break;
                case TBHotkey::Op_Delete: {
                    delete hotkeys[i];
                    hotkeys.erase(hotkeys.begin() + i);
                    changed = true;
                    break;
                }
                case TBHotkey::Op_Duplicate: {
                    ToolboxIni tmp_ini;
                    char section_buf[64];
                    snprintf(section_buf, sizeof(section_buf), "hotkey-dup:%s", hotkeys[i]->Name());
                    hotkeys[i]->Save(&tmp_ini, section_buf);
                    TBHotkey* copy = TBHotkey::HotkeyFactory(&tmp_ini, section_buf);
                    if (copy) {
                        char base[128];
                        if (*hotkeys[i]->label) {
                            snprintf(base, sizeof(base), "%s", hotkeys[i]->label);
                            if (char* suffix = strstr(base, " (Copy "))
                                *suffix = '\0';
                        } else {
                            hotkeys[i]->Description(base, sizeof(base));
                        }
                        int copy_num = 0;
                        const size_t base_len = strlen(base);
                        for (const auto* hk : hotkeys) {
                            if (!*hk->label) continue;
                            if (strncmp(hk->label, base, base_len) == 0 &&
                                strncmp(hk->label + base_len, " (Copy ", 7) == 0) {
                                int n = 0;
                                if (sscanf(hk->label + base_len + 7, "%d)", &n) == 1)
                                    copy_num = std::max(copy_num, n);
                            }
                        }
                        snprintf(copy->label, sizeof(copy->label), "%s (Copy %d)", base, copy_num + 1);
                        hotkeys[i]->open_state_override = 0;
                        copy->open_state_override = 1;
                        hotkeys.insert(hotkeys.begin() + i + 1, copy);
                        changed = true;
                    }
                    break;
                }
                default:
                    break;
            }
            if (changed) break; // re-render next frame after list mutation
        }

        // Add hotkey to group button
        ImGui::Separator();
        if (!changed) {
            if (ImGui::Button("Add Hotkey to Group...", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                ImGui::OpenPopup("Add Hotkey to Group");
            }
            TBHotkey* new_child = nullptr;
            if (ImGui::BeginPopup("Add Hotkey to Group")) {
                if (ImGui::Selectable("Send Chat"))      new_child = new HotkeySendChat(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send a message or command to chat");
                if (ImGui::Selectable("Use Item"))       new_child = new HotkeyUseItem(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use an item from your inventory");
                if (ImGui::Selectable("Drop or Use Buff")) new_child = new HotkeyDropUseBuff(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use or cancel a skill such as Recall or UA");
                if (ImGui::Selectable("Toggle..."))      new_child = new HotkeyToggle(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle a GWToolbox++ functionality such as clicker");
                if (ImGui::Selectable("Execute..."))     new_child = new HotkeyAction(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execute a single task such as opening chests");
                if (ImGui::Selectable("Guild Wars Key")) new_child = new HotkeyGWKey(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Trigger an in-game hotkey via toolbox");
                if (ImGui::Selectable("Target"))         new_child = new HotkeyTarget(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Target a game entity by its ID");
                if (ImGui::Selectable("Move to"))        new_child = new HotkeyMove(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move to a specific (x,y) coordinate");
                if (ImGui::Selectable("Dialog"))         new_child = new HotkeyDialog(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send a Dialog");
                if (ImGui::Selectable("Equip Item"))     new_child = new HotkeyEquipItem(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Equip an item from your inventory");
                if (ImGui::Selectable("Flag Hero"))      new_child = new HotkeyFlagHero(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Flag a hero relative to your position");
                if (ImGui::Selectable("Command Pet"))    new_child = new HotkeyCommandPet(nullptr, nullptr);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Change behavior of your pet");
                ImGui::EndPopup();
            }
            if (new_child) {
                hotkeys.push_back(new_child);
                changed = true;
            }
        }

        // Delete group button
        const float half_btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.f;
        if (ImGui::Button("Delete Group", ImVec2(half_btn_w, 0))) {
            ImGui::OpenPopup("Delete Group?");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete this group and all its hotkeys");
        if (ImGui::BeginPopupModal("Delete Group?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete group \"%s\" and all %zu hotkey(s) inside?\nThis cannot be undone.", *label ? label : "(unnamed)", hotkeys.size());
            if (ImGui::Button("OK", ImVec2(120.f, 0))) {
                *op = Op_Delete;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.f, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Unindent();
    }

    ImGui::PopID();
    return changed;
}
