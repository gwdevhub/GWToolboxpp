#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/Hotkeys/HotkeyTarget.h>

HotkeyTarget::HotkeyTarget(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    // don't print target hotkey to chat by default
    show_message_in_emote_channel = false;
    name[0] = 0;
    if (!ini) {
        return;
    }
    std::string ini_name = ini->GetValue(section, "TargetID", "");
    strcpy_s(id, ini_name.substr(0, sizeof(id) - 1).c_str());
    id[sizeof(id) - 1] = 0;
    long ini_type = ini->GetLongValue(section, "TargetType", -1);
    if (ini_type >= NPC && ini_type < Count) {
        type = static_cast<HotkeyTargetType>(ini_type);
    }
    ini_name = ini->GetValue(section, "TargetName", "");
    strcpy_s(name, ini_name.substr(0, sizeof(name) - 1).c_str());
    name[sizeof(name) - 1] = 0;

    ini->GetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                      show_message_in_emote_channel);
}

void HotkeyTarget::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "TargetID", id);
    ini->SetLongValue(section, "TargetType", type);
    ini->SetValue(section, "TargetName", name);
}

int HotkeyTarget::Description(char* buf, const size_t bufsz)
{
    if (!name[0]) {
        return snprintf(buf, bufsz, "Target %s %s", type_labels[type], id);
    }
    return snprintf(buf, bufsz, "Target %s", name);
}

bool HotkeyTarget::Draw()
{
    const float w = ImGui::GetContentRegionAvail().x / 1.5f;
    ImGui::PushItemWidth(w);
    bool hotkey_changed = ImGui::Combo("Target Type", (int*)&type, type_labels, 3);
    hotkey_changed |= ImGui::InputText(identifier_labels[type], id, _countof(id));
    ImGui::PopItemWidth();
    ImGui::ShowHelp("See Settings > Help > Chat Commands for /target options");
    ImGui::PushItemWidth(w);
    hotkey_changed |= ImGui::InputText("Hotkey label", name, _countof(name));
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 0);
    ImGui::TextDisabled(" (optional)");
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    return hotkey_changed;
}

void HotkeyTarget::Execute()
{
    if (!CanUse()) {
        return;
    }

    constexpr size_t len = 122;
    auto message = new wchar_t[len];
    message[0] = 0;
    switch (type) {
        case Item:
            swprintf(message, len, L"target item %S", id);
            break;
        case NPC:
            swprintf(message, len, L"target npc %S", id);
            break;
        case Signpost:
            swprintf(message, len, L"target gadget %S", id);
            break;
        default:
            Log::ErrorW(L"Unknown target type %d", type);
            delete[] message;
            return;
    }
    GW::GameThread::Enqueue([message] {
        GW::Chat::SendChat('/', message);
        delete[] message;
    });

    if (show_message_in_emote_channel) {
        char buf[256];
        Description(buf, 256);
        Log::Flash("Triggered %s", buf);
    }
}
