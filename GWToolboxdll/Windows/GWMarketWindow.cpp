#include "stdafx.h"

#include "GWMarketWindow.h"

#include <GWCA/GWCA.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <Utils/RateLimiter.h>

#include <easywsclient.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <Utils/TextUtils.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>

namespace {
    using easywsclient::WebSocket;
    using json = nlohmann::json;

    // Connection rate limiting
    constexpr uint32_t COST_PER_CONNECTION_MS = 30 * 1000;
    constexpr uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;

    // Enums for type-safe representations
    enum class Currency : uint8_t { Platinum = 0, Ecto = 1, Zkeys = 2, Arms = 3 };

    enum class OrderType : uint8_t { Sell = 0, Buy = 1 };

    enum class OrderSortMode : uint8_t { MostRecent = 0, Currency = 1 };


    const char* GetPriceTypeString(Currency currency)
    {
        switch (currency) {
            case Currency::Platinum:
                return "Plat";
            case Currency::Ecto:
                return "Ecto";
            case Currency::Zkeys:
                return "Zkeys";
            case Currency::Arms:
                return "Arms";
            default:
                return "Unknown";
        }
    }

    IDirect3DTexture9** GetCurrencyImage(Currency currency)
    {
        switch (currency) {
            case Currency::Platinum:
                return GwDatTextureModule::LoadTextureFromFileId(0x1df32);
            case Currency::Ecto:
                return GwDatTextureModule::LoadTextureFromFileId(0x151fa);
            case Currency::Zkeys:
                return GwDatTextureModule::LoadTextureFromFileId(0x52ecf);
            case Currency::Arms:
                return GwDatTextureModule::LoadTextureFromFileId(0x2ae18);
        }
        return nullptr;
    }

    const char* GetOrderTypeString(OrderType type)
    {
        switch (type) {
            case OrderType::Sell:
                return "SELL";
            case OrderType::Buy:
                return "BUY";
            default:
                return "SELL";
        }
    }

    // String interning to reduce memory footprint
    std::unordered_set<std::string> string_pool;

    const std::string* InternString(const std::string& str)
    {
        auto it = string_pool.insert(str);
        return &(*it.first);
    }

    // Data structures
    struct Price {
        Currency type;
        int quantity;
        double price;

        static Price FromJson(const json& j)
        {
            Price p;
            int typeNum = j.value("type", 0);
            p.type = static_cast<Currency>(typeNum);
            p.quantity = j.value("quantity", 0);
            p.price = j.value("price", 0.0);
            return p;
        }
    };

    struct MarketItem {
        const std::string* name;
        const std::string* player;
        OrderType orderType;
        int quantity;
        std::vector<Price> prices;
        time_t lastRefresh = 0;

        static MarketItem FromJson(const json& j)
        {
            MarketItem item;

            std::string name_str = j.value("name", "");
            item.name = InternString(name_str);

            std::string player_str = j.value("player", "");
            item.player = InternString(player_str);

            int orderTypeNum = j.value("orderType", 0);
            item.orderType = static_cast<OrderType>(orderTypeNum);

            item.quantity = j.value("quantity", 0);

            uint64_t lastRefresh_ms = j.value("lastRefresh", 0ULL);
            item.lastRefresh = lastRefresh_ms / 1000;

            if (j.contains("prices") && j["prices"].is_array()) {
                for (const auto& price_json : j["prices"]) {
                    item.prices.push_back(Price::FromJson(price_json));
                }
            }

            return item;
        }
    };

    struct AvailableItem {
        const std::string* name;
        int sellOrders = 0;
        int buyOrders = 0;
    };

    // Settings
    bool auto_refresh = true;
    int refresh_interval = 60;
    constexpr char ws_host[] = "wss://gwmarket.net/socket.io/?EIO=4&transport=websocket";

    // WebSocket
    WebSocket* ws = nullptr;
    bool ws_connecting = false;
    WSAData wsaData = {0};
    RateLimiter ws_rate_limiter;

    // Thread
    std::queue<std::function<void()>> thread_jobs{};
    bool should_stop = false;
    std::thread* worker = nullptr;

    // Data
    std::vector<AvailableItem> available_items;
    std::vector<MarketItem> last_items;
    std::vector<MarketItem> current_item_orders;
    std::string current_viewing_item;

    // UI
    char search_buffer[256] = "";
    enum FilterMode { SHOW_ALL, SHOW_SELL_ONLY, SHOW_BUY_ONLY };
    FilterMode filter_mode = SHOW_ALL;
    float refresh_timer = 0.0f;

    // Socket.IO
    bool socket_io_ready = false;
    clock_t last_ping_time = 0;
    int ping_interval = 25000;
    int ping_timeout = 20000;

    OrderSortMode order_sort_mode = OrderSortMode::MostRecent;
    bool available_items_needs_sort = true;
    bool current_orders_needs_sort = true;

    // Forward declarations
    void SendSocketStarted();
    void SendGetAvailableOrders();
    void SendGetLastItemsByFamily(const std::string& family);
    void SendGetItemOrders(const std::string& item_name);
    void SendPing();
    void OnGetAvailableOrders(const json& orders);
    void OnGetLastItems(const json& items);
    void OnGetItemOrders(const json& orders);
    void HandleSocketIOHandshake(const std::string& message);
    void OnNamespaceConnected();
    void OnWebSocketMessage(const std::string& message);
    void DeleteWebSocket(WebSocket* socket);
    void ConnectWebSocket(const bool force);
    void DrawItemList();
    void DrawItemDetails();
    void Refresh();

    std::string EncodeSocketIOMessage(const std::string& event, const std::string& data = "")
    {
        json msg = json::array();
        msg.push_back(event);
        if (!data.empty()) {
            msg.push_back(data);
        }
        return "42" + msg.dump();
    }

    std::string EncodeSocketIOMessage(const std::string& event, const json& data)
    {
        json msg = json::array();
        msg.push_back(event);
        msg.push_back(data);
        return "42" + msg.dump();
    }

    void ParseSocketIOMessage(const std::string& message, std::string& event, json& data)
    {
        if (message.length() < 2 || message.substr(0, 2) != "42") return;

        json parsed = json::parse(message.substr(2));
        if (parsed.is_array() && parsed.size() >= 1) {
            event = parsed[0].get<std::string>();
            if (parsed.size() >= 2) {
                data = parsed[1];
            }
        }
    }

    void OnGetAvailableOrders(const json& orders)
    {
        available_items.clear();
        available_items.reserve(orders.size());
        for (auto it = orders.begin(); it != orders.end(); ++it) {
            AvailableItem item;
            item.name = InternString(it.key());
            if (it.value().contains("sellOrders")) {
                item.sellOrders += it.value()["sellOrders"].get<int>();
            }
            if (it.value().contains("buyOrders")) {
                item.buyOrders += it.value()["buyOrders"].get<int>();
            }
            available_items.push_back(item);
        }
        available_items_needs_sort = true;
        Log::Log("Received %zu available items", available_items.size());
    }

    void OnGetLastItems(const json& items)
    {
        last_items.clear();
        if (items.is_array()) {
            last_items.reserve(items.size());
            for (const auto& item_json : items) {
                last_items.push_back(MarketItem::FromJson(item_json));
            }
        }
        Log::Log("Received %zu recent listings", last_items.size());
    }

    void OnGetItemOrders(const json& orders)
    {
        current_item_orders.clear();
        if (orders.is_array()) {
            current_item_orders.reserve(orders.size());
            for (const auto& order_json : orders) {
                current_item_orders.push_back(MarketItem::FromJson(order_json));
            }
        }
        current_orders_needs_sort = true;
        Log::Log("Received %zu orders", current_item_orders.size());
    }

    void HandleSocketIOHandshake(const std::string& message)
    {
        if (message[0] != '0') return;

        json handshake = json::parse(message.substr(1));
        if (handshake.contains("pingInterval")) {
            ping_interval = handshake["pingInterval"].get<int>();
        }
        if (handshake.contains("pingTimeout")) {
            ping_timeout = handshake["pingTimeout"].get<int>();
        }

        Log::Log("Handshake: ping %dms, timeout %dms", ping_interval, ping_timeout);

        if (ws && ws->getReadyState() == WebSocket::OPEN) {
            ws->send("40");
            Log::Log("[SEND] 40 (connecting to namespace)");
        }

        last_ping_time = clock();
    }

    void OnNamespaceConnected()
    {
        Log::Log("Namespace connected, ready to send events");
        socket_io_ready = true;

        SendSocketStarted();
        Refresh();
    }

    void OnWebSocketMessage(const std::string& message)
    {
        if (message.empty()) return;

        Log::Log("[RECV] %s", message.c_str());

        char type = message[0];

        switch (type) {
            case '0':
                HandleSocketIOHandshake(message);
                break;
            case '1':
                Log::Warning("Server close");
                break;
            case '2':
                if (ws && ws->getReadyState() == WebSocket::OPEN) {
                    ws->send("3");
                    Log::Log("[SEND] 3");
                }
                break;
            case '3':
                Log::Log("Pong received");
                break;
            case '4':
                if (message.length() > 1) {
                    if (message[1] == '0') {
                        Log::Log("Namespace connect confirmed");
                        OnNamespaceConnected();
                    }
                    else if (message[1] == '2') {
                        std::string event;
                        json data;
                        ParseSocketIOMessage(message, event, data);

                        if (event == "GetAvailableOrders")
                            OnGetAvailableOrders(data);
                        else if (event == "GetLastItems")
                            OnGetLastItems(data);
                        else if (event == "GetItemOrders")
                            OnGetItemOrders(data);
                    }
                }
                break;
        }
    }

    void SendSocketStarted()
    {
        if (ws && ws->getReadyState() == WebSocket::OPEN && socket_io_ready) {
            std::string msg = EncodeSocketIOMessage("SocketStarted");
            ws->send(msg);
            Log::Log("[SEND] %s", msg.c_str());
        }
    }

    void SendGetAvailableOrders()
    {
        if (ws && ws->getReadyState() == WebSocket::OPEN && socket_io_ready) {
            std::string msg = EncodeSocketIOMessage("getAvailableOrders");
            ws->send(msg);
            Log::Log("[SEND] %s", msg.c_str());
        }
    }

    void SendGetLastItemsByFamily(const std::string& family)
    {
        if (ws && ws->getReadyState() == WebSocket::OPEN && socket_io_ready) {
            std::string msg = EncodeSocketIOMessage("getLastItemsByFamily", family);
            ws->send(msg);
            Log::Log("[SEND] %s", msg.c_str());
        }
    }

    void SendGetItemOrders(const std::string& item_name)
    {
        if (ws && ws->getReadyState() == WebSocket::OPEN && socket_io_ready) {
            std::string msg = EncodeSocketIOMessage("getItemOrders", item_name);
            ws->send(msg);
            Log::Log("[SEND] %s", msg.c_str());
        }
    }

    void SendPing()
    {
        if (ws && ws->getReadyState() == WebSocket::OPEN) {
            ws->send("2");
            last_ping_time = clock();
            Log::Log("[SEND] 2");
        }
    }

    void DeleteWebSocket(WebSocket* socket)
    {
        if (!socket) return;
        if (socket->getReadyState() == WebSocket::OPEN) socket->close();
        while (socket->getReadyState() != WebSocket::CLOSED)
            socket->poll();
        delete socket;
    }

    void ConnectWebSocket(const bool force = false)
    {
        if (ws || ws_connecting) return;

        if (!force && !ws_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS)) {
            return;
        }

        int res;
        if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            Log::Error("WSAStartup failed: %d", res);
            return;
        }

        ws_connecting = true;
        thread_jobs.push([] {
            ws = WebSocket::from_url(ws_host);
            if (ws) {
                Log::Log("Connected");
                socket_io_ready = false;
                last_ping_time = clock();
            }
            else {
                Log::Error("Connection failed");
            }
            ws_connecting = false;
        });
    }

    void DrawItemList()
    {
        ImGui::Text("Available Listings (%zu)", available_items.size());
        ImGui::Separator();
        // Sort only when data changes
        if (available_items_needs_sort) {
            std::sort(available_items.begin(), available_items.end(), [](const AvailableItem& a, const AvailableItem& b) {
                return *a.name < *b.name;
            });

            available_items_needs_sort = false;
        }

        for (const auto& item : available_items) {
            // Apply filter mode
            if (filter_mode == SHOW_SELL_ONLY && item.sellOrders == 0) continue;
            if (filter_mode == SHOW_BUY_ONLY && item.buyOrders == 0) continue;

            // Apply search filter
            if (search_buffer[0] != '\0') {
                std::string search_lower = search_buffer;
                std::string name_lower = *item.name;
                std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                if (name_lower.find(search_lower) == std::string::npos) continue;
            }

            ImGui::PushID(item.name);

            bool selected = (current_viewing_item == *item.name);
            if (ImGui::Selectable(item.name->c_str(), selected)) {
                current_viewing_item = *item.name;
                SendGetItemOrders(*item.name);
            }

            ImGui::SameLine(300);
            if (item.sellOrders > 0 && item.buyOrders > 0) {
                ImGui::Text("%d  Seller%s, %d Buyer%s", item.sellOrders, item.sellOrders == 1 ? "" : "s", item.buyOrders, item.buyOrders == 1 ? "" : "s");
            }
            else if (item.sellOrders > 0) {
                ImGui::Text("%d  Seller%s", item.sellOrders, item.sellOrders == 1 ? "" : "s");
            }
            else if (item.buyOrders > 0) {
                ImGui::Text("%d  Buyer%s", item.buyOrders, item.buyOrders == 1 ? "" : "s");
            }
            ImGui::PopID();
        }
    }

    void DrawItemDetails()
    {
        if (current_viewing_item.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select an item");
            return;
        }

        ImGui::Text("Item: %s", current_viewing_item.c_str());

                // Sort mode dropdown
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##sort_mode", order_sort_mode == OrderSortMode::MostRecent ? "Most Recent" : "Cheapest")) {
            if (ImGui::Selectable("Most Recent", order_sort_mode == OrderSortMode::MostRecent)) {
                order_sort_mode = OrderSortMode::MostRecent;
                current_orders_needs_sort = true;
            }
            if (ImGui::Selectable("Currency", order_sort_mode == OrderSortMode::Currency)) {
                order_sort_mode = OrderSortMode::Currency;
                current_orders_needs_sort = true;
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        if (current_item_orders.empty()) {
            ImGui::Text("Loading...");
            return;
        }

        // Sort orders only when data changes or sort mode changes
        if (current_orders_needs_sort) {
            if (order_sort_mode == OrderSortMode::Currency) {
                // Sort by price (cheapest first)
                std::sort(current_item_orders.begin(), current_item_orders.end(), [](const MarketItem& a, const MarketItem& b) {
                    if (a.prices.empty() || b.prices.empty()) return false;
                    return a.prices[0].type < b.prices[0].type;
                });
            }
            else {
                // Sort by most recent
                std::sort(current_item_orders.begin(), current_item_orders.end(), [](const MarketItem& a, const MarketItem& b) {
                    return a.lastRefresh > b.lastRefresh;
                });
            }
            current_orders_needs_sort = false;
        }

        const auto font_size = ImGui::CalcTextSize(" ");
        auto DrawOrder = [font_size](const MarketItem& order) {
            ImGui::PushID(&order);
            const auto top = ImGui::GetCursorPosY();
            ImGui::TextUnformatted(order.player->c_str());
            const auto timetext = TextUtils::RelativeTime(order.lastRefresh);
            ImGui::SameLine();
            ImGui::TextDisabled(timetext.c_str());

            ImGui::Text("Wants to %s %d for ", order.orderType == OrderType::Sell ? "sell" : "buy", order.quantity);

            // NB: Seems to be an array of prices given by the API, but the website only shows the first one?
            const auto& price = order.prices[0];

            ImGui::SameLine(0, 0);
            const auto tex = GetCurrencyImage(price.type);
            if (tex) {
                ImGui::ImageCropped((void*)*tex, {font_size.y, font_size.y});
                ImGui::SameLine(0, 0);
            }
            if (price.price == static_cast<int>(price.price)) {
                ImGui::Text("%.0f %s", price.price, GetPriceTypeString(price.type));
            }
            else {
                ImGui::Text("%.2f %s", price.price, GetPriceTypeString(price.type));
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%.0f %s each)", price.price / order.quantity, GetPriceTypeString(price.type));
            const auto original_cursor_pos = ImGui::GetCursorPos();
            const auto height = original_cursor_pos.y - top;
            
            ImGui::SetCursorPos({ImGui::GetContentRegionAvail().x - 100.f, top + 5.f});
            if (ImGui::Button("Whisper##seller", {100.f, height - 10.f})) {
                GW::GameThread::Enqueue([player = *order.player] {
                    std::wstring name_ws = TextUtils::StringToWString(player);
                    GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, (wchar_t*)name_ws.c_str());
                });
            }
            ImGui::SetCursorPos(original_cursor_pos);

            ImGui::Separator();
            ImGui::PopID();
        };


        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "SELL ORDERS:");
        ImGui::Separator();
        
        for (const auto& order : current_item_orders) {
            if (!(order.orderType == OrderType::Sell && order.quantity && !order.prices.empty())) 
                continue;
            DrawOrder(order);
        }

        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "BUY ORDERS:");
        ImGui::Separator();

        for (const auto& order : current_item_orders) {
            if (!(order.orderType == OrderType::Buy && order.quantity && !order.prices.empty())) 
                continue;
            DrawOrder(order);
        }
    }

    void Refresh() {
        if (!socket_io_ready) return;
        SendGetAvailableOrders();
        //SendGetLastItemsByFamily("all");
    }

} // namespace

void GWMarketWindow::Initialize()
{
    ToolboxWindow::Initialize();

    worker = new std::thread([this] {
        while (!should_stop) {
            std::function<void()> job;
            bool found = false;

            if (!thread_jobs.empty()) {
                job = thread_jobs.front();
                thread_jobs.pop();
                found = true;
            }

            if (found) job();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    ConnectWebSocket(true);
    Log::Log("Market Browser initialized");
}

void GWMarketWindow::Terminate()
{
    should_stop = true;
    if (worker && worker->joinable()) worker->join();
    delete worker;
    DeleteWebSocket(ws);
    ToolboxWindow::Terminate();
}

void GWMarketWindow::Update(float delta)
{
    ToolboxWindow::Update(delta);

    if (ws) {
        ws->poll();
        ws->dispatch([](const std::string& msg) {
            OnWebSocketMessage(msg);
        });

        // Don't send our own pings - just respond to server pings with pongs
        // The server will ping us every 25 seconds

        if (ws->getReadyState() == WebSocket::CLOSED) {
            Log::Warning("Disconnected");
            DeleteWebSocket(ws);
            ws = nullptr;
            socket_io_ready = false;
        }
    }
    else if (!ws_connecting) {
        ConnectWebSocket();
    }

    if (auto_refresh && socket_io_ready) {
        refresh_timer += delta;
        if (refresh_timer >= refresh_interval) {
            refresh_timer = 0.0f;
            Refresh();
        }
    }
}

void GWMarketWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(auto_refresh);
    refresh_interval = static_cast<int>(ini->GetLongValue(Name(), "refresh_interval", 60));
}

void GWMarketWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(auto_refresh);
    ini->SetLongValue(Name(), "refresh_interval", refresh_interval);
}

void GWMarketWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Auto-refresh", &auto_refresh);
    if (auto_refresh) {
        ImGui::SliderInt("Interval (sec)", &refresh_interval, 30, 300);
    }

    ImGui::Separator();

    if (socket_io_ready) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
    }
    else if (ws_connecting) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Connecting...");
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disconnected");
    }

    if (socket_io_ready && ImGui::Button("Refresh")) {
        Refresh();
    }
}

void GWMarketWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (socket_io_ready) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
        }
        else if (ws_connecting) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Connecting...");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disconnected");
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            Refresh();
        }

        ImGui::Separator();

        ImGui::Text("Show:");
        ImGui::SameLine();
        if (ImGui::RadioButton("All", filter_mode == SHOW_ALL)) filter_mode = SHOW_ALL;
        ImGui::SameLine();
        if (ImGui::RadioButton("WTS", filter_mode == SHOW_SELL_ONLY)) filter_mode = SHOW_SELL_ONLY;
        ImGui::SameLine();
        if (ImGui::RadioButton("WTB", filter_mode == SHOW_BUY_ONLY)) filter_mode = SHOW_BUY_ONLY;

        ImGui::Separator();
        ImGui::InputText("Search", search_buffer, sizeof(search_buffer));
        ImGui::Separator();

        ImGui::BeginChild("ItemList", ImVec2(450, 0), true);
        DrawItemList();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ItemDetails", ImVec2(0, 0), true);
        DrawItemDetails();
        ImGui::EndChild();
    }
    ImGui::End();
}
