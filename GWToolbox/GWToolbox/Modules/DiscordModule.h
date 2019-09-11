#pragma once

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>
#include <discord_game_sdk/c/discord_game_sdk.h>

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUserManager* users;
    struct IDiscordAchievementManager* achievements;
    struct IDiscordActivityManager* activities;
    struct IDiscordRelationshipManager* relationships;
    struct IDiscordApplicationManager* application;
    struct IDiscordLobbyManager* lobbies;
    DiscordUserId user_id;
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

    void Initialize() override;

    void Update(float delta) override;

    void UpdateActivity();

    bool connect_to_discord = true;
private:
    DiscordCreateParams params;
    Application app;
    IDiscordUserEvents users_events;
    IDiscordActivityEvents activities_events;
    IDiscordRelationshipEvents relationships_events;

    DiscordActivity activity;

    static bool LoadDll();
    bool Connect();
};
