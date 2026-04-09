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
#include <nlohmann/json.hpp>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Timer.h>
#include <Utils/ToolboxUtils.h>
#include "ToolboxSettings.h"

using json = nlohmann::json;

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
        uint8_t search_type = 0; // 0=hunting, 1=mission, 2=quest, 3=trade, 4=guild
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

    void to_json(nlohmann::json& j, const PartySearchAdvertisement& p)
    {
        j = nlohmann::json{{"i", p.party_id}};
        if (!p.party_size) {
            // Aka "remove"
            j["r"] = 1;
            return;
        }
        j["t"] = p.search_type;
        j["p"] = p.primary;
        j["s"] = p.sender;

        // The following fields can be assumed to be reasonable defaults by the server, so only need to send if they're not standard.
        if (p.party_size > 1) j["ps"] = p.party_size;
        if (p.hero_count) j["hc"] = p.hero_count;
        if (p.hardmode) j["hm"] = p.hardmode;
        if (p.language) j["dl"] = p.language;
        if (p.secondary) j["sc"] = p.secondary;
        if (p.district_number) j["dn"] = p.district_number;
        if (!p.message.empty()) j["ms"] = p.message;
        if (p.level != 20) j["l"] = p.level;
    }

    bool get_api_key(std::string& out)
    {
        GWToolboxRelease current_release;
        if (!Updater::GetCurrentVersionInfo(&current_release)) {
            Log::Error("Failed to get current toolbox version info");
            return false;
        }
        out = std::format("gwtoolbox-{}-{}", current_release.version, current_release.size);
        return true;
    }

    bool get_uuid(std::string& out)
    {
        const auto account_uuid = GW::AccountMgr::GetPortalAccountUuid();
        if (!account_uuid) return false;
        out = TextUtils::GuidToString(account_uuid);
        return true;
    }

    void on_websocket_closed()
    {
        // NOTE: called on the worker thread by ThreadedWebSocket.
        // Only touch atomics / self-contained state here; anything requiring
        // the game thread should be enqueued via GW::GameThread::Enqueue.
        last_update_timestamp = 0;
        server_parties.clear();
        need_to_send_party_searches = TIMER_INIT();
        memset(&last_sent_district_info, 0, sizeof(last_sent_district_info));
        Log::Log("Websocket disconnected");
    }

    void InitWebSocket()
    {
        party_ws.SetUrl("wss://party.gwtoolbox.com");
        party_ws.SetReconnectCost(30'000, 60'000);

        party_ws.SetHeadersFactory([] {
            std::string api_key, uuid;
            get_api_key(api_key);
            get_uuid(uuid);
            easywsclient::HeaderKeyValuePair headers = {{"User-Agent", "GWToolboxpp"}, {"X-Api-Key", api_key}, {"X-Account-Uuid", uuid}, {"X-Bot-Version", "101"}};
            Log::Log("Connecting to wss://party.gwtoolbox.com (X-Api-Key: %s, X-Account-Uuid: %s)", api_key.c_str(), uuid.c_str());
            return headers;
        });

        party_ws.SetOnMessage([](const std::string&) {
            // Log::Log("Websocket message\n%s", message.c_str());
        });

        party_ws.SetOnClose(on_websocket_closed);
    }

    // Run on game thread!
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
                if (!(agent && agent->GetIsLivingType())) continue; // Although the player might be present, the party size depends on the agent being in compass range
                const auto sender = TextUtils::WStringToString(player.name);
                const auto found = std::ranges::find_if(ads.begin(), ads.end(), [sender](const PartySearchAdvertisement& e) {
                    return sender == e.sender;
                });
                if (found != ads.end()) continue; // Player has a party search entry
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

    // Run on game thread!
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

        json j;
        j["type"] = "client_parties";
        j["map_id"] = (uint32_t)GW::Map::GetMapID();
        j["district_region"] = (int)GW::Map::GetRegion();
        j["parties"] = parties;

        const auto payload = j.dump();
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
            // Map has changed since last attempt; send full list
            return send_all_party_searches();
        }

        std::vector<PartySearchAdvertisement> to_send;

        for (auto& existing_party : parties) {
            const auto found = server_parties.find(existing_party.party_id);
            if (found != server_parties.end() && memcmp(&existing_party, &found->second, sizeof(existing_party)) == 0) {
                continue; // No change, don't send
            }
            to_send.push_back(existing_party);
        }

        for (auto& it : server_parties) {
            const auto& last_sent_party = it.second;
            if (!last_sent_party.party_size) continue; // Already flagged for removal
            const auto found = std::ranges::find_if(parties.begin(), parties.end(), [last_sent_party](const PartySearchAdvertisement& entry) {
                return entry.party_id == last_sent_party.party_id;
            });
            if (found == parties.end()) {
                // Party no longer exists, flag for removal
                auto cpy = last_sent_party;
                cpy.party_size = 0; // Aka "remove"
                to_send.push_back(cpy);
            }
        }

        if (to_send.empty()) return true; // No change
        json j;
        j["type"] = "updated_parties";
        j["map_id"] = (uint32_t)GW::Map::GetMapID();
        j["district_region"] = (int)GW::Map::GetRegion();
        j["parties"] = to_send;

        const auto payload = j.dump();
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
    // Join the worker thread once it's finished, then reset for potential re-use.
    if (party_ws.Update()) {
        // Thread just completed a graceful disconnect; re-init resets the rate limiter.
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
        GW::UI::UIMessage::kPartySearchUpdated,  // Party search updated
        GW::UI::UIMessage::kPartySearchCreated,  // Party search updated
        GW::UI::UIMessage::kPartySearchIdChanged // Party search Remove
    };
    for (const auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_Hook, message_id, OnUIMessage, 0x8000);
    }
}

void PartyBroadcast::Terminate()
{
    ToolboxModule::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Hook);
    party_ws.Disconnect(true);
}
