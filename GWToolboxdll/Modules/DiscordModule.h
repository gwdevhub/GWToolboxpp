#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Guild.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUserManager* users;
    struct IDiscordAchievementManager* achievements;
    struct IDiscordActivityManager* activities;
    struct IDiscordRelationshipManager* relationships;
    struct IDiscordApplicationManager* application;
    struct IDiscordLobbyManager* lobbies;
    struct IDiscordNetworkManager* network;
    DiscordUserId user_id;
};
// Encoded/decoded when joining another player's game.
struct DiscordJoinableParty {
    unsigned short map_id = 0;
    short district_id;
    short region_id;
    short language_id;
    uint32_t ghkey[4];
    wchar_t player[32];
};
// Used to record current GH info
struct CurrentGuildHall {
    wchar_t tag[8];
    wchar_t name[32];
    uint32_t ghkey[4];
};

class DiscordModule : public ToolboxModule {
    DiscordModule() {};
    ~DiscordModule() {};
public:
    static DiscordModule& Instance() {
        static DiscordModule instance;
        return instance;
    }

    const char* Name() const override { return "Discord"; }
    const char* Description() const override { return "Show better 'Now Playing' info in Discord from Guild Wars"; }
    const char* Icon() const override { return ICON_FA_HEADSET; }

    const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

    void UpdateActivity();

    void InviteUser(DiscordUser* user);
    void FailedJoin(const char* error_msg);
    void JoinParty();
    bool IsInJoinablePartyMap();

    Application app;
    DiscordActivity activity;
    DiscordActivity last_activity;

private:
    DiscordCreateParams params;
    
    IDiscordUserEvents users_events;
    IDiscordActivityEvents activities_events;
    IDiscordRelationshipEvents relationships_events;
    IDiscordNetworkEvents network_events;
    IDiscordCoreEvents core_events;
    

    // setting vars
    bool discord_enabled = true;
    bool hide_activity_when_offline = true;
    bool show_location_info = true;
    bool show_character_info = true;
    bool show_party_info = true;

    // runtime vars
    bool discord_connected = false;
    time_t zone_entered_time = 0;
    bool pending_activity_update = false;
    bool pending_discord_connect = true;
    std::wstring dll_location;
    time_t last_activity_update = 0;

    bool LoadDll();
    bool UnloadDll();
    bool Connect();
    void ConnectCanary();
    void Disconnect();

    GW::HookEntry ErrorMessage_Callback;
    GW::HookEntry PartyUpdateSize_Callback;
    GW::HookEntry PartyPlayerAdd_Callback;
    GW::HookEntry InstanceLoadInfo_Callback;

};
