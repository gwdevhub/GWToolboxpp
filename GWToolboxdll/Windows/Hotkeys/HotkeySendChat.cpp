#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/Hotkeys/HotkeySendChat.h>

HotkeySendChat::HotkeySendChat(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (!ini) return;
    strcpy_s(message, ini->GetValue(section, "msg", ""));
    channel = ini->GetValue(section, "channel", "/")[0];
}

void HotkeySendChat::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "msg", message);
    char buf[8];
    snprintf(buf, 8, "%c", channel);
    ini->SetValue(section, "channel", buf);
}

int HotkeySendChat::Description(char* buf, const size_t bufsz)
{
    return snprintf(buf, bufsz, "Send chat '%c%s'", channel, message);
}

bool HotkeySendChat::DrawSettings()
{
    bool hotkey_changed = false;

    int index = 0;
    switch (channel) {
        case '/':
            index = 0;
            break;
        case '!':
            index = 1;
            break;
        case '@':
            index = 2;
            break;
        case '#':
            index = 3;
            break;
        case '$':
            index = 4;
            break;
        case '%':
            index = 5;
            break;
        case '"':
            index = 6;
            break;
    }
    static const char* channels[] = {"/ Commands", "! All", "@ Guild",
                                     "# Group", "$ Trade", "% Alliance",
                                     "\" Whisper"};
    if (ImGui::Combo("Channel", &index, channels, 7)) {
        switch (index) {
            case 0:
                channel = '/';
                break;
            case 1:
                channel = '!';
                break;
            case 2:
                channel = '@';
                break;
            case 3:
                channel = '#';
                break;
            case 4:
                channel = '$';
                break;
            case 5:
                channel = '%';
                break;
            case 6:
                channel = '"';
                break;
            default:
                channel = '/';
                break;
        }
        show_message_in_emote_channel = channel == '/' &&
                                        show_message_in_emote_channel;
        hotkey_changed = true;
    }
    hotkey_changed |= ImGui::InputText("Message", message, _countof(message));
    hotkey_changed |= channel == '/' && ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
    return hotkey_changed;
}

void HotkeySendChat::Execute()
{
    if (!CanUse()) {
        return;
    }
    if (show_message_in_emote_channel && channel == L'/') {
        Log::Flash("/%s", message);
    }
    GW::GameThread::Enqueue([&]() {
        GW::Chat::SendChat(channel, message);
    });
}
