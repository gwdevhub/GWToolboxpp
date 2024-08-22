#include "stdafx.h"

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/PartyBroadcastModule.h>
#include <Defines.h>
#include <Utils/TextUtils.h>
#include <easywsclient/easywsclient.hpp>
#include <nlohmann/json.hpp>
#include <time.h>
#include <vector>

using json = nlohmann::json;

namespace {
    bool enabled;
    std::string api_key(256, 0);
    easywsclient::WebSocket* ws;
    std::string last_update_content;
    time_t last_update_timestamp = 0;
    uint32_t cooldown_sec = 5;

    #define UUID_NODE_LEN 6
    struct uuid
    {
        uint32_t time_low;
        uint16_t time_mid;
        uint16_t time_hi_and_version;
        uint8_t clock_seq_hi_and_reserved;
        uint8_t clock_seq_low;
        uint8_t node[UUID_NODE_LEN];
    };

    static const struct uuid null_uuid = {0};

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

    void convertArrayToUUID(const uint32_t player_uuid[4], struct uuid& out_uuid)
    {
        // Extract the time_low (32 bits) from the first element
        out_uuid.time_low = player_uuid[0];

        // Extract the time_mid (16 bits) from the higher half of the second element
        out_uuid.time_mid = static_cast<uint16_t>(player_uuid[1] >> 16);

        // Extract the time_hi_and_version (16 bits) from the lower half of the second element
        out_uuid.time_hi_and_version = static_cast<uint16_t>(player_uuid[1] & 0xFFFF);

        // Extract the clock_seq_hi_and_reserved (8 bits) from the higher byte of the third element
        out_uuid.clock_seq_hi_and_reserved = static_cast<uint8_t>(player_uuid[2] >> 24);

        // Extract the clock_seq_low (8 bits) from the second highest byte of the third element
        out_uuid.clock_seq_low = static_cast<uint8_t>((player_uuid[2] >> 16) & 0xFF);

        // Extract the node (48 bits or 6 bytes) from the lower 48 bits of the third and fourth elements
        out_uuid.node[0] = static_cast<uint8_t>((player_uuid[2] >> 8) & 0xFF);
        out_uuid.node[1] = static_cast<uint8_t>(player_uuid[2] & 0xFF);
        out_uuid.node[2] = static_cast<uint8_t>(player_uuid[3] >> 24);
        out_uuid.node[3] = static_cast<uint8_t>((player_uuid[3] >> 16) & 0xFF);
        out_uuid.node[4] = static_cast<uint8_t>((player_uuid[3] >> 8) & 0xFF);
        out_uuid.node[5] = static_cast<uint8_t>(player_uuid[3] & 0xFF);
    }


    void to_json(nlohmann::json& j, const PartySearchAdvertisement& p)
    {
        /*
    const json_keys = {
        'party_id':'i',
        'party_size':'ps',
        'sender':'s',
        'message':'ms',
        'hero_count':'hc',
        'search_type':'t',
        'map_id':'m',
        'district':'d',
        'hardmode':'hm',
        'district_number':'dn',
        'district_region':'dr',
        'district_language':'dl',
        'level':'1',
        'primary':'p',
        'secondary':'sc'
    }
    */
        j = nlohmann::json{{"i", p.party_id}, {"t", p.search_type}, {"dn", p.district_number}, {"p", p.primary}, {"s", p.sender}};

        // The following fields can be assumed to be reasonable defaults by the server, so only need to send if they're not standard.
        if (p.party_size > 1) j["ps"] = p.party_size;
        if (p.hero_count) j["hc"] = p.hero_count;
        if (p.hardmode) j["hm"] = p.hardmode;
        if (p.language) j["dl"] = p.language;
        if (p.secondary) j["sc"] = p.secondary;
        if (p.message.size()) j["ms"] = p.message;
        if (p.level != 20) j["l"] = p.level;
    }

    static inline int uuid_snprint(char* s, size_t n, const struct uuid* u)
    {
        u = u ? u : &null_uuid;
        return snprintf(s, n, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", u->time_low, u->time_mid, u->time_hi_and_version, u->clock_seq_hi_and_reserved, u->clock_seq_low, u->node[0], u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
    }

    std::string uuidToString(const struct uuid& u)
    {
        char buffer[37]; // 36 characters for UUID + 1 for null terminator

        uuid_snprint(buffer, sizeof(buffer), &u);
        return std::string(buffer);
    }

    bool connect_ws(const std::string user_agent, const std::string uuid) {
        if (ws) {
            ws->poll();
            if (ws->getReadyState() == easywsclient::WebSocket::readyStateValues::OPEN) {
                return true;
            }
            else {
                ws->close();
                delete ws;
            }
        }

        std::vector<easywsclient::HeaderKeyValuePair> headers;
        headers.push_back({"User-Agent", user_agent});
        headers.push_back({"X-Api-Key", api_key});
        headers.push_back({"X-Account-Uuid", uuid});
        headers.push_back({"X-Bot-Version", "100"});
        ws = easywsclient::WebSocket::from_url("wss://party.gwtoolbox.com", headers);
        if (!ws) {
            return false;
        }

        ws->poll();
        return ws->getReadyState() == easywsclient::WebSocket::readyStateValues::OPEN;
    }

    void send_payload(const std::string char_name, const std::string uuid, const std::string payload) {
        if (!connect_ws(char_name, uuid)) {
            last_update_timestamp = time(nullptr);
            return;
        }

        ws->poll();
        if (!ws || ws->getReadyState() != easywsclient::WebSocket::readyStateValues::OPEN) {
            return;
        }

        if (last_update_content != payload) {
            ws->send(payload);
            last_update_content = payload;
            last_update_timestamp = time(nullptr);
        }
    }
}

void PartyBroadcast::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(enabled);
    SAVE_STRING(api_key);
}

void PartyBroadcast::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(enabled);
    LOAD_STRING(api_key);
}

void PartyBroadcast::Update(float) {
    if (!enabled || api_key.size() == 0) {
        if (ws) {
            ws->close();
            delete ws;
            ws = nullptr;
        }

        return;
    }

    // Don't spam updates, only check parties every 5 seconds
    const auto current_time = time(nullptr);
    if (difftime(current_time, last_update_timestamp) < cooldown_sec) {
        return;
    }

    if (!GW::Map::GetIsMapLoaded()) {
        return;
    }

    uuid player_uuid;
    const auto map_id = GW::Map::GetMapID();
    const auto char_ctx = GW::GetCharContext();
    std::wstring wplayer_name = char_ctx->player_name;
    const auto player_name = TextUtils::WStringToString(wplayer_name);
    convertArrayToUUID(char_ctx->player_uuid, player_uuid);
    const auto player_uuid_str = uuidToString(player_uuid);
    json j;
    j["type"] = "client_parties";
    j["map_id"] = (uint32_t)map_id;
    j["district"] = 0;
    std::vector<PartySearchAdvertisement> ads;
    for (const auto search : GW::GetPartyContext()->party_search) {
        if (!search) {
            continue;
        }

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
        ad.sender = player_name;
        std::wstring wmessage = search->message;
        const auto message = TextUtils::WStringToString(wmessage);
        ad.message = message;
        ads.push_back(ad);
    }

    j["parties"] = ads;
    const auto payload = j.dump();
    send_payload(player_name, player_uuid_str, payload);
}

void PartyBroadcast::Initialize()
{
    ToolboxModule::Initialize();
}

void PartyBroadcast::Terminate() {
    ToolboxModule::Terminate();
}

void PartyBroadcast::DrawSettingsInternal()
{
    ImGui::Checkbox("Post Party Search Updates", &enabled);
    ImGui::ShowHelp("Post party searches to https://party.gwtoolbox.com");
    if (ImGui::InputText("API Key", api_key.data(), 256)) {
        api_key.resize(strlen(api_key.c_str()));
    }

    ImGui::ShowHelp("This key is used to post party search updates to https://party.gwtoolbox.com");
}
