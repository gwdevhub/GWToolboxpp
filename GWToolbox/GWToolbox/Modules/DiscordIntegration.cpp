#include "stdafx.h"
// #include "base64.h"
// #include "sha1.hpp"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Friendslist.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/FriendListMgr.h>

#include <logger.h>
#include <GuiUtils.h>

#include <Windows/TravelWindow.h>

#include <Modules/Resources.h>
#include <Modules/DiscordIntegration.h>
#include <GWToolbox.h>

#define DISCORD_APP_ID 378706083788881961
#define DISCORD_DLL_REMOTE_URL L"https://github.com/HasKha/GWToolboxpp/releases/download/2.28_Release/discord_game_sdk.dll"

static const char* region_assets[] = {
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

static const char* region_names[] = {
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

static const char* profession_names[] = {
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

static const char* map_languages[] = {
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

static const char* region_abbreviations[] = {
    "America", // America
    "Asia Korea", // Asia Korean
    "Europe", // Europe
    "Asia Chinese", // Asia Chinese
    "Asia Japan" // Asia Japanese
};

static const char* language_abbreviations[] = {
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

DiscordIntegration::DiscordIntegration() {
    DiscordCreateParamsSetDefault(&params);

    memset(&network_events, 0, sizeof(network_events));
    memset(&activity_events, 0, sizeof(activity_events));
}

void DiscordIntegration::Initialize() {
    ToolboxModule::Initialize();
    // Initialise discord objects

#ifdef DEBUG_DISCORD_INTEGRATION
    SetEnvironmentVariable("DISCORD_INSTANCE_ID", "1");
#endif

    params.client_id = DISCORD_APP_ID;
    params.flags = DiscordCreateFlags_NoRequireDiscord;
    params.event_data = this;
    params.network_events = &network_events;
    params.activity_events = &activity_events;

    // Make sure discord dll is injected. dll_location used later.
    dll_location = Resources::GetPath(L"discord_game_sdk.dll");

    // Try to download and inject discord_game_sdk.dll for discord.
    // NOTE: We're using the one we know matches our API version, not checking for any other discord dll on the machine.
    Resources::Instance().EnsureFileExists(dll_location, DISCORD_DLL_REMOTE_URL,
    [this](bool success) {
        if (!success) {
            Log::Error("Failed to find and download the file 'discord_game_sdk.dll'. To try again, please restart GWToolbox");
            return;
        }

        hDiscordDll = LoadLibraryW(dll_location.c_str());
        if (!hDiscordDll) {
            Log::Error("'LoadLibraryW' failed on '%S' (%lu)", dll_location.c_str(), GetLastError());
            return;
        }

        DiscordCreate = (DiscordCreate_pt)GetProcAddress(hDiscordDll, "DiscordCreate");
        if (!DiscordCreate) {
            Log::Error("Failed to find address for DiscordCreate");
            FreeLibrary(hDiscordDll);
            hDiscordDll = nullptr;
            return;
        }

        Connect();
    });

    timestamp_start = time(nullptr) - (GW::Map::GetInstanceTime() / 1000);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceTimer>(&InstanceTimer_Callback,
    [this](GW::HookStatus* status, GW::Packet::StoC::InstanceTimer* packet) -> void {
        // This packet is to set the "instance timer" that is then increased
        // from an other packet ~2 times a second.
        timestamp_start = time(nullptr) - (packet->instance_time / 1000);
        update_activity_required = true;
    });

    player_status = GW::FriendListMgr::GetMyStatus();
}

void DiscordIntegration::Terminate() {
    Disconnect();

    if (hDiscordDll) {
        FreeLibrary(hDiscordDll);
        hDiscordDll = nullptr;
    }
}

void DiscordIntegration::Update(float delta) {
    if (!discord_core)
        return;

    if (!update_activity_required) {
        uint32_t party_size = GW::PartyMgr::GetPartySize();
        if (activity_party_size != party_size) {
            update_activity_required = true;
        }
    }

    if (!update_activity_required) {
        uint32_t current_player_status = GW::FriendListMgr::GetMyStatus();
        if (player_status != current_player_status) {
            player_status = current_player_status;
            update_activity_required = true;
        }
    }

    if (update_activity_required)
        UpdateActivity();
    discord_core->run_callbacks(discord_core);
}

void DiscordIntegration::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    hide_activity_when_offline = ini->GetBoolValue(Name(), VAR_NAME(hide_activity_when_offline), true);
}

void DiscordIntegration::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(hide_activity_when_offline), hide_activity_when_offline);
}

void DiscordIntegration::DrawSettingInternal() {
    if (IsConnectionOwner()) {
        if (ImGui::Button("Disconnect")) {
            Disconnect();
        }
        ImGui::ShowHelp("Try to use the current process for Discord integration");
    } else {
        if (ImGui::Button("Connect")) {
            Connect();
        }
        ImGui::ShowHelp("Release the ownership of the Discord integration");
    }

    if (ImGui::Checkbox("Hide in-game info when appearing offline", &hide_activity_when_offline)) {
        update_activity_required = true;
    }
    ImGui::ShowHelp("Setting your status to offline in friend list hides your info from Discord");
}

void DiscordIntegration::Connect() {
    owner_mutex = CreateMutexW(nullptr, FALSE, L"GWToolboxpp-Mutex-DiscordIntegration");
    if (!owner_mutex) {
        Log::Error("CreateMutexW failed (%lu)", GetLastError());
        return;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Log::Info("Discord connection already owned by an other GWToolboxpp");
        CloseHandle(owner_mutex);
        owner_mutex = nullptr;
        return;
    }

    IDiscordCore* core = nullptr;
    if (DiscordCreate(DISCORD_VERSION, &params, &core) != DiscordResult_Ok) {
        Log::Error("Failed to create discord connection");
        return;
    }

    network_mgr = core->get_network_manager(core);
    activity_mgr = core->get_activity_manager(core);

    core->set_log_hook(core, EDiscordLogLevel::DiscordLogLevel_Error, this, OnDiscordLog);
    core->set_log_hook(core, EDiscordLogLevel::DiscordLogLevel_Warn,  this, OnDiscordLog);
    core->set_log_hook(core, EDiscordLogLevel::DiscordLogLevel_Info,  this, OnDiscordLog);

    update_activity_required = true;

    // This is atomic and it's important :)
    // We rely on that in update.
    discord_core = core;
}

void DiscordIntegration::Disconnect() {
    if (discord_core) {
        discord_core->destroy(discord_core); // Do this for each discord connection
        discord_core = nullptr;
    }
    if (owner_mutex) {
        CloseHandle(owner_mutex);
        owner_mutex = nullptr;
    }
}

void DiscordIntegration::UpdateActivity() {
    uint32_t current_player_status = GW::FriendListMgr::GetMyStatus();
    if (hide_activity_when_offline && current_player_status == GW::FriendStatus_Offline) {
        activity_mgr->clear_activity(
            activity_mgr,
            this,
            UpdateActivityCallback);
        update_activity_required = false;
        return;
    }

    if (!GW::Map::GetIsMapLoaded())
        return;

    GW::Guild* guild = nullptr;
    GW::AreaInfo* area_info = GW::Map::GetCurrentMapInfo();
    GW::AgentLiving* player_agent = GW::Agents::GetPlayerAsAgentLiving();
    GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    if (!area_info || !player_agent)
        return;

    GW::Region region_asset_id = area_info->region;
    if (region_asset_id >= _countof(region_assets)) {
        region_asset_id = GW::Region_BattleIslands;
    }

    bool is_guild_hall = area_info->type == GW::RegionType::RegionType_GuildHall;
    if (is_guild_hall) {
        guild = GetCurrentGHGuild();
        if (!guild) return; // Current gh not found - guild array not loaded yet
        region_asset_id = GW::Region_BattleIslands;
    }

    int map_region = GW::Map::GetRegion();
    
    DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));

    if (instance_type == GW::Constants::InstanceType::Explorable) {
        // Leaving "activity.timestamps.start" will print the elapsed time.
        activity.timestamps.start = timestamp_start;
        activity.instance = true;
    } else {
        activity.instance = false;
    }

    // "assets.large_image" is the big image instead of the avatar.
    // We uses the image of the loading screen.
    GuiUtils::StrCopy(
        activity.assets.large_image,
        region_assets[region_asset_id],
        sizeof(activity.assets.large_image));
    
    // Tool tip of the large image.
    snprintf(activity.assets.large_text, sizeof(activity.assets.large_text),
        "Region: %s",
        region_names[region_asset_id]);
    
    // This is the small "round" image in the bottom right.
    snprintf(activity.assets.small_image, sizeof(activity.assets.small_image),
        "profession_%d_512px",
        player_agent->primary);
    
    // Tool tip of the small image.
    snprintf(activity.assets.small_text, sizeof(activity.assets.small_text),
        "%ls (%s)",
        GW::GameContext::instance()->character->player_name,
        profession_names[player_agent->primary]);

    // This variable is used to detect when the party change to update the activity.
    activity_party_size = GW::PartyMgr::GetPartySize();
    activity.party.size.current_size = activity_party_size;
    activity.party.size.max_size = area_info->max_party_size;

#if 0
    activity.party.id = ;
    activity.secrets.match = ;
    activity.secrets.join = ;
    activity.secrets.spectacte = ;
#endif

    char region_info[32] = {0};
    if ((instance_type == GW::Constants::InstanceType::Outpost) &&
        !is_guild_hall) {

        switch (static_cast<GW::Constants::MapRegion>(GW::Map::GetRegion()))
        {
        case GW::Constants::MapRegion::International:
            snprintf(region_info, sizeof(region_info),
                "International %d",
                GW::Map::GetDistrict());
            break;
        case GW::Constants::MapRegion::Chinese:
        case GW::Constants::MapRegion::Korean:
        case GW::Constants::MapRegion::Japanese:
            snprintf(region_info, sizeof(region_info),
                "%s %d",
                region_abbreviations[GW::Map::GetRegion()],
                GW::Map::GetDistrict());
            break;
        default:
            snprintf(region_info, sizeof(region_info),
                "%s %s %d",
                region_abbreviations[GW::Map::GetRegion()],
                map_languages[GW::Map::GetLanguage()],
                GW::Map::GetDistrict());
            break;
        }
    }

    // the player's current party status
    if (is_guild_hall)
        snprintf(activity.state, sizeof(activity.state), "In Guild Hall");
    else if (instance_type == GW::Constants::InstanceType::Outpost)
        snprintf(activity.state, sizeof(activity.state), region_info);
    else
        snprintf(activity.state, sizeof(activity.state), "In Explorable");
    
    // what the player is currently doing
    if (is_guild_hall) {
        snprintf(activity.details, sizeof(activity.details), "%ls [%ls]", guild->name, guild->tag);
    } else {
        GW::Constants::MapID map_id = GW::Map::GetMapID();
        const char* map_name = "Unknown map";
        if (static_cast<int>(map_id) > 0 &&
            static_cast<int>(map_id) < _countof(GW::Constants::NAME_FROM_ID)) {

            map_name = GW::Constants::NAME_FROM_ID[static_cast<int>(map_id)];
        }

        snprintf(activity.details, sizeof(activity.details), "%s", map_name);
    }

    update_activity_required = false;
    activity_mgr->update_activity(
        activity_mgr,
        &activity,
        this,
        UpdateActivityCallback);
}

// Used to figure out if this char has access to the outpost without waiting for server error
bool DiscordIntegration::IsMapUnlocked(uint32_t map_id) {
    GW::Array<uint32_t> unlocked_map = GW::GameContext::instance()->world->unlocked_map;
    uint32_t real_index = map_id / 32;
    if (real_index >= unlocked_map.size())
        return false;
    uint32_t shift = map_id % 32;
    uint32_t flag = 1 << shift;
    return (unlocked_map[real_index] & flag) != 0;
}

// Returns guild struct of current location. Returns null on fail or non-guild map.
GW::Guild* DiscordIntegration::GetCurrentGHGuild() {
    GW::AreaInfo* area_info = GW::Map::GetCurrentMapInfo();
    if (!area_info || area_info->type != GW::RegionType::RegionType_GuildHall) return nullptr;
    GW::Array<GW::Guild*> guilds = GW::GuildMgr::GetGuildArray();
    if (!guilds.valid()) return nullptr;
    for (size_t i = 0; i < guilds.size(); i++) {
        if (!guilds[i]) continue;
        return guilds[i];
    }
    return nullptr;
}

void DiscordIntegration::UpdateActivityCallback(void* event_data, EDiscordResult result) {
    assert(event_data != nullptr);
    DiscordIntegration* discord_module = reinterpret_cast<DiscordIntegration*>(event_data);
    if (result != DiscordResult_Ok) {
        Log::Log("Activity update failed with error %d\n", result);
    }
}

void DiscordIntegration::OnDiscordLog(void* event_data, enum EDiscordLogLevel level, const char* message) {
    assert(event_data != nullptr);
    DiscordIntegration* discord_module = reinterpret_cast<DiscordIntegration*>(event_data);
    Log::Log("Discord Log Level %d: '%s'\n", level, message);
}
