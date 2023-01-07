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
#include <Modules/Teamspeak5Module.h>

using easywsclient::WebSocket;
using nlohmann::json;
using json_vec = std::vector<json>;

namespace {
    int ws_port = 5899;
    std::string ws_host;
    std::string gwtoolbox_teamspeak5_api_key;

    const char* gwtoolbox_teamspeak5_identifier = "com.guildwars.gwtoolboxpp";
    const char* gwtoolbox_teamspeak5_version = "1.0.0";
    const char* gwtoolbox_teamspeak5_name = "GWToolbox++ Teamspeak 5";
    const char* gwtoolbox_teamspeak5_description = "Allows GWToolbox retrieve info from Teamspeak 5";

    enum ConnectionStep : uint8_t {
        Idle,
        Connecting,
        Downloading
    };
    ConnectionStep step = ConnectionStep::Idle;

    bool enabled = true;
    bool disconnecting = false;
    bool pending_connect = false;
    bool pending_disconnect = false;
    WSAData wsaData = { 0 };
    easywsclient::WebSocket* websocket = nullptr;

    struct TS3Server {
        uint32_t my_client_id = 0;
        std::string my_channel_id;
        std::string name;
        //char welcome_message[512];
        std::string host;
        uint32_t port = 9987;
        uint32_t user_count = 0;
        //char password[128];
    };

    std::map<uint32_t, TS3Server*> connected_servers;

    uint32_t current_server = 0xffffffff;
    bool GetValue(const json& content, const char* key, uint32_t * out) {
        auto found = content.find(key);
        if (!(found != content.end() && found->is_number_unsigned()))
            return false;
        *out = *found;
        return true;
    }
    bool GetValue(const json& content, const char* key, std::string * out) {
        auto found = content.find(key);
        if (!(found != content.end() && found->is_string()))
            return false;
        *out = *found;
        return true;
    }
    bool GetValue(const json& content, const char* key, bool* out) {
        auto found = content.find(key);
        if (!(found != content.end() && found->is_boolean()))
            return false;
        *out = *found;
        return true;
    }
    bool GetValue(const json& content, const char* key, json * out) {
        auto found = content.find(key);
        if (!(found != content.end() && found->is_object()))
            return false;
        *out = *found;
        return true;
    }
    bool GetValue(const json& content, const char* key, json_vec * out) {
        auto found = content.find(key);
        if (!(found != content.end() && found->is_array()))
            return false;
        *out = found->get<json_vec>();
        return true;
    }


    void DeleteWebSocket() {
        if (!websocket) return;
        if (websocket->getReadyState() == easywsclient::WebSocket::OPEN)
            websocket->close();
        disconnecting = true;
        while (websocket->getReadyState() != easywsclient::WebSocket::CLOSED)
            websocket->poll();
        delete websocket;
        websocket = nullptr;
    }

    TS3Server* GetServer(uint32_t connection_id) {
        const auto& found = connected_servers.find(connection_id);
        return found == connected_servers.end() ? nullptr : found->second;
    }

    TS3Server* GetCurrentServer() {
        return GetServer(current_server);
    }
    const std::string& GetWebsocketHost(bool force = false) {
        if (!(ws_host.size() && !force)) {
            ws_host = std::format("ws://localhost:{}", ws_port);
        }
        return ws_host;
    }

    const bool IsConnected() {
        return websocket && websocket->getReadyState() == easywsclient::WebSocket::OPEN;
    }

    void GetServerInviteLink(TS3Server* server, std::string channel_id, std::function<void(const std::string&)> callback) {
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
    void OnTeamspeakCommand(const wchar_t*, int , LPWSTR* ) {
        if (!IsConnected()) {
            Log::Error("GWToolbox isn't connected to Teamspeak 5");
            return;
        }
        const auto teamspeak_server = GetCurrentServer();
        if (!(teamspeak_server && teamspeak_server->host.size())) {
            Log::Error("Teamspeak 5 isn't connected to a server");
            return;
        }
        wchar_t buf[120];
        swprintf(buf, _countof(buf) - 1, L"%s (%d users)",
            GuiUtils::StringToWString(teamspeak_server->name).c_str(),
            teamspeak_server->user_count);
        GW::Chat::SendChat('#', buf);

        swprintf(buf, _countof(buf) - 1, L"TS3: [http://invite.teamspeak.com/%S/?port=%d;xx]",
            teamspeak_server->host.c_str(),
            teamspeak_server->port);
        GW::Chat::SendChat('#', buf);

        GetServerInviteLink(teamspeak_server, teamspeak_server->my_channel_id, [](const std::string& url) {
            wchar_t buf[120];
            swprintf(buf, _countof(buf) - 1, L"TS5: [%S;xx]", url.c_str());
            GW::Chat::SendChat('#', buf);
            });
    }
    void SendTeamspeakHandshake() {
        json packet;
        packet["type"] = "auth";
        json payload;
        payload["identifier"] = gwtoolbox_teamspeak5_identifier;
        payload["version"] = gwtoolbox_teamspeak5_version;
        payload["name"] = gwtoolbox_teamspeak5_name;
        payload["description"] = gwtoolbox_teamspeak5_description;
        json content;
        content["apiKey"] = gwtoolbox_teamspeak5_api_key;
        payload["content"] = content;
        packet["payload"] = payload;

        websocket->send(packet.dump());
    }
    bool Connect(bool user_invoked = false) {
        pending_connect = false;
        if (step == Connecting || IsConnected())
            return true;
        if (step == Downloading)
            return true;
        step = Connecting;
        if (!enabled) {
            step = Idle;
            return false;
        }
        int res;
        if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            if (user_invoked)
                Log::Error("Failed to call WSAStartup: %d\n", res);
            step = Idle;
            return false;
        }
        step = Connecting;
        Resources::EnqueueWorkerTask([user_invoked]() {
            websocket = WebSocket::from_url(GetWebsocketHost());
            if (websocket == nullptr) {
                if (user_invoked)
                    Log::Error("Couldn't connect to the teamspeak 5 websocket; ensure Teamspeak 5 is running and that the 'Remote Apps' feature is enabled");
            }
            else {
                if (user_invoked)
                    Log::Info("Teamspeak 5 connected");
                SendTeamspeakHandshake();
                GW::Chat::CreateCommand(L"ts", OnTeamspeakCommand);
                GW::Chat::CreateCommand(L"ts5", OnTeamspeakCommand);
            }
            step = Idle;
            pending_connect = false;
            });
        return true;
    }



    TS3Server* UpsertServer(const json& props_json, uint32_t connection_id) {
        if(!props_json.is_object())
            return nullptr;
        TS3Server* teamspeak_server = GetServer(connection_id);
        if(!teamspeak_server) {
            teamspeak_server = new TS3Server();
            connected_servers[connection_id] = teamspeak_server;
        }
        GetValue(props_json, "ip", &teamspeak_server->host);
        GetValue(props_json, "name", &teamspeak_server->name);
        GetValue(props_json, "port", &teamspeak_server->port);
        GetValue(props_json, "clientsOnline", &teamspeak_server->user_count);
        return teamspeak_server;
    }
    void RemoveServer(uint32_t connection_id) {
        const auto found = connected_servers.find(connection_id);
        if (found != connected_servers.end()) {
            auto server = found->second;
            connected_servers.erase(found);
            delete server;
        }
        if (current_server == connection_id)
            current_server = 0xffffffff;
    }

    bool OnTeamspeakAuth(const json& payload) {
        current_server = 0xffffffff;
        json_vec connections;
        json properties;
        if (!GetValue(payload, "connections", &connections))
            return false;
        for (size_t connection_id = 0; connection_id < connections.size();connection_id++) {
            const auto& connection = connections[connection_id];
            if (!connection.is_object())
                continue;
            if (!GetValue(connection, "properties", &properties))
                continue;
            const auto teamspeak_server = UpsertServer(properties,connection_id);
            if (!teamspeak_server) {
                Log::Log("Failed to parse teamspeak server info");
                continue;
            }
            // Cycle and find our channel
            if (GetValue(connection, "clientId", &teamspeak_server->my_client_id)) {
                json_vec clientInfos;
                if (GetValue(connection, "clientInfos", &clientInfos)) {
                    uint32_t identifier;
                    for (const auto& client : clientInfos) {
                        if (GetValue(client, "id", &identifier) && identifier == teamspeak_server->my_client_id) {
                            GetValue(client, "channelId", &teamspeak_server->my_channel_id);
                            break;
                        }
                    }
                }
            }
            Log::Log("Teamspeak server info updated:\n%s (%s:%d), %d users", teamspeak_server->name.c_str(), teamspeak_server->host.c_str(), teamspeak_server->port, teamspeak_server->user_count);
        }
        GetValue(payload, "currentConnectionId", &current_server);
        return true;
    }
    bool OnTeamspeakServerPropertiesUpdated(const json& payload) {
        uint32_t connection_id;
        if (!GetValue(payload, "connectionId", &connection_id))
            return false;
        json properties;
        if (!GetValue(payload, "properties", &properties))
            return false;
        const auto teamspeak_server = UpsertServer(properties,connection_id);
        if (!teamspeak_server) {
            Log::Log("Failed to parse teamspeak server info");
            return false;
        }
        Log::Log("Teamspeak server info updated:\n%s (%s:%d), %d users", teamspeak_server->name.c_str(), teamspeak_server->host.c_str(), teamspeak_server->port, teamspeak_server->user_count);
        return true;
    }
    bool OnTeamspeakConnectStatusChanged(const json& payload) {
        uint32_t connection_id, status;
        if (!GetValue(payload, "connectionId", &connection_id))
            return false;
        if (!GetValue(payload, "status", &status))
            return false;
        auto server = GetServer(connection_id);
        if (server) {
            GetValue(payload, "clientId", &server->my_client_id);
        }
       

        switch (status) {
        case 0: // Disconnected
            RemoveServer(connection_id);
            break;
        case 1: // Connected (current server)
            current_server = connection_id;
            break;

        }
        Log::Log("OnTeamspeakConnectStatusChanged, status %d, connectionId %d", status, connection_id);
        return true;
    }

    bool OnTeamspeakClientSelfPropertyUpdated(const json& payload) {
        bool newValue = false;
        if (!(GetValue(payload, "newValue", &newValue) && newValue == true))
            return false;
        std::string flag;
        if (!(GetValue(payload, "flag", &flag) && flag == "inputHardware"))
            return false;

        return GetValue(payload, "connectionId", &current_server);
    }
    bool OnTeamspeakClientMoved(const json& payload) {
        uint32_t connection_id = 0, client_id = 0;
        if (!GetValue(payload, "connectionId", &connection_id))
            return false;
        const auto server = GetServer(connection_id);
        if (!server)
            return false;
        if (!GetValue(payload, "clientId", &client_id))
            return false;
        if (client_id != server->my_client_id)
            return false;

        return GetValue(payload, "newChannelId", &server->my_channel_id);
    }

    bool OnWebsocketMessage(const std::string& data) {
        //Log::Log("%s\n", data.c_str());
        const json& res = json::parse(data.c_str(), nullptr, false);
        if (res == json::value_t::discarded) {
            Log::Log("ERROR: Failed to parse res JSON from response in websocket->dispatch\n");
            return false;
        }
        json payload;
        if (!GetValue(res, "payload", &payload))
            return false;
        GetValue(payload, "apiKey", &gwtoolbox_teamspeak5_api_key);

        std::string type;
        if (!GetValue(res, "type", &type))
            return false;
        if (type == "auth") {
            OnTeamspeakAuth(payload);
        }
        else if (type == "serverPropertiesUpdated") {
            OnTeamspeakServerPropertiesUpdated(payload);
        }
        else if (type == "connectStatusChanged") {
            OnTeamspeakConnectStatusChanged(payload);
        }
        else if (type == "clientMoved") {
            OnTeamspeakClientMoved(payload);
        }
        else {
            Log::Log("Unhandled TS5 websocket type %s", type.c_str());
        }
        const auto teamspeak_server = GetCurrentServer();
        if (teamspeak_server) {
            Log::Log("Current server:\n%s (%s:%d), channel id %s, client_id %d, %d users", teamspeak_server->name.c_str(), teamspeak_server->host.c_str(), teamspeak_server->port, teamspeak_server->my_channel_id.c_str(), teamspeak_server->my_client_id, teamspeak_server->user_count);
        }
        else {
            Log::Log("Current server: None");
        }
        
        return true;
    }

}


void Teamspeak5Module::Initialize() {
    ToolboxModule::Initialize();
    GW::Chat::CreateCommand(L"ts", OnTeamspeakCommand);
    Connect();
}


void Teamspeak5Module::Terminate() {
    DeleteWebSocket();
    if (wsaData.wVersion) {
        WSACleanup();
        wsaData = { 0 };
    }
    GW::Chat::DeleteCommand(L"ts");
}
void Teamspeak5Module::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    enabled = ini->GetBoolValue(Name(), VAR_NAME(enabled), enabled);
    ws_port = ini->GetLongValue(Name(), VAR_NAME(ws_port), ws_port);
    gwtoolbox_teamspeak5_api_key = ini->GetValue(Name(), VAR_NAME(gwtoolbox_teamspeak5_api_key), gwtoolbox_teamspeak5_api_key.c_str());
    GetWebsocketHost(true);
    pending_connect = true;
}
void Teamspeak5Module::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(enabled), enabled);
    ini->SetLongValue(Name(), VAR_NAME(ws_port), ws_port);
    ini->SetValue(Name(), VAR_NAME(gwtoolbox_teamspeak5_api_key), gwtoolbox_teamspeak5_api_key.c_str());
}
void Teamspeak5Module::Update(float) {
    if (pending_connect) {
        Connect();
        pending_connect = false;
    }
    if (!enabled && IsConnected())
        pending_disconnect = true;
    if (pending_disconnect) {
        if(websocket)
            websocket->close();
        pending_disconnect = false;
        return;
    }
    if (websocket) {
        switch (websocket->getReadyState()) {
        case easywsclient::WebSocket::OPEN:
            if (pending_connect) {
                pending_connect = pending_disconnect = false;
                return;
            }
        case easywsclient::WebSocket::CLOSING:
        case easywsclient::WebSocket::CONNECTING:
            websocket->poll();
            break;
        case easywsclient::WebSocket::CLOSED:
            delete websocket;
            websocket = nullptr;
            pending_connect = pending_disconnect = false;
            return;
        }
        websocket->dispatch(OnWebsocketMessage);
    }

}

void Teamspeak5Module::DrawSettingInternal() {
    ImGui::PushID("Teamspeak5Module");
    if (ImGui::Checkbox("Enable Teamspeak 5 integration", &enabled)) {
        if (enabled)
            Connect(true);
        else
            pending_disconnect = true;
    }
    ImGui::ShowHelp(gwtoolbox_teamspeak5_description);
    if (enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IsConnected() ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        auto status_str = []() {
            if (IsConnected())
                return "Connected";
            if (step == Connecting)
                return "Connecting";
            if (step == Downloading)
                return "Downloading";
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
            const auto& teamspeak_server = GetCurrentServer();
            if (!teamspeak_server)
                ImGui::TextDisabled("Not Connected");
            else {
                ImGui::Text("%s", teamspeak_server->name.c_str());
                ImGui::Text("Host:"); ImGui::SameLine(); ImGui::Text("%s:%d", teamspeak_server->host.c_str(), teamspeak_server->port);
                ImGui::Text("Users:"); ImGui::SameLine(); ImGui::Text("%d", teamspeak_server->user_count);
            }
            ImGui::Unindent();
        }
        ImGui::TextDisabled("Use the /ts5 command to send your current server info in chat");
    }
    ImGui::PopID();
}
