#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <Logger.h>
#include <Color.h>

#include <Modules/TwitchModule.h>
#include <ImGuiAddons.h>
#include <Utils/TextUtils.h>

#include <Modules/Resources.h>
#include <Timer.h>

namespace twitch_api {
    struct DeviceAuthResponse {
        std::string verification_uri;
        std::string device_code;
        int interval = 0;
    };

    struct TokenResponse {
        std::string access_token;
        std::string error;
    };
}

namespace {
    constexpr glz::opts json_opts{.error_on_unknown_keys = false};

    TwitchModule::Settings settings;

    // IRC 详情
    int irc_port = 443; // 不使用 6667，以防路由器封锁。
    const char* client_id = "8vlivyypw5qmaxtknh44t98h9uun6l";

    bool pending_connect = false;
    bool pending_disconnect = false;
    bool connected = false;
    bool show_irc_password = false;
    bool hooked = false;

    IRC irc_conn;

    GW::HookEntry SendChatCallback_Entry;
    GW::HookEntry StartWhisperCallback_Entry;

    bool fetch_oauth_token = false;
    bool cancel_fetch_oauth_token = false;
    void GetOauthToken() {
        if (fetch_oauth_token) return;
        fetch_oauth_token = true;
        Resources::EnqueueWorkerTask([] {
            const auto scopes = "chat:edit chat:read";
            const auto url = "https://id.twitch.tv/oauth2/device"; // URL 中无参数
            std::string request_body = std::format("client_id={}&scope={}", client_id, scopes);
            std::string response;
            if (!Resources::Post(url, request_body, response)) { // 设置正确的内容类型
                return Log::Warning(response.c_str()), fetch_oauth_token = false;
            }

            twitch_api::DeviceAuthResponse auth{};
            if (auto ec = glz::read<json_opts>(auth, response); ec) {
                return Log::Warning("解析 Twitch 认证响应失败"), fetch_oauth_token = false;
            }
            if (auth.verification_uri.empty() || auth.device_code.empty() || auth.interval <= 0) {
                return Log::Warning("Twitch 认证响应字段无效或缺失"), fetch_oauth_token = false;
            }

            ShellExecuteA(nullptr, "open", auth.verification_uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

            std::string access_token;

            // 轮询 Twitch 获取令牌
            const auto token_url = "https://id.twitch.tv/oauth2/token";
            auto start_time = TIMER_INIT();
            int interval = auth.interval;

            while (access_token.empty() && !cancel_fetch_oauth_token) {
                if (TIMER_DIFF(start_time) > 30000) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(interval)); // 等待轮询间隔

                // 手动格式化为表单 URL 编码数据
                std::string request_data = std::format("client_id={}&scopes={}&device_code={}&grant_type=urn:ietf:params:oauth:grant-type:device_code",
                        TextUtils::UrlEncode(client_id),
                        TextUtils::UrlEncode(scopes),
                        TextUtils::UrlEncode(auth.device_code));
                std::string token_response;

                if (!Resources::Post(token_url, request_data, token_response)) {
                    continue;
                }

                twitch_api::TokenResponse token{};
                if (auto ec = glz::read<json_opts>(token, token_response); ec) {
                    continue;
                }
                if (!token.access_token.empty()) {
                    access_token = std::move(token.access_token);
                    break;
                }
                if (!token.error.empty()) {
                    if (token.error == "authorization_pending") {
                        continue;
                    }
                    if (token.error == "slow_down") {
                        interval += 5; // 增加等待时间
                        continue;
                    }
                    Log::Warning(token.error.c_str());
                    break;
                }
            }
            if (!access_token.empty()) {
                settings.irc_password = std::move(access_token);
                Log::Info("Oauth 令牌更新成功");
            }
            return fetch_oauth_token = false;
        });
    }


    void WriteChat(const wchar_t* message, const char* nick = nullptr)
    {
        const auto sender = nick ? std::format("{} @ {}", nick, settings.irc_alias) : settings.irc_alias;
        std::wstring sender_ws = TextUtils::StringToWString(sender);
        auto message_ws = new wchar_t[255];
        size_t message_len = 0;
        const size_t original_len = wcslen(message);
        const bool is_emote = wmemcmp(message, L"\x1" L"ACTION ", 7) == 0;
        if (is_emote) {
            message_ws[message_len++] = '*';
        }
        for (size_t i = is_emote ? 8 : 0; i < original_len; i++) {
            // 在消息结尾处中断
            if (message[i] == '\x1' || !message[i]) {
                break;
            }
            // 双重转义反斜杠
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
            // 注意：消息发送到 GWCA_1 频道——据我所知目前未使用
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, message_ws, sender_ws.c_str());
            delete[] message_ws;
        });
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnJoin(const char* params, irc_reply_data* hostd, void*)
    {
        if (!params[0] || !settings.show_messages) {
            return 0; // 空消息
        }
        wchar_t buf[600];
        if (strcmp(hostd->nick, settings.irc_username.c_str()) == 0) {
            if (strcmp(&params[1], settings.irc_username.c_str()) == 0) {
                WriteChat(L"已连接");
                return 0;
            }
            swprintf(buf, 599, L"已连接到 %s 作为 %S", TextUtils::StringToWString(&params[1]).c_str(), settings.irc_username.c_str());
            WriteChat(buf);
            return 0;
        }
        if (!settings.notify_on_user_join) {
            return 0;
        }
        swprintf(buf, 599, L"%s 加入了频道。", TextUtils::StringToWString(hostd->nick).c_str());
        WriteChat(buf);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnLeave(const char* params, irc_reply_data* hostd, void*)
    {
        if (!params[0] || !settings.show_messages || !settings.notify_on_user_leave) {
            return 0; // 空消息
        }

        wchar_t buf[600];
        swprintf(buf, 599, L"%s 离开了频道。", TextUtils::StringToWString(hostd->nick).c_str());
        WriteChat(buf);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnConnected(const char* params, irc_reply_data*, void* wparam)
    {
        const auto conn = static_cast<IRC*>(wparam);
        settings.irc_username = params;
        settings.irc_username.erase(settings.irc_username.find_first_of(' '));
        // 频道 == 用户名。可以更改为连接其他 Twitch 频道/IRC 频道。
        if (settings.irc_channel.empty()) {
            settings.irc_channel = settings.irc_username;
        }
        char buf[128];
        Log::Log("%s：已连接 %s", settings.irc_alias.c_str(), params);
        sprintf(buf, "#%s", settings.irc_channel.c_str());
        conn->join(buf);
        conn->raw("CAP REQ :twitch.tv/membership twitch.tv/commands\r\n");
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnMessage(const char* params, irc_reply_data* hostd, void*)
    {
        if (!params[0] || !settings.show_messages) {
            return 0; // 空消息
        }
        const std::wstring message_ws = TextUtils::StringToWString(&params[1]);
        WriteChat(message_ws.c_str(), hostd->nick);
        Log::Log("来自 %s 的消息：%s", hostd->nick, &params[1]);
        return 0;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    int OnNotice(const char* params, irc_reply_data*, void* conn)
    {
        Log::Log("NOTICE：%s\n", params);
        if (strcmp(params, "Login authentication failed") == 0) {
            Log::Error("Twitch 连接失败 - Oauth 令牌无效");
            static_cast<IRC*>(conn)->disconnect();
            return 0;
        }
        if (params[1] && strcmp(&params[1], "Invalid NICK") == 0) {
            Log::Error("Twitch 连接失败 - 用户名无效");
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
        // 当开始向 "<irc_nickname> @ <irc_channel>" 发送私聊时，将收件人重写为 "<irc_channel>"
        RegisterUIMessageCallback(&StartWhisperCallback_Entry, GW::UI::UIMessage::kStartWhisper, [](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            wchar_t* name = *(wchar_t**)wparam;
            if (!(name && *name)) {
                return;
            }
            wchar_t buf[128];
            const std::wstring w_alias = TextUtils::StringToWString(settings.irc_alias);
            swprintf(buf, 128, L" @ %s", w_alias.c_str());
            if (std::wstring(name).find(buf) != std::wstring::npos) {
                wcscpy(name, w_alias.c_str());
            }
        });
        RegisterUIMessageCallback(&SendChatCallback_Entry, GW::UI::UIMessage::kSendChatMessage, [](GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*) {
            const auto msg = *static_cast<const wchar_t**>(wparam);
            const auto chan = GW::Chat::GetChannel(*msg);
            if (chan != GW::Chat::Channel::CHANNEL_WHISPER || !connected) {
                return;
            }
            std::string message = TextUtils::WStringToString(msg);
            const size_t sender_idx = message.find(',');
            if (sender_idx == std::string::npos) {
                return; // 发送者无效
            }
            const std::string to = message.substr(1, sender_idx - 1);
            if (to != settings.irc_alias) {
                return;
            }
            std::string content = message.substr(sender_idx + 1);
            Log::Log("发送到 IRC：%s", content.c_str());
            if (irc_conn.raw("PRIVMSG #%s :%s\r\n", settings.irc_channel.c_str(), content.c_str())) {
                Log::Error("发送消息失败");
            }
            else {
                irc_reply_data d{};
                d.nick = const_cast<char*>(settings.irc_username.c_str());
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
    SettingsRegistry::Register(this, settings);

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

bool TwitchModule::IsConnected() { return connected; }
IRC* TwitchModule::irc() { return &irc_conn; }

void TwitchModule::Disconnect()
{
    connected = irc_conn.is_connected();
    if (!connected) {
        return;
    }
    irc_conn.disconnect();
    connected = irc_conn.is_connected();
}

void TwitchModule::SignalTerminate() {
    Disconnect();
    GW::UI::RemoveUIMessageCallback(&SendChatCallback_Entry);
    GW::UI::RemoveUIMessageCallback(&StartWhisperCallback_Entry);
    cancel_fetch_oauth_token = true;
}
bool TwitchModule::CanTerminate() {
    return !fetch_oauth_token;
}

bool TwitchModule::Connect()
{
    const auto irc_connection = irc();
    if (!settings.twitch_enabled) {
        return true;
    }
    connected = irc_connection->is_connected();
    if (connected) {
        return true;
    }
    printf("正在连接到 IRC\n");
    if (settings.irc_server.empty()) {
        printf("服务器名称无效！\n");
        return false;
    }
    if (settings.irc_password == "oauth:<your_token_here>") {
        return false;
    }
    std::ranges::transform(
        settings.irc_server, settings.irc_server.begin(),
        [](const char c) -> char {
            return static_cast<char>(tolower(c));
        });
    /*std::transform(irc_username.begin(), irc_username.end(), irc_username.begin(),
        [](unsigned char c) { return std::tolower(c); });*/
    std::ranges::transform(
        settings.irc_channel, settings.irc_channel.begin(),
        [](const char c) -> char {
            return static_cast<char>(tolower(c));
        });

    auto with_oauth_prefix = std::format("{}{}", settings.irc_password.starts_with("oauth:") ? "" : "oauth:", settings.irc_password);
    if (irc_connection->start(
            settings.irc_server.c_str(),
            irc_port,
            "unused",
            "unused",
            "unused", with_oauth_prefix.c_str()) != 0) {
        printf("IRC::start 失败！\n");
        return false;
    }
    printf("已连接到 IRC！\n");
    return connected = irc_connection->is_connected();
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
    if (ImGui::Checkbox("启用 Twitch 集成", &settings.twitch_enabled)) {
        Disconnect();
    }
    ImGui::ShowHelp("允许 GWToolbox 使用游戏内聊天窗口与 Twitch 收发消息。");
    if (settings.twitch_enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, connected ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        if (ImGui::Button(connected ? "已连接" : "已断开", ImVec2(0, 0))) {
            if (connected) {
                pending_disconnect = true;
            }
            else {
                pending_connect = true;
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(connected ? "点击断开" : "点击连接");
        }
        ImGui::Indent();

        ImGui::Checkbox("在聊天窗口中显示消息。颜色：", &settings.show_messages);
        ImGui::SameLine();
        constexpr ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel;
        if (Colors::DrawSettingHueWheel("Twitch 颜色：", &settings.irc_chat_color.value, flags)) {
            SetSenderColor(GW::Chat::Channel::CHANNEL_GWCA1, settings.irc_chat_color);
        }
        ImGui::CheckboxWithHelp("用户离开时通知", &settings.notify_on_user_leave, "当观众离开 Twitch 频道时在聊天窗口中收到消息");
        ImGui::CheckboxWithHelp("用户加入时通知", &settings.notify_on_user_join, "当观众加入 Twitch 频道时在聊天窗口中收到消息");

        const float width = ImGui::GetContentRegionAvail().x / 2;
        ImGui::PushItemWidth(width);
        ImGui::InputText("Twitch Oauth Token", settings.irc_password, 255, show_irc_password ? 0 : ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();
        ImGui::ShowHelp("用于连接 Twitch。\n例如 oauth:3fplxiscsq1550zdkf8z2kh1jk7mqs");
        ImGui::SameLine();
        ImGui::Checkbox("显示", &show_irc_password);
        ImGui::Indent();
        const ImColor col(102, 187, 238, 255);
        ImGui::TextColored(col.Value, "点击此处获取 Twitch Oauth 令牌");
        if (ImGui::IsItemClicked()) {
            GetOauthToken();
        }
        ImGui::Unindent();
        ImGui::PushItemWidth(width);
        ImGui::InputText("Twitch 频道", settings.irc_channel, 56);
        ImGui::ShowHelp("您要连接的频道对应的 Twitch 用户名。\n在此输入您自己的 Twitch 用户名，以便在直播时接收频道消息。");
        ImGui::PopItemWidth();
        ImGui::TextDisabled("更改后重新连接以使更新信息生效");
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void TwitchModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    show_irc_password = settings.irc_password == "oauth:<your_token_here>";

    SetSenderColor(GW::Chat::Channel::CHANNEL_GWCA1, settings.irc_chat_color);

    pending_connect = true;
}

void TwitchModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}
