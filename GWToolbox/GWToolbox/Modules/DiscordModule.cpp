#include "stdafx.h"
#include "DiscordModule.h"

#include <GuiUtils.h>
#include "GWToolbox.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include "GuiUtils.h"
#include "logger.h"



#define DISCORD_APP_ID 378706083788881961

#define DiscordCreate (discordCreate)

typedef enum EDiscordResult(__cdecl* DiscordCreate_pt)(DiscordVersion version,struct DiscordCreateParams* params,struct IDiscordCore** result);



char* RegionAssets[28] {
    "region_kryta",
    "region_maguuma",
    "region_ascalon",
    "region_shiverpeaks",
    "region_ha",
    "region_crystaldesert",
    "region_fow",
    "region_presearing",
    "region_kaineng",
    "region_kurz",
    "region_lux",
    "region_shingjea",
    "region_kourna",
    "region_vabbi",
    "region_deso",
    "region_istan",
    "region_torment",
    "region_tarnished",
    "region_depths",
    "region_farshivs",
    "region_charrhomelands",
    "region_battleisles",
    "region_battlejahai",
    "region_flightnorth",
    "region_tenguaccords",
    "region_whitemantle",
    "region_swat",
    "region_swat"
};
char* RegionNames[28]{
    "Kryta",
    "Maguuma",
    "Ascalon",
    "Shiverpeak Mountains",
    "Heroes' Ascent",
    "Crystal Desert",
    "Fissure of Woe",
    "Presearing",
    "Kaineng",
    "Kurzick",
    "Luxon",
    "Shing Jea",
    "Kourna",
    "Vabbi",
    "The Desolation",
    "Istan",
    "Realm of Torment",
    "The Tarnished Coast",
    "The Depths of Tyria",
    "Far Shiverpeaks",
    "Charr Homelands",
    "Battle Isles",
    "Battle of Jahai",
    "The Flight North",
    "The Tengu Accords",
    "The Rise of The White Mantle",
    "Swat",
    "Dev Region"
};
char* ProfessionNames[11]{
    "No Profession",
    "Warrior",
    "Ranger",
    "Monk",
    "Necromancer",
    "Mesmer",
    "Elementalist",
    "Assassin",
    "Ritualist",
    "Paragon",
    "Dervish"
};

DiscordCreate_pt discordCreate;

bool discord_connected = false;
time_t zone_entered_time = 0;
bool pending_activity_update = false;
bool show_party_info = false;

void OnUserUpdated(void* data)
{
    struct Application* app = (struct Application*)data;
    struct DiscordUser user;
    app->users->get_current_user(app->users, &user);
    app->user_id = user.id;
    Log::Log("OnUserUpdated: %s\n", user.username);
}
void UpdateActivityCallback(void* data, enum EDiscordResult result) {
    struct Application* app = (struct Application*)data;
    if (result == DiscordResult_Ok)
        Log::Log("Activity updated successfully\n");
    else
        Log::Log("Activity update FAILED!\n");
}
void DiscordModule::Initialize() {
    ToolboxModule::Initialize();
    // Initialise discord objects
    memset(&app, 0, sizeof(app));
    memset(&users_events, 0, sizeof(users_events));
    memset(&activities_events, 0, sizeof(activities_events));
    memset(&relationships_events, 0, sizeof(relationships_events));
    memset(&activity, 0, sizeof(activity));

    DiscordCreateParamsSetDefault(&params);
    params.client_id = DISCORD_APP_ID;
    params.flags = DiscordCreateFlags_NoRequireDiscord;
    params.event_data = &app;
    params.activity_events = &activities_events;
    params.relationship_events = &relationships_events;
    params.user_events = &users_events;
    // Callbacks
    users_events.on_current_user_update = OnUserUpdated;

    // Connect to discord if enabled.
    if(connect_to_discord)
        discord_connected = Connect();

    GW::StoC::AddCallback<GW::Packet::StoC::InstanceLoadInfo>(
        [this](GW::Packet::StoC::InstanceLoadInfo* packet) -> bool {
            zone_entered_time = time(nullptr);
            pending_activity_update = true;
            if (!discord_connected)
                discord_connected = Connect();
            return false;
        });
    // Update discord status on party size change
    GW::StoC::AddCallback<GW::Packet::StoC::PartyPlayerAdd>(
        [this](GW::Packet::StoC::PartyPlayerAdd* packet) -> bool {
            if (packet->invite_stage != 3)
                return false; // Player not added, only invited
            pending_activity_update = true;
            return false;
        });
    GW::StoC::AddCallback<GW::Packet::StoC::PartyPlayerRemove>(
        [this](GW::Packet::StoC::PartyPlayerRemove* packet) -> bool {
            pending_activity_update = true;
            return false;
        });
    if(GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
        zone_entered_time = time(nullptr) - GW::Map::GetInstanceTime();
    pending_activity_update = true;
}
bool DiscordModule::Connect() {
    if (!LoadDll())
        return false; // Failed to hook into discord_game_sdk.dll
    if (DiscordCreate(DISCORD_VERSION, &params, &app.core) != DiscordResult_Ok) {
        Log::Error("Failed to create discord connection");
        return false;
    }
    app.users = app.core->get_user_manager(app.core);
    app.achievements = app.core->get_achievement_manager(app.core);
    app.activities = app.core->get_activity_manager(app.core);
    app.application = app.core->get_application_manager(app.core);
    app.lobbies = app.core->get_lobby_manager(app.core);

    DiscordBranch branch;
    app.application->get_current_branch(app.application, &branch);
    Log::Log("Current branch %s\n", branch);

    app.relationships = app.core->get_relationship_manager(app.core);
    Log::Log("Successful discord\n");
    return true;
}
bool DiscordModule::InjectDll() {
    // Check first
    HINSTANCE hGetProcIDDLL = LoadLibrary("discord_game_sdk.dll");
    if (hGetProcIDDLL)
        return true; // Already injected.
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
    if (!h)
        return false; // Failed to get current process handle
    LPVOID LoadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!LoadLibAddr)
        return false; // Failed to find LoadLibraryA function
    LPVOID dereercomp = VirtualAllocEx(h, NULL, strlen(dllName), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(h, dereercomp, dllName, strlen(dllName), NULL);
    HANDLE asdc = CreateRemoteThread(h, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddr, dereercomp, 0, NULL);
    WaitForSingleObject(asdc, INFINITE);
    VirtualFreeEx(h, dereercomp, strlen(dllName), MEM_RELEASE);
    CloseHandle(asdc);
    CloseHandle(h);
    return true;
}
bool DiscordModule::LoadDll() {
    if (discordCreate)
        return true; // Already loaded.
    HINSTANCE hGetProcIDDLL = LoadLibrary("discord_game_sdk.dll");
    if (!hGetProcIDDLL) {
        Log::Error("Failed to load discord_game_sdk.dll");
        return false;
    }
    // resolve function address here
    discordCreate = (DiscordCreate_pt)GetProcAddress(hGetProcIDDLL, "DiscordCreate");
    if (!discordCreate) {
        Log::Error("Failed to find address for DiscordCreate");
        return false;
    }
    Log::Log("Discord DLL hooked!\n");
    return true;
}
bool set_status = false;
void DiscordModule::Update(float delta) {
    if (!discord_connected) 
        return;
    if (app.core->run_callbacks(app.core) != DiscordResult_Ok) {
        Log::Error("Discord disconnected");
        discord_connected = false;
    }
    if (pending_activity_update)
        UpdateActivity();
}
void DiscordModule::UpdateActivity() {
    if (!pending_activity_update || !discord_connected)
        return;
    GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
    GW::AreaInfo* m = GW::Map::GetCurrentMapInfo();
    GW::Agent* a = GW::Agents::GetPlayer();
    GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();
    if (!p || !m || !a || !GW::Map::GetIsMapLoaded())
        return;

    uint32_t map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    activity.party.size.current_size = p->players.size() + p->heroes.size() + p->henchmen.size();
    activity.party.size.max_size = m->max_party_size;
    
    std::string map_name = "Unknown Location";
    if (map_id && map_id < 729)
        map_name = GW::Constants::NAME_FROM_ID[map_id];
    if (m->type == GW::RegionType::RegionType_GuildHall) {
        sprintf(activity.details, "In Guild Hall");
    } else
        sprintf(activity.details, "%s", map_name.c_str());
    activity.timestamps.start = 0;
    if (instance_type == GW::Constants::InstanceType::Explorable) {
        activity.timestamps.start = zone_entered_time;
        activity.instance = true;
    } else {
        sprintf(activity.party.id, "%d-%d", map_id, p->party_id);
    }
    sprintf(activity.state, "%ls", GW::GameContext::instance()->character->player_name);
    sprintf(activity.assets.small_image, "profession_%d_512px", a->primary);
    sprintf(activity.assets.small_text, ProfessionNames[a->primary]);
    sprintf(activity.assets.large_image, RegionAssets[m->region]);
    sprintf(activity.assets.large_text, "Region: %s", RegionNames[m->region]);

    Log::Log("Outgoing discord state = %s\n", activity.state);// activity.state);
    Log::Log("Outgoing discord details = %s\n", activity.details); //activity.details);
    app.activities->update_activity(app.activities, &activity, &app, UpdateActivityCallback);
    pending_activity_update = false;
}



