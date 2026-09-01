/*
感谢 KAOS 提供了 https://github.com/GregLando113/gw-discord 的原始版本

如何在调试模式下用 2 个 Discord 实例测试：
1. 关闭 DiscordCanary.exe，加载第一个 GW 客户端并启动工具箱
    客户端 1 现在正在向 Discord.exe 发送状态
2. 打开 DiscordCanary.exe，加载第二个 GW 客户端并启动工具箱
    客户端 2 现在正在向 DiscordCanary.exe 发送状态

注意：断开/重新连接会打乱这个设置，所以请重复此过程。
*/

#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Guild.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>

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
#include <Utils/ToolboxUtils.h>

#ifndef DISCORD_API
#define DISCORD_API
#endif

namespace {


    struct Application {
        IDiscordCore* core;
        IDiscordUserManager* users;
        IDiscordAchievementManager* achievements;
        IDiscordActivityManager* activities;
        IDiscordRelationshipManager* relationships;
        IDiscordApplicationManager* application;
        IDiscordLobbyManager* lobbies;
        IDiscordNetworkManager* network;
        DiscordUserId user_id;
    };

    // Encoded/decoded when joining another player's game.
    struct DiscordJoinableParty {
        unsigned short map_id = 0;
        short district_id{};
        short region_id{};
        short language_id{};
        uint32_t ghkey[4]{};
        wchar_t player[32]{};
    };

    // Used to record current GH info
    struct CurrentGuildHall {
        wchar_t tag[8];
        wchar_t name[32];
        uint32_t ghkey[4];
    };

    constexpr auto DISCORD_APP_ID = 378706083788881961;

    using DiscordCreate_pt = EDiscordResult(DISCORD_API*)(DiscordVersion version, DiscordCreateParams* params, IDiscordCore** result);
    using DiscordVersion_pt = int(__cdecl*)(int unk, int* out);

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
        "克瑞塔",
        "玛古玛丛林",
        "阿斯卡隆",
        "席瓦山脉",
        "英雄殿堂",
        "水晶沙漠",
        "哀伤之熔炉",
        "前席瓦山脉",
        "凯宁",
        "库兹柯",
        "卢克森",
        "星岬岛",
        "柯尔纳",
        "瓦比",
        "荒芜之地",
        "伊斯坦",
        "痛苦领域",
        "锈蚀海岸",
        "泰瑞亚深渊",
        "远北席瓦山脉",
        "夏尔家园",
        "战斗群岛",
        "贾海之战",
        "北逃之路",
        "天狗协议",
        "白斗篷崛起",
        "Swat",
        "开发区域"
    };

    const char* map_languages[] = {
        "英语",
        "未知",
        "法语",
        "德语",
        "意大利语",
        "西班牙语",
        "未知",
        "未知",
        "未知",
        "波兰语",
        "俄语"
    };
    const char* region_abbreviations[] = {
        "美洲",      // America
        "亚洲韩国",   // Asia Korean
        "欧洲",       // Europe
        "亚洲中国",   // Asia Chinese
        "亚洲日本"    // Asia Japanese
    };
    const char* language_abbreviations[] = {
        "E", // 英语
        "",
        "F", // 法语
        "G", // 德语
        "I", // 意大利语
        "S", // 西班牙语
        "", "", "",
        "P", // 波兰语
        "R"  // 俄语
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

    DiscordCreateParams params{};

    IDiscordUserEvents users_events{};
    IDiscordActivityEvents activities_events{};
    IDiscordRelationshipEvents relationships_events{};
    IDiscordNetworkEvents network_events{};
    IDiscordCoreEvents core_events{};

    DiscordModule::Settings settings;

    bool discord_connected = false;
    time_t zone_entered_time = 0;
    bool pending_activity_update = false;
    bool pending_discord_connect = true;
    std::wstring dll_location;
    time_t last_activity_update = 0;

    Application app{};
    DiscordActivity activity{};
    DiscordActivity last_activity{};

    void DISCORD_API UpdateActivityCallback(void*, const EDiscordResult result)
    {
        Log::Log(result == DiscordResult_Ok ? "活动已成功更新。\n" : "活动更新失败！\n");
    }

    void DISCORD_API OnJoinRequestReplyCallback(void*, const EDiscordResult result)
    {
        Log::Log(result == DiscordResult_Ok ? "加入请求回复已成功发送。\n" : "加入请求回复发送失败！\n");
    }

    void DISCORD_API OnSendInviteCallback(void*, const EDiscordResult result)
    {
        Log::Log(result == DiscordResult_Ok ? "邀请已成功发送。\n" : "邀请发送失败！\n");
    }

    void DISCORD_API OnNetworkMessage(void*, DiscordNetworkPeerId, DiscordNetworkChannelId, uint8_t*, const uint32_t)
    {
        Log::Log("Discord：网络消息\n");
    }

    void DISCORD_API OnJoinParty([[maybe_unused]] void* event_data, const char* secret)
    {
        Log::Log("Discord：on_activity_join %s\n", secret);
        memset(&join_in_progress, 0, sizeof(join_in_progress));
        b64_dec(secret, &join_in_progress);
    }

    // NOTE: In our game, anyone can join anyone else's party - work around for "ask to join" by auto-accepting.
    void DISCORD_API OnJoinRequest([[maybe_unused]] void* data, DiscordUser* user)
    {
        Log::Log("收到来自 %s 的加入请求；自动接受\n", user->username);
        app.activities->send_request_reply(app.activities, user->id, DiscordActivityJoinRequestReply_Yes, &app, OnJoinRequestReplyCallback);
    }

    void DISCORD_API OnPartyInvite([[maybe_unused]] void* event_data, EDiscordActivityActionType, DiscordUser* user, DiscordActivity*)
    {
        Log::Log("收到来自 %s 的队伍邀请\n", user->username);
    }

    void DISCORD_API OnDiscordLog([[maybe_unused]] void* data, const EDiscordLogLevel level, const char* message)
    {
        Log::Log("Discord 日志级别 %d：%s\n", level, message);
    }

    DWORD GetProcId(const char* ProcName)
    {
        PROCESSENTRY32 pe32;
        uint32_t pid = 0;
        const uint32_t len = strlen(ProcName);
        pe32.dwSize = sizeof(PROCESSENTRY32);
        const HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (strcmp(pe32.szExeFile, ProcName) == 0 && strlen(pe32.szExeFile) == len) {
                    pid = pe32.th32ProcessID;
                }
            } while (!pid && Process32Next(hSnapshot, &pe32));
        }

        if (hSnapshot != INVALID_HANDLE_VALUE) {
            CloseHandle(hSnapshot);
        }

        return pid;
    }

    bool IsInJoinablePartyMap()
    {
        if (!join_in_progress.map_id) {
            return false;
        }
        if (join_in_progress.ghkey[0]) {
            // If ghkey is set, we need to be in a guild hall
            const GW::Guild* g = GW::GuildMgr::GetCurrentGH();
            if (!g) {
                return false;
            }
            for (size_t i = 0; i < 4; i++) {
                if (join_in_progress.ghkey[i] != g->key.k[i]) {
                    return false;
                }
            }
            return true;
        }
        return join_in_progress.map_id == static_cast<unsigned short>(GW::Map::GetMapID())
            && join_in_progress.district_id == static_cast<unsigned short>(GW::Map::GetDistrict())
            && join_in_progress.region_id == static_cast<unsigned short>(GW::Map::GetRegion())
            && join_in_progress.language_id == static_cast<unsigned short>(GW::Map::GetLanguage());
    }

    void FailedJoin(const char* error_msg)
    {
        Log::Error("加入队伍失败：%s", error_msg);
        join_party_started = 0;
        join_party_next_action = 0;
        join_in_progress.map_id = 0;
    }

    void JoinParty()
    {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
            return; // Loading
        }
        if (!join_party_started) // Started to join party
        {
            join_party_started = time(nullptr);
        }
        if (join_party_started < time(nullptr) - 10) // Join timeout (try again please!)
        {
            return FailedJoin("10 秒后加入队伍失败");
        }
        if (join_party_next_action > time(nullptr)) {
            return; // Delay between steps. Used to wait for packets to load etc
        }
        if (!join_in_progress.map_id) {
            return FailedJoin("没有要加入的队伍");
        }
        if (!IsInJoinablePartyMap()) {
            Log::Log("不在同一地图；尝试传送过去。\n");
            if (!GW::Map::GetIsMapUnlocked(static_cast<GW::Constants::MapID>(join_in_progress.map_id))) {
                return FailedJoin("此角色无法进入该前哨站");
            }
            if (join_in_progress.ghkey[0]) {
                Log::Log("传送至公会大厅\n");
                GW::GuildMgr::TravelGH({ join_in_progress.ghkey[0], join_in_progress.ghkey[1], join_in_progress.ghkey[2], join_in_progress.ghkey[3] });
            }
            else {
                Log::Log("传送至前哨站\n");
                GW::Map::Travel(
                    static_cast<GW::Constants::MapID>(join_in_progress.map_id),
                    static_cast<GW::Constants::ServerRegion>(join_in_progress.region_id),
                    join_in_progress.district_id,
                    static_cast<GW::Constants::Language>(join_in_progress.language_id));
            }
            join_party_next_action = time(nullptr) + 5;
            return;
        }
        // In map - try to join party!
        wchar_t buf[128] = { 0 };
        swprintf(buf, 128, L"invite %s", join_in_progress.player);
        GW::Chat::SendChat('/', buf);
        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        SetForegroundWindow(hwnd);
        ShowWindow(hwnd, SW_RESTORE);
        Log::Log("加入过程完成\n");
        join_party_started = 0;
        join_party_next_action = 0;
        join_in_progress.map_id = 0;
    }

    GW::HookEntry PostUIMessage_HookEntry;

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
            case GW::UI::UIMessage::kMapLoaded: {
                zone_entered_time = time(nullptr); // Because you cant rely on instance time at this point.
                pending_activity_update = true;
                if (!discord_connected) {
                    pending_discord_connect = true; // Connect in Update() loop instead of callback, just incase its blocking
                }
                join_party_next_action = time(nullptr) + 2; // 2 seconds for other packets to be received e.g. players, guild info
            } break;
            case GW::UI::UIMessage::kErrorMessage: {
                const auto packet = (GW::UI::UIPacket::kErrorMessage*)wparam;
                Log::Warning("TODO: GW::UI::UIPacket::kErrorMessage");
                switch (packet->error_id) {
                    case 0x35: // Cannot enter outpost (e.g. char has no access to outpost or GH)
                        FailedJoin("此角色无法进入该前哨站");
                        break;
                    case 0x3C: // Already in active district (try to join party)
                        JoinParty();
                        break;
                }
            } break;
            case GW::UI::UIMessage::kPartyAddPlayer:
            case GW::UI::UIMessage::kPartyRemovePlayer:
            case GW::UI::UIMessage::kPartyAddHenchman:
            case GW::UI::UIMessage::kPartyRemoveHenchman:
            case GW::UI::UIMessage::kPartyAddHero:
            case GW::UI::UIMessage::kPartyRemoveHero: {
                pending_activity_update = true;
            } break;
        }
    }

    bool UnloadDll()
    {
        const HINSTANCE hGetProcIDDLL = GetModuleHandleW(dll_location.c_str());
        return !hGetProcIDDLL || FreeLibrary(hGetProcIDDLL);
    }

    bool LoadDll()
    {
        if (discordCreate) {
            return true; // Already loaded.
        }
        const HINSTANCE hGetProcIDDLL = LoadLibraryW(dll_location.c_str());
        if (!hGetProcIDDLL) {
            Log::LogW(L"LoadLibraryW 失败 %s\n", dll_location.c_str());
            return false;
        }

        DiscordVersion_pt discordVersion = (DiscordVersion_pt)GetProcAddress(hGetProcIDDLL, "DiscordVersion");
        if (!discordVersion) {
            ASSERT(UnloadDll());
            Log::LogW(L"找不到 DiscordVersion 的地址\n");
            return false;
        }
        int out[3] = { 0 };
        const auto res = discordVersion(0, out);
        if (res || *out != DISCORD_VERSION) {
            ASSERT(UnloadDll());
            Log::LogW(L"Discord 版本不匹配：%d %d.%d.%d\n", DISCORD_VERSION, out[0],out[1],out[2]);
            return false;
        }

        discordCreate = (DiscordCreate_pt)(uintptr_t)GetProcAddress(hGetProcIDDLL, "DiscordCreate");
        if (!discordCreate) {
            ASSERT(UnloadDll());
            Log::LogW(L"找不到 DiscordCreate 的地址\n");
            return false;
        }
        Log::Log("Discord DLL 已挂钩！\n");
        return true;
    }
    // Sets DISCORD_INSTANCE_ID to match DiscordCanary.exe if its open. debug only.
    void ConnectCanary()
    {
        const uint32_t discord_pid = GetProcId("Discord.exe");
        const uint32_t discord_canary_pid = GetProcId("DiscordCanary.exe");
        uint32_t discord_env = 0;
        // Prefer canary over vanilla. To use vanilla, just close canary...
        if (discord_canary_pid && discord_pid) {
            FILETIME discord_canary_started;
            FILETIME discord_started;
            FILETIME dummy;
            HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, discord_canary_pid);
            GetProcessTimes(proc, &discord_canary_started, &dummy, &dummy, &dummy);
            if (proc) {
                CloseHandle(proc);
            }
            proc = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, discord_pid);
            GetProcessTimes(proc, &discord_started, &dummy, &dummy, &dummy);
            if (proc) {
                CloseHandle(proc);
            }
            discord_env = CompareFileTime(&discord_canary_started, &discord_started) ? 1u : 0u;
        }
        SetEnvironmentVariable("DISCORD_INSTANCE_ID", discord_env ? "1" : "0");
    }

    bool Connect()
    {
        pending_discord_connect = false;
        if (!settings.discord_enabled || !LoadDll()) {
            return false; // Failed to hook into discord_game_sdk.dll
        }
        if (discord_connected) {
            return true; // Already connected
        }
#ifdef _DEBUG
        ConnectCanary(); // Sets env var to attach to canary if its open.
#endif
        SetLastError(0);
        const auto result = discordCreate(DISCORD_VERSION, &params, &app.core);
        if (result != DiscordResult_Ok) {
#ifdef _DEBUG
            Log::ErrorW(L"创建 Discord 连接失败；错误代码 %d，最后错误 %d", result, GetLastError());
#endif
            return false;
        }
        discord_connected = true;
        app.core->set_log_hook(app.core, DiscordLogLevel_Error, &app, OnDiscordLog);
        app.core->set_log_hook(app.core, DiscordLogLevel_Warn, &app, OnDiscordLog);
        app.core->set_log_hook(app.core, DiscordLogLevel_Info, &app, OnDiscordLog);
        app.activities = app.core->get_activity_manager(app.core);
        app.network = app.core->get_network_manager(app.core);
        Log::Log("Discord 已连接\n");
        discord_connected_at = time(nullptr);
        return true;
    }

    void Disconnect()
    {
        if (discord_connected) {
            app.core->destroy(app.core); // Do this for each discord connection
        }
        discord_connected = pending_activity_update = pending_discord_connect = false;
    }

    void InviteUser(const DiscordUser* user)
    {
        char invite_str[128];
        sprintf(invite_str, "%s, %s", activity.details, activity.state);
        app.activities->send_invite(app.activities, user->id, DiscordActivityActionType_Join, invite_str, &app, OnSendInviteCallback);
    }

    void UpdateActivity()
    {
        if (!pending_activity_update || !discord_connected || time(nullptr) - 4 < last_activity_update) {
            return;
        }
        if (!GW::Map::GetIsMapLoaded()) {
            return;
        }
        GW::Guild* g = nullptr;
        const GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
        const GW::AreaInfo* m = GW::Map::GetCurrentMapInfo();
        const GW::AgentLiving* a = GW::Agents::GetControlledCharacter();
        const GW::CharContext* c = GW::GetGameContext()->character;
        const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();
        if (!p || !m || !a || !c) {
            return;
        }
        const bool is_guild_hall = m->type == GW::RegionType::GuildHall;
        if (is_guild_hall) {
            g = GW::GuildMgr::GetCurrentGH();
            if (!g) {
                return; // Current gh not found - guild array not loaded yet
            }
        }
        const bool show_activity = !settings.hide_activity_when_offline || GW::FriendListMgr::GetMyStatus() != GW::FriendStatus::Offline;
        if (!show_activity) {
            Disconnect(); // Disconnect from discord if we're set to offline
            return;
        }
        // Reset activity info. Easier to set everything over again rather than split them out into separate functions
        memset(activity.details, 0, 128);
        activity.timestamps.start = 0;
        activity.instance = false;
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
            const auto map_id = static_cast<unsigned short>(GW::Map::GetMapID());
            const auto server_region = static_cast<short>(GW::Map::GetRegion());
            const auto map_language = static_cast<short>(GW::Map::GetLanguage());
            const auto map_district = static_cast<short>(GW::Map::GetDistrict());
            char party_id[128];
            if (settings.show_party_info) {
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
                    sprintf(party_id, "%d-%d-%d-%d-%d-%d", map_id, server_region, m->type, map_language, map_district, p->party_id);
                }
                SHA1 checksum;
                checksum.update(party_id);
                sprintf(activity.party.id, "%s", checksum.final().c_str());
                // Add a party secret if in an outpost. TODO: Joining feature?
                DiscordJoinableParty secret;
                secret.map_id = map_id;
                secret.region_id = server_region;
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

            if (settings.show_character_info) {
                sprintf(activity.assets.small_image, "profession_%d_512px", a->primary);
                sprintf(activity.assets.small_text, "%S（%s）", GW::GetGameContext()->character->player_name, ToolboxUtils::GetProfessionName(static_cast<GW::Constants::Profession>(a->primary))->string().c_str());
            }

            if (settings.show_location_info) {
                // Details
                map_name_decoded.reset(m->name_id);
                if (map_name_decoded.wstring().empty()) {
                    return; // Map name not decoded yet.
                }
                auto map_region = static_cast<short>(m->region);
                char region_info[32] = { 0 };
                if (instance_type == GW::Constants::InstanceType::Outpost && !is_guild_hall) {
                    switch (static_cast<GW::Constants::ServerRegion>(server_region)) {
                    case GW::Constants::ServerRegion::International:
                        sprintf(region_info, "国际 %d", map_district);
                        break;
                    case GW::Constants::ServerRegion::China:
                    case GW::Constants::ServerRegion::Korea:
                    case GW::Constants::ServerRegion::Japan:
                        sprintf(region_info, "%s %d", region_abbreviations[server_region], map_district);
                        break;
                    default:
                        sprintf(region_info, "%s %s %d", region_abbreviations[server_region], map_languages[map_language], map_district);
                        break;
                    }
                }
                // State
                if (is_guild_hall) {
                    sprintf(activity.state, "在公会大厅中");
                    map_region = static_cast<short>(GW::Region::Region_BattleIslands);
                }
                else if (instance_type == GW::Constants::InstanceType::Outpost) {
                    sprintf(activity.state, "%s", region_info);
                }
                else {
                    sprintf(activity.state, "在可探索区域中");
                }
                if (is_guild_hall) {
                    sprintf(activity.details, "%S [%S]", g->name, g->tag);
                }
                else {
                    sprintf(activity.details, "%s", map_name_decoded.string().c_str());
                }
                sprintf(activity.assets.large_image, "%s", region_assets[map_region]);
                sprintf(activity.assets.large_text, "区域：%s", region_names[map_region]);
                activity.instance = instance_type == GW::Constants::InstanceType::Explorable;
                activity.timestamps.start = zone_entered_time;
            }
            else {
                sprintf(activity.state, "游戏中");
            }
        }
        if (memcmp(&last_activity, &activity, sizeof(last_activity)) != 0) {
            last_activity_update = time(nullptr);
            if (show_activity) {
                Log::Log("传出 Discord 状态 = %s, %s\n", activity.details, activity.state);
                app.activities->update_activity(app.activities, &activity, &app, UpdateActivityCallback);
            }
            else {
                Log::Log("清除活动详情\n");
                app.activities->clear_activity(app.activities, &app, UpdateActivityCallback);
            }
            last_activity = activity;
        }
        else {
            Log::Log("尝试更新 Discord 活动，但没有任何变化。");
        }

        pending_activity_update = false;
    }
}

void DiscordModule::Terminate()
{
    ToolboxModule::Terminate();

    GW::UI::RemoveUIMessageCallback(&PostUIMessage_HookEntry);
    Disconnect();
    ASSERT(UnloadDll());
}

void DiscordModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);

    strcpy(activity.name, "Guild Wars");
    activity.application_id = DISCORD_APP_ID;
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
    activities_events.on_activity_invite = OnPartyInvite;       // Invite received
    activities_events.on_activity_join = OnJoinParty;           // Need to join party
    params.network_events = &network_events;
    network_events.on_message = OnNetworkMessage;

    map_name_decoded.language(GW::Constants::Language::English);

    const GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kMapLoaded,           GW::UI::UIMessage::kPartyAddPlayer, GW::UI::UIMessage::kPartyRemovePlayer, GW::UI::UIMessage::kPartyAddHenchman,
                                             GW::UI::UIMessage::kPartyRemoveHenchman, GW::UI::UIMessage::kPartyAddHero,   GW::UI::UIMessage::kPartyRemoveHero};


    for (auto message_id : ui_messages) {
        RegisterUIMessageCallback(&PostUIMessage_HookEntry, message_id, OnPostUIMessage, 0x4000);
    }

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        zone_entered_time = time(nullptr) - GW::Map::GetInstanceTime() / 1000;
    }
    pending_activity_update = true;
    // Try to download and inject discord_game_sdk.dll for discord.
    dll_location = Resources::GetPath(L"discord_game_sdk.dll");
    // NOTE: We're using the one we know matches our API version, not checking for any other discord dll on the machine.
    Resources::EnsureFileExists(dll_location,
                                "https://raw.githubusercontent.com/gwdevhub/GWToolboxpp/master/resources/discord_game_sdk.dll",
                                [&](const bool success, const std::wstring& error) {
                                    if (!success || !LoadDll()) {
                                        Log::LogW(L"加载 discord_game_sdk.dll 失败。请重启 GWToolbox 重试\n%s", error.c_str());
                                        return;
                                    }
                                    pending_discord_connect = pending_activity_update = settings.discord_enabled;
                                });


}

void DiscordModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void DiscordModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void DiscordModule::DrawSettingsInternal()
{
    bool edited = false;
    edited |= ImGui::CheckboxWithHelp("启用 Discord 集成", &settings.discord_enabled, "允许 GWToolbox 向 Discord 发送游戏内信息");
    if (settings.discord_enabled) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, discord_connected ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        if (ImGui::Button(discord_connected ? "已连接" : "已断开", ImVec2(0, 0))) {
            if (discord_connected) {
                Disconnect();
            }
            else {
                edited |= true;
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(discord_connected ? "点击断开连接" : "点击连接");
        }

        ImGui::Indent();
        edited |= ImGui::CheckboxWithHelp("离线时隐藏游戏内信息", &settings.hide_activity_when_offline, "在好友列表中设置离线状态将从 Discord 隐藏您的信息");

        edited |= ImGui::CheckboxWithHelp("显示游戏内位置信息", &settings.show_location_info, "例如 'Sifhalla, 美洲 英语 1'");

        edited |= ImGui::CheckboxWithHelp("显示角色信息", &settings.show_character_info, "即职业图标和角色名称");

        edited |= ImGui::CheckboxWithHelp("显示队伍信息", &settings.show_party_info, "允许其他玩家在前哨站时加入您，\n同时显示当前队伍状态，例如（3/8）");
        ImGui::Unindent();
    }
    if (edited) // Picked up in the Update() loop
    {
        pending_discord_connect = pending_activity_update = settings.discord_enabled;
    }
}

void DiscordModule::Update(const float)
{
    if (!settings.discord_enabled && discord_connected) {
        Disconnect();
    }
    if (pending_discord_connect) {
        Connect();
    }
    if (discord_connected && app.core->run_callbacks(app.core) != DiscordResult_Ok) {
        Log::Error("Discord 已断开连接");
        discord_connected = false;
        // Note that when not logged into discord (but Discord.exe running), DiscordCreate will still return an OK result but a subsequent transaction will disconnect the API.
        // Don't auto-reconnect here; if discord API is borked, you can retry to connect on map load or if user tried to click connect.
    }
    if (pending_activity_update) {
        UpdateActivity();
    }
    if (discord_connected) {
        app.network->flush(app.network);
    }
    if (join_in_progress.map_id) {
        JoinParty();
    }
}
