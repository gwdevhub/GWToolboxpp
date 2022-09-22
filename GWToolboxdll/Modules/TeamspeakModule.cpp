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

#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>

#include <GWCA\Packets\StoC.h>

#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <ImGuiAddons.h>

#include <Modules/Resources.h>
#include <Modules/TeamspeakModule.h>

namespace {
    const char* ws_host = "ws://localhost:27032";
    std::thread connector;
    UINT WM_TEAMSPEAK3_JSON_API_STARTED = 0;
}
using easywsclient::WebSocket;
using nlohmann::json;
using json_vec = std::vector<json>;

void TeamspeakModule::Initialize() {
    ToolboxModule::Initialize();
    GW::Chat::CreateCommand(L"ts", OnTeamspeakCommand);
    Connect(false);
}
const bool TeamspeakModule::IsConnected() const {
    return websocket && websocket->getReadyState() == easywsclient::WebSocket::OPEN;
}
// Connect when WM_TEAMSPEAK3_JSON_API_STARTED broadcast is received from the teamspeak plugin
bool TeamspeakModule::WndProc(UINT Message, WPARAM, LPARAM) {
    if (!WM_TEAMSPEAK3_JSON_API_STARTED)
        WM_TEAMSPEAK3_JSON_API_STARTED = RegisterWindowMessageW(L"WM_TEAMSPEAK3_JSON_API_STARTED");
    if (Message == WM_TEAMSPEAK3_JSON_API_STARTED)
        Connect();
    return false;
}

void TeamspeakModule::OnTeamspeakCommand(const wchar_t*, int , LPWSTR* ) {
    auto& instance = Instance();
    if (!instance.IsConnected()) {
        Log::Error("GWToolbox isn't connected to Teamspeak 3");
        return;
    }
    if (instance.teamspeak_server.host.empty()) {
        Log::Error("Teamspeak 3 isn't connected to a server");
        return;
    }
    wchar_t name_buf[120];
    swprintf(name_buf, sizeof(name_buf) / sizeof(*name_buf), L"%s (%d users)",
        GuiUtils::StringToWString(instance.teamspeak_server.name).c_str(),
        instance.teamspeak_users.size());
    GW::Chat::SendChat('#', name_buf);
    wchar_t invite_buf[120];
    swprintf(invite_buf, sizeof(invite_buf) / sizeof(*invite_buf), L"http://invite.teamspeak.com/%s/?port=%d",
        GuiUtils::StringToWString(instance.teamspeak_server.host).c_str(),
        instance.teamspeak_server.port);
    GW::Chat::SendChat('#', invite_buf);
}
TeamspeakModule::~TeamspeakModule() {
    if (connector.joinable())
       connector.join();
    DeleteWebSocket();
    if (wsaData.wVersion)
        WSACleanup();
}
void TeamspeakModule::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    enabled = ini->GetBoolValue(Name(), VAR_NAME(enabled), enabled);
    pending_connect = true;
}
void TeamspeakModule::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(enabled), enabled);
}
bool TeamspeakModule::IsTS3ProtocolSupported() {
    HKEY phkResult;
    //NB: Don't need to call RegCloseKey. No need to do anything more fancy than check the existence here.
    return RegOpenKeyW(HKEY_CLASSES_ROOT, L"ts3server", &phkResult) == ERROR_SUCCESS;
}
bool TeamspeakModule::Connect(bool user_invoked) {
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
    std::filesystem::path settings_folder = GetTeamspeakSettingsFolderPath() / "plugins";
    if (!std::filesystem::exists(settings_folder)) {
        if(user_invoked)
            Log::Error("Failed to find TS3Client setting folder; is Teamspeak 3 installed?");
        step = Idle;
        return false;
    }
    BOOL is_x64 = false;
    std::filesystem::path running_teamspeak_exe;
    if (!GetTeamspeakProcess(&running_teamspeak_exe, &is_x64)) {
        if (user_invoked)
            Log::Error("Error finding running teamspeak executable (%04X)",GetLastError());
        step = Idle;
        return false;
    }
    if (running_teamspeak_exe.empty()) {
        if (user_invoked)
            Log::Error("Failed to find running teamspeak executable; is Teamspeak 3 running?");
        step = Idle;
        return false;
    }
    std::filesystem::path plugin_dll_filename(is_x64 ? teamspeak_plugin_dll_name_64 : teamspeak_plugin_dll_name_32);
    std::filesystem::path file_location(settings_folder / plugin_dll_filename);
    std::wstring download_url(teamspeak_plugin_download_prefix / plugin_dll_filename);
    if (!std::filesystem::exists(file_location)) {
        DownloadPlugin(user_invoked, is_x64, file_location);
        return false;
    }
    // This far; dll exists, exe is running; try to connect to websocket.
    int res;
    if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        if (user_invoked)
            Log::Error("Failed to call WSAStartup: %d\n", res);
        step = Idle;
        return false;
    }
    step = Connecting;
    if (connector.joinable())
        connector.join();
    connector = std::thread([this, user_invoked]() {
        websocket = WebSocket::from_url(ws_host);
        if (websocket == nullptr) {
            if(user_invoked)
                Log::Error("Couldn't connect to the teamspeak plugin; restart Teamspeak 3 and try again");
        }
        else {
            if (user_invoked)
                Log::Info("Teamspeak 3 plugin connected");
        }
        step = Idle;
        pending_connect = false;
        });
    return true;
}
void TeamspeakModule::Update(float) {
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
std::filesystem::path TeamspeakModule::GetTeamspeakSettingsFolderPath()
{
    WCHAR path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
    return std::filesystem::path(path) / "TS3Client";
}
bool TeamspeakModule::GetTeamspeakProcess(std::filesystem::path* filename, PBOOL is_x64)
{
    DWORD aProcesses[1024], cRead;
    unsigned int i;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cRead)) {
        Log::Log("Failed to call EnumProcesses\n", GetLastError());
        return false;
    }
    // Calculate how many process identifiers were returned.
    DWORD cProcesses = cRead / sizeof(DWORD);
    // Print the name and process identifier for each process.
    for (i = 0; i < cProcesses; i++) {
        if (!aProcesses[i]) {
            // @Cleanup: Why would this happen?
            //Log::Log("Empty PID\n");
            continue;
        }
        // Get a handle to the process.
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
        if (!hProcess) {
            // @Cleanup: Why would this happen?
            //Log::Log("Failed to get process by PID %d (%04X)\n", aProcesses[i], GetLastError());
            continue;
        }
        wchar_t szProcessName[MAX_PATH];
        DWORD exeLen = sizeof(szProcessName) / sizeof(wchar_t);
        if (!QueryFullProcessImageNameW(hProcess, 0, szProcessName, &exeLen)) {
            Log::Log("Failed to call QueryFullProcessImageNameW on PID %d (%04X)\n", aProcesses[i], GetLastError());
            continue;
        }
        std::wstring processNameWs(szProcessName);
        if (processNameWs.find(L"ts3client_win64.exe") == std::wstring::npos
            && processNameWs.find(L"ts3client_win32.exe") == std::wstring::npos) {
            CloseHandle(hProcess);
            continue; // This is not the droid you're looking for...
        }
        if (is_x64) {
            if (!IsWow64Process(hProcess, is_x64)) {
                Log::Log("Failed to call IsWow64Process on PID %d (%04X)\n", aProcesses[i], GetLastError());
                CloseHandle(hProcess);
                return false;
            }
            if (!*is_x64) {
                // 64 bit process on 64 bit OS, or 32 bit process on 32 bit OS
                SYSTEM_INFO lpSystemInfo;
                GetNativeSystemInfo(&lpSystemInfo);
                *is_x64 = lpSystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
            }
            else {
                // 32 bit process on 64 bit OS. Sorry for confusing var name.
                *is_x64 = *is_x64 ? 0 : 1;
            }
        }
        // NB: We COULD now traverse the modules for the process to see if the TS3 DLL is loaded.
        // Because TS3 could be x64 bit, and we're a x32 bit program, lets not try to handle this differently and instead presume
        // that if the DLL exists in the TS3 appdata folder, then it is loaded.
        CloseHandle(hProcess);
        *filename = std::filesystem::path(szProcessName);
        return true;
    }
    *filename = L"";
    return true; // No errors, but no exe either
}
void TeamspeakModule::DrawSettingInternal() {
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
        auto status_str = [this]() {
            if (this->IsConnected())
                return "Connected";
            if (this->step == Connecting)
                return "Connecting";
            if (this->step == Downloading)
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
            ImGui::Text("Server:");
            ImGui::SameLine();
            if (teamspeak_server.name.empty())
                ImGui::TextDisabled("Not Connected");
            else {
                ImGui::Text("%s", teamspeak_server.name.c_str());
                ImGui::Text("Host:"); ImGui::SameLine(); ImGui::Text("%s:%d", teamspeak_server.host.c_str(), teamspeak_server.port);
                ImGui::Text("Users:"); ImGui::SameLine(); ImGui::Text("%d", teamspeak_users.size());
            }
            ImGui::Unindent();
        }
        ImGui::TextDisabled("Use the /ts command to send your current server info in chat");
    }
    ImGui::PopID();
}
bool TeamspeakModule::GetLatestRelease(PluginRelease* release, bool is_x64) {
    // Get list of releases
    std::string response;
    unsigned int tries = 0;
    const char* url = "https://api.github.com/repos/3vcloud/Teamspeak3_WinAPI/releases";
    bool success = false;
    do {
        success = Resources::Instance().Download(url, response);
        tries++;
    } while (!success && tries < 5);
    if (!success) {
        Log::Log("Failed to download %s\n%s", url, response.c_str());
        return false;
    }
    using Json = nlohmann::json;
    Json json = Json::parse(response.c_str(), nullptr, false);
    if (json == Json::value_t::discarded || !json.is_array() || !json.size())
        return false;
    for (unsigned int i = 0; i < json.size(); i++) {
        if (!(json[i].contains("tag_name") && json[i]["tag_name"].is_string()))
            continue;
        if (!(json[i].contains("assets") && json[i]["assets"].is_array() && json[i]["assets"].size() > 0))
            continue;
        if (!(json[i].contains("body") && json[i]["body"].is_string()))
            continue;
        for (unsigned int j = 0; j < json[i]["assets"].size(); j++) {
            const Json& asset = json[i]["assets"][j];
            if (!(asset.contains("name") && asset["name"].is_string())
                || !(asset.contains("browser_download_url") && asset["browser_download_url"].is_string()))
                continue;
            std::string asset_name = asset["name"].get<std::string>();
            if (is_x64 && asset_name != "ts3_json_api_plugin_win64.dll")
                continue;
            if (!is_x64 && asset_name != "ts3_json_api_plugin_win32.dll")
                continue;
            release->download_url = asset["browser_download_url"].get<std::string>();
            release->version = json[i]["tag_name"].get<std::string>();
            release->body = json[i]["body"].get<std::string>();
            return true;
        }
    }
    return false;
}
void TeamspeakModule::DownloadPlugin(bool user_invoked, bool is_x64, const std::filesystem::path& download_to) {
    pending_connect = true;
    step = Downloading;
    Resources::Instance().EnqueueWorkerTask([user_invoked, is_x64, download_to]() {
        // Here we are in the worker thread and can do blocking operations
        // Reminder: do not send stuff to gw chat from this thread!
        auto& instance = Instance();
        PluginRelease release;
        if (!instance.GetLatestRelease(&release, is_x64)) {
            // Error getting server version. Server down? We can do nothing.
            if (user_invoked)
                Log::Error("Error checking for teamspeak plugin update");
            instance.step = Idle;
            instance.pending_connect = false;
            return;
        }
        Resources::Instance().Download(download_to, release.download_url,
            [user_invoked](bool success, const std::wstring& error) -> void {
                auto& instance = Instance();
                if (success) {
                    instance.step = Idle;
                    instance.Connect(user_invoked);
                }
                else {
                    if (user_invoked) {
                        Log::ErrorW(L"Updated error - cannot download teamspeak plugin dll\n%s", error.c_str());
                    }
                    else {
                        Log::LogW(L"Updated error - cannot download teamspeak plugin dll\n%s", error.c_str());
                    }

                    instance.step = Idle;
                }
            });
        });
}
void TeamspeakModule::OnWebsocketMessage(const std::string& data) {
    Log::Log("%s\n", data.c_str());
    const json& res = json::parse(data.c_str(), nullptr, false);
    if (res == json::value_t::discarded) {
        Log::Log("ERROR: Failed to parse res JSON from response in websocket->dispatch\n");
        return;
    }
    auto* teamspeak_server = &Instance().teamspeak_server;
    auto* teamspeak_users = &Instance().teamspeak_users;
    if (res.find("server") != res.end()) {
        json server = res["server"];
        if (res["server"].is_object()) {
            TS3Server tmp_server;
            if (!TS3Server::fromJson(server, &tmp_server)) {
                Log::Log("ERROR: Failed to parse Teamspeak server\n");
                return;
            }
            if (teamspeak_server->host != tmp_server.host)
                teamspeak_users->clear();
            *teamspeak_server = tmp_server; // Copy
        }
    }
    if (!teamspeak_server->host[0]) {
        teamspeak_users->clear();
        return; // Not connected to a teamspeak server, drop out here.
    }
    json_vec clients;
    if (res.is_array()) {
        // Only a few users updated.
        clients = res.get<json_vec>();
    }
    else if (res.find("clients") != res.end() && res["clients"].is_array()) {
        // Full refresh
        teamspeak_users->clear();
        clients = res["clients"].get<json_vec>();
    }
    TS3User user;
    for (size_t i = 0; i < clients.size(); i++) {
        if (!TS3User::fromJson(clients[i], &user)) {
            Log::Log("ERROR: Failed to parse Teamspeak user\n");
            continue;
        }
        teamspeak_users->emplace(user.id, user); // Copy
    }
    //Log::Info("Updated from teamspeak");
}
void TeamspeakModule::DeleteWebSocket() {
    if (!websocket) return;
    if (websocket->getReadyState() == easywsclient::WebSocket::OPEN)
        websocket->close();
    disconnecting = true;
    while (websocket->getReadyState() != easywsclient::WebSocket::CLOSED)
        websocket->poll();
    delete websocket;
    websocket = nullptr;
}
bool TeamspeakModule::TS3User::fromJson(nlohmann::json& json, TeamspeakModule::TS3User* user) {
    user->name = json["name"].get<std::string>();
    user->channel = json["channel"].get<uint32_t>();
    user->id = json["id"].get<uint32_t>();
    user->is_talking = json["is_talking"].get<char>();
    user->mic_muted = json["mic_muted"].get<char>();
    user->speakers_muted = json["speakers_muted"].get<char>();
    return true;
}
bool TeamspeakModule::TS3Server::fromJson(nlohmann::json& json, TeamspeakModule::TS3Server* server) {
    server->name = json["name"].get<std::string>();
    server->host = json["host"].get<std::string>();
    server->port = json["port"].get<unsigned short>();
    server->my_client_id = json["my_id"].get<unsigned int>();
    return true;
}