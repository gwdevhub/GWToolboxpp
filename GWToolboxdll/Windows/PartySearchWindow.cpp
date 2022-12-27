#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/GameContainers/Array.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Resources.h>
#include <Windows/PartySearchWindow.h>

// Every connection cost 30 seconds.
// You have 2 tries.
// After that, you can try every 30 seconds.
static const uint32_t COST_PER_CONNECTION_MS     = 30 * 1000;
static const uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;
using easywsclient::WebSocket;
using nlohmann::json;
using json_vec = std::vector<json>;

static const char ws_host[] = "wss://lfg.gwtoolbox.com";
static const char https_host[] = "https://lfg.gwtoolbox.com";

namespace {
    static wchar_t* GetMessageCore()
    {
        GW::Array<wchar_t>* buff = &GW::GetGameContext()->world->message_buff;
        return buff ? buff->begin() : nullptr;
    }
    GW::PartySearch* GetRegionParty(uint32_t party_id) {
        GW::GameContext* g = GW::GetGameContext();
        if (!g || !g->party)
            return nullptr;
        auto& parties = g->party->party_search;
        if (!parties.valid() || !(party_id < parties.size()))
            return nullptr;
        return parties[party_id];
    }
    GW::PartyInfo* GetLocalParty(uint32_t party_id) {
        GW::GameContext* g = GW::GetGameContext();
        if (!g || !g->party)
            return nullptr;
        auto& parties = g->party->parties;
        if (!parties.valid() || !(party_id < parties.size()))
            return nullptr;
        return parties[party_id];
    }
    GW::Player* GetPartyLeader(GW::PartyInfo* party) {
        if (!(party && party->players.valid() && party->players.size()))
            return nullptr;
        return GW::PlayerMgr::GetPlayerByID(party->players[0].login_number);
    }
    GW::PartyInfo* GetPartyFromPlayer(uint32_t player_number) {
        GW::GameContext* g = GW::GetGameContext();
        if (!g || !g->party)
            return nullptr;
        auto& parties = g->party->parties;
        if (!parties.valid())
            return nullptr;
        for (size_t i = 0; i < parties.size(); i++) {
            if (parties[i] && parties[i]->players.valid()) {
                for (size_t j = 0; j < parties[i]->players.size(); j++) {
                    if (parties[i]->players[j].login_number == player_number)
                        return parties[i];
                }
            }
        }
        return nullptr;
    }

    const char* party_types[] {
        "Hunting",
        "Mission",
        "Quest",
        "Trade",
        "Guild",
        "Local"
    };
    const char* DistrictAbbr(int32_t region, int32_t language) {
        switch (static_cast<GW::Constants::MapRegion>(region)) {
        case GW::Constants::MapRegion::International:
            return "INT";
        case GW::Constants::MapRegion::American:
            return "AE";
        case GW::Constants::MapRegion::Korean:
            return "KR";
        case GW::Constants::MapRegion::Chinese:
            return "CN";
        case GW::Constants::MapRegion::Japanese:
            return "JP";
        default:
            switch (static_cast<GW::Constants::MapLanguage>(language)) {
            case GW::Constants::MapLanguage::French:
                return "FR";
            case GW::Constants::MapLanguage::German:
                return "DE";
            case GW::Constants::MapLanguage::Italian:
                return "IT";
            case GW::Constants::MapLanguage::Spanish:
                return "ES";
            case GW::Constants::MapLanguage::Polish:
                return "PL";
            case GW::Constants::MapLanguage::Russian:
                return "RU";
            default:
                return "EN";
            }
        }
    }
}

uint32_t PartySearchWindow::TBParty::IdFromRegionParty(uint32_t party_id) {
#pragma warning (push)
#pragma warning (disable: 4244)
    return (uint32_t)((uint16_t)0 << 16 | (uint16_t)party_id);
#pragma warning (pop)
}
uint32_t PartySearchWindow::TBParty::IdFromLocalParty(uint32_t party_id) {
#pragma warning (push)
#pragma warning (disable: 4244)
    return (uint32_t)((uint16_t)party_id << 16);
#pragma warning (pop)
}

bool PartySearchWindow::TBParty::FromRegionParty(GW::PartySearch* party) {
#pragma warning (push)
#pragma warning (disable: 4244)
    if (!party) return false;
    concat_party_id = IdFromRegionParty(party->party_search_id);
    party_size = party->party_size;
    hero_count = party->hero_count;
    search_type = party->party_search_type;
    is_hard_mode = party->hardmode;
    map_id = static_cast<uint16_t>(GW::Map::GetMapID());
    district = party->district;
    language = party->language;
    region_id = GW::Map::GetRegion();
    message = GuiUtils::WStringToString(party->message);
    primary = party->primary;
    secondary = party->secondary;
    player_name = GuiUtils::WStringToString(party->party_leader);
    Log::Log("Party %d updated\n", concat_party_id);
    return true;
#pragma warning (pop)
}
bool PartySearchWindow::TBParty::FromPlayerInMap(GW::Player* player) {
#pragma warning (push)
#pragma warning (disable: 4244)
    if (!player || player->party_size < 2)
        return false;
    GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(player->agent_id));
    if (!agent || !agent->GetIsLivingType() || !agent->IsPlayer())
        return false;
    party_size = player->party_size;
    // TODO: Can we find out if the party is HM?
    map_id = static_cast<uint16_t>(GW::Map::GetMapID());
    district = GW::Map::GetDistrict();
    language = GW::Map::GetLanguage();
    region_id = GW::Map::GetRegion();
    primary = player->primary;
    secondary = player->secondary;
    player_name = GuiUtils::WStringToString(player->name);
    Log::Log("Party %d updated\n", concat_party_id);
    return true;
#pragma warning (pop)
}
bool PartySearchWindow::TBParty::FromLocalParty(GW::PartyInfo* party) {
#pragma warning (push)
#pragma warning (disable: 4244)
    if (!party) return false;
    GW::Player* player = GetPartyLeader(party);
    if (!player) return false;
    concat_party_id = IdFromLocalParty(party->party_id);
    hero_count = party->heroes.valid() ? party->heroes.size() : 0;
    hero_count += party->henchmen.valid() ? party->henchmen.size() : 0;
    party_size = party->players.valid() ? party->players.size() : 0;
    party_size += hero_count;
    // TODO: Can we find out if the party is HM?
    map_id = static_cast<uint16_t>(GW::Map::GetMapID());
    district = GW::Map::GetDistrict();
    language = GW::Map::GetLanguage();
    region_id = GW::Map::GetRegion();
    primary = player->primary;
    secondary = player->secondary;
    player_name = GuiUtils::WStringToString(player->name);
    Log::Log("Party %d updated\n", concat_party_id);
    return true;
#pragma warning (pop)
}

void PartySearchWindow::Initialize() {
    ToolboxWindow::Initialize();

    party_advertisements.reserve(100);
    messages = CircularBuffer<Message>(100);

    should_stop = false;
    worker = std::thread([this]() {
        while (!should_stop) {
            if (thread_jobs.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                thread_jobs.front()();
                thread_jobs.pop();
            }
        }
    });
    // local messages
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_SEARCH_REMOVE, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_SEARCH_SIZE, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_SEARCH_TYPE, OnRegionPartyUpdated);

    //GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_RE, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_PLAYER_ADD, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_PLAYER_REMOVE, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_HENCHMAN_ADD, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_HENCHMAN_REMOVE, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_HERO_ADD, OnRegionPartyUpdated);
    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_PARTY_HERO_REMOVE, OnRegionPartyUpdated);

    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_UPDATE_AGENT_PARTYSIZE, OnRegionPartyUpdated);
    GW::StoC::RegisterPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_AGENT_DESTROY_PLAYER, OnRegionPartyUpdated);

    GW::StoC::RegisterPostPacketCallback(&OnMessageLocal_Entry, GAME_SMSG_INSTANCE_LOADED, [](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
        Instance().refresh_parties = clock() + 2000;
        });
    refresh_parties = clock();
}
void PartySearchWindow::ClearParties() {
    for (const auto& it : party_advertisements) {
        delete it.second;
    }
    party_advertisements.clear();
}
void PartySearchWindow::FillParties() {
    ClearParties();
    struct FakePacket : GW::Packet::StoC::PacketBase {
        uint32_t id;
    } packet;
    packet.header = GAME_SMSG_UPDATE_AGENT_PARTYSIZE;
    GW::PlayerArray* players = GW::PlayerMgr::GetPlayerArray();
    for (size_t i = 0; players && i < players->size(); i++) {
        packet.id = i;
        OnRegionPartyUpdated(nullptr, &packet);
    }
    GW::GameContext* g = GW::GetGameContext();
    if (!g || !g->party)
        return;
    packet.header = GAME_SMSG_PARTY_PLAYER_ADD;
    auto& local_parties = g->party->parties;
    for (size_t i = 0; local_parties.valid() && i < local_parties.size(); i++) {
        GW::PartyInfo* party = local_parties[i];
        if (!party) continue;
        packet.id = party->party_id;
        OnRegionPartyUpdated(nullptr, &packet);
    }
    packet.header = GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT;
    auto& region_parties = g->party->party_search;
    for (size_t i = 0; region_parties.valid() && i < region_parties.size(); i++) {
        auto* party = region_parties[i];
        if (!party) continue;
        packet.id = party->party_search_id;
        OnRegionPartyUpdated(nullptr, &packet);
    }

}
PartySearchWindow::TBParty* PartySearchWindow::GetParty(uint32_t party_id,wchar_t** leader_out) {
    for (auto& party : party_advertisements) {
        if (!party.second) continue;
        if (party.second->concat_party_id == party_id) {
            if (leader_out != nullptr)
                *leader_out = (wchar_t*)party.first.data();
            return party.second;
        }
    }
    return nullptr;
}
PartySearchWindow::TBParty* PartySearchWindow::GetPartyByName(std::wstring leader) {
    auto it = party_advertisements.find(leader);
    if (it == party_advertisements.end())
        return nullptr;
    return it->second;
}
void PartySearchWindow::OnRegionPartyUpdated(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet) {
    auto& instance = Instance();
    const std::lock_guard<std::recursive_mutex> lock(instance.party_mutex);

    // Unless pigs fly and district/party numbers go over 16 byte length, storing party_ids as uint16_t is fine.
    wchar_t* party_name = 0;
    uint32_t party_id;
    TBParty* party = nullptr;
    switch (packet->header) {
        case GAME_SMSG_PARTY_SEARCH_TYPE:
        case GAME_SMSG_PARTY_SEARCH_REMOVE:
        case GAME_SMSG_PARTY_SEARCH_SIZE:
        case GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT: {
            uint32_t this_party_id = *(&packet->header + 1);
            party_id = TBParty::IdFromRegionParty(this_party_id);
            party = instance.GetParty(party_id,&party_name);
            GW::PartySearch* region_party = GetRegionParty(this_party_id);
            if (region_party) {
                if (!party) party = new TBParty();
                if (!party->FromRegionParty(region_party)) {
                    region_party = nullptr;
                }
                else {
                    party_name = region_party->party_leader;
                }
            }
            if (!region_party) {
                delete party;
                party = nullptr;
            }
        } break;
        case GAME_SMSG_AGENT_DESTROY_PLAYER: {
            uint32_t player_id = *(&packet->header + 1);
            GW::Player* player = GW::PlayerMgr::GetPlayerByID(player_id);
            if (!player || !player->name)
                break;
            party_name = player->name;
            party = instance.GetPartyByName(player->name);
            if (party) {
                delete party;
                party = nullptr;
            }
        } break;
        case GAME_SMSG_UPDATE_AGENT_PARTYSIZE: {
            uint32_t player_id = *(&packet->header + 1);
            GW::Player * player = GW::PlayerMgr::GetPlayerByID(player_id);
            if (!player || !player->name)
                break;
            party_name = player->name;
            party = instance.GetPartyByName(player->name);
            if (!party) {
                party = new TBParty();
            }
            if (!party->FromPlayerInMap(player)) {
                delete party;
                party = nullptr;
            }
        } break;
        case GAME_SMSG_PARTY_PLAYER_ADD:
            // Redirect back around to the above case to remove the previous player's party listing.
            packet->header = GAME_SMSG_UPDATE_AGENT_PARTYSIZE;
            OnRegionPartyUpdated(nullptr, packet);
            packet->header = GAME_SMSG_PARTY_PLAYER_ADD;
        case GAME_SMSG_PARTY_PLAYER_REMOVE:
        case GAME_SMSG_PARTY_HENCHMAN_ADD:
        case GAME_SMSG_PARTY_HENCHMAN_REMOVE:
        case GAME_SMSG_PARTY_HERO_ADD:
        case GAME_SMSG_PARTY_HERO_REMOVE:
        case GAME_SMSG_PARTY_MEMBER_STREAM_END: {
            uint32_t this_party_id = *(&packet->header + 1);
            party_id = TBParty::IdFromLocalParty(this_party_id);
            party = instance.GetParty(party_id,&party_name);
            GW::PartyInfo* local_party = GetLocalParty(this_party_id);
            if (local_party) {
                if (!party) party = new TBParty();
                if (!party->FromLocalParty(local_party)) {
                    local_party = nullptr;
                }
                else {
                    party_name = GetPartyLeader(local_party)->name;
                }
            }
            if (!local_party) {
                delete party;
                party = nullptr;
            }
        } break;
    }
    if (party_name) {
        instance.party_advertisements[party_name] = party;
        if (!party) {
            instance.party_advertisements.erase(party_name);
        }
    }
}
void PartySearchWindow::SignalTerminate() {
    ToolboxWindow::SignalTerminate();
    should_stop = true;
    if (worker.joinable())
        worker.join();
    DeleteWebSocket(ws_window);
    ws_window = nullptr;
    if (wsaData.wVersion)
        WSACleanup();
}

void PartySearchWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (ws_window && ws_window->getReadyState() == WebSocket::CLOSED) {
        delete ws_window;
        ws_window = nullptr;
    }
    if (ws_window && ws_window->getReadyState() != WebSocket::CLOSED) {
        ws_window->poll();
    }
    bool maintain_socket = false;// (visible && !collapsed) || (print_game_chat && GW::UI::GetCheckboxPreference(GW::UI::CheckboxPreference_ChannelTrade) == 0);
    if (maintain_socket && !ws_window) {
        AsyncWindowConnect();
    }
    if (!maintain_socket && ws_window && ws_window->getReadyState() == WebSocket::OPEN) {
        ws_window->close();
        messages.clear();
        window_rate_limiter = RateLimiter(); // Deliberately closed; reset rate limiter.
    }
    fetch();
    if (refresh_parties && clock() > refresh_parties) {
        Instance().ClearParties();
        Instance().FillParties();
        refresh_parties = 0;
        max_party_size = 0;

    }
    if (!max_party_size) {
        GW::AreaInfo* this_map = GW::Map::GetCurrentMapInfo();
        if (this_map) {
            max_party_size = this_map->max_party_size;
        }
    }

}

bool PartySearchWindow::parse_json_message(const json& js, Message* msg) {
    if (js == json::value_t::discarded)
        return false;
    if (!(js.contains("s") && js["s"].is_string())
        || !(js.contains("m") && js["m"].is_string())
        || !(js.contains("t") && js["t"].is_number_unsigned()))
        return false;
    msg->name = js["s"].get<std::string>();
    msg->message = js["m"].get<std::string>();
    msg->name = js["s"].get<std::string>();
    msg->timestamp = static_cast<uint32_t>(js["t"].get<uint64_t>() / 1000); // Messy?
    return true;
}

void PartySearchWindow::fetch() {
    if (!ws_window || ws_window->getReadyState() != WebSocket::OPEN)
        return;

    ws_window->dispatch([this](const std::string& data) {
        const json& res = json::parse(data.c_str(), nullptr, false);
        if (res == json::value_t::discarded) {
            Log::Log("ERROR: Failed to parse res JSON from response in ws_window->dispatch\n");
                return;
        }
        // Add to message feed
        Message msg;
        if (!parse_json_message(res, &msg))
            return; // Not valid message object
        messages.add(msg);

        // Check alerts
        // do not display trade chat while in kamadan AE district 1
        bool print_message = print_game_chat && IsLfpAlert(msg.message);

        if (print_message) {
            wchar_t buffer[512];
            std::wstring name_ws = GuiUtils::ToWstr(msg.name);
            std::wstring msg_ws = GuiUtils::ToWstr(msg.message);
            swprintf(buffer, 512, L"<a=1>%s</a>: <c=#f96677><quote>%s", name_ws.c_str(), msg_ws.c_str());
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_TRADE, buffer);
        }
    });
}
bool PartySearchWindow::IsLfpAlert(std::string &message)
{
    if (!filter_alerts)
        return true;
    std::regex word_regex;
    std::smatch m;
    static const std::regex regex_check = std::regex("^/(.*)/[a-z]?$", std::regex::ECMAScript | std::regex::icase);
    for (const auto& word : alert_words) {
        if (std::regex_search(word, m, regex_check)) {
            try {
                word_regex = std::regex(m._At(1).str(), std::regex::ECMAScript | std::regex::icase);
            } catch (const std::exception &) {
                // Silent fail; invalid regex
            }
            if (std::regex_search(message, word_regex))
                return true;
        } else {
            auto found = std::ranges::search(message, word, [](char c1, char c2) -> bool { return tolower(c1) == c2; }).begin();
            if (found != message.end())
                return true;
        }
    }
    return false;
}

void PartySearchWindow::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    /* Alerts window */
    if (show_alert_window) {
        const float &font_scale = ImGui::GetIO().FontGlobalScale;
        ImGui::SetNextWindowSize(ImVec2(250.f * font_scale, 220.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Trade Alerts", &show_alert_window)) {
            DrawAlertsWindowContent(true);
        }
        ImGui::End();
    }
    /* Main trade window */
    if (!visible)
        return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
    collapsed = !ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
    if (collapsed) {
        ImGui::End();
        return;
    }
    /* Search bar header */
    const float &font_scale = ImGui::GetIO().FontGlobalScale;
    const float btn_width = 100.0f * font_scale;
    bool display_messages = true;
    /* Main trade chat area */

    /* Connection checks */
    /*if (!ws_window && !ws_window_connecting) {
        char buf[255];
        snprintf(buf, 255, "The connection to %s has timed out.", ws_host);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(buf).x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::Text(buf);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Click to reconnect").x) / 2);
        if (ImGui::Button("Click to reconnect")) {
            AsyncWindowConnect(true);
        }
        display_messages = false;
    } else if (ws_window_connecting || (ws_window && ws_window->getReadyState() == WebSocket::CONNECTING)) {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Connecting...").x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::Text("Connecting...");
        display_messages = false;
    } */
    if(display_messages) {
        const float &innerspacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float playernamewidth = 200.0f * font_scale;
        const float partycountleft = playernamewidth + innerspacing * 2;
        const float partycountwidth = 100.0f * font_scale;
        const float districtleft = partycountleft + partycountwidth + innerspacing;
        const float districtwidth = 100.0f * font_scale;
        const float message_left = districtleft + districtwidth + innerspacing;

        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - (btn_width * _countof(display_party_types)));
        ImGui::PushItemWidth(btn_width);
        float start_x = ImGui::GetCursorPosX();
        for (size_t i = 0; i < _countof(display_party_types); i++) {
            if (ignore_party_types[i])
                continue;
            if (i > 0)
                ImGui::SameLine(start_x += btn_width);
            ImGui::Checkbox(party_types[i], &display_party_types[i]);
        }
        ImGui::PopItemWidth();
        ImGui::Text("Party Leader");
        ImGui::SameLine(partycountleft);
        ImGui::Text("Size");
        ImGui::SameLine(districtleft);
        ImGui::Text("District");
        ImGui::SameLine(message_left);
        ImGui::Text("Description");
        ImGui::SameLine(message_left);
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btn_width);
        if (ImGui::Button("Alerts", ImVec2(btn_width, 0))) {
            show_alert_window = !show_alert_window;
        }
        ImGui::Separator();
        ImGui::BeginChild("lfg_scroll", ImVec2(0, -20.0f - ImGui::GetStyle().ItemInnerSpacing.y));
        ImVec4 green(0x00,0xff,0x00,0xff);
        ImVec4 yellow(0xff, 0xff, 0x00, 0xff);
        ImVec4 white(0xff, 0xff, 0xff, 0xff);
        int32_t language = GW::Map::GetLanguage();
        int32_t district = GW::Map::GetDistrict();
        uint32_t map = static_cast<uint32_t>(GW::Map::GetMapID());
        //auto& parties = party_ctx->party_search;
        for (const auto& it : party_advertisements) {
            auto* party = it.second;
            if (!party) continue;
            if (!display_party_types[party->search_type])
                continue;
            if (ignore_party_types[party->search_type])
                continue;
            ImGui::PushID(static_cast<int>(party->concat_party_id));

            char label[64];
            if (party->secondary) {
                snprintf(label, 64, "%s/%s %s",
                    GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(party->primary)).c_str(),
                    GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(party->secondary)).c_str(),
                    party->player_name.c_str());
            }
            else {
                snprintf(label, 64, "%s %s",
                    GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(party->primary)).c_str(),
                    party->player_name.c_str());
            }

            if (ImGui::Button(label, ImVec2(playernamewidth, 0))) {
                std::wstring leader_name = GuiUtils::StringToWString(party->player_name);
                // open whisper to player
                GW::GameThread::Enqueue([leader_name]() {
                    GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, (wchar_t*)leader_name.data(), nullptr);
                    });
            }
            ImGui::SameLine(partycountleft);
            ImGui::TextColored(party->party_size < max_party_size ? white : yellow,"%d/%d", party->party_size, max_party_size);
            ImGui::SameLine(districtleft);
            ImGui::TextColored(party->language == language && party->district == district && party->map_id == map ? white : yellow, "%s - %d", DistrictAbbr(party->region_id,party->language), party->district);

            /*auto map_name = map_names_by_id.find(party->map_id);
            if (map_name == map_names_by_id.end()) {
                std::wstring* map_name_ws = new std::wstring();
                map_names_by_id[party->map_id] = { {0}, map_name_ws };
                GW::AreaInfo* map = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(party->map_id));
                if (map && GW::UI::UInt32ToEncStr(map->name_id, map_names_by_id[party->map_id].first) {
                    uint32_t map_id = 0;
                    if()
                }
            }*/

            ImGui::SameLine(message_left);
            ImGui::Text(party->is_hard_mode ? "[Hard Mode] [%s] %s" : "[%s] %s", party_types[party->search_type], party->message.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
    }


    /* Link to website footer */
    static char buf[128];
    if (!buf[0]) {
        snprintf(buf, 128, "Powered by %s", https_host);
    }
    if (ImGui::Button(buf, ImVec2(ImGui::GetContentRegionAvail().x, 20.0f))) {
        if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
            ShellExecuteA(NULL, "open", https_host, NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::End();
}

void PartySearchWindow::DrawAlertsWindowContent(bool) {
    ImGui::Text("Alerts");
    ImGui::Checkbox("Send party advertisements to your trade chat", &print_game_chat);
    ImGui::ShowHelp("Only when trade chat channel is visible in-game");
    ImGui::Checkbox("Only show messages containing:", &filter_alerts);
    ImGui::TextDisabled("(Each line is a separate keyword. Not case sensitive.)");
    if (ImGui::InputTextMultiline("##alertfilter", alert_buf, ALERT_BUF_SIZE,
        ImVec2(-1.0f, 0.0f))) {

        ParseBuffer(alert_buf, alert_words);
        alertfile_dirty = true;
    }
}

void PartySearchWindow::DrawSettingInternal() {
    DrawAlertsWindowContent(false);
}

void PartySearchWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    print_game_chat = ini->GetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
    filter_alerts = ini->GetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);

    std::ifstream alert_file;
    alert_file.open(Resources::GetPath(L"AlertKeywords.txt"));
    if (alert_file.is_open()) {
        alert_file.get(alert_buf, ALERT_BUF_SIZE, '\0');
        alert_file.close();
        ParseBuffer(alert_buf, alert_words);
    }
    alert_file.close();
}

void PartySearchWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
    ini->SetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);

    if (alertfile_dirty) {
        std::ofstream bycontent_file;
        bycontent_file.open(Resources::GetPath(L"AlertKeywords.txt"));
        if (bycontent_file.is_open()) {
            bycontent_file.write(alert_buf, strlen(alert_buf));
            bycontent_file.close();
            alertfile_dirty = false;
        }
    }
}

void PartySearchWindow::ParseBuffer(const char *text, std::vector<std::string>& words) {
    words.clear();
    std::istringstream stream(text);
    std::string word;
    while (std::getline(stream, word)) {
        for (size_t i = 0; i < word.length(); i++)
            word[i] = static_cast<char>(tolower(word[i]));
        words.push_back(word);
    }
}

void PartySearchWindow::AsyncWindowConnect(bool force) {
    if (ws_window) return;
    if (ws_window_connecting) return;
    if (!force && !window_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS))
        return;
    int res;
    if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        printf("Failed to call WSAStartup: %d\n", res);
        return;
    }
    ws_window_connecting = true;
    thread_jobs.push([this]() {
        if ((ws_window = WebSocket::from_url(ws_host)) == nullptr) {
            printf("Couldn't connect to the host '%s'", ws_host);
        }
        ws_window_connecting = false;
    });
}

void PartySearchWindow::DeleteWebSocket(easywsclient::WebSocket *ws) {
    if (!ws) return;
    if (ws->getReadyState() == easywsclient::WebSocket::OPEN)
        ws->close();
    while ( ws->getReadyState() != easywsclient::WebSocket::CLOSED)
        ws->poll();
    delete ws;
}
