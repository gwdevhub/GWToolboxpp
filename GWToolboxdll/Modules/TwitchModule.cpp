#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Defines.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <GWCA/Managers/UIMgr.h>

#include <Modules/TwitchModule.h>

namespace {
    // IRC details
    std::string irc_server = "irc.chat.twitch.tv";
    int irc_port = 443; // Not 6667, just in case router blocks it.
    std::string irc_username = "";
    std::string irc_password = "oauth:<your_token_here>";
    std::string irc_channel = "";
    std::string irc_alias = "Twitch";
    Color irc_chat_color = Colors::RGB(0xAD, 0x83, 0xFA);

    bool show_messages = true;
    bool notify_on_user_leave = true;
    bool notify_on_user_join = true;
    bool pending_connect = false;
    bool pending_disconnect = false;
    bool connected = false;
    bool show_irc_password = false;
    bool twitch_enabled = true;
    bool hooked = false;

    char message_buffer[1024] = {0};

    IRC irc_conn;

    GW::HookEntry SendChatCallback_Entry;
    GW::HookEntry StartWhisperCallback_Entry;




    void WriteChat(const wchar_t* message, const char* nick = nullptr)
    {
        char sender[128];
        if (nick) {
            snprintf(sender, sizeof(sender) / sizeof(*sender), "%s @ %s", nick, irc_alias.c_str());
        }
        else {
            snprintf(sender, sizeof(sender) / sizeof(*sender), "%s", irc_alias.c_str());
        }
        std::wstring sender_ws = GuiUtils::StringToWString(sender);
        auto message_ws = new wchar_t[255];
        size_t message_len = 0;
        const size_t original_len = wcslen(message);
        const bool is_emote = wmemcmp(message, L"\x1" L"ACTION ", 7) == 0;
        if (is_emote) {
            message_ws[message_len++] = '*';
        }
        for (size_t i = is_emote ? 8 : 0; i < original_len; i++) {
            // Break on the end of the message
            if (message[i] == '\x1' || !message[i]) {
                break;
            }
            // Double escape backsashes
            if (message[i] == '\\') {
                message_ws[message_len++] = message[i];
            }
            if (message_len >= 254) {
                break;
            }
            message_ws[message_len++] = message[i];
        }
        if (is_emote) {
            message_ws[message_len++] = '*';
        }
        message_ws[message_len] = 0;
        if (!message_len) {
            delete[] message_ws;
            return;
        }
        GW::GameThread::Enqueue([message_ws, sender_ws] {
            // NOTE: Messages are sent to the GWCA_1 channel - unused atm as far as i can see
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, message_ws, sender_ws.c_str());
            delete[] message_ws;
        });
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnJoin(const char* params, irc_reply_data* hostd, void*)
    {
        if (!params[0] || !show_messages) {
            return 0; // Empty msg
        }
        wchar_t buf[600];
        if (strcmp(hostd->nick, irc_username.c_str()) == 0) {
            if (strcmp(&params[1], irc_username.c_str()) == 0) {
                WriteChat(L"Connected");
                return 0;
            }
            swprintf(buf, 599, L"Connected to %s as %S", GuiUtils::StringToWString(&params[1]).c_str(), irc_username.c_str());
            WriteChat(buf);
            return 0;
        }
        if (!notify_on_user_join) {
            return 0;
        }
        swprintf(buf, 599, L"%s joined your channel.", GuiUtils::StringToWString(hostd->nick).c_str());
        WriteChat(buf);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnLeave(const char* params, irc_reply_data* hostd, void*)
    {
        if (!params[0] || !show_messages || !notify_on_user_leave) {
            return 0; // Empty msg
        }

        wchar_t buf[600];
        swprintf(buf, 599, L"%s left your channel.", GuiUtils::StringToWString(hostd->nick).c_str());
        WriteChat(buf);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnConnected(const char* params, irc_reply_data*, void* wparam)
    {
        const auto conn = static_cast<IRC*>(wparam);
        // Set the username to be the connected name.
        irc_username = params;
        irc_username.erase(irc_username.find_first_of(' '));
        // Channel == username. This could be changed to connect to other Twitch channels/IRC channels.
        if (irc_channel[0] == 0) {
            irc_channel = irc_username;
        }
        char buf[128];
        Log::Log("%s: Connected %s", irc_alias.c_str(), params);
        sprintf(buf, "#%s", irc_channel.c_str());
        conn->join(buf);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnMessage(const char* params, irc_reply_data* hostd, void*)
    {
        if (!params[0] || !show_messages) {
            return 0; // Empty msg
        }
        const std::wstring message_ws = GuiUtils::StringToWString(&params[1]);
        WriteChat(message_ws.c_str(), hostd->nick);
        Log::Log("Message from %s: %s", hostd->nick, &params[1]);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnNotice(const char* params, irc_reply_data*, void* conn)
    {
        Log::Log("NOTICE: %s\n", params);
        if (strcmp(params, "Login authentication failed") == 0) {
            Log::Error("Twitch Failed to connect - Invalid Oauth token");
            static_cast<IRC*>(conn)->disconnect();
            return 0;
        }
        if (params[1] && strcmp(&params[1], "Invalid NICK") == 0) {
            Log::Error("Twitch Failed to connect - Invalid Username");
            static_cast<IRC*>(conn)->disconnect();
            return 0;
        }

        return 0;
    }

    void AddHooks()
    {
        if (hooked) {
            return;
        }
        hooked = true;
        // When starting a whisper to "<irc_nickname> @ <irc_channel>", rewrite recipient to be "<irc_channel>"
        GW::UI::RegisterUIMessageCallback(&StartWhisperCallback_Entry, GW::UI::UIMessage::kStartWhisper, [](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            wchar_t* name = *(wchar_t**)wparam;
            if (!(name && *name)) {
                return;
            }
            wchar_t buf[128];
            const std::wstring walias = GuiUtils::StringToWString(irc_alias);
            swprintf(buf, 128, L" @ %s", walias.c_str());
            if (std::wstring(name).find(buf) != std::wstring::npos) {
                wcscpy(name, walias.c_str());
            }
            });
        GW::UI::RegisterUIMessageCallback(&SendChatCallback_Entry, GW::UI::UIMessage::kSendChatMessage, [](GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*) {
            wchar_t* msg = *(wchar_t**)wparam;
            GW::Chat::Channel chan = GW::Chat::GetChannel(*msg);
            if (chan != GW::Chat::Channel::CHANNEL_WHISPER || !connected) {
                return;
            }
            wchar_t msgcpy[255];
            wcscpy(msgcpy, msg);
            std::string message = GuiUtils::WStringToString(msgcpy);
            const size_t sender_idx = message.find(',');
            if (sender_idx == std::string::npos) {
                return; // Invalid sender
            }
            const std::string to = message.substr(0, sender_idx);
            if (to.compare(irc_alias) != 0) {
                return;
            }
            std::string content = message.substr(sender_idx + 1);
            Log::Log("Sending to IRC: %s", content.c_str());
            if (irc_conn.raw("PRIVMSG #%s :%s\r\n", irc_channel.c_str(), content.c_str())) {
                Log::Error("Failed to send message");
            }
            else {
                irc_reply_data d{};
                d.nick = const_cast<char*>(irc_username.c_str());
                content.insert(0, ":");
                OnMessage(content.c_str(), &d, &irc_conn);
            }
            status->blocked = true;
            });
    }
}

void TwitchModule::Initialize()
{
    ToolboxModule::Initialize();

    irc_server.resize(255);
    irc_username.resize(32);
    irc_password.resize(255);
    irc_channel.resize(56);

    irc_conn.hook_irc_command("376", &OnConnected);
    irc_conn.hook_irc_command("JOIN", &OnJoin);
    irc_conn.hook_irc_command("PART", &OnLeave);
    irc_conn.hook_irc_command("PRIVMSG", &OnMessage);
    irc_conn.hook_irc_command("NOTICE", &OnNotice);

    AddHooks();

    Color col1, col2;
    GetChannelColors(GW::Chat::Channel::CHANNEL_GUILD, &col1, &col2);
    SetMessageColor(GW::Chat::Channel::CHANNEL_GWCA1, col2);
}



bool TwitchModule::isConnected() { return connected; };
IRC* TwitchModule::irc() { return &irc_conn; };

void TwitchModule::Disconnect()
{
    connected = irc_conn.is_connected();
    if (!connected) {
        return;
    }
    irc_conn.disconnect();
    connected = irc_conn.is_connected();
}
void TwitchModule::Terminate()
{
    ToolboxModule::Terminate();
    Disconnect();
    GW::UI::RemoveUIMessageCallback(&SendChatCallback_Entry);
    GW::UI::RemoveUIMessageCallback(&StartWhisperCallback_Entry);
}

bool TwitchModule::Connect()
{
    auto conn = irc();
    if (!twitch_enabled) {
        return true;
    }
    connected = conn->is_connected();
    if (connected) {
        return true;
    }
    printf("Connecting to IRC\n");
    if (irc_server.empty()) {
        printf("Invalid server name!\n");
        return false;
    }
    if (strcmp(irc_password.c_str(), "oauth:<your_token_here>") == 0) {
        return false;
    }
    /*if(irc_username.empty()) {
        printf("Invalid username!\n");
        return false;
    }*/
    // Sanitise strings to lower case
    std::ranges::transform(irc_server, irc_server.begin(),
                           [](const char c) -> char {
                               return static_cast<char>(tolower(c));
                           });
    /*std::transform(irc_username.begin(), irc_username.end(), irc_username.begin(),
        [](unsigned char c) { return std::tolower(c); });*/
    std::ranges::transform(irc_channel, irc_channel.begin(),
                           [](const char c) -> char {
                               return static_cast<char>(tolower(c));
                           });

    if (conn->start(
            irc_server.c_str(),
            irc_port,
            "unused",
            "unused",
            "unused",
            irc_password.c_str()) != 0) {
        printf("IRC::start failed!\n");
        return false;
    }
    printf("Connected to IRC!\n");
    return connected = conn->is_connected();
}

void TwitchModule::Update(const float)
{
    connected = irc_conn.is_connected();
    if (pending_disconnect) {
        Disconnect();
        pending_connect = pending_disconnect = false;
    }
    if (pending_connect) {
        Connect();
        pending_connect = pending_disconnect = false;
    }
    if (connected) {
        irc_conn.ping();
    }
}

void TwitchModule::DrawSettingsInternal()
{
    ImGui::PushID("twitch_settings");
    if (ImGui::Checkbox("Enable Twitch integration", &twitch_enabled)) {
        Disconnect();
    }
    ImGui::ShowHelp("Allows GWToolbox to send & receive messages with Twitch using the in-game chat window.");
    if (twitch_enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, connected ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        if (ImGui::Button(connected ? "Connected" : "Disconnected", ImVec2(0, 0))) {
            if (connected) {
                pending_disconnect = true;
            }
            else {
                pending_connect = true;
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(connected ? "Click to disconnect" : "Click to connect");
        }
        ImGui::Indent();

        ImGui::Checkbox("Show messages in chat window. Color:", &show_messages);
        ImGui::SameLine();
        constexpr ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel;
        if (Colors::DrawSettingHueWheel("Twitch Color:", &irc_chat_color, flags)) {
            SetSenderColor(GW::Chat::Channel::CHANNEL_GWCA1, irc_chat_color);
        }
        ImGui::Checkbox("Notify on user leave", &notify_on_user_leave);
        ImGui::ShowHelp("Receive a message in the chat window when a viewer leaves the Twitch Channel");
        ImGui::Checkbox("Notify on user join", &notify_on_user_join);
        ImGui::ShowHelp("Receive a message in the chat window when a viewer joins the Twitch Channel");

        const float width = ImGui::GetContentRegionAvail().x / 2;
        ImGui::PushItemWidth(width);
        /*ImGui::InputText("Twitch Alias", const_cast<char*>(irc_alias.c_str()), 32);
        ImGui::ShowHelp("Sending a whisper to this name will send the message to Twitch.\nCannot contain spaces.");
        ImGui::InputText("Twitch Server", const_cast<char*>(irc_server.c_str()), 255);
        ImGui::ShowHelp("Shouldn't need to change this.\nDefault: irc.chat.twitch.tv");
        ImGui::InputText("Twitch Username", const_cast<char*>(irc_username.c_str()), 32);
        ImGui::ShowHelp("Your username that you use for Twitch.");*/
        ImGui::InputText("Twitch Oauth Token", irc_password.data(), 255, show_irc_password ? 0 : ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();
        ImGui::ShowHelp("Used to connect to Twitch.\ne.g. oauth:3fplxiscsq1550zdkf8z2kh1jk7mqs");
        ImGui::SameLine();
        ImGui::Checkbox("Show", &show_irc_password);
        ImGui::Indent();
        const ImColor col(102, 187, 238, 255);
        ImGui::TextColored(col.Value, "Click Here to get a Twitch Oauth Token");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Go to %s", "https://twitchapps.com/tmi/");
        }
        if (ImGui::IsItemClicked()) {
            ShellExecute(nullptr, "open", "https://twitchapps.com/tmi/", nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::Unindent();
        ImGui::PushItemWidth(width);
        ImGui::InputText("Twitch Channel", irc_channel.data(), 56);
        ImGui::ShowHelp("The Twitch username of the person who's channel you want to connect to.\nEnter your own Twitch username here to receive messages from your channel whilst streaming.");
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Re-connect after making changes to use updated info");
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void TwitchModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);

    LOAD_STRING(irc_alias);
    LOAD_STRING(irc_server);
    LOAD_STRING(irc_username);
    LOAD_STRING(irc_password);
    LOAD_STRING(irc_channel);

    LOAD_BOOL(twitch_enabled);
    LOAD_BOOL(show_messages);
    LOAD_BOOL(notify_on_user_join);
    LOAD_BOOL(notify_on_user_leave);

    LOAD_COLOR(irc_chat_color);
    show_irc_password = strcmp(irc_password.c_str(), "oauth:<your_token_here>") == 0;

    SetSenderColor(GW::Chat::Channel::CHANNEL_GWCA1, irc_chat_color);

    pending_connect = true;
}

void TwitchModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);

    SAVE_STRING(irc_alias);
    SAVE_STRING(irc_server);
    SAVE_STRING(irc_username);
    SAVE_STRING(irc_password);
    SAVE_STRING(irc_channel);

    SAVE_BOOL(twitch_enabled);
    SAVE_BOOL(show_messages);
    SAVE_BOOL(notify_on_user_join);
    SAVE_BOOL(notify_on_user_leave);

    SAVE_COLOR(irc_chat_color);
}
