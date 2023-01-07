/*
    Module to keep track of current Teamspeak 3 status

    Created it initially because I was pissed off with having to bind
    different hotkeys to send different TS3 servers to chat.

    Enhancements:
     + Filter incoming http URLs to refactor to ts3server URL protocol where applicable.
     + Teamspeak overlay like Overwolf, but not as shit
     + Messages to/from your current channel
     + Whispers to/from other TS3 users

     -- Jon
*/

#include "stdafx.h"
#include <Psapi.h>
#include <Defines.h>

#include <GWCA\Managers\ChatMgr.h>

#include <Utils/GuiUtils.h>
#include <ImGuiAddons.h>

#include <Modules/Resources.h>
#include <Modules/TeamspeakModule.h>
#include <Timer.h>

namespace {
    const char* teamspeak3_host = "127.0.0.1";
    char teamspeak3_api_key[128] = { 0 };
    u_short teamspeak3_port = 25639;

    clock_t check_interval = 0;
    clock_t last_check = 0;

    enum ConnectionStep : uint8_t {
        Idle,
        Connecting,
        Downloading
    };
    ConnectionStep step = ConnectionStep::Idle;

    bool enabled = false;
    bool disconnecting = false;
    bool pending_connect = false;
    bool pending_disconnect = false;
    WSAData wsaData = { 0 };
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

    TS3Server* GetCurrentServer() {
        return current_server;
    }

    void OnTeamspeakCommand(const wchar_t*, int, LPWSTR*);
    bool ConnectBlocking(bool user_invoked = false);

    const bool IsConnected() {
        return server_socket != INVALID_SOCKET && step != Connecting;
    }

    struct ClientQueryResponse {
        std::string error_id;
        std::string error_text;
        std::string content;
    };

    bool ParseError(const std::string& response, std::string& id, std::string& text) {
        if (!response.starts_with("error"))
            return false;
        auto id_offset = response.find("id=");
        auto msg_offset = response.find(" msg=");
        if (id_offset == std::string::npos || msg_offset == std::string::npos)
            return false;
        id_offset += 3;
        id = response.substr(id_offset, msg_offset - id_offset);
        text = response.substr(msg_offset + 5);
        return true;
    }
    
    void DeleteSocket() {
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
    const ClientQueryResponse* PollSocket(const std::string& request) {
        if (server_socket == INVALID_SOCKET)
            return nullptr;
        int res = 0;
        if (!request.empty()) {
            res = send(server_socket, request.c_str(), request.size(), 0);
            if (res == SOCKET_ERROR)
                return nullptr;
        }
        _client_query_response.error_id.clear();
        _client_query_response.error_text.clear();
        _client_query_response.content.clear();
        while (true) {
            res = recv(server_socket, response_buffer, sizeof(response_buffer) - 1, 0);
            if (res == SOCKET_ERROR || res == 0)
                break;
            response_buffer[res] = 0;
            if (ParseError(response_buffer, _client_query_response.error_id, _client_query_response.error_text))
                break;
            _client_query_response.content.append(response_buffer);
        }
        return _client_query_response.content.empty() && _client_query_response.error_id.empty() ? nullptr : &_client_query_response;
    }

    void GetServerInfoBlocking() {
        last_check = TIMER_INIT();
        TS3Server* server = nullptr;

        if (!ConnectBlocking())
            return;
        if (current_server) {
            delete current_server;
            current_server = nullptr;
        }
        auto response = PollSocket("serverconnectinfo\r\n");
        if (!response)
            return;
        
        Log::Log("content: %s\n error: %s %s",response->content.c_str(), response->error_id.c_str(), response->error_text.c_str());

        {
            std::regex server_info_regex("ip=([^ ]+) port=([0-9]+)");
            std::smatch m;
            std::regex_search(response->content, m, server_info_regex);
            if (!m.size())
                return;

            server = new TS3Server();

            server->host = m[1].str();
            server->port = m[2].str();
            response = PollSocket("whoami\r\n");
            if (!response)
                goto cleanup;
            std::regex client_info_regex("clid=([0-9]+) cid=([0-9]+)");
            if (!std::regex_search(response->content, m, client_info_regex))
                goto cleanup;
            server->my_channel_id = m[2].str();
            server->my_client_id = m[1].str();

            response = PollSocket("servervariable virtualserver_name\r\n");
            if (!response)
                goto cleanup;
            std::regex server_name_regex("virtualserver_name=([^\n]+)");
            if (!std::regex_search(response->content, m, server_name_regex))
                goto cleanup;
            server->name = m[1].str();

            auto replace_all = [](std::string& subject, const std::string& find, const std::string& replace) {
                while(true) {
                    auto found = subject.find(find);
                    if (found == std::string::npos) break;
                    subject.replace(found, find.size(), replace);
                }
            };

            replace_all(server->name, "\\s", " ");

            response = PollSocket("clientlist\r\n");
            if (!response)
                goto cleanup;
            const auto& res = response->content;
            size_t offset = 0;
            while (true) {
                offset = res.find("clid=", offset);
                if (offset == std::string::npos)
                    break;
                server->user_count++;
                offset += 5;
            }

            current_server = server;
            server = nullptr;
        }


    cleanup:
        if (server)
            delete server;

    }
    void GetServerInfo(std::function<void()> callback = nullptr) {
        Resources::EnqueueWorkerTask([callback]() {
            GetServerInfoBlocking();
            if (callback)
                callback();
            });
    }
    bool ConnectBlocking(bool user_invoked) {
        auto failed = [user_invoked](const char* format, ...) {
            if (user_invoked && format) {
                va_list vl;
                va_start(vl, format);
                size_t len = vsnprintf(NULL, 0, format, vl);
                auto buf = new char[len + 1];
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
        if (step == Connecting || IsConnected())
            return true;
        step = Connecting;
        if (!enabled)
            return failed(0);
        //BOOL is_x64 = false;
        //std::filesystem::path running_teamspeak_exe;
        //if (!GetTeamspeakProcess(&running_teamspeak_exe, &is_x64))
        //    return failed("Error finding running teamspeak executable (%04X)",GetLastError());
        //if (running_teamspeak_exe.empty())
        //    return failed("Failed to find running teamspeak executable; is Teamspeak 3 running?");
        if (!teamspeak3_api_key[0])
            return failed("No API Key provided; find this in Teamspeak > Tools > Options > Addons > ClientQuery > Settings");
        int res;
        if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
            return failed("Failed to call WSAStartup: %d\n", res);
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET)
            return failed("Couldn't connect to teamspeak 3; socket failure");

        DWORD timeout = 500;
        res = setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
        if(res == SOCKET_ERROR)
            return failed("Couldn't connect to teamspeak 3; setsockopt failure");
        res = setsockopt(server_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof timeout);
        if(res == SOCKET_ERROR)
            return failed("Couldn't connect to teamspeak 3; setsockopt failure");

        u_long ip = 0;
        u_short port = teamspeak3_port;
        u_long* ptr = &ip;
        res = inet_pton(AF_INET, teamspeak3_host, ptr);
        if (res != 1)
            return failed("Couldn't connect to teamspeak 3; inet_pton failure");
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ip;
        addr.sin_port = htons(port);

        res = connect(server_socket, (SOCKADDR*)&addr, sizeof(addr));
        if (res == SOCKET_ERROR)
            return failed("Couldn't connect to teamspeak 3; connect failure - is Teamspeak 3 running with the ClientQuery Addon enabled?");

        auto response = PollSocket("");
        if(!response)
            return failed("Couldn't connect to teamspeak 3; auth failure or empty response");
        Log::Log("Teamspeak 3 welcome message:\n%s", response->content.c_str());

        // Send auth message
        std::string to_send = std::format("auth apikey={}\r\n", teamspeak3_api_key);
        response = PollSocket(to_send);
        if(!response)
            return failed("Couldn't connect to teamspeak 3; auth failure or empty response");
        Log::Log("Teamspeak 3 auth response:\n%s", response->content.c_str());

        if (user_invoked)
            Log::Info("Teamspeak 3 connected");

        GW::Chat::CreateCommand(L"ts", OnTeamspeakCommand);
        GW::Chat::CreateCommand(L"ts3", OnTeamspeakCommand);

        GetServerInfo();

        step = Idle;
        return true;
    }

    bool Connect(bool user_invoked = false, std::function<void(bool)> callback = nullptr) {
        Resources::EnqueueWorkerTask([user_invoked,callback]() {
            bool success = ConnectBlocking(user_invoked);
            if (callback)
                callback(success);
            });
        return true;
    }

    bool GetValue(const nlohmann::json& content, const char* key, std::string * out) {
        auto found = content.find(key);
        if (!(found != content.end() && found->is_string()))
            return false;
        *out = *found;
        return true;
    }

    void GetServerInviteLink(TS3Server* server, std::string channel_id, std::function<void(const std::string&)> callback) {
        using nlohmann::json;
        json packet;
        packet["address"] = std::format("{}:{}", server->host, server->port);
        packet["name"] = server->name;
        packet["password"] = NULL;
        packet["channel_id"] = channel_id;
        packet["channel_name"] = channel_id;
        packet["expires_in_days"] = 1;


        Resources::Post("https://invites.teamspeak.com/servers/create", packet.dump(), [callback](bool success, const std::string& response) {
            if (!success) {
                Log::Error("Failed to get teamspeak invite link (1)");
                Log::Log("%s", response.c_str());
                return;
            }
            const json& res = json::parse(response.c_str(), nullptr, false);
            if (res == json::value_t::discarded) {
                Log::Error("Failed to get teamspeak invite link (2)");
                return;
            }
            std::string invite_id;
            if (!GetValue(res, "id", &invite_id)) {
                Log::Error("Failed to get teamspeak invite link (3)");
                return;
            }
            std::string url = std::format("https://tmspk.gg/{}", invite_id);
            callback(url);
            });
    }
    
    void OnGotServerInfo() {
        if (!IsConnected()) {
            Log::Error("GWToolbox isn't connected to Teamspeak 3");
            return;
        }
        const auto teamspeak_server = GetCurrentServer();
        if (!(teamspeak_server && teamspeak_server->host.size())) {
            Log::Error("Teamspeak 3 isn't connected to a server");
            return;
        }
        wchar_t buf[120];
        swprintf(buf, _countof(buf) - 1, L"%s (%d users)",
            GuiUtils::StringToWString(teamspeak_server->name).c_str(),
            teamspeak_server->user_count);
        GW::Chat::SendChat('#', buf);

        swprintf(buf, _countof(buf) - 1, L"TS3: [http://invite.teamspeak.com/%S/?port=%S;xx]",
            teamspeak_server->host.c_str(),
            teamspeak_server->port.c_str());
        GW::Chat::SendChat('#', buf);

        GetServerInviteLink(teamspeak_server, teamspeak_server->my_channel_id, [](const std::string& url) {
            wchar_t buf[120];
            swprintf(buf, _countof(buf) - 1, L"TS5: [%S;xx]", url.c_str());
            GW::Chat::SendChat('#', buf);
            });

    }
    void OnTeamspeakCommand(const wchar_t*, int , LPWSTR* ) {
        Resources::EnqueueWorkerTask([]() {
            GetServerInfoBlocking();
            OnGotServerInfo();
            });
    }
}

void TeamspeakModule::Initialize() {
    ToolboxModule::Initialize();
    GW::Chat::CreateCommand(L"ts", OnTeamspeakCommand);
    GetServerInfo();
}

void TeamspeakModule::Terminate() {
    enabled = false;
    DeleteSocket();
    if (wsaData.wVersion) {
        WSACleanup();
        wsaData = { 0 };
    }
    GW::Chat::DeleteCommand(L"ts");
    
}
void TeamspeakModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    enabled = ini->GetBoolValue(Name(), VAR_NAME(enabled), enabled);
    const char* tmp = ini->GetValue(Name(), VAR_NAME(teamspeak3_api_key), teamspeak3_api_key);
    strncpy(teamspeak3_api_key, tmp,sizeof(teamspeak3_api_key) - 1);
    pending_connect = true;
}
void TeamspeakModule::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(enabled), enabled);
    ini->SetValue(Name(), VAR_NAME(teamspeak3_api_key), teamspeak3_api_key);
}
void TeamspeakModule::Update(float) {
    if (pending_connect) {
        Connect();
        pending_connect = false;
    }
    if (!enabled && IsConnected())
        pending_disconnect = true;
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

void TeamspeakModule::DrawSettingInternal() {
    check_interval = 5000;
    ImGui::PushID("TeamspeakModule");
    if (ImGui::Checkbox("Enable Teamspeak 3 integration", &enabled)) {
        if (enabled)
            Connect(true);
        else
            pending_disconnect = true;
    }
    ImGui::ShowHelp("Allows GWToolbox retrieve info from Teamspeak 3");
    if (enabled) {
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IsConnected() ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        auto status_str = []() {
            if (IsConnected())
                return "Connected";
            if (step == Connecting)
                return "Connecting";
            return "Disconnected";
        };
        if (ImGui::Button(status_str(), ImVec2(0, 0))) {
            if (IsConnected())
                pending_disconnect = true;
            else
                Connect(true);
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(IsConnected() ? "Click to disconnect" : "Click to connect");
        if (IsConnected()) {
            ImGui::Indent();
            ImGui::TextUnformatted("Server:");
            ImGui::SameLine();
            auto teamspeak_server = GetCurrentServer();
            if (!teamspeak_server) {
                ImGui::TextDisabled("Not Connected");
            }    
            else {
                ImGui::Text("%s", teamspeak_server->name.c_str());
                ImGui::Text("Host:"); ImGui::SameLine(); ImGui::Text("%s:%s", teamspeak_server->host.c_str(), teamspeak_server->port.c_str());
                ImGui::Text("Users:"); ImGui::SameLine(); ImGui::Text("%d", teamspeak_server->user_count);
            }
            ImGui::Unindent();
        }
        ImGui::InputText("Teamspeak 3 ClientQuery API Key", teamspeak3_api_key, sizeof(teamspeak3_api_key) - 1);
        ImGui::ShowHelp("Find this in Teamspeak > Tools > Options > Addons > ClientQuery > Settings");
        ImGui::TextDisabled("Use the /ts3 command to send your current server info in chat");
    }
    ImGui::PopID();
}
