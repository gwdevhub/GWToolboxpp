#pragma once

#include <ToolboxModule.h>

class TeamspeakModule : public ToolboxModule {
    TeamspeakModule() {};
    ~TeamspeakModule();
public:
    const wchar_t* teamspeak_plugin_dll_name_64 = L"ts3_json_api_plugin_win64.dll";
    const wchar_t* teamspeak_plugin_dll_name_32 = L"ts3_json_api_plugin_win32.dll";
    const wchar_t* teamspeak_plugin_download_prefix = L"https://github.com/3vcloud/Teamspeak3_WinAPI/releases/download/1.0/";

    static TeamspeakModule& Instance() {
        static TeamspeakModule instance;
        return instance;
    }

    const char* Name() const override { return "Teamspeak"; }
    const char8_t* Icon() const override { return ICON_FA_HEADSET; }

    const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void Update(float) override;
    void DrawSettingInternal() override;
    const bool IsConnected() const;
    static bool IsTS3ProtocolSupported();

    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void LoadSettings(CSimpleIni* ini);
    void SaveSettings(CSimpleIni* ini);

    struct PluginRelease {
        std::string body;
        std::string version;
        std::string download_url;
    };

    bool GetLatestRelease(PluginRelease* release, bool is_x64);

private:

    enum ConnectionStep : uint8_t {
        Idle,
        Connecting,
        Downloading
    } step = Idle;

    bool enabled = false;
    bool disconnecting = false;
    bool pending_connect = false;
    bool pending_disconnect = false;
    WSAData wsaData = { 0 };
    easywsclient::WebSocket* websocket = nullptr;

    struct TS3User {
        std::string name;
        uint32_t id = 0;
        uint32_t channel = 0;
        char is_talking = 0;
        char speakers_muted = 0;
        char mic_muted = 0;
        static bool fromJson(nlohmann::json& json, TS3User* user);
    };
    struct TS3Server {
        uint32_t my_client_id = 0;
        std::string name;
        //char welcome_message[512];
        std::string host;
        unsigned short port = 0;
        //char password[128];
        static bool fromJson(nlohmann::json& json, TS3Server* server);
    } teamspeak_server;
    std::map<uint32_t, TS3User> teamspeak_users;

    std::filesystem::path GetTeamspeakSettingsFolderPath();
    bool GetTeamspeakProcess(std::filesystem::path* filename, PBOOL is_x64 = nullptr);
    bool Connect(bool user_invoked = false);
    void DeleteWebSocket();
    void DownloadPlugin(bool user_invoked, bool is_x64, const std::filesystem::path& download_to);


    static void OnWebsocketMessage(const std::string& data);
    static void OnTeamspeakCommand(const wchar_t*, int argc, LPWSTR* argv);
};
