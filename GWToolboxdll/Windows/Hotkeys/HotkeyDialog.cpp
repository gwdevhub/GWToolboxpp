#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/Hotkeys/HotkeyDialog.h>

HotkeyDialog::HotkeyDialog(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (!ini) return;
    id = static_cast<size_t>(ini->GetLongValue(section, "DialogID", 0));
    strcpy_s(name, ini->GetValue(section, "DialogName", ""));
}

void HotkeyDialog::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "DialogID", id);
    ini->SetValue(section, "DialogName", name);
}

int HotkeyDialog::Description(char* buf, const size_t bufsz)
{
    if (!name[0]) {
        return snprintf(buf, bufsz, "Dialog #%zu", id);
    }
    return snprintf(buf, bufsz, "Dialog %s", name);
}

bool HotkeyDialog::DrawSettings()
{
    bool hotkey_changed = false;

    hotkey_changed |= ImGui::InputInt("Dialog ID", reinterpret_cast<int*>(&id));
    ImGui::ShowHelp("If dialog is 0, accepts the first available quest dialog (either reward or accept quest).");
    hotkey_changed |= ImGui::InputText("Dialog Name", name, _countof(name));
    hotkey_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
    return hotkey_changed;
}

void HotkeyDialog::Execute()
{
    if (!CanUse()) {
        return;
    }
    char buf[32];
    if (id == 0) {
        snprintf(buf, _countof(buf), "dialog take");
    }
    else {
        snprintf(buf, _countof(buf), "dialog 0x%X", id);
    }

    GW::Chat::SendChat('/', buf);
    if (show_message_in_emote_channel) {
        Log::Flash("Sent dialog %s (%d)", name, id);
    }
}
