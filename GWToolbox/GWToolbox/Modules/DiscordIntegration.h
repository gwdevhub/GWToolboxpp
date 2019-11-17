#pragma once

#include <discord_game_sdk.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameEntities/Guild.h>

class DiscordIntegration : public ToolboxModule {
    DiscordIntegration();
    ~DiscordIntegration() {};

public:
    static DiscordIntegration& Instance() {
        static DiscordIntegration instance;
        return instance;
    }

private: // from ToolboxModule
    const char* Name() const override { return "Discord Integration"; }
    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

public:
    // Try to take ownership of Discord integration (by GWToolboxpp)
    // and connect if it succeed
    void Connect();

    // Close Discord connection and release ownership of the connection.
    void Disconnect();

    void UpdateActivity();
    bool IsConnectionOwner() const { return owner_mutex != nullptr; }

    static bool IsMapUnlocked(uint32_t map_id);
    static GW::Guild* GetCurrentGHGuild();

private:
    static void UpdateActivityCallback(void* data, EDiscordResult result);
    static void OnDiscordLog(void* data, enum EDiscordLogLevel level, const char* message);

private:
    DiscordCreateParams params;
    
    IDiscordNetworkEvents network_events;
    IDiscordActivityEvents activity_events;

    IDiscordCore* discord_core = nullptr;
    IDiscordNetworkManager* network_mgr = nullptr;
    IDiscordActivityManager* activity_mgr = nullptr;

    HMODULE hDiscordDll = nullptr;

    // setting vars
    bool hide_activity_when_offline = true;

    // runtime vars
    HANDLE owner_mutex = nullptr;
    time_t timestamp_start = 0;
    bool update_activity_required = false;
    uint32_t activity_party_size = 0;
    uint32_t player_status = 0;

    std::wstring dll_location;

    typedef EDiscordResult (__cdecl* DiscordCreate_pt)(
        DiscordVersion version,
        DiscordCreateParams* params,
        IDiscordCore** result);
    DiscordCreate_pt DiscordCreate;

    GW::HookEntry InstanceTimer_Callback;
};
