#include "stdafx.h"

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Modules/GameSettings.h>
#include <Utils/ToolboxUtils.h>

#include "ChatSettings.h"

#include "Defines.h"
#include "GWCA/Managers/FriendListMgr.h"
#include "GWCA/Managers/StoCMgr.h"

namespace {
    // Settings
    bool show_timestamps = false;
    bool enable_chat_log = true;
    bool hide_player_speech_bubbles = false;
    bool show_timestamp_seconds = false;
    bool show_timestamp_24h = false;
    bool npc_speech_bubbles_as_chat = false;
    bool redirect_npc_messages_to_emote_chat = false;
    bool openlinks = true;
    bool auto_url = true;

    Color timestamps_color = Colors::RGB(0xc0, 0xc0, 0xbf);

    // Runtime
    bool ctrl_enter_whisper = false;
    std::wstring afk_message;
    clock_t afk_message_time = 0;

    GW::HookEntry WhisperCallback_Entry;
    GW::HookEntry StartWhisperCallback_Entry;
    GW::HookEntry SendChatCallback_Entry;
    GW::HookEntry SpeechBubble_Entry;
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry OnCheckboxPreferenceChanged_Entry;
    GW::HookEntry MessageNPC_Entry;
    GW::HookEntry MessageLocal_Entry;
    GW::HookEntry MessageServer_Entry;
    GW::HookEntry OnPlayerChatMessage_Entry;

    // used by chat colors grid
    constexpr float chat_colors_grid_x[] = {0, 100, 160, 240};
    std::vector<PendingChatMessage*> pending_messages;

    void PrintTime(wchar_t* buffer, size_t n, DWORD time_sec)
    {
        const DWORD secs = time_sec % 60;
        const DWORD minutes = (time_sec / 60) % 60;
        const DWORD hours = time_sec / 3600;
        DWORD time = 0;
        const wchar_t* time_unit = L"";
        if (hours != 0) {
            time_unit = L"hour";
            time = hours;
        }
        else if (minutes != 0) {
            time_unit = L"minute";
            time = minutes;
        }
        else {
            time_unit = L"second";
            time = secs;
        }
        if (time > 1) {
            swprintf(buffer, n, L"%lu %ss", time, time_unit);
        }
        else {
            swprintf(buffer, n, L"%lu %s", time, time_unit);
        }
    }

    void DrawChannelColor(const char* name, GW::Chat::Channel chan)
    {
        ImGui::PushID(chan);
        ImGui::Text(name);
        constexpr ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel;
        GW::Chat::Color color, sender_col, message_col;
        GW::Chat::GetChannelColors(chan, &sender_col, &message_col);

        ImGui::SameLine(chat_colors_grid_x[1]);
        color = sender_col;
        if (Colors::DrawSettingHueWheel("Sender Color:", &color, flags) && color != sender_col) {
            GW::Chat::SetSenderColor(chan, color);
        }

        ImGui::SameLine(chat_colors_grid_x[2]);
        color = message_col;
        if (Colors::DrawSettingHueWheel("Message Color:", &color, flags) && color != message_col) {
            GW::Chat::SetMessageColor(chan, color);
        }

        ImGui::SameLine(chat_colors_grid_x[3]);
        if (ImGui::Button("Reset")) {
            GW::Chat::Color col1, col2;
            GW::Chat::GetDefaultColors(chan, &col1, &col2);
            GW::Chat::SetSenderColor(chan, col1);
            GW::Chat::SetMessageColor(chan, col2);
        }
        ImGui::PopID();
    }

    // Automatically send /age2 on /age.
    void OnServerMessage(GW::HookStatus*, GW::Packet::StoC::MessageServer* pak)
    {
        if (!GameSettings::GetSettingBool("auto_age2_on_age") || static_cast<GW::Chat::Channel>(pak->channel) != GW::Chat::Channel::CHANNEL_GLOBAL) return; // Disabled or message pending
        const wchar_t* msg = ToolboxUtils::GetMessageCore();
        // 0x8101 0x641F 0x86C3 0xE149 0x53E8 0x101 0x107 = You have been in this map for n minutes.
        // 0x8101 0x641E 0xE7AD 0xEF64 0x1676 0x101 0x107 0x102 0x107 = You have been in this map for n hours and n minutes.
        if (wmemcmp(msg, L"\x8101\x641F\x86C3\xE149\x53E8", 5) == 0 || wmemcmp(msg, L"\x8101\x641E\xE7AD\xEF64\x1676", 5) == 0) {
            GW::Chat::SendChat('/', "age2");
        }
    }

    // Redirect NPC messages from team chat to emote chat (emulate speech bubble instead)
    void OnNPCChatMessage(GW::HookStatus* status, GW::Packet::StoC::MessageNPC* pak)
    {
        if (!redirect_npc_messages_to_emote_chat || !pak->sender_name) return; // Disabled or message pending
        const wchar_t* message = ToolboxUtils::GetMessageCore();
        PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_EMOTE, message, pak->sender_name);
        if (m) pending_messages.push_back(m);
        if (pak->agent_id) {
            // Then forward the message on to speech bubble
            GW::Packet::StoC::SpeechBubble packet;
            packet.agent_id = pak->agent_id;
            wcscpy(packet.message, message);
            if (GW::Agents::GetAgentByID(packet.agent_id)) GW::StoC::EmulatePacket(&packet);
        }
        ToolboxUtils::ClearMessageCore();
        status->blocked = true; // consume original packet.
    }

    // Allow clickable name when a player pings "I'm following X" or "I'm targeting X"
    void OnLocalChatMessage(GW::HookStatus* status, GW::Packet::StoC::MessageLocal* pak)
    {
        if (status->blocked) return;                                                                                // Sender blocked, packet handled.
        if (pak->channel != static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_GROUP) || !pak->player_number) return; // Not team chat or no sender
        std::wstring message(ToolboxUtils::GetMessageCore());
        if (message[0] != 0x778 && message[0] != 0x781) return; // Not "I'm Following X" or "I'm Targeting X" message.
        size_t start_idx = message.find(L"\xba9\x107");
        if (start_idx == std::wstring::npos) return; // Not a player name.
        start_idx += 2;
        const size_t end_idx = message.find(L'\x1', start_idx);
        if (end_idx == std::wstring::npos) return; // Not a player name, this should never happen.
        const std::wstring player_pinged = GuiUtils::SanitizePlayerName(message.substr(start_idx, end_idx));
        if (player_pinged.empty()) return; // No recipient
        const auto sender = GW::PlayerMgr::GetPlayerByID(pak->player_number);
        if (!sender) return;                                                                                                               // No sender
        if (GameSettings::GetSettingBool("flash_window_on_name_ping") && ToolboxUtils::GetPlayerName() == player_pinged) GuiUtils::FlashWindow(); // Flash window - we've been followed!
        // Allow clickable player name
        message.insert(start_idx, L"<a=1>");
        message.insert(end_idx + 5, L"</a>");
        PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_GROUP, message.c_str(), sender->name_enc);
        if (m) pending_messages.push_back(m);
        ToolboxUtils::ClearMessageCore();
        status->blocked = true; // consume original packet.
    }

    // Print NPC speech bubbles to emote chat.
    void OnSpeechBubble(GW::HookStatus* status, GW::Packet::StoC::SpeechBubble* pak)
    {
        UNREFERENCED_PARAMETER(status);
        if (!npc_speech_bubbles_as_chat || !pak->message || !pak->agent_id) return; // Disabled, invalid, or pending another speech bubble
        size_t len = 0;
        for (size_t i = 0; pak->message[i] != 0; i++)
            len = i + 1;
        if (len < 3) return; // Shout skill etc
        const auto agent = reinterpret_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
        if (!agent || agent->login_number) return; // Agent not found or Speech bubble from player e.g. drunk message.
        PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, GW::Agents::GetAgentEncName(agent));
        if (m) pending_messages.push_back(m);
    }

    // NPC dialog messages to emote chat
    void OnSpeechDialogue(GW::HookStatus* status, GW::Packet::StoC::DisplayDialogue* pak)
    {
        if (!redirect_npc_messages_to_emote_chat) return; // Disabled or message pending
        GW::Chat::WriteChatEnc(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, pak->name);
        status->blocked = true; // consume original packet.
    }

    // Disable native timestamps
    void OnCheckboxPreferenceChanged(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void* lParam)
    {
        UNREFERENCED_PARAMETER(lParam);
        if (!(msgid == GW::UI::UIMessage::kCheckboxPreference && wParam)) return;
        const GW::UI::FlagPreference pref = *static_cast<GW::UI::FlagPreference*>(wParam); // { uint32_t pref, uint32_t value } - don't care about value atm.
        if (pref == GW::UI::FlagPreference::ShowChatTimestamps && show_timestamps) {
            status->blocked = true; // Always block because this UI Message will redraw all timestamps later in the call stack
            if (show_timestamps && GW::UI::GetPreference(GW::UI::FlagPreference::ShowChatTimestamps) == 1) {
                Log::Error("Disable GWToolbox timestamps to enable this setting");
                GW::UI::SetPreference(GW::UI::FlagPreference::ShowChatTimestamps, 0);
            }
        }
    }

    // Turn /wiki into /wiki <location>
    void OnSendChat(GW::HookStatus*, GW::Chat::Channel chan, wchar_t* msg)
    {
        if (!auto_url || !msg) return;
        size_t len = wcslen(msg);
        size_t max_len = 120;

        if (chan == GW::Chat::Channel::CHANNEL_WHISPER) {
            // msg == "Whisper Target Name,msg"
            size_t i;
            for (i = 0; i < len; i++)
                if (msg[i] == ',') break;

            if (i < len) {
                msg += i + 1;
                len -= i + 1;
                max_len -= i + 1;
            }
        }

        if (wcsncmp(msg, L"http://", 7) && wcsncmp(msg, L"https://", 8)) return;

        if (len + 5 < max_len) {
            for (size_t i = len; i != 0; --i)
                msg[i] = msg[i - 1];
            msg[0] = '[';
            msg[len + 1] = ';';
            msg[len + 2] = 'x';
            msg[len + 3] = 'x';
            msg[len + 4] = ']';
            msg[len + 5] = 0;
        }
    }

    // Hide player chat message speech bubbles by redirecting from 0x10000081 to 0x1000007E
    void OnPlayerChatMessage(GW::HookStatus* status, GW::UI::UIMessage, void* wParam, void*)
    {
        if (hide_player_speech_bubbles) {
            status->blocked = true;
            const auto msg = static_cast<PlayerChatMessage*>(wParam);
            const auto agent = GW::PlayerMgr::GetPlayerByID(msg->player_number);
            if (!agent) return;
            GW::Chat::WriteChatEnc(static_cast<GW::Chat::Channel>(msg->channel), msg->message, agent->name_enc);
        }
    }

    // Open links on player name click, Ctrl + click name to target, Ctrl + Shift + click name to invite
    void OnStartWhisper(GW::HookStatus* status, wchar_t* name)
    {
        if (!name) return;
        switch (name[0]) {
            case 0x200B: {
                // Zero-Width Space - wiki link
                GuiUtils::SearchWiki(&name[1]);
                status->blocked = true;
                return;
            }
            case 0x200C: {
                // Zero Width Non-Joiner - location on disk
                const std::filesystem::path p(&name[1]);
                ShellExecuteW(nullptr, L"open", p.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                status->blocked = true;
                return;
            }
        }
        if (openlinks && (!wcsncmp(name, L"http://", 7) || !wcsncmp(name, L"https://", 8))) {
            ShellExecuteW(nullptr, L"open", name, nullptr, nullptr, SW_SHOWNORMAL);
            status->blocked = true;
            return;
        }
        if (!ImGui::GetIO().KeyCtrl) return; // - Next logic only applicable when Ctrl is held

        const std::wstring _name = GuiUtils::SanitizePlayerName(name);
        if (ImGui::GetIO().KeyShift && GW::PartyMgr::GetIsLeader()) {
            wchar_t buf[64];
            swprintf(buf, 64, L"invite %s", _name.c_str());
            GW::Chat::SendChat('/', buf);
            status->blocked = true;
            return;
        }
        const auto player = ToolboxUtils::GetPlayerByName(_name.c_str());
        if (!ctrl_enter_whisper && player && GW::Agents::GetAgentByID(player->agent_id)) {
            GW::Agents::ChangeTarget(player->agent_id);
            status->blocked = true;
        }
    }

    void OnWhisper(GW::HookStatus*, const wchar_t* from, const wchar_t*)
    {
        const auto status = GW::FriendListMgr::GetMyStatus();
        if (status == GW::FriendStatus::Away && !afk_message.empty()) {
            wchar_t buffer[120];
            const auto diff_time = (clock() - afk_message_time) / CLOCKS_PER_SEC;
            wchar_t time_buffer[128];
            PrintTime(time_buffer, 128, diff_time);
            swprintf(buffer, 120, L"Automatic message: \"%s\" (%s ago)", afk_message.c_str(), time_buffer);
            // Avoid infinite recursion
            if (ToolboxUtils::GetPlayerName() != from) GW::Chat::SendChat(from, buffer);
        }
    }

}

void ChatSettings::Initialize()
{
    ToolboxModule::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&MessageServer_Entry, OnServerMessage);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&MessageLocal_Entry, OnLocalChatMessage);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SpeechBubble>(&SpeechBubble_Entry, OnSpeechBubble);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, OnSpeechDialogue);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageNPC>(&MessageNPC_Entry, OnNPCChatMessage);
    GW::UI::RegisterUIMessageCallback(&OnCheckboxPreferenceChanged_Entry, GW::UI::UIMessage::kCheckboxPreference, OnCheckboxPreferenceChanged);
    GW::UI::RegisterUIMessageCallback(&OnPlayerChatMessage_Entry, GW::UI::UIMessage::kPlayerChatMessage, OnPlayerChatMessage);

    GW::Chat::RegisterStartWhisperCallback(&StartWhisperCallback_Entry, OnStartWhisper);
    GW::Chat::RegisterSendChatCallback(&SendChatCallback_Entry, OnSendChat);
    GW::Chat::RegisterWhisperCallback(&WhisperCallback_Entry, OnWhisper);
}

void ChatSettings::Terminate()
{
    ToolboxModule::Terminate();

    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&MessageServer_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageLocal>(&MessageLocal_Entry);

    GW::StoC::RemoveCallback<GW::Packet::StoC::SpeechBubble>(&SpeechBubble_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageNPC>(&MessageNPC_Entry);
    GW::UI::RemoveUIMessageCallback(&OnCheckboxPreferenceChanged_Entry);
    GW::UI::RemoveUIMessageCallback(&OnPlayerChatMessage_Entry);

    GW::Chat::RemoveSendChatCallback(&SendChatCallback_Entry);
    GW::Chat::RemoveStartWhisperCallback(&WhisperCallback_Entry);
    GW::Chat::RemoveWhisperCallback(&WhisperCallback_Entry);
}

void ChatSettings::Update(float)
{
    // Try to print any pending messages.
    for (auto it = pending_messages.begin(); it != pending_messages.end(); ++it) {
        PendingChatMessage* m = *it;
        if (m->IsSend() && PendingChatMessage::Cooldown()) continue;
        if (m->Consume()) {
            it = pending_messages.erase(it);
            delete m;
            if (it == pending_messages.end()) break;
        }
    }
}

void ChatSettings::DrawSettingInternal()
{
    ToolboxModule::DrawSettingInternal();

    const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel;
    if (ImGui::TreeNodeEx("Chat Colors", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::Text("Channel");
        ImGui::SameLine(chat_colors_grid_x[1]);
        ImGui::Text("Sender");
        ImGui::SameLine(chat_colors_grid_x[2]);
        ImGui::Text("Message");
        ImGui::Spacing();

        DrawChannelColor("Local", GW::Chat::Channel::CHANNEL_ALL);
        DrawChannelColor("Guild", GW::Chat::Channel::CHANNEL_GUILD);
        DrawChannelColor("Team", GW::Chat::Channel::CHANNEL_GROUP);
        DrawChannelColor("Trade", GW::Chat::Channel::CHANNEL_TRADE);
        DrawChannelColor("Alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
        DrawChannelColor("Whispers", GW::Chat::Channel::CHANNEL_WHISPER);
        DrawChannelColor("Emotes", GW::Chat::Channel::CHANNEL_EMOTE);
        DrawChannelColor("Other", GW::Chat::Channel::CHANNEL_GLOBAL);

        ImGui::TextDisabled("(Left-click on a color to edit it)");
        ImGui::TreePop();
        ImGui::Spacing();
    }
    if (ImGui::Checkbox("Show chat messages timestamp", &show_timestamps)) GW::Chat::ToggleTimestamps(show_timestamps);
    ImGui::ShowHelp("Show timestamps in message history.");
    if (show_timestamps) {
        ImGui::Indent();
        if (ImGui::Checkbox("Use 24h", &show_timestamp_24h)) GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        ImGui::SameLine();
        if (ImGui::Checkbox("Show seconds", &show_timestamp_seconds)) GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        ImGui::SameLine();
        ImGui::Text("Color:");
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("Color:", &timestamps_color, flags)) GW::Chat::SetTimestampsColor(timestamps_color);
        ImGui::Unindent();
    }
    ImGui::Checkbox("Hide player chat speech bubbles", &hide_player_speech_bubbles);
    ImGui::ShowHelp("Don't show in-game speech bubbles over player characters that send a message in chat");
    ImGui::Checkbox("Show NPC speech bubbles in emote channel", &npc_speech_bubbles_as_chat);
    ImGui::ShowHelp("Speech bubbles from NPCs and Heroes will appear as emote messages in chat");
    ImGui::Checkbox("Redirect NPC dialog to emote channel", &redirect_npc_messages_to_emote_chat);
    ImGui::ShowHelp("Messages from NPCs that would normally show on-screen and in team chat are instead redirected to the emote channel");
    if (ImGui::Checkbox("Open web links from templates", &openlinks)) {
        GW::UI::SetOpenLinks(openlinks);
    }
    ImGui::ShowHelp("Clicking on template that has a URL as name will open that URL in your browser");

    ImGui::Checkbox("Automatically change urls into build templates.", &auto_url);
    ImGui::ShowHelp("When you write a message starting with 'http://' or 'https://', it will be converted in template format");
}

void ChatSettings::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);

    LOAD_BOOL(show_timestamps);
    LOAD_BOOL(show_timestamp_24h);
    LOAD_BOOL(show_timestamp_seconds);
    LOAD_BOOL(hide_player_speech_bubbles);
    LOAD_BOOL(npc_speech_bubbles_as_chat);
    LOAD_BOOL(redirect_npc_messages_to_emote_chat);
    LOAD_BOOL(openlinks);
    LOAD_BOOL(auto_url);

    timestamps_color = Colors::Load(ini, Name(), VAR_NAME(timestamps_color), Colors::RGB(0xc0, 0xc0, 0xbf));
    GW::UI::SetOpenLinks(openlinks);
    GW::Chat::ToggleTimestamps(show_timestamps);
    GW::Chat::SetTimestampsColor(timestamps_color);
    GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
}

void ChatSettings::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);

    SAVE_BOOL(show_timestamps);
    SAVE_BOOL(show_timestamp_24h);
    SAVE_BOOL(show_timestamp_seconds);
    SAVE_BOOL(hide_player_speech_bubbles);
    SAVE_BOOL(npc_speech_bubbles_as_chat);
    SAVE_BOOL(redirect_npc_messages_to_emote_chat);
    SAVE_BOOL(openlinks);
    SAVE_BOOL(auto_url);

    Colors::Save(ini, Name(), VAR_NAME(timestamps_color), timestamps_color);
}

bool ChatSettings::WndProc(UINT Message, WPARAM wParam, LPARAM)
{
    // Open Whisper to targeted player with Ctrl + Enter
    if (Message == WM_KEYDOWN && wParam == VK_RETURN && !ctrl_enter_whisper && ImGui::GetIO().KeyCtrl && !GW::Chat::GetIsTyping()) {
        const auto target = GW::Agents::GetTargetAsAgentLiving();
        if (target && target->IsPlayer()) {
            const wchar_t* player_name = GW::PlayerMgr::GetPlayerName(target->login_number);
            ctrl_enter_whisper = true;
            GW::GameThread::Enqueue([player_name] {
                GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, const_cast<wchar_t*>(player_name));
                ctrl_enter_whisper = false;
            });
            return true;
        }
    }
    return false;
}

void ChatSettings::AddPendingMessage(PendingChatMessage* pending_chat_message)
{
    pending_messages.push_back(pending_chat_message);
}

void ChatSettings::SetAfkMessage(std::wstring&& message)
{
    static size_t MAX_AFK_MSG_LEN = 80;
    if (message.empty()) {
        afk_message.clear();
    }
    else if (message.size() <= MAX_AFK_MSG_LEN) {
        afk_message = std::move(message);
        afk_message_time = clock();
        Log::Info("Afk message set to \"%S\"", afk_message.c_str());
    }
    else {
        Log::Error("Afk message must be under 80 characters. (Yours is %zu)", message.size());
    }
}
