#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Resources.h>
#include <Windows/TradeWindow.h>

// Every connection cost 30 seconds.
// You have 2 tries.
// After that, you can try every 30 seconds.
static const uint32_t COST_PER_CONNECTION_MS     = 30 * 1000;
static const uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;
static const char* months[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
using easywsclient::WebSocket;
using nlohmann::json;
using json_vec = std::vector<json>;

static const char ws_host_kmd[] = "wss://kamadan.gwtoolbox.com";
static const char https_host_kmd[] = "https://kamadan.gwtoolbox.com";
static const char ws_host_asc[] = "wss://ascalon.gwtoolbox.com";
static const char https_host_asc[] = "https://ascalon.gwtoolbox.com/";

static wchar_t *GetMessageCore()
{
    GW::Array<wchar_t> *buff = &GW::GetGameContext()->world->message_buff;
    return buff ? buff->begin() : nullptr;
}
void TradeWindow::OnMessageLocal(GW::HookStatus *status, GW::Packet::StoC::MessageLocal *pak)
{
    if (pak->channel != GW::Chat::CHANNEL_TRADE || status->blocked || !Instance().filter_alerts)
        return;
    // Only filter incoming trade messages if the user wants them filtered.
    if (!Instance().filter_local_trade)
        return;
    wchar_t *message = GetMessageCore();
    if (!message)
        return;

    const wchar_t *start = nullptr;
    const wchar_t *end = nullptr;
    size_t i = 0;
    while (start == nullptr && message[i]) {
        if (message[i] == 0x107)
            start = &message[i + 1];
        i++;
    }
    if (start == nullptr)
        return; // no string segment in this packet
    while (end == nullptr && message[i]) {
        if (message[i] == 0x1)
            end = &message[i];
        i++;
    }
    if (end == nullptr)
        end = &message[i];

    // std::string temp(start, end);
    std::string message_utf8 = GuiUtils::WStringToString(std::wstring(start, end));
    if (!Instance().IsTradeAlert(message_utf8))
        status->blocked = true;
}
void TradeWindow::CmdPricecheck(const wchar_t *, int argc, LPWSTR *argv)
{
    if (argc < 2)
        return Log::Error("Try '/pc <item>'");

    std::string item_to_search;
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            item_to_search += " ";
        item_to_search += GuiUtils::WStringToString(argv[i]);
    }
    Log::Info("Searching trade for \"%s\"...", item_to_search.c_str());
    TradeWindow::Instance().search(item_to_search, true);
}
void TradeWindow::Initialize() {
    ToolboxWindow::Initialize();

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
    GW::Chat::CreateCommand(L"pc", CmdPricecheck);
    // local messages
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&OnMessageLocal_Entry, OnMessageLocal);
    GW::StoC::RegisterPostPacketCallback(&OnPartySearch_Entry, GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT, [](GW::HookStatus*, void* pak) {
        struct Packet {
            uint32_t header;
            uint32_t other_atts[7];
            wchar_t message[32];
            wchar_t player[20];
        } *packet = (Packet*)pak;
        wchar_t* player_name = GW::PlayerMgr::GetPlayerName(GW::PlayerMgr::GetPlayerNumber());
        if (wcscmp(player_name, packet->player) == 0)
            FindPlayerPartySearch();
        });
    GW::StoC::RegisterPostPacketCallback(&OnPartySearch_Entry, GAME_SMSG_PARTY_SEARCH_REMOVE, FindPlayerPartySearch);
    GW::StoC::RegisterPostPacketCallback(&OnPartySearch_Entry, GAME_SMSG_TRANSFER_GAME_SERVER_INFO, FindPlayerPartySearch);
    FindPlayerPartySearch();
}
void TradeWindow::SignalTerminate()
{
    ToolboxWindow::SignalTerminate();
    should_stop = true;
    if (worker.joinable())
        worker.join();
    DeleteWebSocket(ws_window);
    ws_window = nullptr;
    if (wsaData.wVersion)
        WSACleanup();
}

bool TradeWindow::GetInKamadanAE1(bool check_district) {
    using namespace GW::Constants;
    switch (GW::Map::GetMapID()) {
    case MapID::Kamadan_Jewel_of_Istan_outpost:
    case MapID::Kamadan_Jewel_of_Istan_Halloween_outpost:
    case MapID::Kamadan_Jewel_of_Istan_Wintersday_outpost:
    case MapID::Kamadan_Jewel_of_Istan_Canthan_New_Year_outpost:
        return !check_district || (GW::Map::GetDistrict() == 1 && GW::Map::GetRegion() == GW::Constants::Region::America);
    default:
        return false;
    }
}
bool TradeWindow::GetInAscalonAE1(bool check_district) {
    using namespace GW::Constants;
    switch (GW::Map::GetMapID()) {
        case MapID::Ascalon_City_pre_searing:
            return !check_district ||
                   (GW::Map::GetDistrict() == 1 && GW::Map::GetRegion() == GW::Constants::Region::America);
        default: return false;
    }
}

void TradeWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (ws_window && ws_window->getReadyState() == WebSocket::CLOSED) {
        delete ws_window;
        ws_window = nullptr;
    }
    if (ws_window && ws_window->getReadyState() != WebSocket::CLOSED) {
        ws_window->poll();
    }
    bool search_pending = !pending_query_string.empty();
    bool maintain_socket = (visible && !collapsed) || ((print_game_chat || print_game_chat_asc) && GW::UI::GetPreference(GW::UI::FlagPreference::ChannelTrade) == 0) || search_pending;
    if (maintain_socket && !ws_window) {
        AsyncWindowConnect();
    }
    if (!maintain_socket && ws_window && ws_window->getReadyState() == WebSocket::OPEN) {
        ws_window->close();
        messages.clear();
        window_rate_limiter = RateLimiter(); // Deliberately closed; reset rate limiter.
    }
    fetch();
}

bool TradeWindow::parse_json_message(const json& js, Message* msg) {
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

void TradeWindow::fetch() {
    if (!ws_window || ws_window->getReadyState() != WebSocket::OPEN)
        return;
    bool search_pending = !pending_query_sent && !pending_query_string.empty();
    if (search_pending) {
        //strcpy(search_buffer, pending_query_string.c_str());
        // Fill searched_words; query to lower to ease on-the-fly search in ::fetch
        ParseBuffer(search_buffer, searched_words);

        // Send request
        json request;
        request["query"] = pending_query_string;
        pending_query_sent = clock();
        ws_window->send(request.dump());
    }

    ws_window->dispatch([this](const std::string& data) {
        const json& res = json::parse(data.c_str(), nullptr, false);
        if (res == json::value_t::discarded) {
            Log::Log("ERROR: Failed to parse res JSON from response in ws_window->dispatch\n");
                return;
        }
        if (res.find("query") != res.end() && res["query"].is_string()) {
            std::string query_string = res["query"].get<std::string>();
            if (query_string != pending_query_string)
                return; // Different query has been made since this search.
            pending_query_string.clear();
            if (!(res.contains("num_results") && res["num_results"].is_number_unsigned())) {
                Log::Log("ERROR: Failed to parse search results in TradeWindow::fetch\n");
                print_search_results = false;
                return;
            }
            size_t num_results = res["num_results"].get<size_t>();
            if (print_search_results && !num_results) {
                Log::Warning("No results found for %s", query_string.c_str());
                print_search_results = false;
                return;
            }
            if (!(res.contains("results") && res["results"].is_array())) {
                Log::Log("ERROR: Failed to parse search results in TradeWindow::fetch\n");
                print_search_results = false;
                return;
            }
            json_vec results = res["results"].get<json_vec>();
            messages.clear();
            if (print_search_results && !results.size()) {
                Log::Warning("No results found for %s", query_string.c_str());
                print_search_results = false;
                return;
            }
            size_t results_size = results.size();
            for (size_t i = results_size - 1; i < results_size; i--) {
                TradeWindow::Message msg;
                if (!parse_json_message(results[i], &msg))
                    continue;
                messages.add(msg);
                if (print_search_results && i < 5) {
                    std::wstring name_ws = GuiUtils::ToWstr(msg.name);
                    std::wstring msg_ws = GuiUtils::ToWstr(msg.message);
                    time_t ts = msg.timestamp;
                    tm* local_tm = localtime(&ts);
                    if (local_tm) {
                        wchar_t buf[512];
                        swprintf(buf, 512, L"<a=1>%s</a> @ %S %d, %02d:%02d: <c=#f96677><quote>%s", name_ws.c_str(), months[local_tm->tm_mon], local_tm->tm_mday, local_tm->tm_hour, local_tm->tm_min, msg_ws.c_str());
                        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_TRADE, buf);
                    }
                }
            }
            print_search_results = false;
            return;
        }
        // Add to message feed
        TradeWindow::Message msg;
        if (!parse_json_message(res, &msg))
            return; // Not valid message object
        bool add_to_window = searched_words.empty();
        if (!add_to_window) {
            // Currently showing a search term in-window. Only add if it matches all words.
            add_to_window = true;
            std::string input(msg.message);
            std::ranges::transform(input, input.begin(),
                                   [](char c) -> char {
                                       return static_cast<char>(::tolower(c));
                                   });
            for (auto& term : searched_words) {
                if (input.find(term) != std::string::npos)
                    continue; // Searched word no found; drop out
                add_to_window = false;
                break;
            }
        }
        if(add_to_window)
            messages.add(msg);

        // Check alerts
        // do not display trade chat while in kamadan AE district 1 or Pre-Searing Ascalon AE district 1
        bool print_message = ((is_kamadan_chat && print_game_chat && !GetInKamadanAE1()) || (!is_kamadan_chat && print_game_chat_asc && !GetInAscalonAE1())) && IsTradeAlert(msg.message);

        if (print_message) {
            wchar_t buffer[512];
            std::wstring name_ws = GuiUtils::ToWstr(msg.name);
            std::wstring msg_ws = GuiUtils::ToWstr(msg.message);
            swprintf(buffer, 512, L"<a=1>%s</a>: <c=#f96677><quote>%s", name_ws.c_str(), msg_ws.c_str());
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_TRADE, buffer);
        }
    });
}
bool TradeWindow::IsTradeAlert(std::string &message)
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
void TradeWindow::search(std::string query, bool print_results_in_chat)
{
    pending_query_string = query.empty() ? " " : query;
    print_search_results = print_results_in_chat;
    pending_query_sent = 0;
}

void TradeWindow::FindPlayerPartySearch(GW::HookStatus*, void*) {
    GW::PartyContext* ctx = GW::GetPartyContext();
    if (ctx && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        auto& party_searches = ctx->party_search;
        if (!party_searches.valid() || !party_searches.size()) {
            Instance().player_party_search = {0};
            return;
        }
        wchar_t* me = GW::PlayerMgr::GetPlayerName(GW::PlayerMgr::GetPlayerNumber());
        for (GW::PartySearch* party_search : party_searches) {
            if (party_search && wcscmp(me, party_search->party_leader) == 0) {
                GW::PartySearch* existing = &Instance().player_party_search;
                bool message_changed = wcscmp(existing->message, party_search->message) != 0;
                *existing = *party_search;
                if (message_changed) {
                    std::string pps_str = GuiUtils::WStringToString(party_search->message);
                    strcpy(Instance().player_party_search_text, pps_str.c_str());
                }
                return;
            }
        }
    }
    Instance().player_party_search = { 0 };
}

void TradeWindow::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    /* Alerts window */
    if (show_alert_window) {
        const float &font_scale = ImGui::GetIO().FontGlobalScale;
        ImGui::SetNextWindowSize(ImVec2(768.f * font_scale, 768.f * font_scale), ImGuiCond_FirstUseEver);
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
    const float btn_width = 80.0f * font_scale;
    const float search_bar_width = (ImGui::GetContentRegionAvail().x - (btn_width * 4) - ImGui::GetStyle().ItemInnerSpacing.x * 7);
    if (GetInKamadanAE1(false) || GetInAscalonAE1(false)) {
        bool advertise_dirty = false;
        static int search_type = GW::PartySearchType::PartySearchType_Trade;
        bool is_seeking = player_party_search.message[0] != 0;
        if (is_seeking) {
            search_type = static_cast<int>(player_party_search.party_search_type);
        }
        ImGui::PushItemWidth(search_bar_width);
        if (ImGui::InputTextWithHint("##search_text", "Seek Party", player_party_search_text, _countof(player_party_search_text), ImGuiInputTextFlags_EnterReturnsTrue)) {
            is_seeking = strlen(player_party_search_text);
            advertise_dirty = true;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(btn_width * 1.5f);
        advertise_dirty |= ImGui::Combo("##search_type", &search_type, "Hunting\0Mission\0Quest\0Trade\0Guild\0\0");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        advertise_dirty |= ImGui::Checkbox("Seek Party", &is_seeking);
        if (advertise_dirty) {
            if (!is_seeking) {
                if (player_party_search.message[0])
                    GW::PartyMgr::SearchPartyCancel();
            }
            else {
                struct {
                    uint32_t header = GAME_CMSG_PARTY_SEARCH_SEEK;
                    uint32_t search_type = 0;
                    wchar_t advert[32]{};
                    uint32_t hard_mode = 0;
                } packet;
                std::wstring out = GuiUtils::StringToWString(player_party_search_text);
                swprintf(packet.advert, _countof(packet.advert), L"%s", out.c_str());

                GW::PartyMgr::SearchParty(search_type, out.data());
            }
        }
        ImGui::Separator();
    }
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    const bool searching = !pending_query_string.empty();
    if (searching) {
        flags |= ImGuiInputTextFlags_ReadOnly;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    bool do_search = false;
    ImGui::PushItemWidth(search_bar_width);
    do_search |= ImGui::InputTextWithHint("##trade_search_buffer", is_kamadan_chat ? "Search Kamadan Trade Chat" : "Search Ascalon Trade Chat", search_buffer, 256, flags);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    do_search |= ImGui::Button(searching ? "Searching" : "Search", ImVec2(btn_width, 0));
    if (searching) {
        ImGui::PopStyleColor();
    } else if (do_search) {
        search(search_buffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(btn_width, 0))) {
        GuiUtils::StrCopy(search_buffer, "", 256);
        search("");
    }
    ImGui::SameLine();
    if (ImGui::Button("Alerts", ImVec2(btn_width, 0))) {
        show_alert_window = !show_alert_window;
    }

    ImGui::SameLine();
    if (ImGui::Button(is_kamadan_chat ? "Kamadan" : "Ascalon", ImVec2(btn_width, 0))) {
        is_kamadan_chat = !is_kamadan_chat;
        SwitchSockets();
    }
    if (is_kamadan_chat) {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Currently viewing messages from Kamadan AE1.\nClick to view messages from Pre-Searing Ascalon AE1 instead.");
    } else {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip( "Currently viewing messages from Pre-Searing Ascalon AE1.\nClick to view messages from Kamadan AE1 instead.");
    }


    /* Main trade chat area */
    ImGui::BeginChild("trade_scroll", ImVec2(0, -20.0f - ImGui::GetStyle().ItemInnerSpacing.y));
    /* Connection checks */
    if (!ws_window && !ws_window_connecting) {
        char buf[255];
        snprintf(buf, 255, "The connection to %s has timed out.", (is_kamadan_chat) ? ws_host_kmd : ws_host_asc);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(buf).x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::Text(buf);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Click to reconnect").x) / 2);
        if (ImGui::Button("Click to reconnect")) {
            AsyncWindowConnect(true);
        }
    } else if (ws_window_connecting || (ws_window && ws_window->getReadyState() == WebSocket::CONNECTING)) {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Connecting...").x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::Text("Connecting...");
    } else {
        /* Display trade messages */
        const bool show_time = ImGui::GetWindowWidth() > 600.0f;

        char timetext[128];
        time_t now = time(nullptr);

        const float &innerspacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float time_width = (show_time ? 100.0f : 0.0f) * font_scale;
        const float playername_left = time_width + innerspacing; // player button left align
        const float playernamewidth = 160.0f * font_scale;
        const float message_left = playername_left + playernamewidth + innerspacing;

        const size_t n_messages = messages.size();
        for (int i = static_cast<int>(n_messages - 1); i >= 0; i--) {
            Message &msg = messages[i];
            ImGui::PushID(i);

            // ==== time elapsed column ====
            if (show_time) {
                // negative numbers have came from this before, it is probably just server client desync
                const int time_since_message = static_cast<int>(now) - static_cast<int>(msg.timestamp);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.7f, .7f, .7f, 1.0f));

                // decide if days, hours, minutes, seconds...
                if (time_since_message / (60 * 60 * 24)) {
                    int days = time_since_message / (60 * 60 * 24);
                    _snprintf(timetext, 128, "%d %s ago", days, days > 1 ? "days" : "day");
                } else if (time_since_message / (60 * 60)) {
                    int hours = time_since_message / (60 * 60);
                    _snprintf(timetext, 128, "%d %s ago", hours, hours > 1 ? "hours" : "hour");
                } else if (time_since_message / 60) {
                    int minutes = time_since_message / 60;
                    _snprintf(timetext, 128, "%d %s ago", minutes, minutes > 1 ? "minutes" : "minute");
                } else {
                    _snprintf(timetext, 128, "%d %s ago", time_since_message, time_since_message > 1 ? "seconds" : "second");
                }
                ImGui::SetCursorPosX(playername_left - innerspacing - ImGui::CalcTextSize(timetext).x);
                ImGui::Text(timetext);
                ImGui::PopStyleColor();
            }

            // ==== Sender name column ====
            if (show_time) {
                ImGui::SameLine(playername_left);
            }
            if (ImGui::Button(msg.name.c_str(), ImVec2(playernamewidth, 0))) {
                // open whisper to player
                GW::GameThread::Enqueue([&msg]() {
                    std::wstring name_ws = GuiUtils::ToWstr(msg.name);
                    GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, name_ws.data());
                });
            }

            // ==== Message column ====
            ImGui::SameLine(message_left);
            ImGui::TextWrapped("%s", msg.message.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    /* Link to website footer */
    static char buf[128];
    if (!buf[0] || refresh_footer) {
        snprintf(buf, 128, "Powered by %s", is_kamadan_chat ? https_host_kmd : https_host_asc);
    }

    if (ImGui::Button(buf, ImVec2(ImGui::GetContentRegionAvail().x, 20.0f))) {
        if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
            ShellExecuteA(NULL, "open", is_kamadan_chat ? https_host_kmd : https_host_asc, NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::End();
}

void TradeWindow::RegisterSettingsContent()
{
    ToolboxWindow::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent(
        "Chat Settings",
        nullptr,
        [this](const std::string&, bool is_showing) {
            if (!is_showing)
                return;
            DrawChatSettings();
        },
        0.95f);
}

void TradeWindow::DrawAlertsWindowContent(bool) {
    ImGui::Text("Alerts");
    ImGui::Checkbox("Send Kamadan AE1 trade chat to your trade chat", &print_game_chat);
    ImGui::ShowHelp("Only when trade chat channel is visible in-game");
    ImGui::Checkbox("Send Pre-Searing Ascalon AE1 trade chat to your trade chat", &print_game_chat_asc);
    ImGui::ShowHelp("Only when trade chat channel is visible in-game");
    ImGui::Checkbox("Only show messages containing:", &filter_alerts);
    ImGui::ShowHelp("Only shows messages from the currently active trade channel (Kamadan OR Ascalon)");
    ImGui::TextDisabled("(Each line is a separate keyword. Not case sensitive.)");
    if (ImGui::InputTextMultiline("##alertfilter", alert_buf, ALERT_BUF_SIZE,
        ImVec2(-1.0f, 0.0f))) {

        ParseBuffer(alert_buf, alert_words);
        alertfile_dirty = true;
    }
    DrawChatSettings(true);
}
void TradeWindow::DrawChatSettings(bool ownwindow)
{
    ImGui::Checkbox("Apply trade alerts to local trade messages", &filter_local_trade);
    ImGui::ShowHelp("If enabled, only trade messages matching your alerts will be shown in chat");
    if (!ownwindow) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.f * ImGui::GetIO().FontGlobalScale, 0);
        if(ImGui::Button("Show Trade Alerts"))
            show_alert_window = !show_alert_window;
    }
}

void TradeWindow::DrawSettingInternal() {
    DrawAlertsWindowContent(false);
}

void TradeWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    print_game_chat = ini->GetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
    print_game_chat_asc = ini->GetBoolValue(Name(), VAR_NAME(print_game_chat_asc), print_game_chat_asc);
    filter_alerts = ini->GetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);
    filter_local_trade = ini->GetBoolValue(Name(), VAR_NAME(filter_local_trade), filter_local_trade);
    is_kamadan_chat = ini->GetBoolValue(Name(), VAR_NAME(is_kamadan_chat), is_kamadan_chat);

    std::ifstream alert_file;
    alert_file.open(Resources::GetPath(L"AlertKeywords.txt"));
    if (alert_file.is_open()) {
        alert_file.get(alert_buf, ALERT_BUF_SIZE, '\0');
        alert_file.close();
        ParseBuffer(alert_buf, alert_words);
    }
    alert_file.close();
    SwitchSockets();
}

void TradeWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
    ini->SetBoolValue(Name(), VAR_NAME(print_game_chat_asc), print_game_chat_asc);
    ini->SetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);
    ini->SetBoolValue(Name(), VAR_NAME(filter_local_trade), filter_local_trade);
    ini->SetBoolValue(Name(), VAR_NAME(is_kamadan_chat), is_kamadan_chat);

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

void TradeWindow::ParseBuffer(const char *text, std::vector<std::string>& words) {
    words.clear();
    std::istringstream stream(text);
    std::string word;
    while (std::getline(stream, word)) {
        for (size_t i = 0; i < word.length(); i++)
            word[i] = static_cast<char>(tolower(word[i]));
        words.push_back(word);
    }
}

void TradeWindow::ParseBuffer(std::fstream stream, std::vector<std::string>& words) {
    words.clear();
    std::string word;
    while (std::getline(stream, word)) {
        for (size_t i = 0; i < word.length(); i++)
            word[i] = static_cast<char>(tolower(word[i]));
        words.push_back(word);
    }
}

void TradeWindow::AsyncWindowConnect(bool force) {
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
        if ((ws_window = WebSocket::from_url((is_kamadan_chat) ? ws_host_kmd : ws_host_asc)) == nullptr) {
            printf("Couldn't connect to the host '%s'", (is_kamadan_chat) ? ws_host_kmd : ws_host_asc);
        }
        ws_window_connecting = false;
        if (messages.size() == 0 && pending_query_string.empty())
            search(""); // Initial draw, gets latest N messages
    });
}

void TradeWindow::DeleteWebSocket(easywsclient::WebSocket *ws) {
    if (!ws) return;
    if (ws->getReadyState() == easywsclient::WebSocket::OPEN)
        ws->close();
    while ( ws->getReadyState() != easywsclient::WebSocket::CLOSED)
        ws->poll();
    delete ws;
}

void TradeWindow::SwitchSockets() {
    refresh_footer = true;
    DeleteWebSocket(ws_window);
    ws_window = nullptr;
    messages.clear();
    AsyncWindowConnect(true);
}
