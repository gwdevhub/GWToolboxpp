#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/GameEntities/Agent.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/Hotkeys/HotkeyMove.h>

HotkeyMove::HotkeyMove(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (!ini) return;
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
