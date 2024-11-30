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
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Modules/GameSettings.h>
#include <Utils/ToolboxUtils.h>
#include <Defines.h>
#include <Utils/GuiUtils.h>
#include "ChatSettings.h"
#include <Utils/TextUtils.h>

namespace {
    // Settings
    bool show_timestamps = false;
    bool hide_player_speech_bubbles = false;
    bool show_timestamp_seconds = false;
    bool show_timestamp_24h = false;
    bool npc_speech_bubbles_as_chat = false;
    bool redirect_npc_messages_to_emote_chat = false;
    bool redirect_outgoing_whisper_to_whisper_channel = false;
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
    GW::HookEntry MessageNPC_Entry;
    GW::HookEntry MessageLocal_Entry;
    GW::HookEntry MessageServer_Entry;
    GW::HookEntry OnPlayerChatMessage_Entry;
    GW::HookEntry OnWriteToChatLog_Entry;
    GW::HookEntry OnUIMessage_Entry;

    // used by chat colors grid
    constexpr float chat_colors_grid_x[] = {0, 100, 160, 240};
    std::vector<PendingChatMessage*> pending_messages;

    std::wstring PrintTime(const DWORD time_sec)
    {
        const DWORD secs = time_sec % 60;
        const DWORD minutes = time_sec / 60 % 60;
        const DWORD hours = time_sec / 3600;
        DWORD time = 0;
        auto time_unit = L"";
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
        if (time > 1)
            return std::format(L"{} {}s", time, time_unit);
        return std::format(L"{} {}", time, time_unit);
    }

    void DrawChannelColor(const char* name, const GW::Chat::Channel chan)
    {
        ImGui::PushID(chan);
        ImGui::Text(name);
        constexpr ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel;
        GW::Chat::Color color, sender_col, message_col;
        GetChannelColors(chan, &sender_col, &message_col);

        ImGui::SameLine(chat_colors_grid_x[1]);
        color = sender_col;
        if (Colors::DrawSettingHueWheel("Sender Color:", &color, flags) && color != sender_col) {
            SetSenderColor(chan, color);
        }

        ImGui::SameLine(chat_colors_grid_x[2]);
        color = message_col;
        if (Colors::DrawSettingHueWheel("Message Color:", &color, flags) && color != message_col) {
            SetMessageColor(chan, color);
        }

        ImGui::SameLine(chat_colors_grid_x[3]);
        if (ImGui::Button("Reset")) {
            GW::Chat::Color col1, col2;
            GetDefaultColors(chan, &col1, &col2);
            SetSenderColor(chan, col1);
            SetMessageColor(chan, col2);
        }
        ImGui::PopID();
    }


    std::wstring rewritten_message;

    // Allow clickable name when a player pings "I'm following X" or "I'm targeting X"
    void OnLocalChatMessage(GW::HookStatus* status, GW::UI::UIMessage, void* wParam, void*)
    {
        if (status->blocked)
            return; // Sender blocked, packet handled.
        auto packet = (GW::UI::UIPacket::kPlayerChatMessage*)wParam;
        if (packet->channel != GW::Chat::Channel::CHANNEL_GROUP)
            return;
        if (*packet->message != 0x778 && *packet->message != 0x781)
            return; // Not "I'm Following X" or "I'm Targeting X" message.

        rewritten_message = packet->message;
        size_t start_idx = rewritten_message.find(L"\xba9\x107");
        if (start_idx == std::wstring::npos) {
            return; // Not a player name.
        }
        start_idx += 2;
        const size_t end_idx = rewritten_message.find(L'\x1', start_idx);
        if (end_idx == std::wstring::npos) {
            return; // Not a player name, this should never happen.
        }
        const std::wstring player_pinged = TextUtils::SanitizePlayerName(rewritten_message.substr(start_idx, end_idx - start_idx));
        if (player_pinged.empty()) {
            return; // No recipient
        }
        const auto sender = GW::PlayerMgr::GetPlayerByID(packet->player_number);
        if (!sender)
            return; // No sender
        if (GameSettings::GetSettingBool("flash_window_on_name_ping") && ToolboxUtils::GetPlayerName() == player_pinged) {
            GuiUtils::FlashWindow(); // Flash window - we've been followed!
        }
        // Allow clickable player name
        rewritten_message.insert(start_idx, L"<a=1>");
        rewritten_message.insert(end_idx + 5, L"</a>");

        packet->message = rewritten_message.data();
    }

    // Print NPC speech bubbles to emote chat.
    void OnSpeechBubble(const GW::HookStatus*, const GW::Packet::StoC::SpeechBubble* pak)
    {
        if (!npc_speech_bubbles_as_chat || !pak->message || !pak->agent_id) {
            return; // Disabled, invalid, or pending another speech bubble
        }
        size_t len = 0;
        for (size_t i = 0; pak->message[i] != 0; i++) {
            len = i + 1;
        }
        if (len < 3) {
            return; // Shout skill etc
        }
        const auto agent = reinterpret_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
        if (!agent || agent->login_number) {
            return; // Agent not found or Speech bubble from player e.g. drunk message.
        }
        PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, GW::Agents::GetAgentEncName(agent));
        if (m) {
            pending_messages.push_back(m);
        }
    }

    // NPC dialog messages to emote chat
    void OnSpeechDialogue(GW::HookStatus* status, const GW::Packet::StoC::DisplayDialogue* pak)
    {
        if (!redirect_npc_messages_to_emote_chat) {
            return; // Disabled or message pending
        }
        WriteChatEnc(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, pak->name);
        status->blocked = true; // consume original packet.
    }

    // Turn /wiki into /wiki <location>
    bool converting_message_into_url = false;
    void OnSendChat(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        ASSERT(message_id == GW::UI::UIMessage::kSendChatMessage);
        auto packet = (GW::UI::UIPacket::kSendChatMessage*)wparam;
        if (converting_message_into_url || !(auto_url && packet->message && *packet->message)) {
            return;
        }
        const auto channel = GW::Chat::GetChannel(*packet->message);
        auto msg = &packet->message[1];
        switch (channel) {
        case GW::Chat::Channel::CHANNEL_WHISPER: {
            // msg == "Whisper Target Name,msg"
            const auto sep = wcschr(msg, ',');
            if (!sep)
                return; // Invalid format for message; no separator between recipient and message
            msg = &sep[1];
        } break;
        case GW::Chat::Channel::CHANNEL_GUILD:
        case GW::Chat::Channel::CHANNEL_ALLIANCE:
        case GW::Chat::Channel::CHANNEL_TRADE:
        case GW::Chat::Channel::CHANNEL_GROUP:
            break;
        default:
            return; // Channel doesn't support templates
        }

        auto url_found = wcsstr(msg, L"http://");
        if(!url_found)
            url_found = wcsstr(msg, L"https://");
        if (!url_found)
            return;

        const auto unused_msg_length = 121 - wcslen(packet->message);
        if (unused_msg_length < 5)
            return; // No space to convert into a template

        auto url_end = wcschr(url_found, ' ');
        if (!url_end) {
            url_end = url_found + wcslen(url_found);
        }
        GW::UI::UIPacket::kSendChatMessage new_packet = *packet;
        std::wstring new_msg;
        // Prefix
        new_msg.append(packet->message, url_found - packet->message);
        // URL via equipment template
        new_msg.append(L"[");
        new_msg.append(url_found, url_end - url_found);
        new_msg.append(L";xx]");
        // Suffix
        new_msg.append(url_end);
        new_packet.message = new_msg.data();
        status->blocked = true;
        converting_message_into_url = true;
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendChatMessage, &new_packet);
        converting_message_into_url = false;
    }

    // Open links on player name click, Ctrl + click name to target, Ctrl + Shift + click name to invite
    void OnStartWhisper(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*)
    {
        const auto name = *(wchar_t**)wparam;
        if (!(name && *name)) {
            return;
        }
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
        if (!ImGui::GetIO().KeyCtrl) {
            return; // - Next logic only applicable when Ctrl is held
        }

        const std::wstring _name = TextUtils::SanitizePlayerName(name);
        if (ImGui::GetIO().KeyShift && GW::PartyMgr::GetIsLeader()) {
            const auto cmd = std::format(L"invite {}", _name);
            GW::Chat::SendChat('/', cmd.c_str());
            status->blocked = true;
            return;
        }
        const auto player = ToolboxUtils::GetPlayerByName(_name.c_str());
        if (!ctrl_enter_whisper && player && GW::Agents::GetAgentByID(player->agent_id)) {
            GW::Agents::ChangeTarget(player->agent_id);
            status->blocked = true;
        }
    }

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void* lParam) {
        if (status->blocked)
            return;
        switch (message_id) {
            case GW::UI::UIMessage::kPreferenceFlagChanged: {
                // Remember user setting for chat timestamps
                const auto packet = (GW::UI::UIPacket::kPreferenceFlagChanged*)wParam;
                if (packet->preference_id == GW::UI::FlagPreference::ShowChatTimestamps)
                    show_timestamps = packet->new_value ? true : false;
            } break;
            case GW::UI::UIMessage::kPlayerChatMessage: {
                OnLocalChatMessage(status, message_id, wParam, lParam);
                // Hide player chat message speech bubbles by redirecting from 0x10000081 to 0x1000007E
                if (hide_player_speech_bubbles)
                    return;
                const auto packet = (GW::UI::UIPacket::kPlayerChatMessage*)wParam;
                const auto agent = GW::PlayerMgr::GetPlayerByID(packet->player_number);
                if (!agent)
                    return;
                status->blocked = true;
                GW::Chat::WriteChatEnc(packet->channel, packet->message, agent->name_enc);
            } break;
            case GW::UI::UIMessage::kWriteToChatLog: {
                // Redirect outgoing whispers to the whisper channel; allows sender to be coloured
                auto param = (GW::UI::UIChatMessage*)wParam;
                if (redirect_outgoing_whisper_to_whisper_channel && param->channel == GW::Chat::Channel::CHANNEL_GLOBAL && *param->message == 0x76e)
                    param->channel = GW::Chat::Channel::CHANNEL_WHISPER;

                // Send /age2 on /age
                if (GameSettings::GetSettingBool("auto_age2_on_age") 
                    && param->channel == GW::Chat::Channel::CHANNEL_GLOBAL) {
                    // 0x8101 0x641F 0x86C3 0xE149 0x53E8 0x101 0x107 = You have been in this map for n minutes.
                    // 0x8101 0x641E 0xE7AD 0xEF64 0x1676 0x101 0x107 0x102 0x107 = You have been in this map for n hours and n minutes.
                    if (wmemcmp(param->message, L"\x8101\x641F\x86C3\xE149\x53E8", 5) == 0 || wmemcmp(param->message, L"\x8101\x641E\xE7AD\xEF64\x1676", 5) == 0) {
                        GW::Chat::SendChat('/', "age2");
                    }
                }
            } break;
            case GW::UI::UIMessage::kWriteToChatLogWithSender: {
                // Redirect NPC messages from team chat to emote chat
                if (!redirect_npc_messages_to_emote_chat)
                    return;
                auto param = (GW::UI::UIPacket::kWriteToChatLogWithSender*)wParam;
                if (param->channel == GW::Chat::Channel::CHANNEL_GROUP)
                    param->channel = GW::Chat::Channel::CHANNEL_EMOTE;
            } break;
            case GW::UI::UIMessage::kStartWhisper: {
                OnStartWhisper(status, message_id, wParam, lParam);
            } break;
            case GW::UI::UIMessage::kSendChatMessage: {
                OnSendChat(status, message_id, wParam, lParam);
            } break;
            case GW::UI::UIMessage::kRecvWhisper: {
                // Automatically send afk message when whisper is received
                auto param = (GW::UI::UIPacket::kRecvWhisper*)wParam;
                ASSERT(param->from && *param->from);

                if (!(GW::FriendListMgr::GetMyStatus() == GW::FriendStatus::Away && !afk_message.empty() && ToolboxUtils::GetPlayerName() != param->from))
                    break;
                const auto reply = std::format(L"Automatic message: \"{}\" ({} ago)", afk_message, PrintTime((clock() - afk_message_time) / CLOCKS_PER_SEC));
                GW::Chat::SendChat(param->from, reply.c_str());
            } break;

        }
    }
}

void ChatSettings::Initialize()
{
    ToolboxModule::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SpeechBubble>(&SpeechBubble_Entry, OnSpeechBubble);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, OnSpeechDialogue);

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kPreferenceFlagChanged,
        GW::UI::UIMessage::kPlayerChatMessage,
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kWriteToChatLogWithSender,
        GW::UI::UIMessage::kRecvWhisper,
        GW::UI::UIMessage::kStartWhisper,
        GW::UI::UIMessage::kSendChatMessage
    };
    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_Entry, message_id, OnUIMessage);
    }
}

void ChatSettings::Terminate()
{
    ToolboxModule::Terminate();

    GW::StoC::RemoveCallback<GW::Packet::StoC::SpeechBubble>(&SpeechBubble_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry);

    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Entry);
}

void ChatSettings::Update(float)
{
    // Try to print any pending messages.
    for (auto it = pending_messages.begin(); it != pending_messages.end(); ++it) {
        PendingChatMessage* m = *it;
        if (m->IsSend() && PendingChatMessage::Cooldown()) {
            continue;
        }
        if (m->Consume()) {
            it = pending_messages.erase(it);
            delete m;
            if (it == pending_messages.end()) {
                break;
            }
        }
    }
}

void ChatSettings::DrawSettingsInternal()
{
    ToolboxModule::DrawSettingsInternal();

    constexpr ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel;
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
    show_timestamps = GW::UI::GetPreference(GW::UI::FlagPreference::ShowChatTimestamps);
    if (ImGui::Checkbox("Show chat messages timestamp", &show_timestamps)) {
        GW::Chat::ToggleTimestamps(show_timestamps);
    }
    ImGui::ShowHelp("Show timestamps in message history.");
    if (show_timestamps) {
        ImGui::Indent();
        if (ImGui::Checkbox("Use 24h", &show_timestamp_24h)) {
            GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Show seconds", &show_timestamp_seconds)) {
            GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        }
        ImGui::SameLine();
        ImGui::Text("Color:");
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("Color:", &timestamps_color, flags)) {
            GW::Chat::SetTimestampsColor(timestamps_color);
        }
        ImGui::Unindent();
    }
    ImGui::Checkbox("Hide player chat speech bubbles", &hide_player_speech_bubbles);
    ImGui::ShowHelp("Don't show in-game speech bubbles over player characters that send a message in chat");
    ImGui::Checkbox("Show NPC speech bubbles in emote channel", &npc_speech_bubbles_as_chat);
    ImGui::ShowHelp("Speech bubbles from NPCs and Heroes will appear as emote messages in chat");
    ImGui::Checkbox("Redirect NPC dialog to emote channel", &redirect_npc_messages_to_emote_chat);
    ImGui::ShowHelp("Messages from NPCs that would normally show on-screen and in team chat are instead redirected to the emote channel");
    ImGui::Checkbox("Redirect outgoing whispers to whisper channel", &redirect_outgoing_whisper_to_whisper_channel);
    ImGui::ShowHelp("Whispers that you send are typically shown in colour of the global channel (green).\n"
                    "This setting makes them appear blue like an incoming message, with -> before the name.");
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
    // NB: Don't want to override the current in-game setting if not found in ini
    show_timestamps = ini->GetBoolValue(Name(), VAR_NAME(show_timestamps), GW::UI::GetPreference(GW::UI::FlagPreference::ShowChatTimestamps));
    LOAD_BOOL(show_timestamp_24h);
    LOAD_BOOL(show_timestamp_seconds);
    LOAD_BOOL(hide_player_speech_bubbles);
    LOAD_BOOL(npc_speech_bubbles_as_chat);
    LOAD_BOOL(redirect_npc_messages_to_emote_chat);
    LOAD_BOOL(redirect_outgoing_whisper_to_whisper_channel);
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
    SAVE_BOOL(redirect_outgoing_whisper_to_whisper_channel);
    SAVE_BOOL(openlinks);
    SAVE_BOOL(auto_url);

    SAVE_COLOR(timestamps_color);
}

bool ChatSettings::WndProc(const UINT Message, const WPARAM wParam, LPARAM)
{
    // Open Whisper to targeted player with Ctrl + Enter
    if (Message == WM_KEYDOWN && wParam == VK_RETURN && !ctrl_enter_whisper && ImGui::GetIO().KeyCtrl && !GW::Chat::GetIsTyping()) {
        const auto target = GW::Agents::GetTargetAsAgentLiving();
        if (target && target->IsPlayer()) {
            const wchar_t* player_name = GW::PlayerMgr::GetPlayerName(target->login_number);
            ctrl_enter_whisper = true;
            GW::GameThread::Enqueue([player_name] {
                SendUIMessage(GW::UI::UIMessage::kOpenWhisper, const_cast<wchar_t*>(player_name));
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
