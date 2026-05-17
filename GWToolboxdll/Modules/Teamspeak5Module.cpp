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
#include <Defines.h>

#include <GWCA/Managers/ChatMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <Utils/GuiUtils.h>
#include <ImGuiAddons.h>

#include <Modules/Resources.h>
#include <Modules/Teamspeak5Module.h>
#include <Utils/TextUtils.h>

using easywsclient::WebSocket;

namespace ts5_api {
    struct InviteCreateRequest {
        std::string address;
        std::string name;
        std::string password;
        std::string channel_id;
        std::string channel_name;
        double expires_in_days = 1.0;
    };

    struct InviteCreateResponse {
        std::string id;
    };

    struct HandshakeContent {
        std::string apiKey;
    };

    struct HandshakePayload {
        std::string identifier;
        std::string version;
        std::string name;
        std::string description;
        HandshakeContent content;
    };

    struct HandshakeEnvelope {
        std::string type;
        HandshakePayload payload;
    };

    // Incoming websocket envelope: peek at `type`, re-parse `payload` as the concrete struct.
    struct IncomingEnvelope {
        std::string type;
        glz::raw_json payload;
        glz::raw_json status;
    };

    struct ServerProperties {
        std::string ip;
        std::string name;
        uint32_t port = 0;
        uint32_t clientsOnline = 0;
    };

    struct ClientInfo {
        uint32_t id = 0;
        std::string channelId;
    };

    struct ConnectionEntry {
        ServerProperties properties;
        uint32_t clientId = 0;
        std::vector<ClientInfo> clientInfos;
    };

    struct AuthPayload {
        std::vector<ConnectionEntry> connections;
        uint32_t currentConnectionId = 0xffffffff;
        std::string apiKey;
    };

    struct ServerPropertiesUpdatedPayload {
        uint32_t connectionId = 0;
        ServerProperties properties;
        std::string apiKey;
    };

    struct ConnectStatusChangedPayload {
        uint32_t connectionId = 0;
        uint32_t status = 0;
        uint32_t clientId = 0;
        std::string apiKey;
    };

    struct ClientMovedPayload {
        uint32_t connectionId = 0;
        uint32_t clientId = 0;
        std::string newChannelId;
        std::string apiKey;
    };
}

namespace {

    constexpr glz::opts json_opts{.error_on_unknown_keys = false};

    GW::HookEntry ChatCmd_HookEntry;

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

    ConnectionStep step = Idle;

    bool enabled = true;
    bool disconnecting = false;
    bool pending_connect = false;
    bool pending_disconnect = false;
    WSAData wsaData = {0};
    WebSocket* websocket = nullptr;

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

    void DeleteWebSocket()
    {
        if (!websocket) {
            return;
        }
        if (websocket->getReadyState() == WebSocket::OPEN) {
            websocket->close();
        }
        disconnecting = true;
        while (websocket->getReadyState() != WebSocket::CLOSED) {
            websocket->poll();
        }
        delete websocket;
        websocket = nullptr;
    }

    TS3Server* GetServer(const uint32_t connection_id)
    {
        const auto& found = connected_servers.find(connection_id);
        return found == connected_servers.end() ? nullptr : found->second;
    }

    TS3Server* GetCurrentServer()
    {
        return GetServer(current_server);
    }

    const std::string& GetWebsocketHost(const bool force = false)
    {
        if (!(!ws_host.empty() && !force)) {
            ws_host = std::format("ws://localhost:{}", ws_port);
        }
        return ws_host;
    }

    const bool IsConnected()
    {
        return websocket && websocket->getReadyState() == WebSocket::OPEN;
    }

    void GetServerInviteLink(TS3Server* server, std::string channel_id, std::function<void(const std::string&)> callback)
    {
        ts5_api::InviteCreateRequest packet{
            .address = std::format("{}:{}", server->host, server->port),
            .name = server->name,
            .channel_id = channel_id,
            .channel_name = channel_id,
        };

        Resources::Post("https://invites.teamspeak.com/servers/create", glz::write_json(packet).value_or(std::string{}), [callback](const bool success, const std::string& response, void*) {
            if (!success) {
                Log::Error("Failed to get teamspeak invite link (1)");
                Log::Log("%s", response.c_str());
                return;
            }
            ts5_api::InviteCreateResponse res{};
            if (auto ec = glz::read<json_opts>(res, response); ec) {
                Log::Error("Failed to get teamspeak invite link (2)");
                return;
            }
            if (res.id.empty()) {
                Log::Error("Failed to get teamspeak invite link (3)");
                return;
            }
            const std::string url = std::format("https://tmspk.gg/{}", res.id);
            callback(url);
        });
    }

    void CHAT_CMD_FUNC(OnTeamspeakCommand)
    {
        if (!IsConnected()) {
            Log::Error("GWToolbox isn't connected to Teamspeak 5");
            return;
        }
        const auto teamspeak_server = GetCurrentServer();
        if (!(teamspeak_server && !teamspeak_server->host.empty())) {
            Log::Error("Teamspeak 5 isn't connected to a server");
            return;
        }
        wchar_t buf[120];
        swprintf(buf, _countof(buf) - 1, L"%s (%d users)",
                 TextUtils::StringToWString(teamspeak_server->name).c_str(),
                 teamspeak_server->user_count);
        GW::Chat::SendChat('#', buf);

        swprintf(buf, _countof(buf) - 1, L"TS3: [https://invite.teamspeak.com/%S/?port=%d;xx]",
                 teamspeak_server->host.c_str(),
                 teamspeak_server->port);
        GW::Chat::SendChat('#', buf);

        GetServerInviteLink(teamspeak_server, teamspeak_server->my_channel_id, [](const std::string& url) {
            wchar_t buf[120];
            swprintf(buf, _countof(buf) - 1, L"TS5: [%S;xx]", url.c_str());
            GW::Chat::SendChat('#', buf);
        });
    }

    void SendTeamspeakHandshake()
    {
        const ts5_api::HandshakeEnvelope packet{
            .type = "auth",
            .payload = {
                .identifier = gwtoolbox_teamspeak5_identifier,
                .version = gwtoolbox_teamspeak5_version,
                .name = gwtoolbox_teamspeak5_name,
                .description = gwtoolbox_teamspeak5_description,
                .content = {.apiKey = gwtoolbox_teamspeak5_api_key},
            },
        };
        websocket->send(glz::write_json(packet).value_or(std::string{}));
    }

    bool Connect(bool user_invoked = false)
    {
        pending_connect = false;
        if (step == Connecting || IsConnected()) {
            return true;
        }
        if (step == Downloading) {
            return true;
        }
        step = Connecting;
        if (!enabled) {
            step = Idle;
            return false;
        }
        int res;
        if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            if (user_invoked) {
                Log::Error("Failed to call WSAStartup: %d\n", res);
            }
            step = Idle;
            return false;
        }
        step = Connecting;
        Resources::EnqueueWorkerTask([user_invoked] {
            websocket = WebSocket::from_url(GetWebsocketHost());
            if (websocket == nullptr) {
                if (user_invoked) {
                    Log::Error("Couldn't connect to the teamspeak 5 websocket; ensure Teamspeak 5 is running and that the 'Remote Apps' feature is enabled");
                }
            }
            else {
                if (user_invoked) {
                    Log::Flash("Teamspeak 5 connected");
                }
                SendTeamspeakHandshake();
                GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"ts", OnTeamspeakCommand);
                GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"ts5", OnTeamspeakCommand);
            }
            step = Idle;
            pending_connect = false;
        });
        return true;
    }


    TS3Server* UpsertServer(const ts5_api::ServerProperties& props, const uint32_t connection_id)
    {
        TS3Server* teamspeak_server = GetServer(connection_id);
        if (!teamspeak_server) {
            teamspeak_server = new TS3Server();
            connected_servers[connection_id] = teamspeak_server;
        }
        teamspeak_server->host = props.ip;
        teamspeak_server->name = props.name;
        teamspeak_server->port = props.port;
        teamspeak_server->user_count = props.clientsOnline;
        return teamspeak_server;
    }

    void RemoveServer(const uint32_t connection_id)
    {
        const auto found = connected_servers.find(connection_id);
        if (found != connected_servers.end()) {
            const auto server = found->second;
            connected_servers.erase(found);
            delete server;
        }
        if (current_server == connection_id) {
            current_server = 0xffffffff;
        }
    }

    template <typename Payload>
    bool ParsePayload(const glz::raw_json& raw, Payload& out)
    {
        return !glz::read<json_opts>(out, raw.str);
    }

    bool OnTeamspeakAuth(const glz::raw_json& raw_payload)
    {
        ts5_api::AuthPayload payload{};
        if (!ParsePayload(raw_payload, payload)) {
            return false;
        }
        if (!payload.apiKey.empty()) gwtoolbox_teamspeak5_api_key = payload.apiKey;
        current_server = 0xffffffff;
        for (size_t connection_id = 0; connection_id < payload.connections.size(); ++connection_id) {
            const auto& connection = payload.connections[connection_id];
            const auto teamspeak_server = UpsertServer(connection.properties, static_cast<uint32_t>(connection_id));
            teamspeak_server->my_client_id = connection.clientId;
            // Cycle and find our channel
            for (const auto& client : connection.clientInfos) {
                if (client.id == teamspeak_server->my_client_id) {
                    teamspeak_server->my_channel_id = client.channelId;
                    break;
                }
            }
            Log::Log("Teamspeak server info updated:\n%s (%s:%d), %d users", teamspeak_server->name.c_str(), teamspeak_server->host.c_str(), teamspeak_server->port, teamspeak_server->user_count);
        }
        current_server = payload.currentConnectionId;
        return true;
    }

    bool OnTeamspeakServerPropertiesUpdated(const glz::raw_json& raw_payload)
    {
        ts5_api::ServerPropertiesUpdatedPayload payload{};
        if (!ParsePayload(raw_payload, payload)) {
            return false;
        }
        if (!payload.apiKey.empty()) gwtoolbox_teamspeak5_api_key = payload.apiKey;
        const auto teamspeak_server = UpsertServer(payload.properties, payload.connectionId);
        Log::Log("Teamspeak server info updated:\n%s (%s:%d), %d users", teamspeak_server->name.c_str(), teamspeak_server->host.c_str(), teamspeak_server->port, teamspeak_server->user_count);
        return true;
    }

    bool OnTeamspeakConnectStatusChanged(const glz::raw_json& raw_payload)
    {
        ts5_api::ConnectStatusChangedPayload payload{};
        if (!ParsePayload(raw_payload, payload)) {
            return false;
        }
        if (!payload.apiKey.empty()) gwtoolbox_teamspeak5_api_key = payload.apiKey;
        const auto server = GetServer(payload.connectionId);
        if (server) {
            server->my_client_id = payload.clientId;
        }

        switch (payload.status) {
            case 0: // Disconnected
                RemoveServer(payload.connectionId);
                break;
            case 1: // Connected (current server)
                current_server = payload.connectionId;
                break;
        }
        Log::Log("OnTeamspeakConnectStatusChanged, status %d, connectionId %d", payload.status, payload.connectionId);
        return true;
    }

    bool OnTeamspeakClientMoved(const glz::raw_json& raw_payload)
    {
        ts5_api::ClientMovedPayload payload{};
        if (!ParsePayload(raw_payload, payload)) {
            return false;
        }
        if (!payload.apiKey.empty()) gwtoolbox_teamspeak5_api_key = payload.apiKey;
        const auto server = GetServer(payload.connectionId);
        if (!server || payload.clientId != server->my_client_id) {
            return false;
        }
        server->my_channel_id = payload.newChannelId;
        return true;
    }

    bool OnWebsocketMessage(const std::string& data)
    {
        //Log::Log("%s\n", data.c_str());
        ts5_api::IncomingEnvelope envelope{};
        if (auto ec = glz::read<json_opts>(envelope, data); ec) {
            Log::Log("ERROR: Failed to parse res JSON from response in websocket->dispatch\n");
            return false;
        }

        if (envelope.type == "auth") {
            OnTeamspeakAuth(envelope.payload);
        }
        else if (envelope.type == "serverPropertiesUpdated") {
            OnTeamspeakServerPropertiesUpdated(envelope.payload);
        }
        else if (envelope.type == "connectStatusChanged") {
            OnTeamspeakConnectStatusChanged(envelope.payload);
        }
        else if (envelope.type == "clientMoved") {
            OnTeamspeakClientMoved(envelope.payload);
        }
        else {
            Log::Log("Unhandled TS5 websocket type %s", envelope.type.c_str());
        }

        const auto teamspeak_server = GetCurrentServer();
        if (teamspeak_server) {
            Log::Log("Current server:\n%s (%s:%d), channel id %s, client_id %d, %d users", teamspeak_server->name.c_str(), teamspeak_server->host.c_str(), teamspeak_server->port, teamspeak_server->my_channel_id.c_str(), teamspeak_server->my_client_id,
                     teamspeak_server->user_count);
        }
        else {
            Log::Log("Current server: None");
        }

        return true;
    }
}


void Teamspeak5Module::Initialize()
{
    ToolboxModule::Initialize();
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"ts", OnTeamspeakCommand);
    Connect();
}


void Teamspeak5Module::Terminate()
{
    DeleteWebSocket();
    if (wsaData.wVersion) {
        WSACleanup();
        wsaData = {0};
    }
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

void Teamspeak5Module::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(enabled);
    LOAD_UINT(ws_port);
    LOAD_STRING(gwtoolbox_teamspeak5_api_key);
    GetWebsocketHost(true);
    pending_connect = true;
}

void Teamspeak5Module::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(enabled);
    SAVE_UINT(ws_port);
    SAVE_STRING(gwtoolbox_teamspeak5_api_key);
}

void Teamspeak5Module::Update(float)
{
    if (pending_connect) {
        Connect();
        pending_connect = false;
    }
    if (!enabled && IsConnected()) {
        pending_disconnect = true;
    }
    if (pending_disconnect) {
        if (websocket) {
            websocket->close();
        }
        pending_disconnect = false;
        return;
    }
    if (websocket) {
        switch (websocket->getReadyState()) {
            case WebSocket::OPEN:
                if (pending_connect) {
                    pending_connect = pending_disconnect = false;
                    return;
                }
            case WebSocket::CLOSING:
            case WebSocket::CONNECTING:
                websocket->poll();
                break;
            case WebSocket::CLOSED:
                delete websocket;
                websocket = nullptr;
                pending_connect = pending_disconnect = false;
                return;
        }
        websocket->dispatch(OnWebsocketMessage);
    }
}

void Teamspeak5Module::DrawSettingsInternal()
{
    ImGui::PushID("Teamspeak5Module");
    if (ImGui::Checkbox("Enable Teamspeak 5 integration", &enabled)) {
        if (enabled) {
            Connect(true);
        }
        else {
            pending_disconnect = true;
        }
    }
    ImGui::ShowHelp(gwtoolbox_teamspeak5_description);
    if (enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IsConnected() ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        auto status_str = [] {
            if (IsConnected()) {
                return "Connected";
            }
            if (step == Connecting) {
                return "Connecting";
            }
            if (step == Downloading) {
                return "Downloading";
            }
            return "Disconnected";
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
            ImGui::SetTooltip(IsConnected() ? "Click to disconnect" : "Click to connect");
        }
        if (IsConnected()) {
            ImGui::Indent();
            ImGui::TextUnformatted("Server:");
            ImGui::SameLine();
            const auto& teamspeak_server = GetCurrentServer();
            if (!teamspeak_server) {
                ImGui::TextDisabled("Not Connected");
            }
            else {
                ImGui::Text("%s", teamspeak_server->name.c_str());
                ImGui::Text("Host:");
                ImGui::SameLine();
                ImGui::Text("%s:%d", teamspeak_server->host.c_str(), teamspeak_server->port);
                ImGui::Text("Users:");
                ImGui::SameLine();
                ImGui::Text("%d", teamspeak_server->user_count);
            }
            ImGui::Unindent();
        }
        ImGui::TextDisabled("Use the /ts5 command to send your current server info in chat");
    }
    ImGui::PopID();
}
