/*
Thanks to KAOS for original version from https://github.com/GregLando113/gw-discord

HOW TO TEST WITH 2 DISCORD INSTANCES IN DEBUG MODE:
1. Close DiscordCanary.exe, load first GW client and start toolbox
    Client 1 is now sending statuses to Discord.exe
2. Open DiscordCanary.exe, load second GW client and start toolbox
    Client 2 is now sending statuses to DiscordCanary.exe

NOTE: Disconnecting/reconnecting will mess this up so repeat process.
*/

#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Friendslist.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <base64.h>
#include <sha1.hpp>

#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/DiscordModule.h>
#include <Modules/Resources.h>
#include <Windows/TravelWindow.h>

#define DISCORD_APP_ID 378706083788881961

typedef enum EDiscordResult(__cdecl* DiscordCreate_pt)(DiscordVersion version,struct DiscordCreateParams* params,struct IDiscordCore** result);

const char* region_assets[] = {
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
const char* region_names[] = {
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
const char* profession_names[] = {
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

const char* map_languages[] = {
    "English",
    "Unknown",
    "French",
    "German",
    "Italian",
    "Spanish",
    "Unknown",
    "Unknown",
    "Unknown",
    "Polish",
    "Russian"
};
const char* region_abbreviations[] = {
    "America", // America
    "Asia Korea", // Asia Korean
    "Europe", // Europe
    "Asia Chinese", // Asia Chinese
    "Asia Japan" // Asia Japanese
};
const char* language_abbreviations[] = {
    "E", // English
    "",
    "F", // French
    "G", // German
    "I", // Italian
    "S", // Spanish
    "","","",
    "P", // Polish
    "R" // Russian
};

DiscordCreate_pt discordCreate;
GuiUtils::EncString map_name_decoded;
short decoded_map_id = 0;
int64_t pending_join_request_reply_user_id = 0;
time_t pending_join_request_reply_user_at = 0;
DiscordJoinableParty join_in_progress;
time_t join_party_next_action = 0;
time_t join_party_started_at = 0;
time_t join_party_started = 0;
time_t discord_connected_at = 0;

static void UpdateActivityCallback(void* data, enum EDiscordResult result) {
    UNREFERENCED_PARAMETER(data);
    Log::Log(result == DiscordResult_Ok ? "Activity updated successfully.\n" : "Activity update FAILED!\n");
}
static void OnJoinRequestReplyCallback(void* data, enum EDiscordResult result) {
    UNREFERENCED_PARAMETER(data);
    Log::Log(result == DiscordResult_Ok ? "Join request reply sent successfully.\n" : "Join request reply send FAILED!\n");
}
static void OnSendInviteCallback(void* data, enum EDiscordResult result) {
    UNREFERENCED_PARAMETER(data);
    Log::Log(result == DiscordResult_Ok ? "Invite sent successfully.\n" : "Invite send FAILED!\n");
}
static void OnNetworkMessage(void* event_data, DiscordNetworkPeerId peer_id, DiscordNetworkChannelId channel_id, uint8_t* data, uint32_t data_length) {
    UNREFERENCED_PARAMETER(event_data);
    UNREFERENCED_PARAMETER(peer_id);
    UNREFERENCED_PARAMETER(channel_id);
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(data_length);
    Log::Log("Discord: Network message\n");
}
static void OnJoinParty(void* event_data, const char* secret) {
    UNREFERENCED_PARAMETER(event_data);
    Log::Log("Discord: on_activity_join %s\n",secret);
    memset(&join_in_progress, 0, sizeof(join_in_progress));
    b64_dec(secret, &join_in_progress);
}
// NOTE: In our game, anyone can join anyone else's party - work around for "ask to join" by auto-accepting.
static void OnJoinRequest(void* data, DiscordUser* user) {
    UNREFERENCED_PARAMETER(data);
    Log::Log("Join request received from %s; automatically accept\n",user->username);
    Application* app = &DiscordModule::Instance().app;
    app->activities->send_request_reply(app->activities, user->id, EDiscordActivityJoinRequestReply::DiscordActivityJoinRequestReply_Yes, app, OnJoinRequestReplyCallback);
}
static void OnPartyInvite(void* event_data, EDiscordActivityActionType type, DiscordUser* user, DiscordActivity* activity) {
    UNREFERENCED_PARAMETER(event_data);
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(activity);
    Log::Log("Party invite received from %s\n", user->username);
}
static void OnDiscordLog(void* data, EDiscordLogLevel level, const char* message) {
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(level);
    UNREFERENCED_PARAMETER(message);
    Log::Log("Discord Log Level %d: %s\n", level, message);
}
// Get pid from executable name (i.e. DiscordCanary.exe)
DWORD GetProcId(const char* ProcName) {
    PROCESSENTRY32   pe32;
    HANDLE         hSnapshot = NULL;
    uint32_t pid = 0;
    uint32_t len = strlen(ProcName);
    pe32.dwSize = sizeof(PROCESSENTRY32);
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(hSnapshot, &pe32))
    {
        do {
            if (strcmp(pe32.szExeFile, ProcName) == 0 && strlen(pe32.szExeFile) == len)
                pid = pe32.th32ProcessID;
        } while (!pid && Process32Next(hSnapshot, &pe32));
    }

    if (hSnapshot != INVALID_HANDLE_VALUE)
        CloseHandle(hSnapshot);

    return pid;
}
void DiscordModule::InviteUser(DiscordUser* user) {
    char invite_str[128];
    sprintf(invite_str, "%s, %s", activity.details, activity.state);
    app.activities->send_invite(app.activities, user->id, EDiscordActivityActionType::DiscordActivityActionType_Join, invite_str, &app, OnSendInviteCallback);
}
void DiscordModule::Terminate() {
    ToolboxModule::Terminate();
    Disconnect();
    UnloadDll();
}
void DiscordModule::Disconnect() {
    if(discord_connected)
        app.core->destroy(app.core); // Do this for each discord connection
    discord_connected = pending_activity_update = pending_discord_connect = false;
}
void DiscordModule::Initialize() {
    ToolboxModule::Initialize();

    strcpy(activity.name,"Guild Wars");
    activity.application_id = DISCORD_APP_ID;
    // Initialise discord objects
    memset(&app, 0, sizeof(app));
    memset(&activities_events, 0, sizeof(activities_events));
    memset(&network_events, 0, sizeof(network_events));
    memset(&join_in_progress, 0, sizeof(join_in_progress));

    DiscordCreateParamsSetDefault(&params);
    params.client_id = DISCORD_APP_ID;
    params.flags = DiscordCreateFlags_NoRequireDiscord;
    params.event_data = &app;
    params.activity_events = &activities_events;
    activities_events.on_activity_join_request = OnJoinRequest; // Someone asked to join
    activities_events.on_activity_invite = OnPartyInvite; // Invite received
    activities_events.on_activity_join = OnJoinParty; // Need to join party
    params.network_events = &network_events;
    network_events.on_message = OnNetworkMessage;

    map_name_decoded.language(GW::Constants::TextLanguage::English);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Callback,
        [this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            UNREFERENCED_PARAMETER(packet);
            zone_entered_time = time(nullptr); // Because you cant rely on instance time at this point.
            pending_activity_update = true;
            if (!discord_connected)
                pending_discord_connect = true; // Connect in Update() loop instead of StoC callback, just incase its blocking
            join_party_next_action = time(nullptr) + 2; // 2 seconds for other packets to be received e.g. players, guild info
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerAdd>(&PartyPlayerAdd_Callback,
        [this](GW::HookStatus* status, GW::Packet::StoC::PartyPlayerAdd* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
            if (player_agent && packet->player_id == player_agent->player_number) {
                pending_activity_update = true; // Update if this is me
                return;
            }
            GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
            if (p && packet->party_id == p->party_id)
                pending_activity_update = true; // Update if this is my party
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyUpdateSize>(&PartyUpdateSize_Callback,
        [this](GW::HookStatus* status, GW::Packet::StoC::PartyUpdateSize* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
            if (p && packet->player_id == p->players[0].login_number)
                pending_activity_update = true; // Update if this is my leader
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ErrorMessage>(&ErrorMessage_Callback,
        [this](GW::HookStatus* status, GW::Packet::StoC::ErrorMessage* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!join_in_progress.map_id)
                return;
            switch (packet->message_id) {
            case 0x35: // Cannot enter outpost (e.g. char has no access to outpost or GH)
                FailedJoin("Cannot enter outpost on this character");
                break;
            case 0x3C: // Already in active district (try to join party)
                JoinParty();
                break;
            }
        });
    if(GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
        zone_entered_time = time(nullptr) - (GW::Map::GetInstanceTime() / 1000);
    pending_activity_update = true;
    // Try to download and inject discord_game_sdk.dll for discord.
    dll_location = Resources::GetPath(L"discord_game_sdk.dll");
    // NOTE: We're using the one we know matches our API version, not checking for any other discord dll on the machine.
    Resources::EnsureFileExists(dll_location,
        "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/discord_game_sdk.dll",
        [&](bool success, const std::wstring& error) {
            if (!success || !LoadDll()) {
                Log::LogW(L"Failed to load discord_game_sdk.dll. To try again, please restart GWToolbox\n%s", error.c_str());
                return;
            }
            pending_discord_connect = pending_activity_update = discord_enabled;
        });
}
bool DiscordModule::IsInJoinablePartyMap() {
    if (!join_in_progress.map_id)
        return false;
    if (join_in_progress.ghkey[0]) {
        // If ghkey is set, we need to be in a guild hall
        GW::Guild* g = GW::GuildMgr::GetCurrentGH();
        if (!g) return false;
        for (size_t i = 0; i < 4; i++) {
            if(join_in_progress.ghkey[i] != g->key.k[i])
                return false;
        }
        return true;
    }
    return join_in_progress.map_id == static_cast<unsigned short>(GW::Map::GetMapID())
        && join_in_progress.district_id == GW::Map::GetDistrict()
        && join_in_progress.region_id == GW::Map::GetRegion()
        && join_in_progress.language_id == GW::Map::GetLanguage();
}
void DiscordModule::FailedJoin(const char* error_msg) {
    Log::Error("Join Party Failed: %s",error_msg);
    join_party_started = 0;
    join_party_next_action = 0;
    join_in_progress.map_id = 0;
}
void DiscordModule::JoinParty() {
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        return; // Loading
    if (!join_party_started) // Started to join party
        join_party_started = time(nullptr);
    if (join_party_started < time(nullptr) - 10) // Join timeout (try again please!)
        return FailedJoin("Failed to join party after 10 seconds");
    if (join_party_next_action > time(nullptr))
        return; // Delay between steps. Used to wait for packets to load etc
    if (!join_in_progress.map_id)
        return FailedJoin("No Party to join");
    if (!IsInJoinablePartyMap()) {
        Log::Log("Not in the same map; try to travel there.\n");
        if (!GW::Map::GetIsMapUnlocked((GW::Constants::MapID)join_in_progress.map_id))
            return FailedJoin("Cannot enter outpost on this character");
        if (join_in_progress.ghkey[0]) {
            Log::Log("Travelling to guild hall\n");
            GW::GuildMgr::TravelGH({ join_in_progress.ghkey[0],join_in_progress.ghkey[1],join_in_progress.ghkey[2],join_in_progress.ghkey[3] });
        }
        else {
            Log::Log("Travelling to outpost\n");
            GW::Map::Travel(static_cast<GW::Constants::MapID>(join_in_progress.map_id), join_in_progress.district_id, join_in_progress.region_id, join_in_progress.language_id);
        }
        // 5s timeout for any GW::Packet::StoC::ErrorMessage packets e.g. "Cannot enter outpost"
        join_party_next_action = time(nullptr) + 5;
        return;
    }
    // In map - try to join party!
    wchar_t buf[128] = { 0 };
    swprintf(buf, 128, L"invite %s", join_in_progress.player);
    GW::Chat::SendChat('/', buf);
    HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    SetForegroundWindow(hwnd);
    ShowWindow(hwnd, SW_RESTORE);
    Log::Log("Join process complete\n");
    join_party_started = 0;
    join_party_next_action = 0;
    join_in_progress.map_id = 0;
}
bool DiscordModule::Connect() {
    pending_discord_connect = false;
    if (!discord_enabled || !LoadDll())
        return false; // Failed to hook into discord_game_sdk.dll
    if (discord_connected)
        return true; // Already connected
#ifdef _DEBUG
/*
    HOW TO TEST WITH 2 DISCORD INSTANCES IN DEBUG MODE:
    1. Close DiscordCanary.exe, load first GW client and start toolbox
        Client 1 is now sending statuses to Discord.exe
    2. Open DiscordCanary.exe, load second GW client and start toolbox
        Client 2 is now sending statuses to DiscordCanary.exe
    NOTE: Disconnecting/reconnecting will mess this up so repeat process.
*/
    ConnectCanary(); // Sets env var to attach to canary if its open.
#endif
    SetLastError(0);
    int result = discordCreate(DISCORD_VERSION, &params, &app.core);
    if (result != DiscordResult_Ok) {
#ifdef _DEBUG
        Log::ErrorW(L"Failed to create discord connection; error code %d, last error %d", result, GetLastError());
#endif
        return false;
    }
    discord_connected = true;
    app.core->set_log_hook(app.core, EDiscordLogLevel::DiscordLogLevel_Error, &app, OnDiscordLog);
    app.core->set_log_hook(app.core, EDiscordLogLevel::DiscordLogLevel_Warn, &app, OnDiscordLog);
    app.core->set_log_hook(app.core, EDiscordLogLevel::DiscordLogLevel_Info, &app, OnDiscordLog);
    app.activities = app.core->get_activity_manager(app.core);
    app.network = app.core->get_network_manager(app.core);
    Log::Log("Discord connected\n");
    discord_connected_at = time(nullptr);
    return true;
}
// Sets DISCORD_INSTANCE_ID to match DiscordCanary.exe if its open. debug only.
void DiscordModule::ConnectCanary() {
    uint32_t discord_pid = GetProcId("Discord.exe");
    uint32_t discord_canary_pid = GetProcId("DiscordCanary.exe");
    uint32_t discord_env = 0;
    // Prefer canary over vanilla. To use vanilla, just close canary...
    if (discord_canary_pid && discord_pid) {
        FILETIME discord_canary_started;
        FILETIME discord_started;
        FILETIME dummy;
        HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, discord_canary_pid);
        GetProcessTimes(proc, &discord_canary_started, &dummy, &dummy, &dummy);
        if(proc) CloseHandle(proc);
        proc = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, discord_pid);
        GetProcessTimes(proc, &discord_started, &dummy, &dummy, &dummy);
        if (proc) CloseHandle(proc);
        discord_env = CompareFileTime(&discord_canary_started, &discord_started) ? 1u : 0u;
    }
    SetEnvironmentVariable("DISCORD_INSTANCE_ID", discord_env ? "1" : "0");
}
bool DiscordModule::LoadDll() {
    if (discordCreate)
        return true; // Already loaded.
    HINSTANCE hGetProcIDDLL = LoadLibraryW(dll_location.c_str());
    if (!hGetProcIDDLL) {
        Log::LogW(L"Failed to LoadLibraryW %s\n", dll_location.c_str());
        return false;
    }
    // resolve function address here
    discordCreate = (DiscordCreate_pt)((uintptr_t)(GetProcAddress(hGetProcIDDLL, "DiscordCreate")));
    if (!discordCreate) {
        UnloadDll();
        Log::LogW(L"Failed to find address for DiscordCreate\n");
        return false;
    }
    Log::Log("Discord DLL hooked!\n");
    return true;
}
bool DiscordModule::UnloadDll() {
    HINSTANCE hGetProcIDDLL = GetModuleHandleW(dll_location.c_str());
    return !hGetProcIDDLL || FreeLibrary(hGetProcIDDLL);
}
void DiscordModule::DrawSettingInternal() {
    bool edited = false;
    edited |= ImGui::Checkbox("Enable Discord integration", &discord_enabled);
    ImGui::ShowHelp("Allows GWToolbox to send in-game information to Discord");
    if (discord_enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, discord_connected ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        if (ImGui::Button(discord_connected ? "Connected" : "Disconnected", ImVec2(0, 0))) {
            if (discord_connected) {
                Disconnect();
            }
            else {
                edited |= true;
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(discord_connected ? "Click to disconnect" : "Click to connect");

        ImGui::Indent();
        edited |= ImGui::Checkbox("Hide in-game info when appearing offline", &hide_activity_when_offline);
        ImGui::ShowHelp("Setting your status to offline in friend list hides your info from Discord");

        edited |= ImGui::Checkbox("Display in-game location info", &show_location_info);
        ImGui::ShowHelp("e.g. 'Sifhalla, America English 1'");

        edited |= ImGui::Checkbox("Display character info", &show_character_info);
        ImGui::ShowHelp("i.e. Profession icon and character name");

        edited |= ImGui::Checkbox("Display party info", &show_party_info);
        ImGui::ShowHelp("Allows other players to join you when in an outpost,\nalso shows current party status e.g. (3 of 8)");
        ImGui::Unindent();
    }
    if(edited) // Picked up in the Update() loop
        pending_discord_connect = pending_activity_update = discord_enabled;
}
void DiscordModule::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(discord_enabled), discord_enabled);
    ini->SetBoolValue(Name(), VAR_NAME(hide_activity_when_offline), hide_activity_when_offline);
    ini->SetBoolValue(Name(), VAR_NAME(show_location_info), show_location_info);
    ini->SetBoolValue(Name(), VAR_NAME(show_character_info), show_character_info);
    ini->SetBoolValue(Name(), VAR_NAME(show_party_info), show_party_info);
}
void DiscordModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    discord_enabled = ini->GetBoolValue(Name(), VAR_NAME(discord_enabled), discord_enabled);
    hide_activity_when_offline = ini->GetBoolValue(Name(), VAR_NAME(hide_activity_when_offline), hide_activity_when_offline);
    show_location_info = ini->GetBoolValue(Name(), VAR_NAME(show_location_info), show_location_info);
    show_character_info = ini->GetBoolValue(Name(), VAR_NAME(show_character_info), show_character_info);
    show_party_info = ini->GetBoolValue(Name(), VAR_NAME(show_party_info), show_party_info);
}
void DiscordModule::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (!discord_enabled && discord_connected)
        Disconnect();
    if (pending_discord_connect)
        Connect();
    if (discord_connected && app.core->run_callbacks(app.core) != DiscordResult_Ok) {
        Log::Error("Discord disconnected");
        discord_connected = false;
        // Note that when not logged into discord (but Discord.exe running), DiscordCreate will still return an OK result but a subsequent transaction will disconnect the API.
        // Don't auto-reconnect here; if discord API is borked, you can retry to connect on map load or if user tried to click connect.
    }
    if (pending_activity_update) {
        UpdateActivity();
    }
    if(discord_connected)
        app.network->flush(app.network);
    if (join_in_progress.map_id) {
        JoinParty();
    }
}
void DiscordModule::UpdateActivity() {
    if (!pending_activity_update || !discord_connected || time(nullptr) - 4 < last_activity_update)
        return;
    if (!GW::Map::GetIsMapLoaded())
        return;
    GW::Guild* g = nullptr;
    GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
    GW::AreaInfo* m = GW::Map::GetCurrentMapInfo();
    GW::AgentLiving* a = GW::Agents::GetCharacter();
    GW::CharContext* c = GW::GetGameContext()->character;
    GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();
    if (!p || !m || !a || !c)
        return;
    bool is_guild_hall = m->type == GW::RegionType::GuildHall;
    if (is_guild_hall) {
        g = GW::GuildMgr::GetCurrentGH();
        if (!g) return; // Current gh not found - guild array not loaded yet
    }
    bool show_activity = !hide_activity_when_offline || GW::FriendListMgr::GetMyStatus() != GW::FriendStatus::Offline;
    if (!show_activity) {
        Disconnect(); // Disconnect from discord if we're set to offline
        return;
    }
    // Reset activity info. Easier to set everything over again rather than split them out into separate functions
    memset(activity.details, 0, 128);
    activity.timestamps.start = 0;
    activity.instance = 0;
    activity.assets.large_image[0] = 0;
    activity.assets.large_text[0] = 0;
    memset(activity.state, 0, 128);
    activity.assets.small_image[0] = 0;
    activity.assets.small_text[0] = 0;
    memset(activity.party.id, 0, 128);
    activity.party.size.current_size = 0;
    activity.party.size.max_size = 0;
    memset(activity.secrets.join, 0, 128);
    // Only update info if we're allowed

    if (show_activity) {
        unsigned short map_id = static_cast<unsigned short>(GW::Map::GetMapID());
        short map_region = static_cast<short>(GW::Map::GetRegion());
        short map_language = static_cast<short>(GW::Map::GetLanguage());
        short map_district = static_cast<short>(GW::Map::GetDistrict());
        char party_id[128];
        if (show_party_info) {
            // Party ID needs to be consistent across maps
            if (instance_type == GW::Constants::InstanceType::Explorable) {
                sprintf(party_id, "%d-%d", c->token1, map_id);
            }
            else if (is_guild_hall) {
                sprintf(party_id, "%d-%d-%d-%d-%d",
                    g->key.k[0], g->key.k[1], g->key.k[2], g->key.k[3],
                    p->party_id);
            }
            else {
                sprintf(party_id, "%d-%d-%d-%d-%d-%d", map_id, map_region, m->type, map_language, map_district, p->party_id);
            }
            SHA1 checksum;
            checksum.update(party_id);
            sprintf(activity.party.id, "%s", checksum.final().c_str());
            // Add a party secret if in an outpost. TODO: Joining feature?
            DiscordJoinableParty secret;
            secret.map_id = map_id;
            secret.region_id = map_region;
            secret.language_id = map_language;
            secret.district_id = map_district;
            secret.ghkey[0] = 0;
            swprintf(secret.player, 32, L"%s", GW::GetGameContext()->character->player_name);
            if (is_guild_hall) {
                for (size_t i = 0; i < 4; i++) {
                    secret.ghkey[i] = g->key.k[i];
                }
            }
            if (instance_type == GW::Constants::InstanceType::Outpost) {
                // NOTE: Guild halls off bounds until I can figure out how to get the GHKey for it.
                b64_enc(&secret, sizeof(secret), activity.secrets.join);
            }
            activity.party.size.current_size = static_cast<int32_t>(GW::PartyMgr::GetPartySize());
            activity.party.size.max_size = static_cast<int32_t>(m->max_party_size);
        }

        if (show_character_info) {
            sprintf(activity.assets.small_image, "profession_%d_512px", a->primary);
            sprintf(activity.assets.small_text, "%S (%s)", GW::GetGameContext()->character->player_name, profession_names[a->primary]);
        }

        if (show_location_info) {
            // Details
            map_name_decoded.reset(m->name_id);
            if (map_name_decoded.wstring().empty())
                return; // Map name not decoded yet.
            map_region = static_cast<short>(m->region);
            char region_info[32] = { 0 };
            if (instance_type == GW::Constants::InstanceType::Outpost && !is_guild_hall) {
                switch (static_cast<GW::Constants::MapRegion>(GW::Map::GetRegion()))
                {
                case GW::Constants::MapRegion::International:
                    sprintf(region_info, "International %d", GW::Map::GetDistrict());
                    break;
                case GW::Constants::MapRegion::Chinese:
                case GW::Constants::MapRegion::Korean:
                case GW::Constants::MapRegion::Japanese:
                    sprintf(region_info, "%s %d", region_abbreviations[GW::Map::GetRegion()], GW::Map::GetDistrict());
                    break;
                default:
                    sprintf(region_info, "%s %s %d", region_abbreviations[GW::Map::GetRegion()], map_languages[GW::Map::GetLanguage()],GW::Map::GetDistrict());
                    break;
                }
            }
            // State
            if (is_guild_hall) {
                sprintf(activity.state, "In Guild Hall");
                map_region = static_cast<short>(GW::Region::Region_BattleIslands);
            }
            else if (instance_type == GW::Constants::InstanceType::Outpost) {
                sprintf(activity.state, "%s", region_info);
            }
            else {
                sprintf(activity.state, "In Explorable");
            }
            if (is_guild_hall) {
                sprintf(activity.details, "%S [%S]", g->name, g->tag);
            }
            else {
                sprintf(activity.details, "%s", map_name_decoded.string().c_str());
            }
            sprintf(activity.assets.large_image, "%s", region_assets[map_region]);
            sprintf(activity.assets.large_text, "Region: %s", region_names[map_region]);
            activity.instance = instance_type == GW::Constants::InstanceType::Explorable;
            activity.timestamps.start = zone_entered_time;
        }
        else {
            sprintf(activity.state, "In Game");
        }
    }
     if (memcmp(&last_activity, &activity, sizeof(last_activity)) != 0) {
         // Only update if activity is new.
         last_activity_update = time(nullptr);
         if (show_activity) {
             Log::Log("Outgoing discord state = %s, %s\n", activity.details, activity.state);
             app.activities->update_activity(app.activities, &activity, &app, UpdateActivityCallback);
         }
         else {
             Log::Log("Clearing activity details\n");
             app.activities->clear_activity(app.activities, &app, UpdateActivityCallback);
         }
         last_activity = activity;
     }
     else {
         Log::Log("Tried to update discord activity, but nothing has changed.");
     }

    pending_activity_update = false;
}
