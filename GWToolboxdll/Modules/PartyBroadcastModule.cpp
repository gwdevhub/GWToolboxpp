#include "stdafx.h"

#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/Resources.h>
#include <Modules/PartyBroadcastModule.h>
#include <Modules/Updater.h>

#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>

#include <easywsclient/easywsclient.hpp>
#include <nlohmann/json.hpp>

#include <Defines.h>
#include <Timer.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/AccountContext.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <base64.h>
#include <wincrypt.h>
#include <Utils/ToolboxUtils.h>

using json = nlohmann::json;

namespace {
    bool enabled = false;
    std::string api_key;
    easywsclient::WebSocket* ws = nullptr;
    std::string last_update_content;
    clock_t last_update_timestamp = 0;
    bool need_to_send_party_searches = true;
    clock_t failed_to_send_ts = 0;

    const char* websocket_url = "wss://party.gwtoolbox.com";

    struct MapDistrictInfo {
        GW::Constants::MapID map_id = (GW::Constants::MapID)0;
        GW::Constants::ServerRegion region = (GW::Constants::ServerRegion)0;
        GW::Constants::Language language = (GW::Constants::Language)0;
        int district_number = 0;
    };

    MapDistrictInfo GetDistrictInfo() {
        return {
            GW::Map::GetMapID(),
            GW::Map::GetRegion(),
            GW::Map::GetLanguage(),
            GW::Map::GetDistrict()
        };
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
        char message[65] = { 0 };
        char sender[42] = { 0 };
    };
    std::vector<PartySearchAdvertisement> last_sent_parties;
    MapDistrictInfo last_sent_district_info;

    void to_json(nlohmann::json& j, const PartySearchAdvertisement& p)
    {
        j = nlohmann::json{ {"i", p.party_id} };
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
        if (*p.message) j["ms"] = p.message;
        if (p.level != 20) j["l"] = p.level;
    }

    bool get_api_key(std::string& out) {
        GWToolboxRelease current_release;
        if (!Updater::GetCurrentVersionInfo(&current_release)) {
            Log::Error("Failed to get current toolbox version info");
            return false;
        }
        out = std::format("gwtoolbox-{}-{}", current_release.version, current_release.size);
        return true;
    }

    void disconnect_ws() {
        if (ws) {
            ws->close();
            ws->poll();
            delete ws;
            ws = nullptr;
            last_update_content = "";
            last_update_timestamp = 0;
            last_sent_parties.clear();
            need_to_send_party_searches = true;
            memset(&last_sent_district_info, 0, sizeof(last_sent_district_info));
            Log::Log("Websocket disconnected");
        }
    }
    bool is_websocket_ready() {
        return ws && ws->getReadyState() == easywsclient::WebSocket::readyStateValues::OPEN;
    }

     bool get_uuid(std::string& out) {
         const auto account_uuid = GW::AccountMgr::GetPortalAccountUuid();
         if (!account_uuid) return false;
         out = TextUtils::GuidToString(account_uuid);
         return true;
    }

    // Run this on a worker thread!!
    bool connect_ws() {
        if (is_websocket_ready())
            return true;
        disconnect_ws();

        if (!api_key.size())
            return false;

        std::string uuid;
        if (!get_uuid(uuid))
            return false;

        easywsclient::HeaderKeyValuePair headers = {
            {"User-Agent", "GWToolboxpp"},
            {"X-Api-Key", api_key},
            {"X-Account-Uuid", uuid},
            {"X-Bot-Version", "100"}
        };
        Log::Log("Connecting to %s (X-Api-Key: %s, X-Account-Uuid: %s)", websocket_url, headers["X-Api-Key"].c_str(), headers["X-Account-Uuid"].c_str());

        ws = easywsclient::WebSocket::from_url(websocket_url, headers);
        if (!ws) {
            return false;
        }

        ws->poll();
        if (!is_websocket_ready()) {
            disconnect_ws();
            return false;
        }
        Log::Log("Websocket connected");
        last_update_content = "";
        last_update_timestamp = 0;
        return true;
    }
    // Run on worker thread!
    bool send_payload(const std::string payload) {
        if (!connect_ws()) {
            return false;
        }
        Log::Log("Websocket Send:\n%s", payload.c_str());
        ws->send(payload);
        return true;
    }

    // Run on game thread!
    std::vector<PartySearchAdvertisement> collect_party_searches() {
        ASSERT(GW::GameThread::IsInGameThread());
        const auto pc = GW::GetPartyContext();
        const auto searches = pc ? &pc->party_search : nullptr;
        const auto district_number = GW::Map::GetDistrict();
        const auto district_language = GW::Map::GetLanguage();
        std::vector<PartySearchAdvertisement> ads;
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
                strcpy(ad.sender, TextUtils::WStringToString(search->party_leader).c_str());
                if (search->message && *search->message) {
                    strcpy(ad.message, TextUtils::WStringToString(search->message).c_str());
                }
                ads.push_back(ad);
            }
        }
        const auto players = GW::PlayerMgr::GetPlayerArray();
        if (players) {
            for (const auto& player : *players) {
                if (!(player.party_size > 1 && player.name))
                    continue;
                const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(player.agent_id));
                if (!(agent && agent->GetIsLivingType()))
                    continue; // Although the player might be present, the party size depends on the agent being in compass range
                const auto sender = TextUtils::WStringToString(player.name);
                const auto found = std::ranges::find_if(ads.begin(), ads.end(), [sender](const PartySearchAdvertisement& e) {
                    return sender == e.sender;
                    });
                if (found != ads.end())
                    continue; // Player has a party search entry
                PartySearchAdvertisement ad;
                ad.party_id = (0xffff0000 | player.player_number);
                ad.party_size = static_cast<uint8_t>(player.party_size);
                ad.district_number = static_cast<uint16_t>(district_number);
                ad.language = static_cast<uint8_t>(district_language);
                ad.primary = static_cast<uint8_t>(player.primary);
                ad.secondary = static_cast<uint8_t>(player.secondary);
                ad.level = static_cast<uint8_t>(agent->level);
                strcpy(ad.sender,sender.c_str());
                ads.push_back(ad);
            }
        }

        return ads;
    }

    std::string last_party_searches_payload = "";
    // Run on game thread!
    bool send_all_party_searches() {
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }

        std::string uuid;
        if (!get_uuid(uuid))
            return false;

        auto to_send = collect_party_searches();

        json j;
        j["type"] = "client_parties";
        j["map_id"] = (uint32_t)GW::Map::GetMapID();
        j["district_region"] = (int)GW::Map::GetRegion();
        j["parties"] = to_send;

        const auto payload = j.dump();
        if (!send_payload(payload))
            return false;
        last_sent_district_info = GetDistrictInfo();
        last_sent_parties = std::move(to_send);
        last_update_timestamp = TIMER_INIT();
        return true;

    }

    bool send_changed_party_searches() {
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }
        std::string uuid;
        if (!get_uuid(uuid))
            return false;

        const auto current_map_info = GetDistrictInfo();
        if (memcmp(&current_map_info, &last_sent_district_info, sizeof(current_map_info)) != 0) {
            // Map has changed since last attempt; send full list
            return send_all_party_searches();
        }


        auto parties = collect_party_searches();

        std::vector<PartySearchAdvertisement> to_send;

        for (auto& existing_party : parties) {
            const auto found = std::ranges::find_if(last_sent_parties.begin(), last_sent_parties.end(), [existing_party](const PartySearchAdvertisement& entry) {
                return entry.party_id == existing_party.party_id;
                });
            if (found != last_sent_parties.end()
                && memcmp(&existing_party, &(*found), sizeof(existing_party)) == 0) {
                continue; // No change, don't send
            }
            to_send.push_back(existing_party);
        }

        for (auto& last_sent_party : last_sent_parties) {
            if (!last_sent_party.party_size)
                continue; // Already flagged for removal
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

        if (to_send.empty())
            return true; // No change
        json j;
        j["type"] = "updated_parties";
        j["map_id"] = (uint32_t)GW::Map::GetMapID();
        j["district_region"] = (int)GW::Map::GetRegion();
        j["parties"] = to_send;

        const auto payload = j.dump();
        if (!send_payload(payload))
            return false;
        last_sent_district_info = GetDistrictInfo();
        last_sent_parties = std::move(to_send);
        last_update_timestamp = TIMER_INIT();
        return true;
    }

    void on_websocket_message(const std::string& message) {
        Log::Log("Websocket message\n%s", message.c_str());
    }

    GW::HookEntry OnUIMessage_Hook;

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        need_to_send_party_searches = true;
    }
    void OnStoCPacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* ) {
        need_to_send_party_searches = true;
    }
}

void PartyBroadcast::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(enabled);
}

void PartyBroadcast::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(enabled);
}

void PartyBroadcast::Update(float) {
    if (enabled) {
        if (!(api_key.size() || get_api_key(api_key))) {
            Log::Error("Failed to get API key for %s. Disabling PartyBroadcast module", websocket_url);
            enabled = false;
        }
    }
    if (!(enabled && api_key.size() && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)) {
        disconnect_ws();
        return;
    }
    if (need_to_send_party_searches && (!failed_to_send_ts || TIMER_DIFF(failed_to_send_ts) > 5000)) {
        need_to_send_party_searches = !send_changed_party_searches();
        if(need_to_send_party_searches)
            failed_to_send_ts = TIMER_INIT();
    }
    if (ws) {
        ws->poll();
        ws->dispatch(on_websocket_message);
        if (ws->getReadyState() != easywsclient::WebSocket::OPEN)
            disconnect_ws();
    }

}

void PartyBroadcast::Initialize()
{
    ToolboxModule::Initialize();

    need_to_send_party_searches = true;

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kMapLoaded,
        (GW::UI::UIMessage)0x10000134, // Party search updated
        (GW::UI::UIMessage)0x10000132, // Party search updated
        (GW::UI::UIMessage)0x10000133, // Party search Remove 
        (GW::UI::UIMessage)0x10000131, // Party search Add
        (GW::UI::UIMessage)0x10000048 // Player size updated (in current range)
    };
    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_Hook, message_id, OnUIMessage, 0x8000);
    }
    GW::StoC::RegisterPacketCallback(&OnUIMessage_Hook, GAME_SMSG_AGENT_DESPAWNED, OnStoCPacket, 0x8000);
}

void PartyBroadcast::Terminate() {
    ToolboxModule::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Hook);
    GW::StoC::RemoveCallbacks(&OnUIMessage_Hook);
    disconnect_ws();

}

void PartyBroadcast::DrawSettingsInternal()
{
    ImGui::NewLine();
    ImGui::Text("Party Broadcast Integration - %s", websocket_url);
    ImGui::Indent();
    ImGui::Checkbox("Broadcast Party Searches", &enabled);
    ImGui::ShowHelp(std::format("Post party searches to {}", websocket_url).c_str());
    ImGui::Unindent();
}
