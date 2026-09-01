#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/TradeContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <Utils/RateLimiter.h>
#include <CircurlarBuffer.h>

#include <Modules/ChatFilter.h>
#include <Modules/Resources.h>
#include <Windows/FriendListWindow.h>
#include <Windows/TradeWindow.h>
#include <GWToolbox.h>
#include <Timer.h>
#include <Utils/TextUtils.h>

#include <atomic>

namespace tradechat_api {
    struct SearchRequest {
        std::string query;
    };

    // `t`（时间戳）有时是字符串，有时是数字 — 通过 raw_json 下游解析。
    struct RawMessage {
        std::string s; // 发送者
        std::string m; // 消息
        glz::raw_json t; // 时间戳（字符串或数字）
    };

    struct WebsocketEnvelope {
        std::string query;
        uint32_t num_results = 0;
        std::vector<RawMessage> results;

        // 以下字段仅用于原始消息信封。
        std::string s;
        std::string m;
        glz::raw_json t;
    };
}

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    constexpr uint32_t COST_PER_CONNECTION_MS = 30 * 1000;
    constexpr uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;
    static const char* months[] = {"一月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "十二月"};
    using easywsclient::WebSocket;
    constexpr glz::opts json_opts{.error_on_unknown_keys = false};

    constexpr char ws_host_kmd[] = "wss://kamadan.gwtoolbox.com";
    constexpr char https_host_kmd[] = "https://kamadan.gwtoolbox.com";
    constexpr char ws_host_asc[] = "wss://ascalon.gwtoolbox.com";
    constexpr char https_host_asc[] = "https://ascalon.gwtoolbox.com/";

    wchar_t* GetMessageCore()
    {
        GW::Array<wchar_t>* buff = &GW::GetGameContext()->world->message_buff;
        return buff ? buff->begin() : nullptr;
    }

    struct Message {
        uint32_t timestamp = 0;
        std::string name;
        std::string message;
        clock_t relative_time_updated = 0;
        char relative_time[64] = {0};
    };

    GW::HookEntry OnPartySearch_Entry;
    GW::PartySearch player_party_search = { 0 };
    char player_party_search_text[64] = { 0 };

    WSAData wsaData = { 0 };

    TradeWindow::Settings settings;

    bool refresh_footer = false;

    bool show_alert_window = false;

    // 窗口可能可见但已折叠 — 使用此变量检查状态。
    bool collapsed = false;

    static constexpr auto ALERT_BUF_SIZE = 1024 * 16;
    char alert_buf[ALERT_BUF_SIZE]{};
    // 当 alert_buf 被修改时设置
    bool alertfile_dirty = false;

    std::string pending_query_string;
    clock_t pending_query_sent = 0;
    bool print_search_results = false;

    char search_buffer[256] = {};

    std::vector<TextUtils::SearchPattern<char>> alert_words{};
    std::vector<TextUtils::SearchPattern<char>> searched_words{};

    CircularBuffer<Message> messages;

    std::atomic<bool> ws_window_connecting = false;

    easywsclient::WebSocket* ws_window = nullptr;

    RateLimiter window_rate_limiter;

    bool external_trade_message = false;

    void search(const std::string& query, const bool print_results_in_chat = false)
    {
        pending_query_string = query.empty() ? " " : query;
        print_search_results = print_results_in_chat;
        pending_query_sent = 0;
    }

    // 服务器可能以 JSON 数字或 JSON 引号字符串的形式发送 `t`。
    // 两种形式都解析为相同的底层毫秒时间戳。
    uint64_t parse_timestamp_raw(std::string_view raw)
    {
        if (raw.empty()) return 0ull;
        if (raw.front() == '"') {
            std::string parsed;
            if (glz::read_json(parsed, raw)) return 0ull;
            return strtoull(parsed.c_str(), nullptr, 10);
        }
        double n = 0.0;
        if (glz::read_json(n, raw)) return 0ull;
        return static_cast<uint64_t>(n);
    }

    bool fill_message(const tradechat_api::RawMessage& raw, Message* msg)
    {
        if (raw.s.empty() || raw.m.empty()) return false;
        const auto timestamp_ull = parse_timestamp_raw(raw.t.str);
        if (timestamp_ull == 0ull) return false;
        msg->name = raw.s;
        msg->message = raw.m;
        msg->timestamp = static_cast<uint32_t>(timestamp_ull / 1000); // 有点乱？
        return true;
    }


    void CHAT_CMD_FUNC(CmdPricecheck)
    {
        if (argc < 2) {
            return Log::Error("试试 '/pc [物品名称]'");
        }

        std::string item_to_search;
        for (int i = 1; i < argc; i++) {
            if (i > 1) {
                item_to_search += " ";
            }
            item_to_search += TextUtils::WStringToString(argv[i]);
        }
        Log::Flash("正在交易频道搜索 \"%s\"...", item_to_search.c_str());
        search(item_to_search, true);
    }

    bool IsTradeAlert(std::string& message)
    {
        if (!settings.filter_alerts) {
            return true;
        }
        // A word wrapped in slashes is a regex, anything else a case-insensitive substring.
        for (const auto& word : alert_words) {
            if (word.Matches(message)) {
                return true;
            }
        }
        return false;
    }

    GW::HookEntry OnUIMessage_Entry;
    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
        if (status->blocked) return;

        const wchar_t* message = nullptr;
        switch (message_id) {
            case GW::UI::UIMessage::kPlayerChatMessage: {
                const auto packet = (GW::UI::UIPacket::kPlayerChatMessage*)wparam;
                if (packet->channel != GW::Chat::Channel::CHANNEL_TRADE) break;
                message = packet->message;
                break;
            } break;
                case GW::UI::UIMessage::kWriteToChatLog: {
                const auto packet = (GW::UI::UIPacket::kWriteToChatLog*)wparam;
                if (packet->channel != GW::Chat::Channel::CHANNEL_TRADE) break;
                message = packet->message;
            } break;
        }
        if (message && settings.filter_alerts && (external_trade_message || settings.filter_local_trade)) {
            auto start = wcsrchr(message, 0x107);
            if (!start) {
                return;
            }
            start++;
            const auto end = wcschr(start, 0x1);
            if (!end) {
                return;
            }
            std::string message_utf8 = TextUtils::WStringToString(std::wstring(start, end));
            if (!IsTradeAlert(message_utf8)) {
                status->blocked = true;
            }
        }
    }

}

void TradeWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    messages = CircularBuffer<Message>(100);

    should_stop = false;
    worker = new std::thread([this] {
        for (;;) {
            std::function<void()> job;
            {
                std::lock_guard lock(thread_jobs_mutex);
                if (!thread_jobs.empty()) {
                    job = std::move(thread_jobs.front());
                    thread_jobs.pop();
                }
            }
            if (!job) {
                if (should_stop) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            job();
        }
    });
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"pc", CmdPricecheck);
    // 本地消息
    GW::StoC::RegisterPostPacketCallback(&OnPartySearch_Entry, GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT, [](GW::HookStatus*, void* pak) {
        const struct Packet {
            uint32_t header;
            uint32_t other_atts[7];
            wchar_t message[32];
            wchar_t player[20];
        }* packet = static_cast<Packet*>(pak);
        const wchar_t* player_name = GW::PlayerMgr::GetPlayerName(GW::PlayerMgr::GetPlayerNumber());
        if (wcscmp(player_name, packet->player) == 0) {
            FindPlayerPartySearch();
        }
    });
    GW::StoC::RegisterPostPacketCallback(&OnPartySearch_Entry, GAME_SMSG_PARTY_SEARCH_REMOVE, FindPlayerPartySearch);
    GW::StoC::RegisterPostPacketCallback(&OnPartySearch_Entry, GAME_SMSG_TRANSFER_GAME_SERVER_INFO, FindPlayerPartySearch);
    FindPlayerPartySearch();

    const auto ui_messages = {
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kPlayerChatMessage
    };
    for (const auto ui_message : ui_messages) {
        RegisterUIMessageCallback(&OnUIMessage_Entry, ui_message, OnUIMessage);
    }

}
void TradeWindow::Terminate()
{
    ToolboxWindow::Terminate();
    DeleteWebSocket(ws_window);
    ws_window = nullptr;
    should_stop = true;
    if (worker) {
        ASSERT(worker->joinable());
        worker->join();
        delete worker;
        worker = nullptr;
    }
    if (wsaData.wVersion) {
        WSACleanup();
        wsaData = { 0 };
    }
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Entry);
}
bool TradeWindow::GetInKamadanAE1(const bool check_district)
{
    using namespace GW::Constants;
    switch (GW::Map::GetMapID()) {
        case MapID::Kamadan_Jewel_of_Istan_outpost:
        case MapID::Kamadan_Jewel_of_Istan_Halloween_outpost:
        case MapID::Kamadan_Jewel_of_Istan_Wintersday_outpost:
        case MapID::Kamadan_Jewel_of_Istan_Canthan_New_Year_outpost:
            return !check_district || (GW::Map::GetDistrict() == 1 && GW::Map::GetRegion() == ServerRegion::America);
        default:
            return false;
    }
}

bool TradeWindow::GetInAscalonAE1(const bool check_district)
{
    using namespace GW::Constants;
    switch (GW::Map::GetMapID()) {
        case MapID::Ascalon_City_pre_searing:
            return !check_district ||
                   (GW::Map::GetDistrict() == 1 && GW::Map::GetRegion() == ServerRegion::America);
        default:
            return false;
    }
}

void TradeWindow::Update(const float)
{
    if (ws_window && ws_window->getReadyState() == WebSocket::CLOSED) {
        delete ws_window;
        ws_window = nullptr;
    }
    if (ws_window && ws_window->getReadyState() != WebSocket::CLOSED) {
        ws_window->poll();
    }
    const bool search_pending = !pending_query_string.empty();
    const bool maintain_socket = (visible && !collapsed) || ((settings.print_game_chat || settings.print_game_chat_asc) && GW::Map::GetIsMapLoaded() && GetPreference(GW::UI::FlagPreference::ChannelTrade) == 0) || search_pending;
    if (maintain_socket && !ws_window) {
        AsyncWindowConnect();
    }
    if (!maintain_socket && ws_window && ws_window->getReadyState() == WebSocket::OPEN) {
        ws_window->close();
        messages.clear();
        window_rate_limiter = RateLimiter(); // 故意关闭；重置速率限制器。
    }
    fetch();
}

void TradeWindow::fetch()
{
    if (!ws_window || ws_window->getReadyState() != WebSocket::OPEN) {
        return;
    }
    const bool search_pending = !pending_query_sent && !pending_query_string.empty();
    if (search_pending) {
        //strcpy(search_buffer, pending_query_string.c_str());
        // Fill searched_words for the on-the-fly search in ::fetch
        searched_words = TextUtils::ParsePatterns<char>(search_buffer);

        const tradechat_api::SearchRequest request{.query = pending_query_string};
        pending_query_sent = clock();
        ws_window->send(glz::write_json(request).value_or(std::string{}));
    }

    ws_window->dispatch([this](const std::string& data) {
        tradechat_api::WebsocketEnvelope res{};
        if (auto ec = glz::read<json_opts>(res, data); ec) {
            Log::Log("错误：在 ws_window->dispatch 中解析响应 JSON 失败\n");
            return;
        }
        if (!res.query.empty()) {
            if (res.query != pending_query_string) {
                return; // 自此次搜索以来已发起不同的查询
            }
            pending_query_string.clear();
            messages.clear();
            if (print_search_results && res.results.empty()) {
                Log::Warning("未找到 %s 的结果", res.query.c_str());
                print_search_results = false;
                return;
            }
            const size_t results_size = res.results.size();
            for (size_t i = results_size - 1; i < results_size; i--) {
                Message msg;
                if (!fill_message(res.results[i], &msg)) {
                    continue;
                }
                messages.add(msg);
                if (print_search_results && i < 12) {
                    std::wstring name_ws = TextUtils::StringToWString(msg.name);
                    if (ChatFilter::IsSenderBlocked(name_ws)) {
                        continue; // 跳过已屏蔽玩家的搜索结果
                    }
                    std::wstring msg_ws = TextUtils::StringToWString(msg.message);
                    time_t ts = msg.timestamp;
                    tm* local_tm = localtime(&ts);
                    if (local_tm) {
                        wchar_t buf[512];
                        swprintf(buf, 512, L"<a=1>%s</a> @ %S %d, %02d:%02d: <c=#f96677><quote>%s", name_ws.c_str(), months[local_tm->tm_mon], local_tm->tm_mday, local_tm->tm_hour, local_tm->tm_min, msg_ws.c_str());
                        WriteChat(GW::Chat::Channel::CHANNEL_TRADE, buf,nullptr,true);
                    }
                }
            }
            print_search_results = false;
            return;
        }
        Message msg;
        const tradechat_api::RawMessage raw{.s = res.s, .m = res.m, .t = res.t};
        if (!fill_message(raw, &msg)) {
            return; // 不是有效的消息对象
        }
        bool add_to_window = searched_words.empty();
        if (!add_to_window) {
            // 当前在窗口中显示搜索词。仅当匹配所有词时才添加。
            add_to_window = true;
            for (const auto& term : searched_words) {
                if (term.Matches(msg.message)) {
                    continue;
                }
                add_to_window = false;
                break;
            }
        }
        if (add_to_window) {
            messages.add(msg);
        }

        // do not display trade chat while in kamadan AE district 1 or Pre-Searing Ascalon AE district 1
        bool print_message = ((settings.is_kamadan_chat && settings.print_game_chat && !GetInKamadanAE1()) || (!settings.is_kamadan_chat && settings.print_game_chat_asc && !GetInAscalonAE1())) && IsTradeAlert(msg.message);

        if (print_message) {
            std::wstring name_ws = TextUtils::StringToWString(msg.name);
            if (FriendListWindow::GetIsPlayerIgnored(name_ws) || ChatFilter::IsSenderBlocked(name_ws)) {
                return; // 跳过已忽略或已屏蔽玩家的消息
            }
            std::wstring msg_ws = std::format(L"<c=#f96677><quote>{}",TextUtils::StringToWString(msg.message));
            external_trade_message = true;
            WriteChat(GW::Chat::Channel::CHANNEL_TRADE, msg_ws.c_str(),name_ws.c_str());
            external_trade_message = false;
        }
    });
}

void TradeWindow::FindPlayerPartySearch(GW::HookStatus*, void*)
{
    GW::PartyContext* ctx = GW::GetPartyContext();
    if (ctx && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        auto& party_searches = ctx->party_search;
        if (!party_searches.valid() || !party_searches.size()) {
            player_party_search = {0};
            return;
        }
        const wchar_t* me = GW::PlayerMgr::GetPlayerName(GW::PlayerMgr::GetPlayerNumber());
        for (const GW::PartySearch* party_search : party_searches) {
            if (party_search && wcscmp(me, party_search->party_leader) == 0) {
                GW::PartySearch* existing = &player_party_search;
                const bool message_changed = wcscmp(existing->message, party_search->message) != 0;
                *existing = *party_search;
                if (message_changed) {
                    const std::string pps_str = TextUtils::WStringToString(party_search->message);
                    strcpy(player_party_search_text, pps_str.c_str());
                }
                return;
            }
        }
    }
    player_party_search = {0};
}

void TradeWindow::Draw(IDirect3DDevice9*)
{
    /* 提醒窗口 */
    if (show_alert_window) {
        const float& font_scale = ImGui::FontScale();
        ImGui::SetNextWindowSize(ImVec2(768.f * font_scale, 768.f * font_scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("交易提醒", &show_alert_window)) {
            DrawAlertsWindowContent(true);
        }
        ImGui::End();
    }
    /* 主交易窗口 */
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
    collapsed = !ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
    if (collapsed) {
        ImGui::End();
        return;
    }
    /* 搜索栏标题 */
    const float& font_scale = ImGui::FontScale();
    const float btn_width = 80.0f * font_scale;
    const float search_bar_width = ImGui::GetContentRegionAvail().x - btn_width * 4 - ImGui::GetStyle().ItemInnerSpacing.x * 7;
    if (GetInKamadanAE1(false) || GetInAscalonAE1(false)) {
        bool advertise_dirty = false;
        static int search_type = GW::PartySearchType::PartySearchType_Trade;
        bool is_seeking = player_party_search.message[0] != 0;
        if (is_seeking) {
            search_type = static_cast<int>(player_party_search.party_search_type);
        }
        ImGui::PushItemWidth(search_bar_width);
        if (ImGui::InputTextWithHint("##search_text", "寻找队伍", player_party_search_text, _countof(player_party_search_text), ImGuiInputTextFlags_EnterReturnsTrue)) {
            is_seeking = strlen(player_party_search_text);
            advertise_dirty = true;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(btn_width * 1.5f);
        advertise_dirty |= ImGui::Combo("##search_type", &search_type, "狩猎\0任务\0委托\0交易\0公会\0\0");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        advertise_dirty |= ImGui::Checkbox("寻找队伍", &is_seeking);
        if (advertise_dirty) {
            if (!is_seeking) {
                if (player_party_search.message[0]) {
                    GW::PartyMgr::SearchPartyCancel();
                }
            }
            else {
                const std::wstring out = TextUtils::StringToWString(player_party_search_text);
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
    do_search |= ImGui::InputTextWithHint("##trade_search_buffer", settings.is_kamadan_chat ? "搜索 Kamadan 交易频道" : "搜索 Ascalon 交易频道", search_buffer, 256, flags);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    do_search |= ImGui::Button(searching ? "搜索中" : "搜索", ImVec2(btn_width, 0));
    if (searching) {
        ImGui::PopStyleColor();
    }
    else if (do_search) {
        search(search_buffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("清空", ImVec2(btn_width, 0))) {
        std::snprintf(search_buffer, _countof(search_buffer), "");
        search("");
    }
    ImGui::SameLine();
    if (ImGui::Button("提醒", ImVec2(btn_width, 0))) {
        show_alert_window = !show_alert_window;
    }

    ImGui::SameLine();
    if (ImGui::Button(settings.is_kamadan_chat ? "Kamadan" : "Ascalon", ImVec2(btn_width, 0))) {
        settings.is_kamadan_chat = !settings.is_kamadan_chat;
        SwitchSockets();
    }
    if (settings.is_kamadan_chat) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("当前查看 Kamadan AE1 的消息。\n点击切换到 Pre-Searing Ascalon AE1 的消息。");
        }
    }
    else {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("当前查看 Pre-Searing Ascalon AE1 的消息。\n点击切换到 Kamadan AE1 的消息。");
        }
    }

    /* 主交易聊天区域 */
    ImGui::BeginChild("trade_scroll", ImVec2(0, -20.0f - ImGui::GetStyle().ItemInnerSpacing.y));
    /* 连接检查 */
    if (!ws_window && !ws_window_connecting) {
        char buf[255];
        snprintf(buf, 255, "到 %s 的连接已超时。", settings.is_kamadan_chat ? ws_host_kmd : ws_host_asc);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(buf).x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::Text(buf);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("点击重新连接").x) / 2);
        if (ImGui::Button("点击重新连接")) {
            AsyncWindowConnect(true);
        }
    }
    else if (ws_window_connecting || (ws_window && ws_window->getReadyState() == WebSocket::CONNECTING)) {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("连接中...").x) / 2);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::Text("连接中...");
    }
    else {
        /* 显示交易消息 */
        const bool show_time = ImGui::GetWindowWidth() > 600.0f;

        const float& innerspacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float time_width = (show_time ? 100.0f : 0.0f) * font_scale;
        const float playername_left = time_width + innerspacing; // 玩家按钮左对齐
        const float playernamewidth = 160.0f * font_scale;
        const float message_left = playername_left + playernamewidth + innerspacing;

        const size_t n_messages = messages.size();
        for (int i = static_cast<int>(n_messages - 1); i >= 0; i--) {
            Message& msg = messages[i];
            ImGui::PushID(i);

            // ==== 时间列 ====
            if (show_time) {

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.7f, .7f, .7f, 1.0f));

                if (!msg.relative_time_updated || TIMER_DIFF(msg.relative_time_updated) > 1000) {
                    snprintf(msg.relative_time, sizeof(msg.relative_time), "%s", TextUtils::RelativeTime(msg.timestamp).c_str());
                    msg.relative_time_updated = TIMER_INIT();
                }
                ImGui::SetCursorPosX(playername_left - innerspacing - ImGui::CalcTextSize(msg.relative_time).x);
                ImGui::TextUnformatted(msg.relative_time);
                ImGui::PopStyleColor();
            }

            // ==== 发送者名称列 ====
            if (show_time) {
                ImGui::SameLine(playername_left);
            }
            if (ImGui::Button(msg.name.c_str(), ImVec2(playernamewidth, 0))) {
                GW::GameThread::Enqueue([&msg] {
                    std::wstring name_ws = TextUtils::StringToWString(msg.name);
                    SendUIMessage(GW::UI::UIMessage::kOpenWhisper, name_ws.data());
                });
            }

            // ==== 消息列 ====
            ImGui::SameLine(message_left);
            ImGui::TextWrapped("%s", msg.message.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    /* 网站链接脚注 */
    static char buf[128];
    if (!buf[0] || refresh_footer) {
        snprintf(buf, 128, "由 %s 提供", settings.is_kamadan_chat ? https_host_kmd : https_host_asc);
    }

    if (ImGui::Button(buf, ImVec2(ImGui::GetContentRegionAvail().x, 20.0f))) {
        ShellExecuteA(nullptr, "open", settings.is_kamadan_chat ? https_host_kmd : https_host_asc, nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::End();
}

void TradeWindow::RegisterSettingsContent()
{
    ToolboxWindow::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent(
        "聊天设置",
        nullptr,
        [this](const std::string&, const bool is_showing) {
            if (!is_showing) {
                return;
            }
            DrawChatSettings();
        },
        0.95f);
}

void TradeWindow::DrawAlertsWindowContent(bool)
{
    ImGui::Text("提醒");
    ImGui::CheckboxWithHelp("将 Kamadan AE1 交易频道发送到你的交易频道", &settings.print_game_chat, "仅当游戏内交易频道可见时");
    ImGui::CheckboxWithHelp("将 Pre-Searing Ascalon AE1 交易频道发送到你的交易频道", &settings.print_game_chat_asc, "仅当游戏内交易频道可见时");
    ImGui::Checkbox("仅显示包含以下关键词的消息：", &settings.filter_alerts);
    ImGui::Indent();
    ImGui::ShowHelp("仅显示当前活跃交易频道（Kamadan 或 Ascalon）的消息");
    ImGui::TextDisabled("（每行一个关键词，不区分大小写）");
    if (ImGui::InputTextMultiline("##alertfilter", alert_buf, ALERT_BUF_SIZE,
                                  ImVec2(-1.0f, 0.0f))) {
        alert_words = TextUtils::ParsePatterns<char>(alert_buf);
        alertfile_dirty = true;
    }
    DrawChatSettings(true);
    ImGui::Unindent();
}

void TradeWindow::DrawChatSettings(const bool ownwindow)
{
    ImGui::CheckboxWithHelp("对本地交易消息应用交易过滤器", &settings.filter_local_trade, "启用后，只有匹配你提醒的交易消息才会显示在聊天中");
    if (!ownwindow) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.f * ImGui::FontScale(), 0);
        if (ImGui::Button("显示交易提醒")) {
            show_alert_window = !show_alert_window;
        }
    }
}

void TradeWindow::DrawSettingsInternal()
{
    DrawAlertsWindowContent(false);
}

void TradeWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    strncpy(player_party_search_text, settings.player_party_search_text.c_str(), _countof(player_party_search_text) - 1);

    // 提醒关键词位于 AlertKeywords.txt 中（与 PartySearchWindow 共享），不在设置文档中
    std::ifstream alert_file;
    alert_file.open(Resources::GetSettingFileOrLegacy(L"AlertKeywords.txt"));
    if (alert_file.is_open()) {
        alert_file.get(alert_buf, ALERT_BUF_SIZE, '\0');
        alert_file.close();
        alert_words = TextUtils::ParsePatterns<char>(alert_buf);
    }
    alert_file.close();
    SwitchSockets();
}

void TradeWindow::SaveSettings(SettingsDoc& doc)
{
    settings.player_party_search_text = player_party_search_text;
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    if (alertfile_dirty || GWToolbox::SettingsFolderChanged()) {
        std::ofstream bycontent_file;
        bycontent_file.open(Resources::GetSettingFile(L"AlertKeywords.txt"));
        if (bycontent_file.is_open()) {
            bycontent_file.write(alert_buf, strlen(alert_buf));
            bycontent_file.close();
            alertfile_dirty = false;
        }
    }
}

void TradeWindow::AsyncWindowConnect(const bool force)
{
    if (ws_window) {
        return;
    }
    if (ws_window_connecting) {
        return;
    }
    if (!force && !window_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS)) {
        return;
    }
    int res;
    if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        printf("调用 WSAStartup 失败: %d\n", res);
        return;
    }
    ws_window_connecting = true;
    {
        std::lock_guard lock(thread_jobs_mutex);
        thread_jobs.push([this] {
            auto new_ws = WebSocket::from_url(settings.is_kamadan_chat ? ws_host_kmd : ws_host_asc);
            if (!new_ws) {
                printf("Couldn't connect to the host '%s'", settings.is_kamadan_chat ? ws_host_kmd : ws_host_asc);
            }
            GW::GameThread::Enqueue([this, new_ws] {
                ws_window = new_ws;
                ws_window_connecting = false;
                if (messages.size() == 0 && pending_query_string.empty()) {
                    search("");
                }
            });
        });
    }
}

void TradeWindow::DeleteWebSocket(WebSocket* ws)
{
    if (!ws) {
        return;
    }
    std::lock_guard lock(Instance().thread_jobs_mutex);
    Instance().thread_jobs.push([ws] {
        if (ws->getReadyState() == WebSocket::OPEN) {
            ws->close();
        }
        while (ws->getReadyState() != WebSocket::CLOSED) {
            ws->poll();
        }
        delete ws;
    });
}

void TradeWindow::SwitchSockets()
{
    refresh_footer = true;
    DeleteWebSocket(ws_window);
    ws_window = nullptr;
    messages.clear();
    AsyncWindowConnect(true);
}
