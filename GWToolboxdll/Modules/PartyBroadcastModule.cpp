#include "stdafx.h"

#include <GWCA/Context/PartyContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>

#include <Modules/PartyBroadcastModule.h>
#include <Modules/Resources.h>
#include <Modules/Updater.h>

#include <Utils/TextUtils.h>

#include <Utils/ThreadedWebSocket.h>
#include <glaze/glaze.hpp>
#include <optional>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Timer.h>
#include <Utils/ToolboxUtils.h>
#include "ToolboxSettings.h"

namespace party_broadcast_api {
    // 紧凑字段名 + std::optional 省略匹配现有的有线格式。
    struct PartyEntry {
        uint32_t i = 0;             // 队伍ID
        std::optional<uint32_t> r;  // 移除标志（当party_size == 0时）
        std::optional<uint32_t> t;  // 搜索类型
        std::optional<uint32_t> p;  // 主职业
        std::optional<std::string> s; // 发送者
        std::optional<uint32_t> ps; // 队伍大小（>1）
        std::optional<uint32_t> hc; // 英雄数量
        std::optional<uint32_t> hm; // 困难模式
        std::optional<uint32_t> dl; // 地区语言
        std::optional<uint32_t> sc; // 副职业
        std::optional<uint32_t> dn; // 地区编号
        std::optional<std::string> ms; // 消息
        std::optional<uint32_t> l;  // 等级（当 != 20 时）
    };

    struct PartiesPayload {
        std::string type;
        uint32_t map_id = 0;
        int32_t district_region = 0;
        std::vector<PartyEntry> parties;
    };
}

namespace {
    clock_t last_update_timestamp = 0;
    clock_t need_to_send_party_searches = 0;

    bool terminating = false;

    ThreadedWebSocket party_ws;

    struct MapDistrictInfo {
        GW::Constants::MapID map_id = (GW::Constants::MapID)0;
        GW::Constants::ServerRegion region = (GW::Constants::ServerRegion)0;
        GW::Constants::Language language = (GW::Constants::Language)0;
        int district_number = 0;
    };

    MapDistrictInfo GetDistrictInfo()
    {
        return {GW::Map::GetMapID(), GW::Map::GetRegion(), GW::Map::GetLanguage(), GW::Map::GetDistrict()};
    }

    struct PartySearchAdvertisement {
        uint32_t party_id = 0;
        uint8_t party_size = 0;
        uint8_t hero_count = 0;
        uint8_t search_type = 0; // 0=狩猎, 1=任务, 2=任务, 3=交易, 4=公会
        uint8_t hardmode = 0;
        uint16_t district_number = 0;
        uint8_t language = 0;
        uint8_t primary = 0;
        uint8_t secondary = 0;
        uint8_t level = 0;
        std::string message;
        std::string sender;
    };
    std::map<uint32_t, PartySearchAdvertisement> server_parties;
    MapDistrictInfo last_sent_district_info;

    bool send_payload(const std::string& payload);

    party_broadcast_api::PartyEntry ToJson(const PartySearchAdvertisement& p)
    {
        party_broadcast_api::PartyEntry j{.i = p.party_id};
        if (!p.party_size) {
            j.r = 1u; // "移除"
            return j;
        }
        j.t = p.search_type;
        j.p = p.primary;
        j.s = p.sender;
        if (p.party_size > 1) j.ps = p.party_size;
        if (p.hero_count) j.hc = p.hero_count;
        if (p.hardmode) j.hm = p.hardmode;
        if (p.language) j.dl = p.language;
        if (p.secondary) j.sc = p.secondary;
        if (p.district_number) j.dn = p.district_number;
        if (!p.message.empty()) j.ms = p.message;
        if (p.level != 20) j.l = p.level;
        return j;
    }

    bool get_api_key(std::string& out)
    {
        GWToolboxRelease current_release;
        if (!Updater::GetCurrentVersionInfo(&current_release)) {
            Log::Error("获取当前工具箱版本信息失败");
            return false;
        }
        out = std::format("gwtoolbox-{}-{}", current_release.version, current_release.size);
        return true;
    }

    void on_websocket_closed()
    {
        last_update_timestamp = 0;
        server_parties.clear();
        need_to_send_party_searches = TIMER_INIT();
        memset(&last_sent_district_info, 0, sizeof(last_sent_district_info));
        Log::Log("WebSocket 已断开");
    }

    void InitWebSocket()
    {
        party_ws.SetUrl("wss://party.gwtoolbox.com");
        party_ws.SetReconnectCost(30'000, 60'000);

        party_ws.SetHeadersFactory([] {
            std::string api_key;
            get_api_key(api_key);
            const auto acct_uuid = GW::AccountMgr::GetAccountUuid();
            const auto uuid = TextUtils::GuidToString(&acct_uuid);
            easywsclient::HeaderKeyValuePair headers = {{"User-Agent", "GWToolboxpp"}, {"X-Api-Key", api_key}, {"X-Account-Uuid", uuid}, {"X-Bot-Version", "101"}};
            Log::Log("正在连接 wss://party.gwtoolbox.com (X-Api-Key: %s, X-Account-Uuid: %s)", api_key.c_str(), uuid.c_str());
            return headers;
        });

        party_ws.SetOnMessage([](const std::string&) {
            // Log::Log("Websocket 消息\n%s", message.c_str());
        });

        party_ws.SetOnClose(on_websocket_closed);
    }

    // 在游戏线程上运行！
    std::vector<PartySearchAdvertisement> collect_party_searches()
    {
        ASSERT(GW::GameThread::IsInGameThread());

        const auto pc = GW::GetPartyContext();
        const auto searches = pc ? &pc->party_search : nullptr;
        const auto district_number = GW::Map::GetDistrict();
        const auto district_language = GW::Map::GetLanguage();
        std::vector<PartySearchAdvertisement> ads;
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return ads;
        if (searches) {
            for (const auto search : *searches) {
                if (!search) {
                    continue;
                }
                ASSERT(search->party_leader && *search->party_leader);

                PartySearchAdvertisement ad;
                ad.party_id = search->party_search_id;
                ad.party_size = static_cast<uint8_t>(search->party_size);
                ad.hero_count = static_cast<uint8_t>(search->hero_count);
                ad.search_type = static_cast<uint8_t>(search->party_search_type);
                ad.hardmode = static_cast<uint8_t>(search->hardmode);
                ad.district_number = static_cast<uint16_t>(search->district);
                ad.language = static_cast<uint8_t>(search->language);
                ad.primary = static_cast<uint8_t>(search->primary);
                ad.secondary = static_cast<uint8_t>(search->secondary);
                ad.level = static_cast<uint8_t>(search->level);
                ad.sender = TextUtils::WStringToString(search->party_leader);
                ad.message = TextUtils::WStringToString(search->message);
                ads.push_back(ad);
            }
        }
        const auto players = GW::PlayerMgr::GetPlayerArray();
        if (players) {
            for (const auto& player : *players) {
                if (!(player.party_size > 1 && player.name)) continue;
                const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(player.agent_id));
                if (!(agent && agent->GetIsLivingType())) continue; // 虽然玩家可能存在，但队伍大小取决于代理是否在罗盘范围内
                const auto sender = TextUtils::WStringToString(player.name);
                const auto found = std::ranges::find_if(ads.begin(), ads.end(), [sender](const PartySearchAdvertisement& e) {
                    return sender == e.sender;
                });
                if (found != ads.end()) continue; // 玩家已有队伍搜索条目
                PartySearchAdvertisement ad;
                ad.party_id = (0xf00 | player.player_number);
                ad.party_size = static_cast<uint8_t>(player.party_size);
                ad.district_number = static_cast<uint16_t>(district_number);
                ad.language = static_cast<uint8_t>(district_language);
                ad.primary = static_cast<uint8_t>(player.primary);
                ad.secondary = static_cast<uint8_t>(player.secondary);
                ad.level = agent->level;
                ad.sender = std::move(sender);
                ads.push_back(ad);
            }
        }

        return ads;
    }

    // 在游戏线程上运行！
    bool send_all_party_searches()
    {
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }

        auto parties = collect_party_searches();
        if (parties.empty()) {
            party_ws.Disconnect();
            return true;
        }

        party_broadcast_api::PartiesPayload j{
            .type = "client_parties",
            .map_id = static_cast<uint32_t>(GW::Map::GetMapID()),
            .district_region = static_cast<int32_t>(GW::Map::GetRegion()),
        };
        j.parties.reserve(parties.size());
        for (const auto& p : parties) j.parties.push_back(ToJson(p));

        const auto payload = glz::write_json(j).value_or(std::string{});
        if (!send_payload(payload)) return false;
        last_sent_district_info = GetDistrictInfo();

        server_parties.clear();
        for (auto& party : parties) {
            server_parties[party.party_id] = party;
        }
        last_update_timestamp = TIMER_INIT();
        return true;
    }

    bool send_changed_party_searches()
    {
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }
        auto parties = collect_party_searches();
        if (parties.empty()) {
            party_ws.Disconnect();
            return true;
        }

        const auto current_map_info = GetDistrictInfo();
        if (memcmp(&current_map_info, &last_sent_district_info, sizeof(current_map_info)) != 0) {
            // 地图自上次尝试后已更改；发送完整列表
            return send_all_party_searches();
        }

        std::vector<PartySearchAdvertisement> to_send;

        for (auto& existing_party : parties) {
            const auto found = server_parties.find(existing_party.party_id);
            if (found != server_parties.end() && memcmp(&existing_party, &found->second, sizeof(existing_party)) == 0) {
                continue; // 无变化，不发送
            }
            to_send.push_back(existing_party);
        }

        for (auto& it : server_parties) {
            const auto& last_sent_party = it.second;
            if (!last_sent_party.party_size) continue; // 已标记为移除
            const auto found = std::ranges::find_if(parties.begin(), parties.end(), [last_sent_party](const PartySearchAdvertisement& entry) {
                return entry.party_id == last_sent_party.party_id;
            });
            if (found == parties.end()) {
                // 队伍不再存在，标记为移除
                auto cpy = last_sent_party;
                cpy.party_size = 0; // 即“移除”
                to_send.push_back(cpy);
            }
        }

        if (to_send.empty()) return true; // 无变化
        party_broadcast_api::PartiesPayload j{
            .type = "updated_parties",
            .map_id = static_cast<uint32_t>(GW::Map::GetMapID()),
            .district_region = static_cast<int32_t>(GW::Map::GetRegion()),
        };
        j.parties.reserve(to_send.size());
        for (const auto& p : to_send) j.parties.push_back(ToJson(p));

        const auto payload = glz::write_json(j).value_or(std::string{});
        if (!send_payload(payload)) return false;
        last_sent_district_info = GetDistrictInfo();
        for (auto& party : to_send) {
            server_parties[party.party_id] = party;
        }
        last_update_timestamp = TIMER_INIT();
        return true;
    }

    bool send_payload(const std::string& payload)
    {
        return party_ws.Send(payload);
    }

    GW::HookEntry OnUIMessage_Hook;

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        need_to_send_party_searches = TIMER_INIT();
    }
} // namespace

void PartyBroadcast::Update(float)
{
    // 一旦工作线程完成，即加入它，然后重置以便可能重用。
    if (party_ws.Update()) {
        // 线程刚刚完成优雅断开；重新初始化以重置速率限制器。
        InitWebSocket();
    }

    if (terminating) return;

    if (!(ToolboxSettings::send_anonymous_gameplay_info && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)) {
        if (!party_ws.IsIdle()) party_ws.Disconnect();
        return;
    }

    if (need_to_send_party_searches && TIMER_DIFF(need_to_send_party_searches) > 250 && send_changed_party_searches()) {
        need_to_send_party_searches = 0;
    }
}

bool PartyBroadcast::CanTerminate()
{
    return party_ws.IsIdle();
}

void PartyBroadcast::SignalTerminate()
{
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Hook);
    GW::StoC::RemoveCallbacks(&OnUIMessage_Hook);
    terminating = true;
    party_ws.Disconnect();
}

void PartyBroadcast::Initialize()
{
    ToolboxModule::Initialize();
    terminating = false;

    InitWebSocket();

    need_to_send_party_searches = TIMER_INIT();

    constexpr GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kMapLoaded,
        (GW::UI::UIMessage)((uint32_t)GW::UI::UIMessage::kMoraleChange + 1), // wparam = player_id
        GW::UI::UIMessage::kPartySearchRemoved,
        GW::UI::UIMessage::kPartySearchUpdated,  // 队伍搜索更新
        GW::UI::UIMessage::kPartySearchCreated,  // 队伍搜索创建
        GW::UI::UIMessage::kPartySearchIdChanged // 队伍搜索移除
    };
    for (const auto message_id : ui_messages) {
        RegisterUIMessageCallback(&OnUIMessage_Hook, message_id, OnUIMessage, 0x8000);
    }
}

void PartyBroadcast::Terminate()
{
    ToolboxModule::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Hook);
    party_ws.Disconnect(true);
}
