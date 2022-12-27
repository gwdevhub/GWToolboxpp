#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWidget.h>

class ServerInfoWidget : public ToolboxWidget {
    ServerInfoWidget() = default;
    ServerInfoWidget(const ServerInfoWidget&) = delete;
    ~ServerInfoWidget() {
        if (server_info_fetcher.joinable())
            server_info_fetcher.join();
    }
public:
    struct ServerInfo {
        ServerInfo(std::string _ip) : ip(_ip) {};
        std::string ip;
        std::string country;
        std::string city;
        time_t last_update = 0;
    };
private:
    std::map<std::string, ServerInfo*> servers_by_ip;
    ServerInfo* current_server_info = nullptr;
    std::thread server_info_fetcher;
    GW::HookEntry InstanceLoadInfo_HookEntry;
public:
    static ServerInfoWidget& Instance() {
        static ServerInfoWidget instance;
        return instance;
    }
    ServerInfo* GetServerInfo();
    const char* Name() const override { return "Server Info"; }

    void Initialize() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingInternal() override;
};
