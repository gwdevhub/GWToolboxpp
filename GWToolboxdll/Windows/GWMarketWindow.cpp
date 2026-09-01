#include "stdafx.h"

#include "GWMarketWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Logger.h>
#include <RestClient.h>
#include <Utils/GuiUtils.h>

#include <Utils/ThreadedWebSocket.h>
#include <glaze/glaze.hpp>


#include <Modules/GwDatModule.h>
#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/GameEntities/Frame.h>
#include <Timer.h>
#include <Utils/TextUtils.h>
#include <algorithm>
#include <sstream>
#include <unordered_set>

#include <Modules/ChatFilter.h>

// 目前商店API不够完善，暂时只支持浏览。
#define GWMARKET_SELLING_ENABLED 0

namespace {
    using json = glz::generic;

    const char* market_host = "gwmarket.net";
    const char* market_name = "GWMarket.net";
    std::string market_uuid = "";


    clock_t last_shop_refresh_sent = 0;

    // 连接速率限制
    constexpr uint32_t COST_PER_CONNECTION_MS = 30 * 1000;
    constexpr uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;

    enum class Currency : uint32_t { Platinum = 0, Ecto = 1, Zkeys = 2, Arms = 3, Count = 4, All = 0xf };

    enum class OrderType : uint8_t { Sell = 0, Buy = 1 };

    enum class OrderSortMode : uint8_t { MostRecent = 0, Currency = 1 };
    Currency order_view_currency = Currency::All;

    const char* GetPriceTypeString(Currency currency)
    {
        switch (currency) {
            case Currency::Platinum:
                return "白金";
            case Currency::Ecto:
                return "伊克托";
            case Currency::Zkeys:
                return "扎基钥匙";
            case Currency::Arms:
                return "臂铠";
            default:
                return "未知";
        }
    }

    IDirect3DTexture9** GetCurrencyImage(Currency currency)
    {
        switch (currency) {
            case Currency::Platinum:
                return GwDatModule::LoadTextureFromFileId(0x1df32);
            case Currency::Ecto:
                return GwDatModule::LoadTextureFromFileId(0x151fa);
            case Currency::Zkeys:
                return GwDatModule::LoadTextureFromFileId(0x52ecf);
            case Currency::Arms:
                return GwDatModule::LoadTextureFromFileId(0x2ae18);
        }
        return nullptr;
    }

    const char* GetOrderTypeString(OrderType type)
    {
        switch (type) {
            case OrderType::Sell:
                return "出售";
            case OrderType::Buy:
                return "求购";
            default:
                return "出售";
        }
    }

    // 字符串池，减少内存占用
    std::unordered_set<std::string> string_pool;

    const std::string* InternString(const std::string& str)
    {
        auto it = string_pool.insert(str);
        return &(*it.first);
    }


    std::string* GetAttributeName(GW::Constants::Attribute attribute)
    {
        const auto attrib_data = GW::SkillbarMgr::GetAttributeConstantData(attribute);
        if (attrib_data) {
            return &Resources::DecodeStringId(attrib_data->name_id, GW::Constants::Language::English)->string();
        }
        return nullptr;
    }

    GW::Constants::Attribute AttributeFromString(const std::string& str)
    {
        using namespace GW::Constants;
        // 幻术师
        if (str == "Fast Casting") return Attribute::FastCasting;
        if (str == "Illusion Magic") return Attribute::IllusionMagic;
        if (str == "Domination Magic") return Attribute::DominationMagic;
        if (str == "Inspiration Magic") return Attribute::InspirationMagic;

        // 死灵法师
        if (str == "Blood Magic") return Attribute::BloodMagic;
        if (str == "Death Magic") return Attribute::DeathMagic;
        if (str == "Soul Reaping") return Attribute::SoulReaping;
        if (str == "Curses") return Attribute::Curses;

        // 元素使
        if (str == "Air Magic") return Attribute::AirMagic;
        if (str == "Earth Magic") return Attribute::EarthMagic;
        if (str == "Fire Magic") return Attribute::FireMagic;
        if (str == "Water Magic") return Attribute::WaterMagic;
        if (str == "Energy Storage") return Attribute::EnergyStorage;

        // 僧侣
        if (str == "Healing Prayers") return Attribute::HealingPrayers;
        if (str == "Smiting Prayers") return Attribute::SmitingPrayers;
        if (str == "Protection Prayers") return Attribute::ProtectionPrayers;
        if (str == "Divine Favor") return Attribute::DivineFavor;

        // 战士
        if (str == "Strength") return Attribute::Strength;
        if (str == "Axe Mastery") return Attribute::AxeMastery;
        if (str == "Hammer Mastery") return Attribute::HammerMastery;
        if (str == "Swordsmanship") return Attribute::Swordsmanship;
        if (str == "Tactics") return Attribute::Tactics;

        // 游侠
        if (str == "Beast Mastery") return Attribute::BeastMastery;
        if (str == "Expertise") return Attribute::Expertise;
        if (str == "Wilderness Survival") return Attribute::WildernessSurvival;
        if (str == "Marksmanship") return Attribute::Marksmanship;

        // 刺客
        if (str == "Dagger Mastery") return Attribute::DaggerMastery;
        if (str == "Deadly Arts") return Attribute::DeadlyArts;
        if (str == "Shadow Arts") return Attribute::ShadowArts;
        if (str == "Critical Strikes") return Attribute::CriticalStrikes;

        // 祭祀
        if (str == "Communing") return Attribute::Communing;
        if (str == "Restoration Magic") return Attribute::RestorationMagic;
        if (str == "Channeling Magic") return Attribute::ChannelingMagic;
        if (str == "Spawning Power") return Attribute::SpawningPower;

        // 圣言者
        if (str == "Spear Mastery") return Attribute::SpearMastery;
        if (str == "Command") return Attribute::Command;
        if (str == "Motivation") return Attribute::Motivation;
        if (str == "Leadership") return Attribute::Leadership;

        // 神唤使
        if (str == "Scythe Mastery") return Attribute::ScytheMastery;
        if (str == "Wind Prayers") return Attribute::WindPrayers;
        if (str == "Earth Prayers") return Attribute::EarthPrayers;
        if (str == "Mysticism") return Attribute::Mysticism;

        return Attribute::None;
    }

    struct Price {
        Currency type = Currency::Platinum;
        float quantity = 1.f;
        float price = 5.f;

        static Price FromJson(const json& j)
        {
            Price p;
            p.type = static_cast<Currency>(TextUtils::parseIntFromJson(j, "type", 0));
            p.quantity = (float)TextUtils::parseIntFromJson(j, "quantity", 0);
            p.quantity = TextUtils::parseFloatFromJson(j, "unit", p.quantity);
            p.price = TextUtils::parseFloatFromJson(j, "price", 0.f);
            return p;
        }
        json ToJson() const
        {
            json j;
            if (!valid()) return j;
            j["type"] = static_cast<int>(type);
            j["quantity"] = quantity;
            j["unit"] = quantity;
            j["price"] = price;
            return j;
        }
        bool valid() const { return quantity && price; }
    };
    struct WeaponDetails {
        // "weaponDetails":{"attribute":"Tactics","requirement":9,"inscription":true,"oldschool":false,"core":null,"prefix":null,"suffix":null},
        GW::Constants::Attribute attribute = GW::Constants::Attribute::None;
        uint8_t requirement = 0;
        bool inscribable = false;
        bool oldschool = false;
        static WeaponDetails FromJson(const json& j)
        {
            WeaponDetails p;
            p.attribute = AttributeFromString(TextUtils::parseStringFromJson(j, "attribute", ""));



            p.requirement = TextUtils::parseIntFromJson(j, "requirement", 0) & 0xf;
            p.inscribable = TextUtils::parseBoolFromJson(j, "inscription", false);
            return p;
        }

        std::string toString() const
        {
            std::string out;
            const auto attrib_data = GW::SkillbarMgr::GetAttributeConstantData(attribute);
            if (attrib_data) {
                out += std::format("需求{} {}", requirement, Resources::DecodeStringId(attrib_data->name_id, GW::Constants::Language::English)->string().c_str());
            }
            if (inscribable) {
                if (!out.empty()) out += ", ";
                out += "可铭文";
            }
            return out;
        }
        bool valid() const { return GetAttributeName(attribute) != nullptr; }
        json ToJson() const
        {
            if (!valid()) return {};
            json j;
            j["attribute"] = *GetAttributeName(attribute);
            j["requirement"] = requirement;
            j["inscription"] = inscribable;
            return j;
        }
    };

    struct MarketItem {
        std::string name;
        std::string player;
        OrderType orderType;
        int quantity;
        std::vector<Price> prices;
        time_t lastRefresh = 0;
        WeaponDetails weaponDetails;
        std::string description;
        float price_per() const { return prices.empty() ? 0.f : prices[0].price / quantity; }
        Currency currency() const { return prices.empty() ? Currency::All : prices[0].type; }

        bool valid() const { return !prices.empty() && lastRefresh && !name.empty(); }

        static MarketItem FromJson(const json& j)
        {
            MarketItem item;
            if (!j.is_object()) return item;

            item.name = TextUtils::parseStringFromJson(j, "name", "");
            item.player = TextUtils::parseStringFromJson(j, "player", "");
            item.description = TextUtils::parseStringFromJson(j, "description", "");
            item.orderType = static_cast<OrderType>(TextUtils::parseIntFromJson(j, "orderType", 0));
            item.quantity = TextUtils::parseIntFromJson(j, "quantity", 0);

            if (j.contains("weaponDetails") && j["weaponDetails"].is_object()) {
                item.weaponDetails = WeaponDetails::FromJson(j["weaponDetails"]);
            }

            uint64_t lastRefresh_ms = TextUtils::parseUint64FromJson(j, "lastRefresh", 0ULL);
            item.lastRefresh = lastRefresh_ms ? lastRefresh_ms / 1000 : 0;

            if (j.contains("prices") && j["prices"].is_array()) {
                for (const auto& price_json : j["prices"].get_array()) {
                    auto p = Price::FromJson(price_json);
                    if (p.valid()) item.prices.push_back(p);
                }
            }

            return item;
        }
        json ToJson() const
        {
            json j;
            if (!valid()) return j;
            j["name"] = name;
            j["player"] = player;
            j["description"] = description;
            j["orderType"] = static_cast<int>(orderType);
            j["quantity"] = quantity;
            j["lastRefresh"] = static_cast<uint64_t>(lastRefresh) * 1000; // 转换为毫秒

            if (has_weapon_details()) {
                j["weaponDetails"] = weaponDetails.ToJson();
            }

            j["prices"] = json::array_t{};
            for (const auto& price : prices) {
                j["prices"].get_array().push_back(price.ToJson());
            }

            return j;
        }

        bool has_weapon_details() const { return weaponDetails.attribute != GW::Constants::Attribute::None; }
    };

    bool collapsed = false;
    bool show_my_shop_window = false;
    bool show_edit_item_window = false;
    size_t editing_item_index = 0;

    std::vector<MarketItem> edit_window_matching_orders;
    std::string edit_window_matching_item_name;
    bool edit_window_orders_needs_sort = true;

    struct PendingPurchaseAnalytic {
        std::string player_name;
        std::string item_name;
        OrderType order_type = OrderType::Sell;
        Price price;
        clock_t timestamp = 0;

        bool IsActive() const
        {
            return !player_name.empty() && timestamp &&
                   TIMER_DIFF(timestamp) < (60 * CLOCKS_PER_SEC);
        }

        void Clear() { player_name.clear(); timestamp = 0; }
    } pending_purchase_analytic;

    struct ShopItem : MarketItem {
        bool hidden = false;
        bool dedicated = false;
        bool pre = false;
        ShopItem() {}
        ShopItem(const MarketItem& item) : MarketItem(item) {}
        json ToJson() const
        {
            json j = MarketItem::ToJson();
            return j;
        }
        static ShopItem FromJson(const json& j)
        {
            ShopItem item = MarketItem::FromJson(j);
            if (j.contains("orderDetails") && j["orderDetails"].is_object()) {
                const auto& od = j["orderDetails"];
                item.dedicated = TextUtils::parseBoolFromJson(od, "dedicated", false);
                item.pre = TextUtils::parseBoolFromJson(od, "pre", false);
            }
            return item;
        }
    };

    struct MarketShop {
        std::string player;
        std::string uuid;
        std::vector<ShopItem> items;
        std::vector<std::string> certified;
        time_t lastRefresh = 0;

        bool is_certified(const std::string& _player) { return std::ranges::find(certified.begin(), certified.end(), _player) != certified.end(); }

        static MarketShop FromJson(const json& j)
        {
            MarketShop shop;
            if (!j.is_object()) return shop;

            shop.player = TextUtils::parseStringFromJson(j, "player", "");
            shop.uuid = TextUtils::parseStringFromJson(j, "uuid", "");

            uint64_t lastRefresh_ms = TextUtils::parseUint64FromJson(j, "lastRefresh", 0ULL);
            shop.lastRefresh = lastRefresh_ms ? lastRefresh_ms / 1000 : 0;

            if (j.contains("items") && j["items"].is_array()) {
                for (const auto& item_json : j["items"].get_array()) {
                    auto item = ShopItem::FromJson(item_json);
                    if (item.valid()) {
                        shop.items.push_back(item);
                    }
                }
            }
            if (j.contains("certified") && j["certified"].is_array()) {
                for (const auto& player_name : j["certified"].get_array()) {
                    if (player_name.is_string()) shop.certified.push_back(player_name.get<std::string>());
                }
            }

            return shop;
        }

        json ToJson() const
        {
            json j;
            if (!valid()) return j;
            j["player"] = player;
            j["uuid"] = uuid;
            j["lastRefresh"] = (uint64_t)(lastRefresh * 1000);
            j["authCertified"] = certified.size() > 0;
            json::array_t certified_json;
            for (auto& player_name : certified) {
                certified_json.push_back(player_name);
            }
            j["certified"] = certified_json;
            j["daybreakOnline"] = true;
            json::array_t items_json;
            for (auto& item : items) {
                items_json.push_back(item.ToJson());
            }
            j["items"] = items_json;
            return j;
        }
        bool valid() const { return !uuid.empty(); }
    };
    MarketShop my_shop;

    struct AvailableItem {
        const std::string* name;
        int sellOrders = 0;
        int buyOrders = 0;
    };

    GWMarketWindow::Settings settings;

    ThreadedWebSocket market_ws;

    std::vector<AvailableItem> available_items;
    std::vector<MarketItem> last_items;
    std::vector<MarketItem> current_item_orders;
    std::string current_viewing_item;
    std::string pending_search_item;
    std::map<std::string, AvailableItem> favorite_items;

    char search_buffer[256] = "";
    enum FilterMode { SHOW_ALL, SHOW_SELL_ONLY, SHOW_BUY_ONLY };
    FilterMode filter_mode = SHOW_ALL;
    float refresh_timer = 0.0f;

    bool socket_io_ready = false;
    clock_t last_ping_time = 0;
    int ping_interval = 25000;
    int ping_timeout = 20000;

    OrderSortMode order_sort_mode = OrderSortMode::MostRecent;
    bool available_items_needs_sort = true;
    bool current_orders_needs_sort = true;

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
    void InitWebSocket();
    void Disconnect(bool blocking = false);
    void DrawItemList();
    void DrawFavoritesList();
    void DrawItemDetails();
    void DrawEditWindowMatchingOrders();
    void Refresh();
    void OnShopCertificationSecret(const json& data);
    void OnMyShopInfo(const json& data);
    void CloseShop(const MarketShop& shop);
    void MigrateShop(const MarketShop& from, MarketShop& to);
    void SaveShop(const MarketShop& shop, bool force = false);

    std::wstring GetGWMarketItemName(InventoryManager::Item* item, const std::wstring& complete_name_decoded, const std::wstring& name_decoded, const std::wstring&)
    {
        std::wstring out = name_decoded;
        if (item->type == GW::Constants::ItemType::Rune_Mod) {
            out = complete_name_decoded;
        }

        if (item->IsInscription()) {
            // 铭文: "Leaf on the Wind" => Leaf on the Wind
            out = TextUtils::Replace(out, LR"(Inscription: \"([^\"]+)\")", L"$1");
        }
        if (item->type == GW::Constants::ItemType::Dye) {
            // 染料瓶 => <颜色> 染料
            const auto dye_color = TextUtils::Replace(complete_name_decoded, LR"(.*\[([^\]]+)\].*)", L"$1");
            out = dye_color + L" 染料";
        }

        return TextUtils::StripTags(out);
    }

    std::wstring GetGWMarketItemDescription(InventoryManager::Item* item, const std::wstring&, const std::wstring&, const std::wstring& description_decoded)
    {
        std::wstring description;
        if (item->IsWeapon()) {
            description = description_decoded;

            std::vector<std::wstring> parts;

            // 去除无关描述，只保留伤害、需求和护甲信息
            parts.push_back(TextUtils::Replace(description_decoded, LR"(^(?!.*(Dmg:|Damage:|\(Requires \d|Armor:)).*$)", L""));
            // 移除 @ItemBasic 或 @ItemDull 行
            parts.push_back(TextUtils::Replace(description_decoded, LR"(^<c=@Item(Basic|Dull).*$\n?)", L""));

            std::vector<std::wstring> extras;
            if (item->IsOldSchool()) {
                extras.push_back(L"旧式");
            }
            if (item->GetIsInscribable()) {
                extras.push_back(L"可铭文");
            }
            if (!extras.empty()) {
                parts.push_back(TextUtils::Join(extras, L", "));
            }
            description = TextUtils::Join(parts, L"\n");
        }
        if (item->type == GW::Constants::ItemType::Rune_Mod) {
            description = description_decoded;

            // 移除 @ItemBasic 或 @ItemDull 行
            description = TextUtils::Replace(description, LR"(^<c=@Item(Basic|Dull).*$\n?)", L"");
        }
        // 移除铭文详情
        description = TextUtils::Replace(description, LR"(^.*Inscription:.*$)", L"");
        description = TextUtils::Replace(description, LR"(\n+)", L"\n");
        return TextUtils::StripTags(description);
    }



    std::string GetCurrentPlayerName()
    {
        auto c = GW::GetCharContext();
        if (c && c->player_name && *c->player_name) return TextUtils::WStringToString(c->player_name);
        return "";
    }


    bool IsSocketIOReady()
    {
        return market_ws.IsReady() && socket_io_ready;
    }

    std::string EncodeSocketIOMessage(const std::string& event, const std::string& data = "")
    {
        json msg = json::array_t{};
        msg.get_array().push_back(event);
        if (!data.empty()) {
            msg.get_array().push_back(data);
        }
        return "42" + glz::write_json(msg).value_or(std::string{});
    }

    std::string EncodeSocketIOMessage(const std::string& event, const json& data)
    {
        json msg = json::array_t{};
        msg.get_array().push_back(event);
        msg.get_array().push_back(data);
        return "42" + glz::write_json(msg).value_or(std::string{});
    }

    bool ParseSocketIOMessage(const std::string& message, std::string& event, json& data)
    {
        if (message.length() < 2 || message.substr(0, 2) != "42") return false;

        json parsed;
        if (auto ec = glz::read_json(parsed, message.substr(2)); ec) return false;
        if (parsed.is_array() && parsed.size() >= 1) {
            if (!parsed[0].is_string()) return false;
            event = parsed[0].get<std::string>();
            if (parsed.size() >= 2) {
                data = parsed[1];
                return true;
            }
        }
        return true;
    }

    void OnGetAvailableOrders(const json& orders)
    {
        available_items.clear();
        if (!orders.is_object()) {
            available_items_needs_sort = true;
            Log::Log("收到 0 个可用物品");
            return;
        }
        available_items.reserve(orders.size());
        for (auto& i : favorite_items) {
            i.second = {};
        }
        for (const auto& [key, j] : orders.get_object()) {
            AvailableItem item;
            item.name = InternString(key);
            item.sellOrders = TextUtils::parseIntFromJson(j, "sellWeek", 0);
            item.buyOrders = TextUtils::parseIntFromJson(j, "buyWeek", 0);
            available_items.push_back(item);
            if (favorite_items.contains(*item.name)) {
                favorite_items[*item.name] = item;
            }
        }
        available_items_needs_sort = true;
        Log::Log("收到 %zu 个可用物品", available_items.size());
    }

    void OnGetLastItems(const json& items)
    {
        last_items.clear();
        if (items.is_array()) {
            last_items.reserve(items.size());
            for (const auto& item_json : items.get_array()) {
                last_items.push_back(MarketItem::FromJson(item_json));
            }
        }
        Log::Log("收到 %zu 条近期列表", last_items.size());
    }

    void OnGetItemOrders(const json& orders)
    {
        std::vector<MarketItem> _orders;
        if (orders.is_array()) {
            _orders.reserve(orders.size());
            for (const auto& order_json : orders.get_array()) {
                _orders.push_back(MarketItem::FromJson(order_json));
            }
        }

        if (!_orders.empty()) {
            const auto& item_name = _orders[0].name;

            if (item_name == current_viewing_item) {
                current_item_orders = _orders;
                current_orders_needs_sort = true;
            }

            if (item_name == edit_window_matching_item_name) {
                edit_window_matching_orders = _orders;
                edit_window_orders_needs_sort = true;
            }
        }

        Log::Log("收到 %zu 条订单", _orders.size());
    }

    void SendAskForShop(const std::string& uuid)
    {
        if (!IsSocketIOReady()) return;
        std::string msg = EncodeSocketIOMessage("getPublicShop", uuid);
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }
    void OnShopInfo(const json& data)
    {
        const auto shop = MarketShop::FromJson(data);

        if (shop.uuid == my_shop.uuid) {
            MigrateShop(shop, my_shop);
        }
    }

    void OnMyShopInfo(const json& data)
    {
        auto shop = MarketShop::FromJson(data);
        const auto old_uuid = my_shop.uuid;
        MigrateShop(shop, my_shop);
        if (!old_uuid.empty() && old_uuid != my_shop.uuid) {
            CloseShop(shop);
            SaveShop(my_shop, true);
        }
    }
    void OnShopError(const json& data)
    {
        if (data.is_string()) {
            const auto warning = std::format("[市场] {}", data.get<std::string>());
            Log::Warning("%s", warning.c_str());
        }
    }

    void HandleSocketIOHandshake(const std::string& message)
    {
        if (message[0] != '0') return;

        json handshake;
        if (auto ec = glz::read_json(handshake, message.substr(1)); ec) return;
        if (handshake.contains("pingInterval") && handshake["pingInterval"].is_number()) {
            ping_interval = static_cast<int>(handshake["pingInterval"].get<double>());
        }
        if (handshake.contains("pingTimeout") && handshake["pingTimeout"].is_number()) {
            ping_timeout = static_cast<int>(handshake["pingTimeout"].get<double>());
        }

        Log::Log("握手: ping %dms, 超时 %dms", ping_interval, ping_timeout);

        market_ws.Send("40");
        Log::Log("[发送] 40 (连接到命名空间)");

        last_ping_time = clock();
    }

    void OnNamespaceConnected()
    {
        Log::Log("命名空间已连接，可以发送事件");
        socket_io_ready = true;

        SendSocketStarted();
        Refresh();

        if (!pending_search_item.empty()) {
            SendGetItemOrders(pending_search_item);
            pending_search_item.clear();
        }
    }

    void OnWebSocketMessage(const std::string& message)
    {
        if (message.empty()) return;

        Log::Log("[接收] %s", message.c_str());

        char type = message[0];

        switch (type) {
            case '0':
                HandleSocketIOHandshake(message);
                break;
            case '1':
                Log::Warning("服务器关闭");
                break;
            case '2':
                market_ws.Send("3");
                Log::Log("[发送] 3");
                break;
            case '3':
                Log::Log("收到Pong");
                break;
            case '4':
                if (message.length() > 1) {
                    if (message[1] == '0') {
                        Log::Log("命名空间连接确认");
                        OnNamespaceConnected();
                    }
                    else if (message[1] == '2') {
                        std::string event;
                        json data;
                        if (!ParseSocketIOMessage(message, event, data)) break;

                        if (event == "GetAvailableOrders")
                            OnGetAvailableOrders(data);
                        else if (event == "GetLastItems")
                            OnGetLastItems(data);
                        else if (event == "GetItemOrders")
                            OnGetItemOrders(data);
                        else if (event == "ShopCertificationSecret")
                            OnShopCertificationSecret(data);
                        else if (event == "RefreshShop")
                            OnMyShopInfo(data);
                        else if (event == "ToasterError")
                            OnShopError(data);
                    }
                }
                break;
        }
    }

    void SendSocketStarted()
    {
        if (!IsSocketIOReady()) return;
        std::string msg = EncodeSocketIOMessage("SocketStarted");
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }

    void SendGetAvailableOrders()
    {
        if (!IsSocketIOReady()) return;
        std::string msg = EncodeSocketIOMessage("getAvailableOrders");
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }

    void SendGetLastItemsByFamily(const std::string& family)
    {
        if (!IsSocketIOReady()) return;
        std::string msg = EncodeSocketIOMessage("getLastItemsByFamily", family);
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }

    void SendAskForCertification()
    {
        if (!IsSocketIOReady()) return;


        std::string msg = EncodeSocketIOMessage("askPlayerCertification", my_shop.uuid);
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }
    void OnShopCertificationSecret(const json& data)
    {
        const auto secret = TextUtils::parseStringFromJson(data, "secret", "");
        const auto uuid = TextUtils::parseStringFromJson(data, "uuid", "");
        if (secret.empty() || uuid.empty()) {
            Log::Warning("OnShopCertificationSecret: uuid 或 secret 为空！");
            return;
        }
        const auto msg = TextUtils::StringToWString(std::format("{}|{}", uuid, secret));
        GW::GameThread::Enqueue([cpy = msg]() {
            ChatFilter::BlockMessageForMs(L"Gwmarket 认证",500);
            GW::Chat::SendChat(L"Gwmarket 认证", cpy.c_str());
        });
    }

    void SendGetItemOrders(const std::string& item_name)
    {
        if (!IsSocketIOReady()) return;
        std::string msg = EncodeSocketIOMessage("getItemOrders", item_name);
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }

    void SendPing()
    {
        market_ws.Send("2");
        last_ping_time = clock();
        Log::Log("[发送] 2");
    }

    void MigrateShop(const MarketShop& from, MarketShop& to)
    {
        for (const auto& item : from.items) {
            bool found = false;
            for (const auto& to_item : to.items) {
                if (to_item.name == item.name && to_item.description == item.description) {
                    found = true;
                    break;
                }
            }
            if (!found) to.items.push_back(item);
        }
        to.uuid = from.uuid;
        to.player = from.player;
        to.certified = from.certified;
    }

    void SaveShop(const MarketShop& shop, bool force)
    {
        if (!IsSocketIOReady()) return;
        if (!force && TIMER_DIFF(last_shop_refresh_sent) < 300 * 1000) return;
        if (!shop.valid()) return;
        last_shop_refresh_sent = TIMER_INIT();
        json data = shop.ToJson();

        std::string msg = EncodeSocketIOMessage("refreshShop", data);
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }
    void DeleteShop(MarketShop& shop)
    {
        if (!shop.valid()) return;
        if (shop.items.empty()) return;
        shop.items = {};
        SaveShop(shop, true);
        if (shop.uuid != my_shop.uuid && my_shop.valid()) SaveShop(my_shop, true);
    }
    void CloseShop(const MarketShop& shop)
    {
        if (!IsSocketIOReady() || shop.uuid.empty()) return;
        std::string msg = EncodeSocketIOMessage("closeShop", shop.uuid);
        market_ws.Send(msg);
        Log::Log("[发送] %s", msg.c_str());
    }

    void InitWebSocket()
    {
        market_ws.SetUrl(std::format("wss://{}/socket.io/?EIO=4&transport=websocket", market_host));
        market_ws.SetReconnectCost(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS);
        market_ws.SetOnMessage([](const std::string& msg) {
            OnWebSocketMessage(msg);
        });
        market_ws.SetOnOpen([] {
            //Log::Log("已连接");
            socket_io_ready = false;
            last_ping_time = clock();
        });
        market_ws.SetOnClose([] {
            //Log::Warning("已断开");
            socket_io_ready = false;
        });
    }

    void Disconnect(bool blocking)
    {
        CloseShop(my_shop);
        market_ws.Disconnect(blocking);
        socket_io_ready = false;
    }

    void DrawItemList()
    {
        ImGui::Text("可用列表 (%zu)", available_items.size());
        ImGui::Separator();
        if (available_items_needs_sort) {
            std::sort(available_items.begin(), available_items.end(), [](const AvailableItem& a, const AvailableItem& b) {
                return *a.name < *b.name;
            });

            available_items_needs_sort = false;
        }

        for (const auto& item : available_items) {
            if (filter_mode == SHOW_SELL_ONLY && item.sellOrders == 0) continue;
            if (filter_mode == SHOW_BUY_ONLY && item.buyOrders == 0) continue;

            if (search_buffer[0] != '\0') {
                std::string search_lower = search_buffer;
                std::string name_lower = *item.name;
                std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                if (name_lower.find(search_lower) == std::string::npos) continue;
            }

            ImGui::PushID(item.name->c_str());

            bool selected = (current_viewing_item == *item.name);
            if (ImGui::Selectable(item.name->c_str(), selected)) {
                current_viewing_item = *item.name;
                SendGetItemOrders(*item.name);
            }

            ImGui::SameLine(300);
            if (item.sellOrders > 0 && item.buyOrders > 0) {
                ImGui::Text("%d 个卖家, %d 个买家", item.sellOrders, item.buyOrders);
            }
            else if (item.sellOrders > 0) {
                ImGui::Text("%d 个卖家", item.sellOrders);
            }
            else if (item.buyOrders > 0) {
                ImGui::Text("%d 个买家", item.buyOrders);
            }
            ImGui::PopID();
        }
    }

    void DrawFavoritesList()
    {
        if (favorite_items.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "没有收藏");
            return;
        }

        for (const auto& favorite : favorite_items) {
            ImGui::PushID(favorite.first.c_str());

            bool selected = current_viewing_item == favorite.first;
            if (ImGui::Selectable(favorite.first.c_str(), selected)) {
                current_viewing_item = favorite.first;
                SendGetItemOrders(favorite.first);
            }

            ImGui::SameLine(300);
            const auto& item = favorite.second;
            if (item.sellOrders > 0 && item.buyOrders > 0) {
                ImGui::Text("%d 个卖家, %d 个买家", item.sellOrders, item.buyOrders);
            }
            else if (item.sellOrders > 0) {
                ImGui::Text("%d 个卖家", item.sellOrders);
            }
            else if (item.buyOrders > 0) {
                ImGui::Text("%d 个买家", item.buyOrders);
            }

            ImGui::PopID();
        }
    }
    void ToggleFavourite(const std::string& item_name, bool is_favorite)
    {
        if (item_name.empty()) return;
        if (!is_favorite) {
            favorite_items.erase(item_name);
        }
        else {
            const auto found = std::ranges::find_if(available_items.begin(), available_items.end(), [item_name](auto& item) {
                return *item.name == item_name;
            });
            favorite_items[item_name] = {};
            if (found != available_items.end()) {
                favorite_items[item_name] = *found;
            }
        }
    }
    void DrawItemDetails()
    {
        if (current_viewing_item.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "选择一个物品");
            return;
        }

        ImGui::Text("物品: %s", current_viewing_item.c_str());

        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##sort_mode", order_sort_mode == OrderSortMode::MostRecent ? "最新" : "货币")) {
            if (ImGui::Selectable("最新", order_sort_mode == OrderSortMode::MostRecent)) {
                order_sort_mode = OrderSortMode::MostRecent;
                current_orders_needs_sort = true;
            }
            if (ImGui::Selectable("货币", order_sort_mode == OrderSortMode::Currency)) {
                order_sort_mode = OrderSortMode::Currency;
                current_orders_needs_sort = true;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##view_currency", order_view_currency == Currency::All ? "全部" : GetPriceTypeString(order_view_currency))) {
            if (ImGui::Selectable("全部", order_view_currency == Currency::All)) {
                order_view_currency = Currency::All;
            }
            for (uint8_t i = 0; i < (uint8_t)Currency::Count; i++) {
                auto c = (Currency)i;
                if (ImGui::Selectable(GetPriceTypeString(c), order_view_currency == c)) {
                    order_view_currency = c;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        if (!current_viewing_item.empty()) {
            bool is_favorite = favorite_items.contains(current_viewing_item);
            std::string fav_label = std::format("{} {}", ICON_FA_STAR, is_favorite ? "取消收藏" : "收藏");
            if (ImGui::Button(fav_label.c_str())) {
                ToggleFavourite(current_viewing_item, !is_favorite);
            }
            ImGui::SameLine();
            std::string url = std::format("https://{}/item/{}", market_host, TextUtils::UrlEncode(current_viewing_item, ' '));
            auto btn_label = std::format("{} {}", ICON_FA_GLOBE, market_name);
            if (ImGui::Button(btn_label.c_str())) {
                GW::GameThread::Enqueue([url]() {
                    GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)url.c_str());
                });
            }
            ImGui::SameLine();
            btn_label = std::format("{} 激战Wiki", ICON_FA_GLOBE);
            if (ImGui::Button(btn_label.c_str())) {
                GW::GameThread::Enqueue([]() {
                    auto url = GuiUtils::WikiUrl(current_viewing_item);
                    GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)url.c_str());
                });
            }
            ImGui::Separator();
        }

        if (current_item_orders.empty()) {
            ImGui::Text("加载中...");
            return;
        }

        if (current_orders_needs_sort) {
            if (order_sort_mode == OrderSortMode::Currency) {
                std::sort(current_item_orders.begin(), current_item_orders.end(), [](const MarketItem& a, const MarketItem& b) {
                    if (a.prices.empty() || b.prices.empty()) return false;
                    if (a.currency() != b.currency()) return a.currency() < b.currency();
                    return a.price_per() < b.price_per();
                });
            }
            else {
                std::sort(current_item_orders.begin(), current_item_orders.end(), [](const MarketItem& a, const MarketItem& b) {
                    return a.lastRefresh > b.lastRefresh;
                });
            }
            current_orders_needs_sort = false;
        }

        const auto font_size = ImGui::CalcTextSize(" ");
        auto DrawOrder = [font_size](const MarketItem& order) {
            // 注意：API 似乎返回一个价格数组，但网站只显示第一个？
            const auto& price = order.prices[0];
            if (order_view_currency != Currency::All && order_view_currency != price.type) return;

            // Use the order's address for a unique ID; descriptions are often empty and would collide,
            // making the per-row "Whisper" buttons share an ImGui ID.
            ImGui::PushID(&order);
            const auto top = ImGui::GetCursorPosY();
            ImGui::TextUnformatted(order.player.c_str());
            const auto timetext = TextUtils::RelativeTime(order.lastRefresh);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", timetext.c_str());
            if (order.has_weapon_details()) {
                ImGui::TextUnformatted(order.weaponDetails.toString().c_str());
            }
            if (!order.description.empty()) {
                ImGui::TextUnformatted(order.description.c_str());
            }

            ImGui::Text("想要 %s %d 个，价格 ", order.orderType == OrderType::Sell ? "出售" : "求购", order.quantity);

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

            const auto price_per = order.price_per();
            int decimal_places = GuiUtils::DecimalPlaces(price_per);
            char fmt[32];
            snprintf(fmt, sizeof(fmt), "(%% .%df %s 每个)", decimal_places, GetPriceTypeString(price.type));
            ImGui::TextDisabled(fmt, price_per);

            const auto original_cursor_pos = ImGui::GetCursorPos();

            ImGui::SetCursorPos({ImGui::GetContentRegionAvail().x - 100.f, top + 5.f});
            if (ImGui::Button("私聊##seller", {100.f, 0.f})) {
                if (!order.prices.empty()) {
                    pending_purchase_analytic.player_name = order.player;
                    pending_purchase_analytic.item_name = order.name;
                    pending_purchase_analytic.order_type = order.orderType;
                    pending_purchase_analytic.price = order.prices[0];
                    pending_purchase_analytic.timestamp = TIMER_INIT();
                }
                auto cpy = new MarketItem();
                *cpy = order;
                GW::GameThread::Enqueue([cpy] {
                    std::wstring name_ws = TextUtils::StringToWString(cpy->player);
                    std::wstring item_ws = TextUtils::StringToWString(cpy->name);

                    GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, (wchar_t*)name_ws.c_str());
                    
                    std::wstring message;
                    if (cpy->orderType == OrderType::Buy) {
                        message = std::format(L"你好，你还在寻找 {} 吗？", item_ws.c_str());
                    }
                    else {
                        message = std::format(L"你好，你还有 {} 出售吗？", item_ws.c_str());
                    }
                    GW::UI::SendUIMessage(GW::UI::UIMessage::kAppendMessageToChat, (void*)message.c_str());
                    delete cpy;
                });
            }
            ImGui::SetCursorPos(original_cursor_pos);

            ImGui::Separator();
            ImGui::PopID();
        };


        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "出售订单:");
        ImGui::Separator();

        for (const auto& order : current_item_orders) {
            if (order.orderType == OrderType::Sell && order.valid()) DrawOrder(order);
        }

        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "求购订单:");
        ImGui::Separator();

        for (const auto& order : current_item_orders) {
            if (order.orderType == OrderType::Buy && order.valid()) DrawOrder(order);
        }
    }

    void DrawEditWindowMatchingOrders()
    {
        if (edit_window_matching_item_name.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "输入物品名称以查看市场价格");
            return;
        }

        ImGui::Text("市场价格:");
        ImGui::TextWrapped("%s", edit_window_matching_item_name.c_str());
        ImGui::Separator();

        if (edit_window_matching_orders.empty()) {
            ImGui::Text("加载中...");
            return;
        }

        if (edit_window_orders_needs_sort) {
            std::sort(edit_window_matching_orders.begin(), edit_window_matching_orders.end(), [](const MarketItem& a, const MarketItem& b) {
                if (a.prices.empty() || b.prices.empty()) return false;
                if (a.currency() != b.currency()) return a.currency() < b.currency();
                return a.price_per() < b.price_per();
            });
            edit_window_orders_needs_sort = false;
        }

        const auto font_size = ImGui::CalcTextSize(" ");
        auto DrawOrder = [font_size](const MarketItem& order) {
            if (order.prices.empty()) return;

            const auto& price = order.prices[0];

            // Use the order's address for a unique ID; descriptions are often empty and would collide.
            ImGui::PushID(&order);
            // const auto top = ImGui::GetCursorPosY();

            ImGui::TextUnformatted(order.player.c_str());
            const auto timetext = TextUtils::RelativeTime(order.lastRefresh);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", timetext.c_str());

            if (order.has_weapon_details()) {
                ImGui::TextUnformatted(order.weaponDetails.toString().c_str());
            }

            if (!order.description.empty()) {
                ImGui::TextUnformatted(order.description.c_str());
            }

            ImGui::Text("Wants to %s %d for ", order.orderType == OrderType::Sell ? "sell" : "buy", order.quantity);

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
            const auto price_per = order.price_per();
            ImGui::TextDisabled(price_per == static_cast<int>(price_per) ? "(%.0f %s 每个)" : "(%.1f %s 每个)", price_per, GetPriceTypeString(price.type));

            ImGui::Separator();
            ImGui::PopID();
        };

        bool has_sell_orders = false;
        for (const auto& order : edit_window_matching_orders) {
            if (order.orderType == OrderType::Sell && order.valid()) {
                if (!has_sell_orders) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "出售订单:");
                    ImGui::Separator();
                    has_sell_orders = true;
                }
                DrawOrder(order);
            }
        }

        bool has_buy_orders = false;
        for (const auto& order : edit_window_matching_orders) {
            if (order.orderType == OrderType::Buy && order.valid()) {
                if (!has_buy_orders) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "求购订单:");
                    ImGui::Separator();
                    has_buy_orders = true;
                }
                DrawOrder(order);
            }
        }

        if (!has_sell_orders && !has_buy_orders) {
            ImGui::TextDisabled("没有有效订单");
        }
    }

    void Refresh()
    {
        if (!IsSocketIOReady()) return;
        SendGetAvailableOrders();
        SaveShop(my_shop, true);
    }



    struct EditingShopItem {
        char name_buffer[256] = {0};
        char description_buffer[512] = {0};
        ShopItem item;
        Price price;


        void Reset()
        {
            memset(name_buffer, 0, sizeof(name_buffer));
            memset(description_buffer, 0, sizeof(description_buffer));
            item = {};
            price = {};
        }

        void LoadFrom(const ShopItem& _item)
        {
            item = _item;
            price = {};
            if (item.prices.size()) {
                price = item.prices[0];
            }

            strncpy(name_buffer, item.name.c_str(), sizeof(name_buffer) - 1);
            strncpy(description_buffer, item.description.c_str(), sizeof(description_buffer) - 1);
        }
    } editing_item;

    bool ShouldConnect()
    {
        return show_my_shop_window || (GWMarketWindow::Instance().visible && !collapsed);
    }

    void DrawShopItem(const MarketItem& item, size_t index)
    {
        ImGui::PushID(static_cast<int>(index));

        ImGui::TextUnformatted(item.name.c_str());

        ImGui::SameLine(250);
        ImGui::Text("x%d", item.quantity);

        ImGui::SameLine(350);
        if (!item.prices.empty()) {
            const auto& price = item.prices[0];
            if (auto* texture = GetCurrencyImage(price.type)) {
                ImGui::Image(*texture, ImVec2(16, 16));
                ImGui::SameLine();
            }
            ImGui::Text("%.0f %s", price.price, GetPriceTypeString(price.type));
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
        if (ImGui::SmallButton("编辑")) {
            editing_item_index = index;
            editing_item.LoadFrom(item);
            show_edit_item_window = true;
        }

        ImGui::PopID();
    }

    void DrawMyShopWindow()
    {
        if (!show_my_shop_window) return;

        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("My Shop", &show_my_shop_window, ImGuiWindowFlags_NoCollapse)) {
            if (!my_shop.uuid.empty() && my_shop.is_certified(GetCurrentPlayerName())) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "商店状态: 已验证");
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "商店状态: 未验证");
                ImGui::SameLine();
                if (ImGui::Button("验证商店")) {
                    SendAskForCertification();
                }
            }

            ImGui::Separator();

            if (my_shop.items.empty()) {
                ImGui::TextDisabled("商店中没有物品");
            }
            else {
                ImGui::BeginChild("ShopItems", ImVec2(0, -30), true);

                ImGui::Text("Item Name");
                ImGui::SameLine(250);
                ImGui::Text("数量");
                ImGui::SameLine(350);
                ImGui::Text("价格");
                ImGui::Separator();

                for (size_t i = 0; i < my_shop.items.size(); i++) {
                    DrawShopItem(my_shop.items[i], i);
                }

                ImGui::EndChild();
            }

            ImGui::Separator();
            if (ImGui::Button("添加物品", ImVec2(120, 0))) {
                editing_item.Reset();
                editing_item_index = 0xffffffff; // 设置为列表末尾（新物品）
                show_edit_item_window = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("关闭", ImVec2(120, 0))) {
                show_my_shop_window = false;
            }
        }
        ImGui::End();
    }

    void DrawEditItemWindow()
    {
        if (!show_edit_item_window) return;

        const bool is_new_item = editing_item_index >= my_shop.items.size();
        const char* window_title = is_new_item ? "添加商店物品" : "编辑商店物品";

        ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(window_title, &show_edit_item_window, ImGuiWindowFlags_NoCollapse)) {
            static char last_search_name[256] = {0};
            if (strcmp(editing_item.name_buffer, last_search_name) != 0 && strlen(editing_item.name_buffer) > 0) {
                strncpy(last_search_name, editing_item.name_buffer, sizeof(last_search_name) - 1);

                const auto found = std::ranges::find_if(available_items.begin(), available_items.end(), [](const AvailableItem& item) {
                    return *item.name == last_search_name;
                });

                if (found != available_items.end()) {
                    edit_window_matching_item_name = *found->name;
                    edit_window_matching_orders.clear();
                    edit_window_orders_needs_sort = true;
                    SendGetItemOrders(edit_window_matching_item_name);
                }
                else {
                    edit_window_matching_item_name.clear();
                    edit_window_matching_orders.clear();
                }
            }

            const float available_width = ImGui::GetContentRegionAvail().x;
            const float available_height = ImGui::GetContentRegionAvail().y - 40.f; // 为按钮留出空间
            const float left_column_width = available_width * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;
            const float right_column_width = available_width * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;

            ImGui::BeginChild("EditForm", ImVec2(left_column_width, available_height), true);

            ImGui::InputText("Item Name", editing_item.name_buffer, sizeof(editing_item.name_buffer));

            ImGui::InputTextMultiline("Description", editing_item.description_buffer, sizeof(editing_item.description_buffer), ImVec2(-1, 80));


            ImGui::InputFloat("Quantity", &editing_item.price.quantity, 1.f, 10.f, "%.0f");
            if (editing_item.price.quantity < 0.f) editing_item.price.quantity = 0.f;

            const char* price_types[] = {"Platinum", "Ecto", "Zkeys", "Arms"};
            ImGui::Combo("Currency", (int*)&editing_item.price.type, price_types, IM_ARRAYSIZE(price_types));

            ImGui::InputFloat("Price Per", &editing_item.price.price, 1.f, 5.f, "%.0f");
            if (editing_item.price.price < 0) editing_item.price.price = 0;

            ImGui::EndChild();

            // 右列 - 匹配的市场订单
            ImGui::SameLine();
            ImGui::BeginChild("MatchingOrders", ImVec2(right_column_width, available_height), true);
            DrawEditWindowMatchingOrders();
            ImGui::EndChild();

            ImGui::Separator();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                if (editing_item_index < my_shop.items.size()) {
                    auto& item = my_shop.items[editing_item_index];

                    item = editing_item.item;
                    item.prices = {editing_item.price};

                    item.name = editing_item.name_buffer;
                    if (*editing_item.description_buffer) {
                        item.description = editing_item.description_buffer;
                    }

                }
                else {
                    auto item = editing_item.item;
                    item = editing_item.item;
                    item.prices = {editing_item.price};

                    item.name = editing_item.name_buffer;
                    if (*editing_item.description_buffer) {
                        item.description = editing_item.description_buffer;
                    }

                    item.player = my_shop.player;
                    item.orderType = OrderType::Sell;
                    item.lastRefresh = time(nullptr);

                    my_shop.items.push_back(item);
                }

                SaveShop(my_shop, true);

                editing_item.Reset();
                edit_window_matching_item_name.clear();
                edit_window_matching_orders.clear();
                memset(last_search_name, 0, sizeof(last_search_name));
                show_edit_item_window = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("取消", ImVec2(120, 0))) {
                editing_item.Reset();
                edit_window_matching_item_name.clear();
                edit_window_matching_orders.clear();
                memset(last_search_name, 0, sizeof(last_search_name));
                show_edit_item_window = false;
            }

            if (!is_new_item) {
                ImGui::SameLine();
                if (ImGui::Button("移除物品", ImVec2(120, 0))) {
                    if (editing_item_index < my_shop.items.size()) {
                        my_shop.items.erase(my_shop.items.begin() + editing_item_index);
                        SaveShop(my_shop, true);
                    }
                    editing_item.Reset();
                    edit_window_matching_item_name.clear();
                    edit_window_matching_orders.clear();
                    memset(last_search_name, 0, sizeof(last_search_name));
                    show_edit_item_window = false;
                }
            }
        }
        ImGui::End();
    }

    GW::HookEntry OnPostUIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        //
    }

    AsyncRestClient purchase_analytics_client;

    void SendPurchaseAnalytics(const std::string& item_name, OrderType order_type, const Price& price)
    {
        if (!ToolboxSettings::send_anonymous_gameplay_info) return;
        if (purchase_analytics_client.IsPending()) return;
        purchase_analytics_client.Clear();

        json price_json;
        price_json["type"] = static_cast<int>(price.type);
        price_json["price"] = price.price;
        price_json["quantity"] = price.quantity;

        json payload;
        payload["name"] = item_name;
        payload["orderType"] = static_cast<int>(order_type);
        payload["price"] = price_json;

        const auto json_str = glz::write_json(payload).value_or(std::string{});

        purchase_analytics_client.SetUrl(std::format("https://{}/api/shop/purchase/", market_host).c_str());
        purchase_analytics_client.SetMethod(HttpMethod::Post);
        purchase_analytics_client.SetHeader("Content-Type", "application/json");
        purchase_analytics_client.SetPostContent(json_str, ContentFlag::Copy);
        purchase_analytics_client.SetTimeoutSec(5);
        purchase_analytics_client.SetConnectTimeoutSec(3);
        purchase_analytics_client.SetVerifyPeer(false);
        purchase_analytics_client.SetVerifyHost(false);
        purchase_analytics_client.ExecuteAsync();
    }

    GW::HookEntry OnSendChatMessage_HookEntry;
    void OnSendChatMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
    {
        if (!pending_purchase_analytic.IsActive()) return;

        const auto message = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam)->message;
        if (!(message && *message)) return;
        if (GW::Chat::GetChannel(*message) != GW::Chat::Channel::CHANNEL_WHISPER) return;

        const wchar_t* name_start = message + 1; // 跳过频道操作码
        const wchar_t* sep = wcschr(name_start, L',');
        if (!sep) return;

        const std::string recipient = TextUtils::WStringToString(std::wstring(name_start, sep - name_start));
        if (recipient != pending_purchase_analytic.player_name) return;

        SendPurchaseAnalytics(pending_purchase_analytic.item_name, pending_purchase_analytic.order_type, pending_purchase_analytic.price);
        pending_purchase_analytic.Clear();
    }

    struct PendingAddToSell {
        uint32_t item_id;
        std::unique_ptr<GuiUtils::EncString> decoded_complete_name;
        std::unique_ptr<GuiUtils::EncString> decoded_name;
        std::unique_ptr<GuiUtils::EncString> decoded_desc;
        bool ready_to_add() { return decoded_name && !decoded_name->IsDecoding() && (!decoded_desc || !decoded_desc->IsDecoding()); }
        void reset(GW::Item* item)
        {
            decoded_name.reset();

            if (!(item && item->name_enc && *item->name_enc)) return;

            item_id = item->item_id;

            decoded_complete_name = std::make_unique<GuiUtils::EncString>();
            if (item->complete_name_enc && *item->complete_name_enc) {
                decoded_complete_name->language(GW::Constants::Language::English);
                decoded_complete_name->reset(item->complete_name_enc, false);
                decoded_complete_name->string();
            }

            decoded_name = std::make_unique<GuiUtils::EncString>();
            decoded_name->language(GW::Constants::Language::English);
            decoded_name->reset(item->name_enc, true);
            decoded_name->string();

            decoded_desc = std::make_unique<GuiUtils::EncString>();
            decoded_desc->reset(nullptr, false);
            if (item->info_string && *item->info_string) {
                decoded_desc->language(GW::Constants::Language::English);
                decoded_desc->reset(item->info_string, false);
                decoded_desc->string();
            }
        }
        ~PendingAddToSell() = default;
    } pending_add_to_sell;

    void CheckPendingAddToSell()
    {
        if (!pending_add_to_sell.ready_to_add()) return;

        const auto item = (InventoryManager::Item*)GW::Items::GetItemById(pending_add_to_sell.item_id);
        if (!GWMarketWindow::CanSellItem(item)) {
            pending_add_to_sell.reset(0);
            return;
        }
        ShopItem market_item;
        market_item.name = TextUtils::trim(TextUtils::WStringToString(GetGWMarketItemName(item, pending_add_to_sell.decoded_complete_name->wstring(), pending_add_to_sell.decoded_name->wstring(), pending_add_to_sell.decoded_desc->wstring())));
        market_item.description = TextUtils::trim(TextUtils::WStringToString(GetGWMarketItemDescription(item, pending_add_to_sell.decoded_complete_name->wstring(), pending_add_to_sell.decoded_name->wstring(), pending_add_to_sell.decoded_desc->wstring())));

        auto attr = item->GetModifier(0x2798);
        if (attr) {
            market_item.weaponDetails.attribute = (GW::Constants::Attribute)attr->arg2();
            market_item.weaponDetails.requirement = attr->arg1() & 0xf;
            market_item.weaponDetails.inscribable = item->GetIsInscribable();
            market_item.weaponDetails.oldschool = item->IsOldSchool();
        }

        const auto i = GW::Map::GetCurrentMapInfo();
        market_item.pre = i && i->region == GW::Region::Region_Presearing;

        pending_add_to_sell.reset(0);

        market_item.quantity = static_cast<int>(item->quantity);
        market_item.prices.push_back({
            .type = Currency::Platinum,
            .quantity = 1,
            .price = 5,
        });

        editing_item.LoadFrom(market_item);
        editing_item_index = 0xffffffff;
        show_edit_item_window = true;
    }


} // namespace

void GWMarketWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
    InitWebSocket();

    const GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kMapLoaded};
    for (auto ui_message : ui_messages) {
        RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, ui_message, OnPostUIMessage, 0x4000);
    }
    OnPostUIMessage(0, GW::UI::UIMessage::kMapLoaded, 0, 0);
    RegisterUIMessageCallback(&OnSendChatMessage_HookEntry, GW::UI::UIMessage::kSendChatMessage, OnSendChatMessage);
}

void GWMarketWindow::Terminate()
{
    Disconnect(true);
    ToolboxWindow::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnSendChatMessage_HookEntry);
    purchase_analytics_client.Abort();
}

void GWMarketWindow::Update(float delta)
{
    ToolboxWindow::Update(delta);

    // 如果工作线程已完成，则加入（例如断开连接后）
    if (market_ws.Update()) {
        // 线程刚停止，重新初始化回调以便下次连接
        InitWebSocket();
    }

    if (!ShouldConnect()) {
        if (!market_ws.IsIdle()) Disconnect();
        return;
    }

    // 当窗口可见时确保 socket 正在连接/已连接
    if (market_ws.IsIdle()) {
        InitWebSocket();
        market_ws.Connect();
    }

    refresh_timer += delta;
    if (refresh_timer >= settings.refresh_interval) {
        refresh_timer = 0.0f;
        if (settings.auto_refresh) Refresh();
    }
}

bool GWMarketWindow::CanSellItem(GW::Item* _item)
{
    if (!GWMARKET_SELLING_ENABLED) return false;
    const auto item = (InventoryManager::Item*)_item;
    return my_shop.valid() && item && item->IsTradable() && !item->customized;
}

void GWMarketWindow::AddItemToSell(GW::Item* _item)
{
    if (!CanSellItem(_item)) return;
    pending_add_to_sell.reset(_item);
}

void GWMarketWindow::SearchItem(const std::string& item_name)
{
    if (item_name.empty()) return;
    auto& instance = Instance();
    instance.visible = true;
    strncpy(search_buffer, item_name.c_str(), sizeof(search_buffer) - 1);
    search_buffer[sizeof(search_buffer) - 1] = '\0';
    current_viewing_item = item_name;
    if (IsSocketIOReady()) {
        SendGetItemOrders(item_name);
    }
    else {
        pending_search_item = item_name;
    }
}

void GWMarketWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    favorite_items.clear();
    std::vector<std::string> favorites_list;
    if (doc.Get(Name(), "favorite_items", favorites_list)) {
        for (const auto& item : favorites_list) {
            if (!item.empty()) {
                ToggleFavourite(item, true);
            }
        }
        return;
    }
    // 旧版 INI 兼容：以管道符分隔的字符串
    const char* favorites_str = legacy->GetValue(Name(), "favorite_items", "");
    if (favorites_str && strlen(favorites_str) > 0) {
        std::string favorites(favorites_str);
        size_t start = 0;
        size_t pos = 0;
        while ((pos = favorites.find('|', start)) != std::string::npos) {
            std::string item = favorites.substr(start, pos - start);
            if (!item.empty()) {
                ToggleFavourite(item, true);
            }
            start = pos + 1;
        }
        // 添加最后一个（如果没有管道，则是唯一项）
        if (start < favorites.length()) {
            std::string item = favorites.substr(start);
            if (!item.empty()) {
                ToggleFavourite(item, true);
            }
        }
    }
}

void GWMarketWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    std::vector<std::string> favorites_list;
    favorites_list.reserve(favorite_items.size());
    for (const auto& item : favorite_items) {
        favorites_list.push_back(item.first);
    }
    doc.Set(Name(), "favorite_items", favorites_list);
}

void GWMarketWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("自动刷新", &settings.auto_refresh);
    if (settings.auto_refresh) {
        ImGui::SliderInt("间隔（秒）", &settings.refresh_interval, 30, 300);
    }

    ImGui::Separator();

    if (socket_io_ready) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "已连接");
    }
    else if (!market_ws.IsIdle()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "连接中...");
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "已断开");
    }

    if (socket_io_ready && ImGui::Button("刷新")) {
        Refresh();
    }
}

void GWMarketWindow::Draw(IDirect3DDevice9*)
{
#if (GWMARKET_SELLING_ENABLED)
    DrawMyShopWindow();
    DrawEditItemWindow();
    CheckPendingAddToSell();
#endif
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
    collapsed = !ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
    if (!collapsed) {
        if (socket_io_ready) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "已连接");
        }
        else if (!market_ws.IsIdle()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "连接中...");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "已断开");
        }

        ImGui::SameLine();
        if (ImGui::Button("刷新")) {
            Refresh();
        }
#if (GWMARKET_SELLING_ENABLED)
        ImGui::SameLine();
        if (ImGui::Button("我的商店")) {
            show_my_shop_window = true;
        }
#endif

        ImGui::Separator();

        ImGui::Text("显示:");
        ImGui::SameLine();
        if (ImGui::RadioButton("全部", filter_mode == SHOW_ALL)) filter_mode = SHOW_ALL;
        ImGui::SameLine();
        if (ImGui::RadioButton("出售", filter_mode == SHOW_SELL_ONLY)) filter_mode = SHOW_SELL_ONLY;
        ImGui::SameLine();
        if (ImGui::RadioButton("求购", filter_mode == SHOW_BUY_ONLY)) filter_mode = SHOW_BUY_ONLY;

        ImGui::Separator();
        ImGui::InputText("搜索", search_buffer, sizeof(search_buffer));
        ImGui::Separator();

        const float available_width = ImGui::GetContentRegionAvail().x;
        const float available_height = ImGui::GetContentRegionAvail().y - 32.f;
        const float left_column_width = available_width * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;
        const float right_column_width = available_width * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;

        const float item_list_height = available_height * 0.7f - ImGui::GetStyle().ItemSpacing.y * 0.5f;
        const float favorites_height = available_height * 0.3f - ImGui::GetStyle().ItemSpacing.y * 0.5f;

        ImGui::BeginChild("ItemList", ImVec2(left_column_width, item_list_height), true);
        DrawItemList();
        ImGui::EndChild();

        const auto favourites_cursor_pos = ImGui::GetCursorPos();

        ImGui::SameLine();
        ImGui::BeginChild("ItemDetails", ImVec2(right_column_width, available_height), true);
        DrawItemDetails();
        ImGui::EndChild();

        ImGui::SetCursorPos(favourites_cursor_pos);

        ImGui::BeginChild("FavoritesList", ImVec2(left_column_width, favorites_height), true);
        ImGui::Text("收藏");
        ImGui::Separator();
        DrawFavoritesList();
        ImGui::EndChild();



        /* 网站链接脚注 */
        static char buf[128];
        if (!buf[0]) {
            const auto url = std::format("https://{}", market_host);
            snprintf(buf, 128, "由 %s 提供", url.c_str());
        }

        if (ImGui::Button(buf, ImVec2(ImGui::GetContentRegionAvail().x, 20.0f))) {
            GW::GameThread::Enqueue([]() {
                const auto url = std::format("https://{}", market_host);
                GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)url.c_str());
            });
        }
    }
    ImGui::End();
}
