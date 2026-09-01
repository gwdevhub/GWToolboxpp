/*
    模块：跟踪当前 Teamspeak 3 状态

    最初创建是因为我厌倦了为不同的 TS3 服务器绑定不同的热键来发送到聊天。

    增强功能：
     + 过滤传入的 HTTP URL，在适用时重构为 ts3server URL 协议。
     + 类似 Overwolf 的 Teamspeak 覆盖层，但没那么糟糕
     + 向/从当前频道的消息
     + 向/从其他 TS3 用户的耳语

     -- Jon
*/

#include "stdafx.h"
#include <GWCA/Managers/ChatMgr.h>

#include <Defines.h>

#include <ImGuiAddons.h>

#include <Modules/Resources.h>
#include <Modules/TeamspeakModule.h>
#include <Timer.h>
#include <Utils/TextUtils.h>

#include <GWCA/Utilities/Hook.h>

namespace teamspeak_invite_api {
    struct CreateRequest {
        std::string address;
        std::string name;
        std::string password; // 无密码时为空
        std::string channel_id;
        std::string channel_name;
        double expires_in_days = 1.0;
    };

    struct CreateResponse {
        std::string id;
    };
}

namespace {

    constexpr glz::opts json_opts{.error_on_unknown_keys = false};
    GW::HookEntry ChatCmd_HookEntry;

    TeamspeakModule::Settings settings;

    const char* teamspeak3_host = "127.0.0.1";
    u_short teamspeak3_port = 25639;

    clock_t check_interval = 0;
    clock_t last_check = 0;

    enum ConnectionStep : uint8_t {
        Idle,
        Connecting,
    };

    ConnectionStep step = Idle;

    bool pending_connect = false;
    bool pending_disconnect = false;
    WSAData wsaData = {0};
    SOCKET server_socket = INVALID_SOCKET;

    struct TS3Server {
        std::string my_client_id;
        std::string my_channel_id;
        std::string name;
        std::string host;
        std::string port;
        uint32_t user_count = 0;
    };

    TS3Server* current_server = nullptr;

    TS3Server* GetCurrentServer()
    {
        return current_server;
    }

    void CHAT_CMD_FUNC(OnTeamspeakCommand);
    bool ConnectBlocking(bool user_invoked = false);

    bool IsConnected()
    {
        return server_socket != INVALID_SOCKET && step != Connecting;
    }

    struct ClientQueryResponse {
        std::string error_id;
        std::string error_text;
        std::string content;
    };

    bool ParseError(const std::string& response, std::string& id, std::string& text)
    {
        if (!response.starts_with("error")) {
            return false;
        }
        auto id_offset = response.find("id=");
        const auto msg_offset = response.find(" msg=");
        if (id_offset == std::string::npos || msg_offset == std::string::npos) {
            return false;
        }
        id_offset += 3;
        id = response.substr(id_offset, msg_offset - id_offset);
        text = response.substr(msg_offset + 5);
        return true;
    }

    void DeleteSocket()
    {
        if (server_socket != INVALID_SOCKET) {
            shutdown(server_socket, 2);
            server_socket = INVALID_SOCKET;
        }
        if (current_server) {
            delete current_server;
            current_server = nullptr;
        }
    }

    char response_buffer[2048];
    ClientQueryResponse _client_query_response;

    const ClientQueryResponse* PollSocket(const std::string& request)
    {
        if (server_socket == INVALID_SOCKET) {
            return nullptr;
        }
        if (!request.empty()) {
            const int res = send(server_socket, request.c_str(), request.size(), 0);
            if (res == SOCKET_ERROR) {
                return nullptr;
            }
        }
        _client_query_response.error_id.clear();
        _client_query_response.error_text.clear();
        _client_query_response.content.clear();
        while (true) {
            const int res = recv(server_socket, response_buffer, sizeof(response_buffer) - 1, 0);
            if (res == SOCKET_ERROR || res == 0) {
                break;
            }
            response_buffer[res] = 0;
            if (ParseError(response_buffer, _client_query_response.error_id, _client_query_response.error_text)) {
                break;
            }
            _client_query_response.content.append(response_buffer);
        }
        return _client_query_response.content.empty() && _client_query_response.error_id.empty() ? nullptr : &_client_query_response;
    }

    void GetServerInfoBlocking()
    {
        last_check = TIMER_INIT();
        TS3Server* server = nullptr;

        if (!ConnectBlocking()) {
            return;
        }
        if (current_server) {
            delete current_server;
            current_server = nullptr;
        }
        auto response = PollSocket("serverconnectinfo\r\n");
        if (!response) {
            return;
        }

        Log::Log("content: %s\n error: %s %s", response->content.c_str(), response->error_id.c_str(), response->error_text.c_str());

        {
            static constexpr ctll::fixed_string server_info_pattern = R"(ip=([^ ]+) port=([0-9]+))";

            if (auto m = ctre::match<server_info_pattern>(response->content)) {
                server = new TS3Server();
                server->host = m.get<1>().to_string();
                server->port = m.get<2>().to_string();
            } else {
                return;
            }

            response = PollSocket("whoami\r\n");
            if (!response) {
                goto cleanup;
            }

            static constexpr ctll::fixed_string client_info_pattern = R"(clid=([0-9]+) cid=([0-9]+))";

            if (auto m = ctre::match<client_info_pattern>(response->content)) {
                server->my_channel_id = m.get<2>().to_string();
                server->my_client_id = m.get<1>().to_string();
            } else {
                goto cleanup;
            }

            response = PollSocket("servervariable virtualserver_name\r\n");
            if (!response) {
                goto cleanup;
            }

            static constexpr ctll::fixed_string server_name_pattern = R"(virtualserver_name=([^\n]+))";

            if (auto m = ctre::match<server_name_pattern>(response->content)) {
                server->name = m.get<1>().to_string();
            } else {
                goto cleanup;
            }

            auto replace_all = [](std::string& subject, const std::string& find, const std::string& replace) {
                while (true) {
                    const auto found = subject.find(find);
                    if (found == std::string::npos) {
                        break;
                    }
                    subject.replace(found, find.size(), replace);
                }
            };

            replace_all(server->name, "\\s", " ");

            response = PollSocket("clientlist\r\n");
            if (!response) {
                goto cleanup;
            }

            const auto& res = response->content;
            size_t offset = 0;
            while (true) {
                offset = res.find("clid=", offset);
                if (offset == std::string::npos) {
                    break;
                }
                server->user_count++;
                offset += 5;
            }

            current_server = server;
            server = nullptr;
        }

    cleanup:

        delete server;
    }

    void GetServerInfo(std::function<void()> callback = nullptr)
    {
        Resources::EnqueueWorkerTask([callback] {
            GetServerInfoBlocking();
            if (callback) {
                callback();
            }
        });
    }

    bool ConnectBlocking(bool user_invoked)
    {
        auto failed = [user_invoked](const char* format, ...) {
            if (user_invoked && format) {
                va_list vl;
                va_start(vl, format);
                const size_t len = vsnprintf(nullptr, 0, format, vl);
                const auto buf = new char[len + 1];
                vsnprintf(buf, len + 1, format, vl);
                va_end(vl);
                Log::Error(buf);
                delete[] buf;
            }
            DeleteSocket();
            step = Idle;
            return false;
        };

        pending_connect = false;
        if (step == Connecting || IsConnected()) {
            return true;
        }
        step = Connecting;
        if (!settings.enabled) {
            return failed(nullptr);
        }
        if (settings.teamspeak3_api_key.empty()) {
            return failed("未提供 API Key；请在 Teamspeak > 工具 > 选项 > 插件 > ClientQuery > 设置 中查找");
        }
        int res;
        if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            return failed("调用 WSAStartup 失败：%d\n", res);
        }
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET) {
            return failed("无法连接到 Teamspeak 3；套接字创建失败");
        }

        constexpr DWORD timeout = 500;
        res = setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
        if (res == SOCKET_ERROR) {
            return failed("无法连接到 Teamspeak 3；setsockopt 失败");
        }
        res = setsockopt(server_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof timeout);
        if (res == SOCKET_ERROR) {
            return failed("无法连接到 Teamspeak 3；setsockopt 失败");
        }

        u_long ip = 0;
        const u_short port = teamspeak3_port;
        u_long* ptr = &ip;
        res = inet_pton(AF_INET, teamspeak3_host, ptr);
        if (res != 1) {
            return failed("无法连接到 Teamspeak 3；inet_pton 失败");
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ip;
        addr.sin_port = htons(port);

        res = connect(server_socket, (SOCKADDR*)&addr, sizeof(addr));
        if (res == SOCKET_ERROR) {
            return failed("无法连接到 Teamspeak 3；连接失败 - Teamspeak 3 是否正在运行且启用了 ClientQuery 插件？");
        }

        auto response = PollSocket("");
        if (!response) {
            return failed("无法连接到 Teamspeak 3；认证失败或空响应");
        }
        Log::Log("Teamspeak 3 欢迎消息：\n%s", response->content.c_str());

        const std::string to_send = std::format("auth apikey={}\r\n", settings.teamspeak3_api_key);
        response = PollSocket(to_send);
        if (!response) {
            return failed("无法连接到 Teamspeak 3；认证失败或空响应");
        }
        Log::Log("Teamspeak 3 认证响应：\n%s", response->content.c_str());

        if (user_invoked) {
            Log::Flash("Teamspeak 3 已连接");
        }

        GW::Chat::CreateCommand(&ChatCmd_HookEntry,L"ts", OnTeamspeakCommand);
        GW::Chat::CreateCommand(&ChatCmd_HookEntry,L"ts3", OnTeamspeakCommand);

        GetServerInfo();

        step = Idle;
        return true;
    }

    bool Connect(bool user_invoked = false, std::function<void(bool)> callback = nullptr)
    {
        Resources::EnqueueWorkerTask([user_invoked,callback] {
            const bool success = ConnectBlocking(user_invoked);
            if (callback) {
                callback(success);
            }
        });
        return true;
    }

    void GetServerInviteLink(TS3Server* server, std::string channel_id, std::function<void(const std::string&)> callback)
    {
        teamspeak_invite_api::CreateRequest packet{
            .address = std::format("{}:{}", server->host, server->port),
            .name = server->name,
            .channel_id = channel_id,
            .channel_name = channel_id,
        };

        Resources::Post("https://invites.teamspeak.com/servers/create", glz::write_json(packet).value_or(std::string{}), [callback](const bool success, const std::string& response, void*) {
            if (!success) {
                Log::Error("获取 Teamspeak 邀请链接失败 (1)");
                Log::Log("%s", response.c_str());
                return;
            }
            teamspeak_invite_api::CreateResponse res{};
            if (auto ec = glz::read<json_opts>(res, response); ec) {
                Log::Error("获取 Teamspeak 邀请链接失败 (2)");
                return;
            }
            if (res.id.empty()) {
                Log::Error("获取 Teamspeak 邀请链接失败 (3)");
                return;
            }
            const std::string url = std::format("https://tmspk.gg/{}", res.id);
            callback(url);
        });
    }

    void OnGotServerInfo()
    {
        if (!IsConnected()) {
            Log::Error("GWToolbox 未连接到 Teamspeak 3");
            return;
        }
        const auto teamspeak_server = GetCurrentServer();
        if (!(teamspeak_server && !teamspeak_server->host.empty())) {
            Log::Error("Teamspeak 3 未连接到服务器");
            return;
        }
        wchar_t buf[120];
        swprintf(buf, _countof(buf) - 1, L"%s（%d 名用户）",
                 TextUtils::StringToWString(teamspeak_server->name).c_str(),
                 teamspeak_server->user_count);
        GW::Chat::SendChat('#', buf);

        swprintf(buf, _countof(buf) - 1, L"TS3: [https://invite.teamspeak.com/%S/?port=%S;xx]",
                 teamspeak_server->host.c_str(),
                 teamspeak_server->port.c_str());
        GW::Chat::SendChat('#', buf);

        GetServerInviteLink(teamspeak_server, teamspeak_server->my_channel_id, [](const std::string& url) {
            wchar_t buf[120];
            swprintf(buf, _countof(buf) - 1, L"TS5: [%S;xx]", url.c_str());
            GW::Chat::SendChat('#', buf);
        });
    }

    void CHAT_CMD_FUNC(OnTeamspeakCommand)
    {
        Resources::EnqueueWorkerTask([] {
            GetServerInfoBlocking();
            OnGotServerInfo();
        });
    }
}

void TeamspeakModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry,L"ts", OnTeamspeakCommand);
    GetServerInfo();
}

void TeamspeakModule::Terminate()
{
    settings.enabled = false;
    DeleteSocket();
    if (wsaData.wVersion) {
        WSACleanup();
        wsaData = {0};
    }
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

void TeamspeakModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    pending_connect = true;
}

void TeamspeakModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void TeamspeakModule::Update(float)
{
    if (pending_connect) {
        Connect();
        pending_connect = false;
    }
    if (!settings.enabled && IsConnected()) {
        pending_disconnect = true;
    }
    if (pending_disconnect) {
        DeleteSocket();
        pending_disconnect = false;
        return;
    }
    if (check_interval && (!last_check || TIMER_DIFF(last_check) > check_interval)) {
        GetServerInfo();
    }
    check_interval = 0;
}

void TeamspeakModule::DrawSettingsInternal()
{
    check_interval = 5000;
    ImGui::PushID("TeamspeakModule");
    if (ImGui::Checkbox("启用 Teamspeak 3 集成", &settings.enabled)) {
        if (settings.enabled) {
            Connect(true);
        }
        else {
            pending_disconnect = true;
        }
    }
    ImGui::ShowHelp("允许 GWToolbox 从 Teamspeak 3 获取信息");
    if (settings.enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IsConnected() ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        auto status_str = [] {
            if (IsConnected()) {
                return "已连接";
            }
            if (step == Connecting) {
                return "连接中";
            }
            return "已断开";
        };
        if (ImGui::Button(status_str(), ImVec2(0, 0))) {
            if (IsConnected()) {
                pending_disconnect = true;
            }
            else {
                Connect(true);
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(IsConnected() ? "点击断开" : "点击连接");
        }
        if (IsConnected()) {
            ImGui::Indent();
            ImGui::TextUnformatted("服务器：");
            ImGui::SameLine();
            const auto teamspeak_server = GetCurrentServer();
            if (!teamspeak_server) {
                ImGui::TextDisabled("未连接");
            }
            else {
                ImGui::Text("%s", teamspeak_server->name.c_str());
                ImGui::Text("主机：");
                ImGui::SameLine();
                ImGui::Text("%s:%s", teamspeak_server->host.c_str(), teamspeak_server->port.c_str());
                ImGui::Text("用户数：");
                ImGui::SameLine();
                ImGui::Text("%d", teamspeak_server->user_count);
            }
            ImGui::Unindent();
        }
        ImGui::InputText("Teamspeak 3 ClientQuery API Key", settings.teamspeak3_api_key, 127);
        ImGui::ShowHelp("请在 Teamspeak > 工具 > 选项 > 插件 > ClientQuery > 设置 中查找");
        ImGui::TextDisabled("使用 /ts3 命令将当前服务器信息发送到聊天");
    }
    ImGui::PopID();
}
