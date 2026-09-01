#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <atomic>
#include <bit>
#include <charconv>
#include <deque>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include "stdafx.h"

#include <GWToolbox.h>
#include <Logger.h>
#include <Utils/EncString.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>

#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Windows/AccountInventoryWindow.h>
#include <Windows/CompletionWindow.h>
#include <Windows/RerollWindow.h>

#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <Utils/ToolboxUtils.h>

#define memeq(a, b) (memcmp((a), (b), sizeof(*(a))) == 0)

namespace account_inventory_json {
    // 成员名称保持可读；glz::meta 块将它们映射到简洁的 JSON 键
    //（在每个物品/节上重复）以保持文件小巧。
    struct ItemJson {
        uint32_t model_id{};
        uint32_t model_file_id{};
        uint32_t interaction{};
        uint16_t quantity{};
        std::optional<uint8_t> equipped; // 未装备时省略（默认 0）
        std::string description;         // 编码的游戏字符串，以 4 位十六进制编码单元的连接形式
        struct glaze {
            using T = ItemJson;
            static constexpr auto value = glz::object("m", &T::model_id, "f", &T::model_file_id, "i", &T::interaction, "q", &T::quantity, "e", &T::equipped, "d", &T::description);
        };
    };
    using BagJson = std::map<uint32_t /*slot*/, ItemJson>;
    struct FreeSlotsJson {
        uint32_t max_inventory{};
        uint32_t max_equipment{};
        uint32_t occupied_inventory{};
        uint32_t occupied_equipment{};
        struct glaze {
            using T = FreeSlotsJson;
            static constexpr auto value = glz::object("mi", &T::max_inventory, "me", &T::max_equipment, "oi", &T::occupied_inventory, "oe", &T::occupied_equipment);
        };
    };
    struct CharacterJson {
        std::optional<FreeSlotsJson> free_slots; // 不存在 => 未知
        std::map<uint32_t /*bag_id*/, BagJson> bags;
        std::map<uint32_t /*hero_id*/, BagJson> heroes;
        struct glaze {
            using T = CharacterJson;
            static constexpr auto value = glz::object("fs", &T::free_slots, "b", &T::bags, "h", &T::heroes);
        };
    };
    struct ChestJson {
        bool anniversary_pane_active{};
        std::optional<FreeSlotsJson> free_slots; // 仅限库存；不存在 => 未知
        std::map<uint32_t /*bag_id*/, BagJson> bags;
        struct glaze {
            using T = ChestJson;
            static constexpr auto value = glz::object("a", &T::anniversary_pane_active, "fs", &T::free_slots, "b", &T::bags);
        };
    };
    struct AccountJson {
        std::string account; // GUID 字符串
        std::string representing_character;
        ChestJson chest;
        std::map<std::string /*character*/, CharacterJson> characters;
        struct glaze {
            using T = AccountJson;
            static constexpr auto value = glz::object("id", &T::account, "rc", &T::representing_character, "c", &T::chest, "ch", &T::characters);
        };
    };
} // namespace account_inventory_json


namespace {
    using namespace account_inventory_json;

    // 基于 boost::hash_combine
    template <typename... Args>
    std::size_t hash_combine(const Args&... args)
    {
        std::size_t seed = 0;
        ((seed ^= std::hash<Args>{}(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ...);
        return seed;
    }

    constexpr float ITEMS_TABLE_MIN_HEIGHT = 220.f;
    constexpr int CHEST_ARMOR_INVENTORY_SLOT = 2;
    constexpr clock_t ADD_HERO_TIMEOUT = 500;
    constexpr clock_t SAVE_HERO_TIMEOUT = 500;
    constexpr clock_t MAP_LOADED_DELAYED_TIMEOUT = 400;
    constexpr clock_t SAVE_DIRTY_INVENTORIES_TIMEOUT = 1000;

    const char* BAG_NAME[] = {"",          "背包",  "腰带包", "背包 1",     "背包 2",     "装备包", "材料存储", "未认领物品", "仓库 1",  "仓库 2",  "仓库 3",     "仓库 4",
                              "仓库 5", "仓库 6", "仓库 7",  "仓库 8", "仓库 9", "仓库 10",     "仓库 11",       "仓库 12",      "仓库 13", "仓库 14", "已装备物品"};
    uint32_t GetMaxBagCapacity(GW::Constants::Bag bag_id)
    {
        if (bag_id == GW::Constants::Bag::None || bag_id >= GW::Constants::Bag::Max) return 0;
        switch (bag_id) {
            case GW::Constants::Bag::Backpack:
                return 20;
            case GW::Constants::Bag::Belt_Pouch:
                return 10;
            case GW::Constants::Bag::Bag_1:
            case GW::Constants::Bag::Bag_2:
                return 15;
            case GW::Constants::Bag::Equipment_Pack:
                return 20;
            case GW::Constants::Bag::Material_Storage:
                return 36;
            case GW::Constants::Bag::Unclaimed_Items:
                return 12;
            case GW::Constants::Bag::Equipped_Items:
                return 9;
            default:
                return 25; // 仓库 1 到 仓库 14
        }
    }

    std::string GetCurrentPlayerNameS()
    {
        const auto player_name = GW::AccountMgr::GetCurrentPlayerName();
        return player_name ? TextUtils::WStringToString(player_name) : "";
    }

    bool IsChestBag(GW::Constants::Bag bag_id)
    {
        if (GW::Constants::Bag::Material_Storage == bag_id) return true;
        if (GW::Constants::Bag::Storage_1 <= bag_id && bag_id <= GW::Constants::Bag::Storage_14) return true;
        return false;
    }

    bool BagCanHoldAnything(GW::Constants::Bag bag_id)
    {
        if (bag_id == GW::Constants::Bag::None || bag_id >= GW::Constants::Bag::Max) return false;
        switch (bag_id) {
            case GW::Constants::Bag::Equipment_Pack:
            case GW::Constants::Bag::Material_Storage:
            case GW::Constants::Bag::Unclaimed_Items:
            case GW::Constants::Bag::Equipped_Items:
                return false;
            default:
                return true;
        }
    }


    bool IsHeroArmor(GW::Constants::HeroID hero_id, uint32_t slot)
    {
        return hero_id != GW::Constants::HeroID::NoHero && slot >= 2;
    }

    bool IsOnHero(GW::Constants::HeroID hero_id)
    {
        return GW::Constants::HeroID::NoHero < hero_id && hero_id < GW::Constants::HeroID::Count;
    }

    bool GetIsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Map::GetIsMapLoaded() && GW::Agents::GetControlledCharacter();
    }

    std::string GetIniID(const GUID& account, const std::string& /*character*/)
    {
        return TextUtils::GuidToString(&account);
    }

    void RightAlignText(const char* text)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x - ImGui::GetScrollX());
        ImGui::TextUnformatted(text);
    }

    void RightAlignTextF(const char* fmt, ...)
    {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        RightAlignText(buf);
    }

    ImVec4 HSVRotate(ImVec4 color, float hue = -1.f, float sat_factor = 1.f)
    {
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
        if (hue < 0.f) hue = h;
        return (ImVec4)ImColor::HSV(hue, s * sat_factor, v, color.w);
    }

    enum ItemColumnID {
        ItemColumnID_Character,
        ItemColumnID_Location,
        ItemColumnID_ModelID,
        ItemColumnID_Description,
        ItemColumnID_Max,
    };

    enum SlotColumnID {
        SlotColumnID_Character,
        SlotColumnID_Inventory,
        SlotColumnID_InventorySize,
        SlotColumnID_Equipment,
        SlotColumnID_EquipmentSize,
        SlotColumnID_Max,
    };



    // 仅限物品固有数据。其账户/角色/英雄/背包/槽位由
    // 其在层次结构中的位置决定（账户 -> 仓库/角色 -> 背包 -> 物品）。
    struct Item {
        uint32_t model_id{};
        uint32_t model_file_id{};
        uint32_t interaction{};
        uint16_t quantity{};
        uint8_t equipped{};
        // 编码的游戏字符串是事实来源；EncString 对其惰性解码以供
        // 显示（解码后的文本不持久化）。.encoded() 是我们保存的内容。
        GuiUtils::EncString description{};
        uint32_t item_id{};                    // 仅限实时会话；从磁盘加载的物品为 0
        uint32_t dyes{};                       // 打包的染料颜色；磁盘物品为 0（未染色）
        IDirect3DTexture9** texture = nullptr; // GetItemImage 缓存，首次绘制时惰性获取；不序列化
    };

    // 空闲槽信息，从旧的 CharacterFreeSlots 中提取到节点中。
    struct FreeSlotInfo {
        bool known = false; // 一旦我们有此节点的空闲槽数据则为 true
        uint32_t max_inventory{};
        uint32_t max_equipment{};
        uint32_t occupied_inventory{};
        uint32_t occupied_equipment{};
    };

    struct Bag {
        GW::Constants::Bag bag_id{};
        std::unordered_map<uint32_t /*slot*/, Item> items; // 节点稳定：原始 Item* 保持有效
    };
    struct Hero {
        GW::Constants::HeroID hero_id{};
        Bag bag; // 单个 Equipped_Items 背包
    };
    struct Character {
        std::string name;
        std::unordered_map<GW::Constants::Bag, Bag> bags;
        std::unordered_map<GW::Constants::HeroID, Hero> heroes;
        FreeSlotInfo free_slots;
    };
    struct Account {
        GUID uuid{};
        std::unordered_map<GW::Constants::Bag, Bag> chest; // 15 个仓库面板
        std::unordered_map<std::string, Character> characters;
        std::string account_representing_character; // 工具提示辅助
        bool anniversary_pane_active = false;
        FreeSlotInfo chest_free_slots; // 仓库的装备栏未使用
    };

    // 展平的临时物品视图，通过遍历层次结构重建。
    // 携带显示/排序/工具提示所需的非规范化字段。
    struct ItemRef {
        Account* account = nullptr;     // 永不为空
        Character* character = nullptr; // 仓库物品为 null
        Hero* hero = nullptr;           // 仅在英雄上时非空
        GW::Constants::Bag bag_id{};
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        uint32_t slot{};
        Item* item = nullptr;
        std::string character_name; // "(仓库)" | Character::name
        std::string location;       // "(玩家)" | 英雄名称 | BAG_NAME[bag]
    };

    // 存储路径：足以在层次结构中定位（或重新定位）一个物品。
    struct ItemPath {
        GUID account{};
        std::string character; // "(仓库)" 或玩家名称
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        GW::Constants::Bag bag_id{};
        uint32_t slot{};
    };

    // 解析后的实时物品：它所在的位置 + 所属背包（以便删除）。
    struct ItemLoc {
        Account* account = nullptr;
        Character* character = nullptr; // 仓库物品为 null
        GW::Constants::Bag bag_id{};
        uint32_t slot{};
        Bag* bag = nullptr;
        Item* item = nullptr;
    };

    struct MergeStack;

    struct ItemCompare {
        ImGuiTableSortSpecs* sort_specs{};
        UUID current_account{};
        bool operator()(const MergeStack& lms, const MergeStack& rms) const;
        bool operator()(ItemRef* l, ItemRef* r) const;
    };

    struct MergeStack {
        uint16_t quantity;
        std::string description;
        int displayed_quantity = -1;
        std::string description_one_line;
        std::set<ItemRef*, ItemCompare> i;
        MergeStack(const UUID& account, const std::string& _description);
        const std::string& GetDescription()
        {
            if (displayed_quantity != quantity) {
                auto build_desc = quantity > 1 ? std::to_string(quantity) + " " + description : description;
                description_one_line = TextUtils::ctre_regex_replace<L"\n", L" - ">(build_desc);
                displayed_quantity = quantity;
            }
            return description_one_line;
        }
    };
    bool ItemCompare::operator()(const MergeStack& lms, const MergeStack& rms) const
    {
        int sort_direction = 1;
        int delta = 0;
        if (rms.i.empty()) return false;
        if (lms.i.empty()) return true;
        const auto l = *(lms.i.begin());
        const auto r = *(rms.i.begin());
        if (sort_specs) {
            for (int n = 0; n < sort_specs->SpecsCount; n++) {
                const ImGuiTableColumnSortSpecs* sort_spec = &sort_specs->Specs[n];
                sort_direction = (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? 1 : -1;
                delta = 0;
                switch (sort_spec->ColumnUserID) {
                    case ItemColumnID_Character:
                        delta = l->character_name.compare(r->character_name);
                        break;
                    case ItemColumnID_Location:
                        delta = l->location.compare(r->location);
                        break;
                    case ItemColumnID_ModelID:
                        delta = l->item->model_id - r->item->model_id;
                        break;
                    case ItemColumnID_Description:
                        delta = lms.description.compare(rms.description);
                        break;
                }
                if (delta != 0) return delta * sort_direction < 0;
            }
        }
        // 回退
        if (delta == 0) delta = l->character_name.compare(r->character_name);
        if (delta == 0) delta = l->location.compare(r->location);
        if (delta == 0) delta = static_cast<int>(l->bag_id) - static_cast<int>(r->bag_id);
        if (delta == 0) delta = l->slot - r->slot;
        if (delta == 0) delta = memcmp(&l->account->uuid, &r->account->uuid, sizeof(l->account->uuid));
        return delta * sort_direction < 0;
    }
    bool ItemCompare::operator()(ItemRef* l, ItemRef* r) const
    {
        if (l->account->uuid != r->account->uuid) {
            // 最低的物品是当前账户中可以交互的那个。确保它优先于其他账户。
            if (memeq(&l->account->uuid, &current_account)) return true;
            if (memeq(&r->account->uuid, &current_account)) return false;
        }
        auto lms = MergeStack(l->account->uuid, "");
        lms.quantity = l->item->quantity;
        lms.i.insert(l);
        auto rms = MergeStack(r->account->uuid, "");
        rms.quantity = r->item->quantity;
        rms.i.insert(r);
        return this->operator()(lms, rms);
    }
    MergeStack::MergeStack(const UUID& account, const std::string& _description) : quantity{}, i(ItemCompare{nullptr, account})
    {
        description = _description;
    }

    // 空闲槽表的一行，从 Character（或 Account 的仓库）构建。
    struct SlotRow {
        UUID account{};
        std::string character{};
        std::string account_representing_character{};
        uint32_t max_inventory{};
        uint32_t max_equipment{};
        uint32_t occupied_inventory{};
        uint32_t occupied_equipment{};
    };

    struct SlotCompare {
        ImGuiTableSortSpecs* sort_specs{};
        bool operator()(const SlotRow* const l, const SlotRow* const r) const
        {
            int sort_direction = 1;
            int delta = 0;
            auto l_free_inventory = l->max_inventory - l->occupied_inventory;
            auto l_free_equipment = l->max_equipment - l->occupied_equipment;
            auto r_free_inventory = r->max_inventory - r->occupied_inventory;
            auto r_free_equipment = r->max_equipment - r->occupied_equipment;
            if (sort_specs) {
                for (int n = 0; n < sort_specs->SpecsCount; n++) {
                    const ImGuiTableColumnSortSpecs* sort_spec = &sort_specs->Specs[n];
                    sort_direction = (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? 1 : -1;
                    delta = 0;
                    switch (sort_spec->ColumnUserID) {
                        case SlotColumnID_Character:
                            delta = l->character.compare(r->character);
                            break;
                        case SlotColumnID_Inventory:
                            delta = l_free_inventory - r_free_inventory;
                            break;
                        case SlotColumnID_InventorySize:
                            delta = l->max_inventory - r->max_inventory;
                            break;
                        case SlotColumnID_Equipment:
                            delta = l_free_equipment - r_free_equipment;
                            break;
                        case SlotColumnID_EquipmentSize:
                            delta = l->max_equipment - r->max_equipment;
                            break;
                    }
                    if (delta != 0) return delta * sort_direction < 0;
                }
            }
            // 回退
            if (delta == 0) delta = l->character.compare(r->character);
            if (delta == 0) delta = memcmp(&l->account, &r->account, sizeof(r->account));
            return delta * sort_direction < 0;
        }
    };

    // 跟踪一个磁盘上的账户文件。取代了旧的 SimpleIni 包装器；
    // 文件内容现在为 JSON，因此仅携带标识 + 脏状态。
    struct InventoryFile {
        std::filesystem::path location_on_disk;
        std::filesystem::file_time_type last_change_time{};
        bool is_loaded = false;
        GUID account{};
        std::string ini_ID{}; // = GuidToString(account)
        std::mutex io_mutex;
        std::string queued_json;
        bool delete_requested = false;
        std::atomic_bool io_pending{false};
        explicit InventoryFile(std::filesystem::path _location) : location_on_disk(std::move(_location)) {}
    };

    void OnItemTooltip(const MergeStack* ms);
    void OnAccountInventoryItemClicked(const ItemPath& path, bool move);
    static bool CheckIniDirty(InventoryFile* ini, std::filesystem::file_time_type write_time);
    InventoryFile* GetIni(const std::string& ini_ID, const GUID& account);
    void LoadFromFiles(bool only_foreign);
    void SaveToFiles(bool include_foreign);
    void SortSlots(ImGuiTableSortSpecs* sort_specs);
    std::wstring GetItemEncDescription(GW::Item* item);
    void ClearMissingItem(const UUID* account, const std::string& character, const GW::Constants::HeroID hero_id, const GW::Constants::Bag bag_id, const uint32_t slot);

    GW::HookEntry OnUIMessage_HookEntry{};
    // 主物品存储：账户层次结构，以规范 GUID 字符串为键。
    std::unordered_map<std::string, Account> accounts{};
    // On*SlotCleared 仅发送 item_id，因此我们保持 item_id -> 解析位置的映射
    // 以便在不遍历层次结构的情况下删除实时物品。
    std::unordered_map<uint32_t, ItemLoc> inventory_lookup{};
    // 用于显示的排序/过滤视图 + 其支持的 ItemRef 存储（稳定地址）
    std::deque<ItemRef> item_refs{};
    std::vector<MergeStack> inventory_sorted{};
    // ini files, 1 per character/chest
    std::unordered_map<std::filesystem::path, std::shared_ptr<InventoryFile>> ini_by_path{};
    std::unordered_map<std::string, InventoryFile*> ini_by_character{};
    // 变更追踪器以减少写入
    std::unordered_set<std::string> inventory_dirty{};
    // 用于空闲槽表的排序/过滤视图 + 其支持的行
    std::vector<SlotRow> slot_rows{};
    std::set<SlotRow*, SlotCompare> free_slots_sorted{};

    bool initializing = false;
    bool needs_sorting = true;
    bool needs_filter = true;
    bool sort_awaiting_decode = false;
    clock_t decode_sort_timer{};
    bool show_delete_note = false;
    size_t filtered_item_count = 0;
    std::string last_character{};
    std::set<std::string> last_available_chars{};

    struct InventoryScanner {
        enum Stage : uint32_t { None, Start, NextCharacter, WaitForCharacterLoad, WaitForEmptyParty, WaitForHeroLoad, DoRestoreHeroes };
        Stage Get() const { return current_stage; }
        void Set(Stage _stage)
        {
            if (current_stage == _stage) return;
            stage_set_at = TIMER_INIT();
            current_stage = _stage;
        }
        void Begin()
        {
            if (current_stage != Stage::None) return;
            Set(Stage::Start);
        }
        void Update();
        bool Cancel(const char* err = 0)
        {
            Set(Stage::None);
            if (err) {
                Log::Warning("%s", err);
            }
            return true;
        }

    private:
        Stage current_stage = Stage::None;
        clock_t stage_set_at = 0;

        std::wstring original_player;
        std::vector<GW::Constants::HeroID> original_player_heroes;
        std::vector<std::wstring> reroll_char_queue{};
        std::wstring current_reroll_char;
        std::vector<GW::Constants::HeroID> queued_hero_ids{};
        std::vector<GW::Constants::HeroID> original_heroes{};
        std::vector<GW::Constants::HeroID> heroes_pending_load{};
    };
    InventoryScanner inventory_scan;

    struct ItemReroller {
        enum Stage : uint32_t { None, Start, WaitForCharacterLoad, WaitForHeroLoad };
        Stage Get() const { return current_stage; }
        void Set(Stage _stage)
        {
            if (current_stage == _stage) return;
            stage_set_at = TIMER_INIT();
            current_stage = _stage;
        }
        void Begin(const ItemPath& _path, bool _move = false)
        {
            if (current_stage != Stage::None) return;
            move = _move;
            item = _path;
            Set(Stage::Start);
        }
        void Update();
        void Cancel(const char* err = 0)
        {
            Set(Stage::None);
            if (err) {
                Log::Warning("%s", err);
            }
        }

    private:
        Stage current_stage = Stage::None;
        clock_t stage_set_at = 0;
        ItemPath item;
        bool move = false;
    };
    ItemReroller item_reroll;
    clock_t add_hero_timer{};
    clock_t save_hero_timer{};
    clock_t map_loaded_delayed_timer{};
    clock_t save_dirty_inventories_timer{};
    bool map_loaded_delayed_trigger = false;

    AccountInventoryWindow::Settings settings;

    inline static const size_t BUFFER_SIZE = 128;
    char name_filter_buf[BUFFER_SIZE]{};
    char location_filter_buf[BUFFER_SIZE]{};
    char model_ID_filter_buf[BUFFER_SIZE]{};
    char item_filter_buf[BUFFER_SIZE]{};

    GUID current_account;
    std::string current_character;

    ImVec4 color_chest_item{};
    ImVec4 color_chest_item_hovered{};
    ImVec4 color_chest_item_active{};

    ImVec4 color_hero_item{};
    ImVec4 color_hero_item_hovered{};
    ImVec4 color_hero_item_active{};

    ImVec4 color_item_foreign{};
    ImVec4 color_item_hovered_foreign{};
    ImVec4 color_item_active_foreign{};

    ImVec4 color_chest_item_foreign{};
    ImVec4 color_chest_item_hovered_foreign{};
    ImVec4 color_chest_item_active_foreign{};

    ImVec4 color_hero_item_foreign{};
    ImVec4 color_hero_item_hovered_foreign{};
    ImVec4 color_hero_item_active_foreign{};

    ImVec4 cached_button_color{};

    static constexpr ImU32 color_quantity = IM_COL32(250, 247, 153, 255);

    std::vector<GW::Constants::HeroID> GetPartyHeroIDs()
    {
        const auto& flags = GW::GetWorldContext()->hero_flags;
        std::vector<GW::Constants::HeroID> hero_ids;
        for (auto& flag : flags) {
            hero_ids.push_back(flag.hero_id);
        }
        return hero_ids;
    }

    // 跳转到点击物品的位置，即打开仓库/添加英雄/切换角色
    // 按住 Ctrl：跳转后将物品移动至/从仓库
    void OnAccountInventoryItemClicked(const ItemPath& path, bool move)
    {
        item_reroll.Begin(path, move);
    }

    void OnItemTooltip(const MergeStack* ms)
    {
        auto char_key = [](const ItemRef* i, const std::string& arc) {
            return arc + "\x1f" + i->character_name;
        };
        auto loc_key = [](const ItemRef* i, const std::string& arc) {
            return arc + "\x1f" + i->character_name + "\x1f" + i->location;
        };
        // 一次只显示一个工具提示；缓存聚合值，仅在悬停物品变化时重新计算。
        static const MergeStack* last_ms = nullptr;
        static std::unordered_map<std::string, uint32_t> char_totals;
        static std::unordered_map<std::string, uint32_t> loc_totals;
        if (ms != last_ms) {
            last_ms = ms;
            char_totals.clear();
            loc_totals.clear();
            for (auto it = ms->i.begin(); it != ms->i.end(); it++) {
                const std::string& arc = (*it)->account->account_representing_character;
                char_totals[char_key(*it, arc)] += (*it)->item->quantity;
                loc_totals[loc_key(*it, arc)] += (*it)->item->quantity;
            }
        }

        std::string prev_character{};
        std::string prev_account_representing_character{};
        std::string prev_location{};
        for (auto it = ms->i.begin(); it != ms->i.end(); it++) {
            int style_count = 0;
            bool is_this_account = memeq(&(*it)->account->uuid, &current_account);
            if (is_this_account) {
                style_count = 1;
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            const std::string& account_representing_character = (*it)->account->account_representing_character;
            bool reprint = (*it)->character_name != prev_character || account_representing_character != prev_account_representing_character;
            if (reprint) {
                std::string suffix = "";
                if (!is_this_account && (*it)->character_name == "(仓库)" && !account_representing_character.empty()) {
                    suffix = " [" + account_representing_character + "]";
                }
                ImGui::Text("%s%s：%u", (*it)->character_name.c_str(), suffix.c_str(), char_totals[char_key(*it, account_representing_character)]);
            }
            reprint |= (*it)->location != prev_location;
            if (reprint) {
                ImGui::Text("- %s：%u", (*it)->location.c_str(), loc_totals[loc_key(*it, account_representing_character)]);
            }
            ImGui::PopStyleColor(style_count);
            prev_account_representing_character = account_representing_character;
            prev_character = (*it)->character_name;
            prev_location = (*it)->location;
        }
        ImGui::Separator();
        ImGui::PushTextWrapPos(440.f * ImGui::FontScale());
        ImGui::Text("%s", ms->description.c_str());
        ImGui::PopTextWrapPos();
    }

    // ===== 层次结构路由 / 查找辅助函数 =====

    std::string AccountKey(const GUID& account)
    {
        return TextUtils::GuidToString(&account);
    }

    Account& GetOrCreateAccount(const GUID& account)
    {
        Account& acc = accounts[AccountKey(account)];
        acc.uuid = account;
        return acc;
    }
    Account* FindAccount(const GUID& account)
    {
        const auto it = accounts.find(AccountKey(account));
        return it == accounts.end() ? nullptr : &it->second;
    }

    // 解析（按需创建 Account/Character/Hero/Bag）存储路径所在的背包。
    // 封装 IsChestBag / IsOnHero 路由逻辑。
    Bag& GetOrCreateBag(const GUID& account, const std::string& character, GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id)
    {
        Account& acc = GetOrCreateAccount(account);
        if (IsChestBag(bag_id)) {
            Bag& bag = acc.chest[bag_id];
            bag.bag_id = bag_id;
            return bag;
        }
        Character& ch = acc.characters[character];
        ch.name = character;
        if (IsOnHero(hero_id)) {
            Hero& hero = ch.heroes[hero_id];
            hero.hero_id = hero_id;
            hero.bag.bag_id = bag_id;
            return hero.bag;
        }
        Bag& bag = ch.bags[bag_id];
        bag.bag_id = bag_id;
        return bag;
    }

    Item& GetOrCreateItem(const GUID& account, const std::string& character, GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id, uint32_t slot)
    {
        return GetOrCreateBag(account, character, hero_id, bag_id).items[slot];
    }

    // 仅查找解析：返回物品所在位置，若任何层级缺失则返回 nullopt。
    std::optional<ItemLoc> FindItemLoc(const GUID& account, const std::string& character, GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id, uint32_t slot)
    {
        Account* acc = FindAccount(account);
        if (!acc) return std::nullopt;
        ItemLoc loc;
        loc.account = acc;
        loc.bag_id = bag_id;
        loc.slot = slot;
        if (IsChestBag(bag_id)) {
            const auto bit = acc->chest.find(bag_id);
            if (bit == acc->chest.end()) return std::nullopt;
            loc.bag = &bit->second;
        }
        else {
            const auto cit = acc->characters.find(character);
            if (cit == acc->characters.end()) return std::nullopt;
            loc.character = &cit->second;
            if (IsOnHero(hero_id)) {
                const auto hit = cit->second.heroes.find(hero_id);
                if (hit == cit->second.heroes.end()) return std::nullopt;
                loc.bag = &hit->second.bag;
            }
            else {
                const auto bit = cit->second.bags.find(bag_id);
                if (bit == cit->second.bags.end()) return std::nullopt;
                loc.bag = &bit->second;
            }
        }
        const auto iit = loc.bag->items.find(slot);
        if (iit == loc.bag->items.end()) return std::nullopt;
        loc.item = &iit->second;
        return loc;
    }

    // 账户-角色对的空闲槽记录。"(仓库)" -> 账户的仓库记录。
    // 若记录未知则返回 nullptr。
    FreeSlotInfo* FindFreeSlots(const GUID& account, const std::string& character)
    {
        Account* acc = FindAccount(account);
        if (!acc) return nullptr;
        if (character == "(仓库)") return acc->chest_free_slots.known ? &acc->chest_free_slots : nullptr;
        const auto it = acc->characters.find(character);
        if (it == acc->characters.end() || !it->second.free_slots.known) return nullptr;
        return &it->second.free_slots;
    }
    FreeSlotInfo& GetOrCreateFreeSlots(const GUID& account, const std::string& character)
    {
        Account& acc = GetOrCreateAccount(account);
        if (character == "(仓库)") {
            acc.chest_free_slots.known = true;
            return acc.chest_free_slots;
        }
        Character& ch = acc.characters[character];
        ch.name = character;
        ch.free_slots.known = true;
        return ch.free_slots;
    }

    // 从整个层次结构（所有账户）重建展平的 ItemRef 视图。
    void RebuildItemRefs()
    {
        item_refs.clear();
        for (auto& [key, acc] : accounts) {
            for (auto& [bag_id, bag] : acc.chest) {
                for (auto& [slot, item] : bag.items) {
                    ItemRef r;
                    r.account = &acc;
                    r.bag_id = bag_id;
                    r.slot = slot;
                    r.item = &item;
                    r.character_name = "(仓库)";
                    r.location = BAG_NAME[(int)bag_id];
                    item_refs.push_back(std::move(r));
                }
            }
            for (auto& [name, ch] : acc.characters) {
                for (auto& [bag_id, bag] : ch.bags) {
                    for (auto& [slot, item] : bag.items) {
                        ItemRef r;
                        r.account = &acc;
                        r.character = &ch;
                        r.bag_id = bag_id;
                        r.slot = slot;
                        r.item = &item;
                        r.character_name = ch.name;
                        r.location = "(玩家)";
                        item_refs.push_back(std::move(r));
                    }
                }
                for (auto& [hero_id, hero] : ch.heroes) {
                    for (auto& [slot, item] : hero.bag.items) {
                        ItemRef r;
                        r.account = &acc;
                        r.character = &ch;
                        r.hero = &hero;
                        r.bag_id = hero.bag.bag_id;
                        r.hero_id = hero_id;
                        r.slot = slot;
                        r.item = &item;
                        r.character_name = ch.name;
                        r.location = Resources::GetHeroName(hero_id)->string();
                        item_refs.push_back(std::move(r));
                    }
                }
            }
        }
    }

    void SortSlots(ImGuiTableSortSpecs* sort_specs)
    {
        // 为每个拥有已知空闲槽数据的角色和每个账户仓库构建一行，
        // 放入稳定的支持向量中，然后对指向它的指针进行排序。
        slot_rows.clear();
        for (auto& [key, acc] : accounts) {
            if (acc.chest_free_slots.known) {
                SlotRow row;
                row.account = acc.uuid;
                row.character = "(仓库)";
                row.account_representing_character = acc.account_representing_character;
                row.max_inventory = acc.chest_free_slots.max_inventory;
                row.occupied_inventory = acc.chest_free_slots.occupied_inventory;
                slot_rows.push_back(std::move(row));
            }
            for (auto& [name, ch] : acc.characters) {
                if (!ch.free_slots.known) continue;
                SlotRow row;
                row.account = acc.uuid;
                row.character = ch.name;
                row.account_representing_character = acc.account_representing_character;
                row.max_inventory = ch.free_slots.max_inventory;
                row.max_equipment = ch.free_slots.max_equipment;
                row.occupied_inventory = ch.free_slots.occupied_inventory;
                row.occupied_equipment = ch.free_slots.occupied_equipment;
                slot_rows.push_back(std::move(row));
            }
        }
        free_slots_sorted = std::set<SlotRow*, SlotCompare>(SlotCompare{sort_specs});
        for (auto& row : slot_rows) {
            if (settings.hide_other_accounts && !memeq(&row.account, &current_account)) {
                continue;
            }
            free_slots_sorted.insert(&row);
        }
        if (sort_specs) sort_specs->SpecsDirty = false;
    }
    void SortAndFilterInventory(ImGuiTableSortSpecs* sort_specs)
    {
        inventory_sorted.clear();
        filtered_item_count = 0;

        auto name_filter = std::string(name_filter_buf);
        auto location_filter = std::string(location_filter_buf);
        auto model_ID_filter = std::string(model_ID_filter_buf);
        auto item_filter = std::string(item_filter_buf);
        bool name_is_lower = std::all_of(name_filter.begin(), name_filter.end(), [](unsigned char c) {
            return !std::isupper(c);
        });
        bool location_is_lower = std::all_of(location_filter.begin(), location_filter.end(), [](unsigned char c) {
            return !std::isupper(c);
        });
        bool item_is_lower = std::all_of(item_filter.begin(), item_filter.end(), [](unsigned char c) {
            return !std::isupper(c);
        });

        const auto item_filter_w = TextUtils::StringToWString(item_filter);


        RebuildItemRefs();
        bool any_decoding = false; // 持续重新排序直到每个可见描述都已解码
        std::unordered_map<std::wstring, size_t> merged_stacks{};
        for (auto& r : item_refs) {
            if (settings.hide_other_accounts && r.account->uuid != current_account) continue;
            if (settings.hide_equipment && (r.bag_id == GW::Constants::Bag::Equipped_Items || r.item->equipped)) continue;
            if (settings.hide_equipment_pack && r.bag_id == GW::Constants::Bag::Equipment_Pack) continue;
            if (settings.hide_hero_armor && IsHeroArmor(r.hero_id, r.slot)) continue;
            if (settings.hide_unclaimed_items && r.bag_id == GW::Constants::Bag::Unclaimed_Items) continue;

            if (!name_filter.empty()) {
                const auto character_check = name_is_lower ? TextUtils::ToLower(r.character_name) : r.character_name;
                if (!character_check.contains(name_filter)) continue;
            }
            if (!location_filter.empty()) {
                const auto location_check = location_is_lower ? TextUtils::ToLower(r.location) : r.location;
                if (!location_check.contains(location_filter)) continue;
            }
            if (!model_ID_filter.empty() && model_ID_filter != std::to_string(r.item->model_id)) continue;
            // 访问 EncString 会惰性启动可见物品的解码。
            const std::wstring& desc = r.item->description.wstring();
            if (!item_filter_w.empty()) {
                const auto description_check = item_is_lower ? TextUtils::ToLower(desc) : desc;
                if (!description_check.contains(item_filter_w)) continue;
            }
            if (r.item->description.IsDecoding()) any_decoding = true;

            auto merge_id = std::to_wstring(r.item->model_id) + desc;
            if (!settings.merge_stacks || !merged_stacks.contains(merge_id)) {
                merged_stacks[merge_id] = inventory_sorted.size();
                inventory_sorted.push_back(MergeStack(current_account, r.item->description.string()));
            }
            MergeStack* ms = &inventory_sorted[merged_stacks[merge_id]];
            ms->quantity += r.item->quantity;
            ms->i.insert(&r);
        }
        filtered_item_count = inventory_sorted.size();
        for (auto& ims : inventory_sorted) ims.GetDescription();

        if (inventory_sorted.size() > 1) std::sort(inventory_sorted.begin(), inventory_sorted.end(), ItemCompare{sort_specs, current_account});

        if (sort_specs) sort_specs->SpecsDirty = false;
        needs_sorting = any_decoding;
        sort_awaiting_decode = any_decoding;
        if (any_decoding) decode_sort_timer = TIMER_INIT();
    }

    bool CheckIniDirty(InventoryFile* ini, std::filesystem::file_time_type write_time)
    {
        if (write_time != ini->last_change_time) {
            ini->last_change_time = write_time;
            ini->is_loaded = false;
            return true;
        }
        return false;
    }


    // 账户库存文件的确定性磁盘位置。每个账户一个 JSON 文件，
    // 以账户 GUID 命名——例如 "inventories/tmp<account-uuid>.json"。
    std::filesystem::path AccountIniPath(const GUID& account)
    {
        const auto name = L"tmp" + TextUtils::StringToWString(TextUtils::GuidToString(&account)) + L".json";
        return Resources::GetPath(L"inventories", name);
    }

    bool IsInventoryIniFilename(const std::filesystem::path& path)
    {
        if (path.extension() != L".json") return false;
        const std::string stem = path.stem().string();
        if (!stem.starts_with("tmp")) return false;
        GUID guid{};
        if (!TextUtils::StringToGuid(stem.substr(3), &guid)) return false;
        return AccountIniPath(guid).filename() == path.filename();
    }

    InventoryFile* GetIni(const std::string& ini_ID, const GUID& account)
    {
        if (const auto found = ini_by_character.find(ini_ID); found != ini_by_character.end()) {
            return found->second;
        }
        // 此会话中首次触及此账户：绑定到其规范文件，
        // 若已存在则重用该路径的 InventoryFile。
        const auto path = AccountIniPath(account);
        InventoryFile* ini;
        if (const auto existing = ini_by_path.find(path); existing != ini_by_path.end()) {
            ini = existing->second.get();
        }
        else {
            Resources::EnsureFolderExists(Resources::GetPath(L"inventories"));
            auto owned = std::make_shared<InventoryFile>(path);
            ini = owned.get();
            ini_by_path[path] = std::move(owned);
        }
        ini->ini_ID = ini_ID;
        ini->account = account;
        ini_by_character[ini_ID] = ini;
        return ini;
    }

    struct LoadResult {
        std::filesystem::path path;
        GUID account{};
        AccountJson account_json;
        bool success = false;
    };

    struct BatchLoadState {
        std::mutex mutex;
        std::vector<LoadResult> results;
        std::atomic<int> tasks_remaining{0};
    };

    // 将宽游戏字符串编码为固定宽度 4 位十六进制编码单元，无分隔符
    //（无损，且比 GuiUtils::ArrayToIni 的空格分隔形式小约 20%）。
    std::string EncodeDescription(const std::wstring& w)
    {
        std::string s;
        s.reserve(w.size() * 4);
        char buf[5];
        for (const wchar_t c : w) {
            snprintf(buf, sizeof(buf), "%04x", (unsigned)(uint16_t)c);
            s += buf;
        }
        return s;
    }
    std::wstring DecodeDescription(const std::string& s)
    {
        std::wstring w;
        w.reserve(s.size() / 4);
        for (size_t i = 0; i + 4 <= s.size(); i += 4) {
            unsigned v = 0;
            if (std::from_chars(s.data() + i, s.data() + i + 4, v, 16).ec != std::errc{}) break;
            w.push_back(static_cast<wchar_t>(v));
        }
        return w;
    }

    ItemJson ToJson(const Item& item)
    {
        ItemJson j;
        j.model_id = item.model_id;
        j.model_file_id = item.model_file_id;
        j.interaction = item.interaction;
        j.quantity = item.quantity;
        if (item.equipped) j.equipped = item.equipped; // 省略常见默认值
        j.description = EncodeDescription(item.description.encoded());
        return j;
    }
    FreeSlotsJson ToJson(const FreeSlotInfo& fs)
    {
        return FreeSlotsJson{fs.max_inventory, fs.max_equipment, fs.occupied_inventory, fs.occupied_equipment};
    }

    // 从层次结构构建账户的可序列化视图。空角色（无物品且无已知空闲槽）被省略。
    AccountJson BuildAccountJson(const Account& acc)
    {
        AccountJson aj;
        aj.account = TextUtils::GuidToString(&acc.uuid);
        aj.representing_character = acc.account_representing_character;
        aj.chest.anniversary_pane_active = acc.anniversary_pane_active;
        if (acc.chest_free_slots.known) aj.chest.free_slots = ToJson(acc.chest_free_slots);
        for (auto& [bag_id, bag] : acc.chest)
            for (auto& [slot, item] : bag.items)
                aj.chest.bags[(uint32_t)bag_id][slot] = ToJson(item);
        for (auto& [name, ch] : acc.characters) {
            CharacterJson cj;
            bool any = false;
            if (ch.free_slots.known) {
                cj.free_slots = ToJson(ch.free_slots);
                any = true;
            }
            for (auto& [bag_id, bag] : ch.bags)
                for (auto& [slot, item] : bag.items) {
                    cj.bags[(uint32_t)bag_id][slot] = ToJson(item);
                    any = true;
                }
            for (auto& [hero_id, hero] : ch.heroes)
                for (auto& [slot, item] : hero.bag.items) {
                    cj.heroes[(uint32_t)hero_id][slot] = ToJson(item);
                    any = true;
                }
            if (any) aj.characters[name] = std::move(cj);
        }
        return aj;
    }

    bool AccountJsonHasData(const AccountJson& aj)
    {
        return !aj.characters.empty() || !aj.chest.bags.empty() || aj.chest.free_slots.has_value();
    }

    void ApplyItemJson(Item& item, const ItemJson& j)
    {
        item.model_id = j.model_id;
        item.model_file_id = j.model_file_id;
        item.interaction = j.interaction;
        item.quantity = j.quantity;
        item.equipped = j.equipped.value_or(0);
        const std::wstring enc = DecodeDescription(j.description);
        item.description.reset(enc.c_str()); // EncString 惰性解码以供显示
        // 纹理在 Draw 中按需获取，因此加载不会为每个存储的物品解码图标
    }

    void ApplyFreeSlots(FreeSlotInfo& fs, const FreeSlotsJson& j)
    {
        fs.known = true;
        fs.max_inventory = j.max_inventory;
        fs.max_equipment = j.max_equipment;
        fs.occupied_inventory = j.occupied_inventory;
        fs.occupied_equipment = j.occupied_equipment;
    }

    // 从解析的账户文件填充层次结构（主线程）。
    void ApplyAccountJson(const GUID& account, const AccountJson& aj)
    {
        Account& acc = GetOrCreateAccount(account);
        if (!aj.representing_character.empty()) acc.account_representing_character = aj.representing_character;
        acc.anniversary_pane_active = aj.chest.anniversary_pane_active;
        if (aj.chest.free_slots) ApplyFreeSlots(acc.chest_free_slots, *aj.chest.free_slots);
        for (const auto& [bag_id, bag] : aj.chest.bags)
            for (const auto& [slot, item] : bag)
                ApplyItemJson(GetOrCreateItem(account, "(仓库)", GW::Constants::HeroID::NoHero, (GW::Constants::Bag)bag_id, slot), item);
        for (const auto& [name, cj] : aj.characters) {
            Character& ch = acc.characters[name];
            ch.name = name;
            if (cj.free_slots) ApplyFreeSlots(ch.free_slots, *cj.free_slots);
            for (const auto& [bag_id, bag] : cj.bags)
                for (const auto& [slot, item] : bag)
                    ApplyItemJson(GetOrCreateItem(account, name, GW::Constants::HeroID::NoHero, (GW::Constants::Bag)bag_id, slot), item);
            for (const auto& [hero_id, bag] : cj.heroes)
                for (const auto& [slot, item] : bag)
                    ApplyItemJson(GetOrCreateItem(account, name, (GW::Constants::HeroID)hero_id, GW::Constants::Bag::Equipped_Items, slot), item);
        }
    }

    bool ReadFileToString(const std::filesystem::path& path, std::string& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }

    // 工作线程：将一个账户文件读入 LoadResult（不访问层次结构）。
    LoadResult LoadIniFile(const std::filesystem::path& path, bool only_foreign, const GUID& account)
    {
        LoadResult r;
        r.path = path;
        std::string contents;
        if (!ReadFileToString(path, contents)) return r;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (glz::read<opts>(r.account_json, contents)) return r; // 解析错误
        GUID file_account{};
        if (!TextUtils::StringToGuid(r.account_json.account, &file_account)) return r;
        if (only_foreign && file_account == account) return r; // 在仅刷新外部时跳过自己的账户
        r.account = file_account;
        r.success = true;
        return r;
    }

    void ApplyLoadResult(const LoadResult& r)
    {
        const auto it = ini_by_path.find(r.path);
        auto* ini = it != ini_by_path.end() ? it->second.get() : nullptr;
        if (!ini || !r.success) return;
        ini->ini_ID = GetIniID(r.account, "");
        ini->account = r.account;
        ini_by_character[ini->ini_ID] = ini;
        ApplyAccountJson(r.account, r.account_json);
        ini->is_loaded = true;
    }

    void LoadFromFiles(bool only_foreign)
    {
        Resources::EnsureFolderExists(Resources::GetPath(L"inventories"));
        std::unordered_set<std::filesystem::path> visited;

        if (only_foreign) {
            for (auto& [path, ini] : ini_by_path) {
                if (ini->account != current_account) ini->is_loaded = false;
            }
            // 丢弃所有外部账户（物品 + 空闲槽）；它们将在下面重新加载
            for (auto it = accounts.begin(); it != accounts.end();)
                it = it->second.uuid != current_account ? accounts.erase(it) : std::next(it);
        }
        else {
            inventory_lookup.clear();
        }

        std::vector<std::filesystem::path> to_load;
        for (const auto& file : std::filesystem::directory_iterator{Resources::GetPath(L"inventories")}) {
            const auto path = file.path();
            if (!IsInventoryIniFilename(path)) continue; // 忽略旧版/无关文件
            visited.insert(path);
            if (!ini_by_path.contains(path)) ini_by_path[path] = std::make_shared<InventoryFile>(path);
            auto* ini = ini_by_path[path].get();
            if (only_foreign && ini->account == current_account) continue;
            const bool dirty = CheckIniDirty(ini, file.last_write_time());
            if (!dirty && ini->is_loaded) continue;
            to_load.push_back(path);
        }

        for (auto& [path, ini] : ini_by_path) {
            if (!visited.contains(path)) {
                ini->is_loaded = false;
            }
        }

        if (to_load.empty()) {
            needs_sorting = true;
            return;
        }

        const size_t batch_count = std::min<size_t>(4, to_load.size());
        auto state = std::make_shared<BatchLoadState>();
        state->tasks_remaining = static_cast<int>(batch_count);
        const GUID captured_account = current_account;

        for (size_t b = 0; b < batch_count; b++) {
            std::vector<std::filesystem::path> batch;
            for (size_t i = b; i < to_load.size(); i += batch_count)
                batch.push_back(to_load[i]);

            Resources::EnqueueWorkerTask([batch, only_foreign, captured_account, state] {
                for (const auto& path : batch) {
                    auto r = LoadIniFile(path, only_foreign, captured_account);
                    std::scoped_lock lock(state->mutex);
                    state->results.push_back(std::move(r));
                }
                --state->tasks_remaining;
            });
        }

        const clock_t load_start = TIMER_INIT();
        while (state->tasks_remaining > 0 && TIMER_DIFF(load_start) < 15000)
            Sleep(10);

        for (const auto& r : state->results)
            ApplyLoadResult(r);

        needs_sorting = true;
    }

    void EnqueueIniIO(std::shared_ptr<InventoryFile> ini)
    {
        if (ini->io_pending.exchange(true)) return;
        Resources::EnqueueWorkerTask([ini] {
            for (;;) {
                std::string payload;
                bool remove_file = false;
                bool done = false;
                {
                    std::scoped_lock lock(ini->io_mutex);
                    payload = std::move(ini->queued_json);
                    ini->queued_json.clear();
                    remove_file = ini->delete_requested;
                    ini->delete_requested = false;
                    if (remove_file || payload.empty()) {
                        ini->io_pending = false;
                        done = true;
                    }
                }
                if (done) {
                    if (remove_file) DeleteFileW(ini->location_on_disk.wstring().c_str());
                    return;
                }
                std::ofstream f(ini->location_on_disk, std::ios::binary | std::ios::trunc);
                if (!f) {
                    Log::Error("Account Inventory: Failed to save inventory file. Inventory tracking data will be lost.");
                    std::scoped_lock lock(ini->io_mutex);
                    ini->queued_json = std::move(payload);
                    ini->io_pending = false;
                    return;
                }
                f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
                f.close();
                std::error_code ec;
                const auto write_time = std::filesystem::last_write_time(ini->location_on_disk, ec);
                std::scoped_lock lock(ini->io_mutex);
                ini->last_change_time = write_time;
            }
        });
    }

    // Write (or delete) the JSON file for one account.
    void SyncAccountFile(const std::string& ini_ID, const GUID& account)
    {
        if (ini_ID.empty()) return;
        InventoryFile* ini = GetIni(ini_ID, account);
        Account* acc = FindAccount(account);
        AccountJson aj;
        if (acc) aj = BuildAccountJson(*acc);
        const auto path = ini->location_on_disk;
        if (!acc || !AccountJsonHasData(aj)) {
            {
                std::scoped_lock lock(ini->io_mutex);
                ini->queued_json.clear();
                ini->delete_requested = true;
            }
            EnqueueIniIO(ini_by_path[path]);
            ini_by_character.erase(ini_ID);
            ini_by_path.erase(path);
            return;
        }
        const auto compact = glz::write_json(aj).value_or(std::string{});
        if (compact.empty()) {
            Log::Error("账户库存：序列化库存失败。库存跟踪数据将丢失。");
            return;
        }
        const std::string json = glz::prettify_json(compact);
        {
            std::scoped_lock lock(ini->io_mutex);
            ini->queued_json = std::move(json);
        }
        EnqueueIniIO(ini_by_path[path]);
    }

    void SaveToFiles(bool include_foreign)
    {
        if (include_foreign) {
            std::vector<std::pair<std::string, GUID>> targets;
            for (auto& [path, file] : ini_by_path)
                targets.emplace_back(file->ini_ID, file->account);
            for (const auto& [ini_ID, account] : targets)
                SyncAccountFile(ini_ID, account);
        }
        else {
            // 只有当前账户会被修改；外部账户是只读的。
            const std::string ini_ID = GetIniID(current_account, "");
            if (inventory_dirty.contains(ini_ID)) SyncAccountFile(ini_ID, current_account);
        }
        inventory_dirty.clear();
    }

    // 为实时物品构建编码描述字符串（名称 + 简写属性）。
    // 这是我们持久化的规范形式；可读文本从此解码。
    std::wstring GetItemEncDescription(GW::Item* item)
    {
        std::wstring enc;
        switch (item->type) {
            case GW::Constants::ItemType::Headpiece:
            case GW::Constants::ItemType::Boots:
            case GW::Constants::ItemType::Chestpiece:
            case GW::Constants::ItemType::Gloves:
            case GW::Constants::ItemType::Leggings:
                // ShorthandItemDescription 包含这些的物品名称
                break;
            default:
                // 默认使用 single_item_name，以便 merge_stacks 可以合并单件和多件物品的堆叠。
                if (item->single_item_name) {
                    enc += item->single_item_name;
                }
                else if (item->complete_name_enc) {
                    enc += item->complete_name_enc;
                }
                else if (item->name_enc) {
                    enc += item->name_enc;
                }
        }
        if (item->info_string) {
            auto shorthand_description = ToolboxUtils::ShorthandItemDescription(item);
            if (shorthand_description.find(L"\xA3E\x10A\xA8A\x10A\xA59\x1\x10B") != 0) {
                if (!enc.empty()) {
                    enc += L"\x2\x102\x2";
                }
                enc += shorthand_description;
            }
        }
        return enc;
    }

    GW::Constants::HeroID GetHeroIDForInventory(GW::Inventory* inv)
    {
        const auto hero_ids = GetPartyHeroIDs();

        for (auto hero_id : hero_ids) {
            if (GW::Items::GetHeroInventory(hero_id) == inv) {
                return hero_id;
            }
        }
        return GW::Constants::HeroID::NoHero;
    }

    void AddItem(uint32_t item_id)
    {
        auto item = GW::Items::GetItemById(item_id);
        if (!(item && item->bag)) return;

        // 收集此物品存储位置的信息，即：
        // 账户、玩家角色、英雄、背包、背包内槽位

        ItemPath path;
        path.account = current_account;
        path.bag_id = item->bag->bag_id();
        if (IsChestBag(path.bag_id)) {
            path.character = "(仓库)";
        }
        else {
            path.character = GetCurrentPlayerNameS();
            if (path.character.empty()) {
                path.character = "不可用";
            }
        }
        path.slot = item->slot;

        path.hero_id = GW::Constants::HeroID::NoHero;
        if (item->bag->inventory != GW::Items::GetInventory()) {
            if (initializing) return;
            path.hero_id = GetHeroIDForInventory((GW::Inventory*)item->bag->inventory);
            if (path.hero_id == GW::Constants::HeroID::NoHero) {
                Log::Log("账户库存：确定装备物品的英雄失败。");
                return;
            }
        }
        // hero_id 变通方案结束

        FreeSlotInfo* fs = FindFreeSlots(current_account, path.character);
        if (fs) {
            if (path.bag_id == GW::Constants::Bag::Equipment_Pack) {
                fs->occupied_equipment++;
            }
            else if (BagCanHoldAnything(path.bag_id)) {
                fs->occupied_inventory++;
            }
        }
        if (auto existing = FindItemLoc(path.account, path.character, path.hero_id, path.bag_id, path.slot)) {
            const auto o_item_id = existing->item->item_id;
            // 确保查找条目尚未被地图加载期间加载的另一个物品覆盖。
            if (const auto found = inventory_lookup.find(o_item_id); found != inventory_lookup.end() && found->second.item == existing->item) {
                inventory_lookup.erase(found);
                // 当堆叠被拆分或合并时，源堆叠和目标堆叠将由 GW 重新添加，而无需先删除。
                // 如果我们已经知道源/目标槽位中的物品，则占用空间数实际未变化。
                if (fs) {
                    if (path.bag_id == GW::Constants::Bag::Equipment_Pack) {
                        fs->occupied_equipment--;
                    }
                    else if (BagCanHoldAnything(path.bag_id)) {
                        fs->occupied_inventory--;
                    }
                }
            }
        }

        Account& acc = GetOrCreateAccount(path.account);
        Bag& bag = GetOrCreateBag(path.account, path.character, path.hero_id, path.bag_id);
        Item& it = bag.items[path.slot];
        it.model_id = item->model_id;
        it.model_file_id = item->model_file_id;
        it.interaction = item->interaction;
        it.quantity = item->quantity;
        it.equipped = item->equipped;
        it.item_id = item->item_id;
        // 存储染料以便 Draw 惰性获取着色图标（参见 ApplyItemJson）；此处不解码
        it.dyes = static_cast<uint32_t>(item->dye.dye1) | (static_cast<uint32_t>(item->dye.dye2) << 8)
                | (static_cast<uint32_t>(item->dye.dye3) << 16) | (static_cast<uint32_t>(item->dye.dye4) << 24);
        it.description.reset(GetItemEncDescription(item).c_str()); // EncString 惰性解码以供显示

        ItemLoc loc;
        loc.account = &acc;
        loc.character = IsChestBag(path.bag_id) ? nullptr : &acc.characters[path.character];
        loc.bag_id = path.bag_id;
        loc.slot = path.slot;
        loc.bag = &bag;
        loc.item = &it;
        inventory_lookup[item->item_id] = loc;

        inventory_dirty.insert(GetIniID(path.account, path.character));
        save_dirty_inventories_timer = TIMER_INIT();
        needs_sorting = true;
    }
    bool RemoveItem(uint32_t item_id);
    void ClearMissingItem(const UUID* account, const std::string& character, const GW::Constants::HeroID hero_id, const GW::Constants::Bag bag_id, const uint32_t slot)
    {
        auto loc = FindItemLoc(*account, character, hero_id, bag_id, slot);
        if (!loc) return;
        inventory_dirty.insert(GetIniID(*account, character));
        save_dirty_inventories_timer = TIMER_INIT();
        // 实时物品通过 RemoveItem（删除查找 + 占用）处理。仅存在于 ini 中的物品（item_id 为 0）
        // 不在查找表中，因此直接删除。
        const auto item_id = loc->item->item_id;
        if (!(item_id != 0 && RemoveItem(item_id))) {
            loc->bag->items.erase(slot);
        }
        needs_sorting = true;
    }

    bool RemoveItem(uint32_t item_id)
    {
        const auto found = inventory_lookup.find(item_id);
        if (found == inventory_lookup.end()) return false;

        const ItemLoc loc = found->second;
        const std::string character = loc.character ? loc.character->name : "(仓库)";
        if (FreeSlotInfo* fs = FindFreeSlots(loc.account->uuid, character)) {
            if (loc.bag_id == GW::Constants::Bag::Equipment_Pack) {
                fs->occupied_equipment--;
            }
            else if (BagCanHoldAnything(loc.bag_id)) {
                fs->occupied_inventory--;
            }
        }
        auto ini_id = GetIniID(loc.account->uuid, character);
        loc.bag->items.erase(loc.slot);
        inventory_lookup.erase(found);
        needs_sorting = true;
        inventory_dirty.insert(std::move(ini_id));
        save_dirty_inventories_timer = TIMER_INIT();
        return true;
    }

} // namespace



void AccountInventoryWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kItemUpdated,
        GW::UI::UIMessage::kEquipmentSlotUpdated,
        GW::UI::UIMessage::kInventorySlotUpdated,
        GW::UI::UIMessage::kEquipmentSlotCleared,
        GW::UI::UIMessage::kInventorySlotCleared,
        GW::UI::UIMessage::kPartyAddHero,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kLogout
    };
    for (auto message_id : ui_messages) {
        RegisterUIMessageCallback(&OnUIMessage_HookEntry, (GW::UI::UIMessage)message_id, [this](GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
            switch (message_id) {
                case GW::UI::UIMessage::kItemUpdated: {
                    const auto p = (GW::UI::UIPacket::kItemUpdated*)wparam;
                    AddItem(p->item_id);
                    break;
                }
                case GW::UI::UIMessage::kEquipmentSlotUpdated:
                case GW::UI::UIMessage::kInventorySlotUpdated: {
                    const auto p = (GW::UI::UIPacket::kInventorySlotUpdated*)wparam;
                    AddItem(p->item_id);
                    break;
                }
                case GW::UI::UIMessage::kEquipmentSlotCleared:
                case GW::UI::UIMessage::kInventorySlotCleared: {
                    const auto p = (GW::UI::UIPacket::kInventorySlotUpdated*)wparam;
                    RemoveItem(p->item_id);
                    break;
                }
                case GW::UI::UIMessage::kPartyAddHero: {
                    const auto hero = ((struct GW::HeroPartyMember**)wparam)[1];
                    const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
                    if (!me || !hero) break;
                    if (hero->owner_player_id != me->login_number) break;
                    HandleHeroBag(((GW::Constants::HeroID*)wparam)[7]);
                    break;
                }
                case GW::UI::UIMessage::kMapChange:
                    PreMapLoad();
                    break;
                case GW::UI::UIMessage::kMapLoaded:
                    PostMapLoad();
                    break;
                case GW::UI::UIMessage::kLogout: {
                    // 为可能的账户切换做准备。
                    SaveToFiles(false);
                    LoadFromFiles(true);
                    show_delete_note = false;
                    // 不能在此处重置 reroll_stage，因为 reroll 触发 kLogout。
                    // 而是在 PreMapLoad 中检查账户是否已更改。
                    break;
                }
            }
        });
    }
    initializing = true;
    LoadFromFiles(false);
    auto ic = GW::GetItemContext();
    if (ic) {
        // 伪造一次地图加载以清除缺失物品并删除已删除的角色。
        PreMapLoad();
        for (const auto& i : ic->item_array) {
            if (i) {
                AddItem(i->item_id);
            }
        }
        PostMapLoad();
    }
    initializing = false;
}

void AccountInventoryWindow::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
    accounts.clear();
    inventory_lookup.clear();
    item_refs.clear();
    inventory_sorted.clear();
    ini_by_character.clear();
    ini_by_path.clear();
    inventory_dirty.clear();
    slot_rows.clear();
    free_slots_sorted.clear();
    last_character = "";
    last_available_chars.clear();
    map_loaded_delayed_trigger = false;
    show_delete_note = false;
    ToolboxWindow::Terminate();
}

void InventoryScanner::Update()
{
    if (current_stage == InventoryScanner::Stage::None) return;
    if (TIMER_DIFF(stage_set_at) > 10000) {
        Log::Warning("InventoryScanner：阶段 %d 超时", current_stage);
        Cancel();
    }

    const auto is_map_loaded = GW::Map::GetIsMapLoaded() && !GW::UI::IsLoadingScreenShown();
    if (!is_map_loaded) return;
    switch (current_stage) {
        case InventoryScanner::Stage::Start: {
            original_player = GW::AccountMgr::GetCurrentPlayerName();
            original_heroes = GetPartyHeroIDs();
            reroll_char_queue.clear();

            reroll_char_queue.push_back(original_player);

            auto available_characters = GW::AccountMgr::GetAvailableChars();
            for (const auto& available_char : *available_characters) {
                if (GWToolbox::ShouldDisableToolbox(available_char.map_id()) || available_char.is_pvp()) continue;
                if (original_player == available_char.player_name) continue;
                reroll_char_queue.push_back(available_char.player_name);
            }
            Set(InventoryScanner::Stage::NextCharacter);
        } break;
        case InventoryScanner::Stage::NextCharacter: {
            if (reroll_char_queue.empty()) {
                for (auto hero_id : original_player_heroes) {
                    GW::PartyMgr::AddHero(hero_id);
                }
                SaveToFiles(false);
                Cancel();
                Log::Info("库存扫描完成");
                break;
            }
            current_reroll_char = reroll_char_queue.back();
            reroll_char_queue.pop_back();
            RerollWindow::Instance().Reroll(current_reroll_char.c_str(), false, false, true, false);
            Set(InventoryScanner::Stage::WaitForCharacterLoad);
        } break;
        case InventoryScanner::Stage::WaitForCharacterLoad: {
            if (!wcseq(GW::AccountMgr::GetCurrentPlayerName(), current_reroll_char.c_str())) break;
            original_heroes = GetPartyHeroIDs();
            queued_hero_ids.clear();
            const auto w = GW::GetWorldContext();
            const auto h = w ? &w->hero_info : nullptr;
            if (h) {
                for (auto& hero : *h) {
                    if (ToolboxUtils::IsHeroUnlocked(hero.hero_id)) queued_hero_ids.push_back(hero.hero_id);
                }
            }
            GW::PartyMgr::LeaveParty();
            Set(InventoryScanner::Stage::WaitForEmptyParty);
        } break;
        case InventoryScanner::Stage::WaitForEmptyParty: {
            const auto party = GW::PartyMgr::GetPartyInfo();
            if (!(party && party->GetPartySize() == 1)) break; // 队伍未空

            // 添加任何需要检查的排队英雄
            if (queued_hero_ids.empty()) {
                // 已检查此角色的所有英雄，恢复原始英雄
                for (auto hero_id : original_heroes) {
                    GW::PartyMgr::AddHero(hero_id);
                }
                Set(InventoryScanner::Stage::DoRestoreHeroes);
                break;
            }
            const auto map_info = GW::Map::GetMapInfo();
            const auto max_heroes_per_batch = std::min(7u, map_info && map_info->max_party_size > 1 ? map_info->max_party_size - 1 : 1);

            heroes_pending_load.clear();
            while (!queued_hero_ids.empty() && heroes_pending_load.size() < max_heroes_per_batch) {
                const auto next_hero = queued_hero_ids.back();
                queued_hero_ids.pop_back();
                if (!GW::PartyMgr::AddHero(next_hero)) {
                    Cancel("添加英雄失败");
                    break;
                }
                heroes_pending_load.push_back(next_hero);
            }
            Set(InventoryScanner::Stage::WaitForHeroLoad);
        } break;
        case InventoryScanner::Stage::WaitForHeroLoad: {
            auto waiting = GetPartyHeroIDs().size() != heroes_pending_load.size();
            for (auto hero_id : heroes_pending_load) {
                if (!GW::Items::GetHeroInventory(hero_id)) {
                    waiting = true;
                    break;
                }
            }
            if (waiting) break;
            // 假设在此阶段我们的代码已将英雄的库存添加到列表中？
            GW::PartyMgr::KickAllHeroes();
            Set(InventoryScanner::Stage::WaitForEmptyParty);
        } break;
        case InventoryScanner::Stage::DoRestoreHeroes: {
            if (GetPartyHeroIDs().size() == original_heroes.size()) Set(InventoryScanner::Stage::NextCharacter);
        } break;
    }
}

void ItemReroller::Update()
{
    if (current_stage == ItemReroller::Stage::None) return;
    if (TIMER_DIFF(stage_set_at) > 10000) {
        Log::Warning("ItemReroller：阶段 %d 超时", current_stage);
        Cancel();
    }

    const auto is_map_loaded = GW::Map::GetIsMapLoaded() && !GW::UI::IsLoadingScreenShown();
    if (!is_map_loaded) return;
    switch (current_stage) {
        case ItemReroller::Stage::Start: {
            if (!memeq(&item.account, &current_account)) {
                Cancel("物品属于另一个账户");
                break;
            }
            move = move && !IsHeroArmor(item.hero_id, item.slot);
            if (!IsChestBag(item.bag_id)) {
                const auto current_player = GW::AccountMgr::GetCurrentPlayerName();
                if (TextUtils::WStringToString(current_player) != item.character) {
                    RerollWindow::Instance().Reroll(TextUtils::StringToWString(item.character).c_str(), false, false, true, false);
                }
            }

            Set(ItemReroller::Stage::WaitForCharacterLoad);
        } break;
        case ItemReroller::Stage::WaitForCharacterLoad: {
            if (!IsChestBag(item.bag_id)) {
                const auto current_player = GW::AccountMgr::GetCurrentPlayerName();
                if (TextUtils::WStringToString(current_player) != item.character) break;
            }
            if (item.hero_id == GW::Constants::HeroID::NoHero) {
                Set(ItemReroller::Stage::WaitForHeroLoad);
                break;
            }
            const auto hero_agent = GW::PartyMgr::GetHeroInfo(item.hero_id);
            if (hero_agent && GW::PartyMgr::IsAgentInParty(hero_agent->agent_id)) {
                Set(ItemReroller::Stage::WaitForHeroLoad);
                break;
            }
            const auto map_info = GW::Map::GetMapInfo();
            const auto party_info = GW::PartyMgr::GetPartyInfo();
            if (map_info && party_info && map_info->max_party_size == party_info->GetPartySize()) {
                for (auto& h : party_info->heroes) {
                    if (h.owner_player_id == GW::PlayerMgr::GetPlayerNumber()) {
                        GW::PartyMgr::KickHero(h.hero_id);
                        break;
                    }
                }
            }
            GW::PartyMgr::AddHero(item.hero_id);
            Set(ItemReroller::Stage::WaitForHeroLoad);
        } break;
        case ItemReroller::Stage::WaitForHeroLoad: {
            if (item.hero_id != GW::Constants::HeroID::NoHero) {
                const auto hero_agent = GW::PartyMgr::GetHeroInfo(item.hero_id);
                if (!(hero_agent && GW::PartyMgr::IsAgentInParty(hero_agent->agent_id))) break;
            }
            if (IsChestBag(item.bag_id)) {
                uint32_t pane;
                if (item.bag_id == GW::Constants::Bag::Material_Storage)
                    pane = (uint32_t)GW::Constants::StoragePane::Material_Storage;
                else
                    pane = (uint32_t)item.bag_id - (uint32_t)GW::Constants::Bag::Storage_1;
                GW::UI::SetPreference(GW::UI::NumberPreference::StorageBagPage, pane);
                GW::Items::OpenXunlaiWindow();
            }
            if (move) {
                auto loc = FindItemLoc(item.account, item.character, item.hero_id, item.bag_id, item.slot);
                if (!loc) break;

                // 只能从当前玩家或仓库移动
                if (item.character != GetCurrentPlayerNameS() && !IsChestBag(item.bag_id)) {
                    Cancel();
                    break;
                }
                InventoryManager::MoveItem((InventoryManager::Item*)GW::Items::GetItemById(loc->item->item_id));
            }
            Cancel();
        } break;
    }
}

void AccountInventoryWindow::Update(float)
{
    inventory_scan.Update();
    item_reroll.Update();
    if (save_dirty_inventories_timer && !inventory_dirty.empty() && TIMER_DIFF(save_dirty_inventories_timer) > SAVE_DIRTY_INVENTORIES_TIMEOUT) {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            SaveToFiles(false);
        }
        else {
            save_dirty_inventories_timer = 0;
        }
    }
}

void AccountInventoryWindow::PreMapLoad()
{
    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();
    inventory_lookup.clear(); // 丢弃现在过时的 ID 缓存

    Account& acc = GetOrCreateAccount(current_account);
    if (acc.account_representing_character.empty()) {
        auto available_characters = GW::AccountMgr::GetAvailableChars();
        if (available_characters->size() > 0) {
            // 按字母顺序排列的第一个角色名称，用于在工具提示中区分多个账户的仓库，而不显示电子邮件地址
            const wchar_t* min = nullptr;
            for (const auto& available_char : *available_characters) {
                if (!min || wcscmp(available_char.player_name, min) < 0) min = available_char.player_name;
            }
            acc.account_representing_character = TextUtils::WStringToString(min ? min : L"");
        }
    }
    std::string characters[] = {"(仓库)", current_character};
    for (auto& character : characters) {
        if (character.empty()) continue;
        FreeSlotInfo& fs = GetOrCreateFreeSlots(current_account, character);
        if (character == current_character) {
            fs.occupied_equipment = 0;
        }
        fs.occupied_inventory = 0;
    }
}

void AccountInventoryWindow::PostMapLoad()
{
    bool character_changed = false;
    map_loaded_delayed_trigger = true;
    map_loaded_delayed_timer = TIMER_INIT();
    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();
    GW::Inventory* gw_inventory = GW::Items::GetInventory();
    if (last_character != current_character) {
        last_character = current_character;
        character_changed = true;
    }

    const auto hero_ids = GetPartyHeroIDs();
    for (const auto& hero_id : hero_ids) {
        HandleHeroBag(hero_id);
    }

    if (gw_inventory) {
        uint32_t max_chest = 0;
        uint32_t max_equipment = 0;
        uint32_t max_inventory = 0;
        bool last_chest_pane_contains_any_item = false;
        for (uint32_t j = 1; j < _countof(gw_inventory->bags); ++j) {
            auto bag = gw_inventory->bags[j];
            const auto bag_id = static_cast<GW::Constants::Bag>(j);
            const auto character = IsChestBag(bag_id) ? "(仓库)" : current_character;
            if (!bag) {
                // 清除之前存在的背包的槽位
                for (uint32_t slot = 0, len = GetMaxBagCapacity(bag_id); slot < len; ++slot) {
                    ClearMissingItem(&current_account, character, GW::Constants::HeroID::NoHero, bag_id, slot);
                }
                continue;
            }
            if (IsChestBag(bag_id)) {
                last_chest_pane_contains_any_item = false;
            }
            for (uint32_t slot = 0; slot < std::size(bag->items); ++slot) {
                auto item = bag->items[slot];
                if (item) {
                    if (IsChestBag(bag_id)) {
                        last_chest_pane_contains_any_item = true;
                    }
                    if (!inventory_lookup.contains(item->item_id)) {
                        AddItem(item->item_id);
                    }
                    // item->equipped 在地图加载时触发 InventorySlotUpdated 时从未设置。
                    // 每次地图加载后手动检查和重新应用。
                    if (const auto found = inventory_lookup.find(item->item_id); found != inventory_lookup.end()) {
                        ItemLoc& loc = found->second;
                        if (loc.item->equipped != item->equipped) {
                            loc.item->equipped = item->equipped;
                            const std::string ch_name = loc.character ? loc.character->name : "(仓库)";
                            inventory_dirty.insert(GetIniID(loc.account->uuid, ch_name));
                        }
                    }
                }
                else {
                    ClearMissingItem(&current_account, character, GW::Constants::HeroID::NoHero, bag_id, slot);
                }
            }
            if (bag_id == GW::Constants::Bag::Equipment_Pack) {
                max_equipment = std::size(bag->items);
            }
            else if (BagCanHoldAnything(bag_id)) {
                if (IsChestBag(bag_id)) {
                    max_chest += std::size(bag->items);
                }
                else {
                    max_inventory += std::size(bag->items);
                }
            }
        }
        if (Account* acc = FindAccount(current_account)) {
            if (const auto it = acc->characters.find(current_character); it != acc->characters.end() && it->second.free_slots.known) {
                it->second.free_slots.max_equipment = max_equipment;
                it->second.free_slots.max_inventory = max_inventory;
            }
            if (acc->chest_free_slots.known) {
                // 由于我们不知道周年仓库面板是否实际可用，
                // 假设它不可用，除非曾经在其中有过至少一个物品。
                if (acc->anniversary_pane_active || last_chest_pane_contains_any_item) {
                    acc->anniversary_pane_active = true;
                }
                else {
                    max_chest -= 25;
                }
                acc->chest_free_slots.max_inventory = max_chest;
            }
        }
    }

    // 删除已删除的角色
    if (character_changed) {
        std::set<std::string> availableChars{};
        const auto chars = GW::AccountMgr::GetAvailableChars();
        for (const auto& availableCharacter : *chars) {
            availableChars.insert(TextUtils::WStringToString(availableCharacter.player_name));
        }
        if (availableChars != last_available_chars) {
            if (Account* acc = FindAccount(current_account)) {
                for (auto it = acc->characters.begin(); it != acc->characters.end();) {
                    if (availableChars.find(it->first) != availableChars.end()) {
                        ++it;
                        continue;
                    }
                    // 角色已在游戏中被删除：丢弃整个节点（其背包、
                    // 英雄和空闲槽）以及指向它的任何实时查找条目。
                    inventory_dirty.insert(GetIniID(current_account, it->first));
                    Character* ch = &it->second;
                    for (auto lit = inventory_lookup.begin(); lit != inventory_lookup.end();)
                        lit = lit->second.character == ch ? inventory_lookup.erase(lit) : std::next(lit);
                    it = acc->characters.erase(it);
                }
            }
        }
        last_available_chars = availableChars;
    }

    needs_sorting = true;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        SaveToFiles(false); // 仅在前哨站保存库存以避免影响游戏体验
    }
}

void AccountInventoryWindow::HandleHeroBag(GW::Constants::HeroID hero_id)
{
    if (initializing) return;
    const auto inventory = GW::Items::GetHeroInventory(hero_id);
    if (!inventory) return;
    for (auto bag_ptr = &inventory->backpack; bag_ptr <= &inventory->equipped_items; ++bag_ptr) {
        const auto bag = *bag_ptr;
        if (!bag) continue;
        if (bag->bag_id() != GW::Constants::Bag::Equipped_Items) {
            Log::Warning("账户库存：英雄库存中出现意外的背包 ID %d", bag->bag_id());
            continue;
        }
        for (uint32_t slot = 0; slot < std::size(bag->items); ++slot) {
            auto item = bag->items[slot];
            if (!item)
                ClearMissingItem(&current_account, current_character, hero_id, bag->bag_id(), slot);
            else
                AddItem(item->item_id);
        }
    }
}

void AccountInventoryWindow::GatherAllInventories()
{
    inventory_scan.Begin();
}

void AccountInventoryWindow::Draw(IDirect3DDevice9*)
{
    const auto font_scale = ImGui::FontScale();
    auto& style = ImGui::GetStyle();
    const float item_spacing = style.ItemInnerSpacing.x;
    const float checkbox_max_width = 160.f * font_scale;
    if (memcmp(&style.Colors[ImGuiCol_Button], &cached_button_color, sizeof(ImVec4)) != 0) {
        cached_button_color = style.Colors[ImGuiCol_Button];
        color_chest_item = HSVRotate(style.Colors[ImGuiCol_Button], 0.333f);
        color_chest_item_hovered = HSVRotate(style.Colors[ImGuiCol_ButtonHovered], 0.333f);
        color_chest_item_active = HSVRotate(style.Colors[ImGuiCol_ButtonActive], 0.333f);
        color_hero_item = HSVRotate(style.Colors[ImGuiCol_Button], 0.166f);
        color_hero_item_hovered = HSVRotate(style.Colors[ImGuiCol_ButtonHovered], 0.166f);
        color_hero_item_active = HSVRotate(style.Colors[ImGuiCol_ButtonActive], 0.166f);
        color_item_foreign = HSVRotate(style.Colors[ImGuiCol_Button], -1.f, 0.4f);
        color_item_hovered_foreign = HSVRotate(style.Colors[ImGuiCol_ButtonHovered], -1.f, 0.4f);
        color_item_active_foreign = HSVRotate(style.Colors[ImGuiCol_ButtonActive], -1.f, 0.4f);
        color_chest_item_foreign = HSVRotate(style.Colors[ImGuiCol_Button], 0.333f, 0.4f);
        color_chest_item_hovered_foreign = HSVRotate(style.Colors[ImGuiCol_ButtonHovered], 0.333f, 0.4f);
        color_chest_item_active_foreign = HSVRotate(style.Colors[ImGuiCol_ButtonActive], 0.333f, 0.4f);
        color_hero_item_foreign = HSVRotate(style.Colors[ImGuiCol_Button], 0.166f, 0.4f);
        color_hero_item_hovered_foreign = HSVRotate(style.Colors[ImGuiCol_ButtonHovered], 0.166f, 0.4f);
        color_hero_item_active_foreign = HSVRotate(style.Colors[ImGuiCol_ButtonActive], 0.166f, 0.4f);
    }
    const auto reroll_stage = inventory_scan.Get();
    if (reroll_stage != InventoryScanner::Stage::None) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f * font_scale, 0.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("账户库存加载进行中")) {
            ImGui::TextWrapped("请不要中断库存加载。");
            if (ImGui::Button("中止！")) {
                inventory_scan.Cancel();
            }
        }
        ImGui::End();
    }

    if (!visible) {
        name_filter_buf[0] = '\0';
        location_filter_buf[0] = '\0';
        model_ID_filter_buf[0] = '\0';
        item_filter_buf[0] = '\0';
        needs_sorting = true;
        return;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760.f * font_scale, 400.f * font_scale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()) || ImGui::IsWindowCollapsed()) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Detailed View", &settings.detailed_view);
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("合并堆叠", &settings.merge_stacks)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("隐藏其他账户", &settings.hide_other_accounts)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("隐藏装备", &settings.hide_equipment)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("隐藏装备包", &settings.hide_equipment_pack)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("隐藏英雄护甲", &settings.hide_hero_armor)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("隐藏未认领物品", &settings.hide_unclaimed_items)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < 110.f * font_scale) ImGui::NewLine();
    if (ImGui::Button("收集所有库存")) {
        ImGui::ConfirmDialog("为了加载所有可用物品，这将遍历\n所有角色和所有英雄。\n如果您有很多角色，这将需要几分钟。\n确定要继续吗？", [](bool result, void*) {
            if (result) AccountInventoryWindow::Instance().GatherAllInventories();
        });
    }

    const auto color_disabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    if (ImGui::CollapsingHeader("空闲槽位")) {
        if (!ImGui::BeginTable("###freeslots", SlotColumnID_Max, ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::End();
            return;
        }
        ImGui::TableSetupColumn("角色", ImGuiTableColumnFlags_WidthFixed, 0.f, SlotColumnID_Character);
        ImGui::TableSetupColumn("背包", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed, 0.f, SlotColumnID_Inventory);
        ImGui::TableSetupColumn("总量", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, SlotColumnID_InventorySize);
        ImGui::TableSetupColumn("装备", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, SlotColumnID_Equipment);
        ImGui::TableSetupColumn("总量", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, SlotColumnID_EquipmentSize);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGuiTableSortSpecs* slot_sort_specs = ImGui::TableGetSortSpecs();
        if (needs_sorting || (slot_sort_specs && slot_sort_specs->SpecsDirty)) {
            SortSlots(slot_sort_specs);
        }
        for (auto& free_slot : free_slots_sorted) {
            const auto free_equipment = free_slot->max_equipment - free_slot->occupied_equipment;
            const auto free_inventory = free_slot->max_inventory - free_slot->occupied_inventory;
            const auto is_current_account = memeq(&free_slot->account, &current_account);
            const bool is_chest = free_slot->character == "(仓库)";
            std::string suffix;
            int style_count = 0;
            if (!is_current_account) {
                style_count = 1;
                ImGui::PushStyleColor(ImGuiCol_Text, color_disabled);
                if (is_chest && !free_slot->account_representing_character.empty()) suffix = " [" + free_slot->account_representing_character + "]";
            }
            ImGui::TableNextColumn();
            if (is_current_account) {
                if (ImGui::Button(free_slot->character.c_str())) {
                    ItemPath p;
                    p.account = free_slot->account;
                    p.character = free_slot->character;
                    p.bag_id = is_chest ? GW::Constants::Bag::Storage_1 : GW::Constants::Bag::None;
                    OnAccountInventoryItemClicked(p, false);
                }
            }
            else {
                ImGui::Text("%s%s", free_slot->character.c_str(), suffix.c_str());
            }
            ImGui::TableNextColumn();
            if (free_slot->max_inventory) RightAlignTextF("%d/", free_inventory);
            ImGui::TableNextColumn();
            if (free_slot->max_inventory) ImGui::Text("%d", free_slot->max_inventory);
            ImGui::TableNextColumn();
            if (free_slot->max_equipment) RightAlignTextF("%d/", free_equipment);
            ImGui::TableNextColumn();
            if (free_slot->max_equipment) ImGui::Text("%d", free_slot->max_equipment);
            ImGui::PopStyleColor(style_count);
        }
        ImGui::EndTable();
    }

    const float items_table_height = std::max(ImGui::GetContentRegionAvail().y, ITEMS_TABLE_MIN_HEIGHT);
    const float inner_width = ImGui::GetContentRegionAvail().x - item_spacing;
    const float button_height = 3.3f * ImGui::GetTextLineHeight();
    const ImVec2 button_size = ImVec2(button_height, button_height);


    ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
    if (settings.detailed_view) {
        flags |= ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg;
    }
    else {
        flags |= ImGuiTableFlags_SizingFixedFit;
    }

    // 过滤/排序头表
    if (!ImGui::BeginTable("###itemstable", ItemColumnID_Max, flags, ImVec2(inner_width, settings.detailed_view ? items_table_height : 2 * ImGui::GetFrameHeight()))) {
        ImGui::End();
        return;
    }
    ImGui::TableSetupColumn("角色", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_Character);
    ImGui::TableSetupColumn("位置 / 英雄", ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_Location);
    ImGui::TableSetupColumn("模型 ID", ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_ModelID);
    ImGui::TableSetupColumn("物品", ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_Description);
    ImGui::TableSetupScrollFreeze(3, 2);
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("###name_filter", name_filter_buf, _countof(name_filter_buf))) needs_sorting = true;

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("###location_filter", location_filter_buf, _countof(location_filter_buf))) needs_sorting = true;

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("###model_ID_filter", model_ID_filter_buf, _countof(model_ID_filter_buf))) needs_sorting = true;

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(300.f * font_scale);
    if (ImGui::InputText("###item_filter", item_filter_buf, _countof(item_filter_buf))) needs_sorting = true;
    ImGui::SameLine();
    ImGui::Text("筛选   %d/%d 件物品", filtered_item_count, item_refs.size());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGuiTableSortSpecs* item_sort_specs = ImGui::TableGetSortSpecs();
    const bool specs_dirty = item_sort_specs && item_sort_specs->SpecsDirty;
    const bool decode_retry_due = !sort_awaiting_decode || TIMER_DIFF(decode_sort_timer) >= 250;
    if ((needs_sorting && decode_retry_due) || specs_dirty) {
        sort_awaiting_decode = false;
        SortAndFilterInventory(item_sort_specs);
    }

    const int item_count = static_cast<int>(inventory_sorted.size());

    auto render_item = [&](int idx) {
        auto& ims = inventory_sorted[idx];
        const auto i_front = *(ims.i.begin());
        bool clicked = false;

        ImGui::PushID(idx);
        int style_count = 0;
        const bool is_foreign = !memeq(&i_front->account->uuid, &current_account);
        if (is_foreign) {
            style_count += 1;
            ImGui::PushStyleColor(ImGuiCol_Text, color_disabled);
        }

        const ImVec4* btn_colors = nullptr;
        if (IsChestBag(i_front->bag_id))
            btn_colors = is_foreign ? &color_chest_item_foreign : &color_chest_item;
        else if (IsOnHero(i_front->hero_id))
            btn_colors = is_foreign ? &color_hero_item_foreign : &color_hero_item;
        else if (is_foreign)
            btn_colors = &color_item_foreign;

        if (btn_colors) {
            style_count += 3;
            ImGui::PushStyleColor(ImGuiCol_Button, btn_colors[0]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_colors[1]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_colors[2]);
        }

        if (settings.detailed_view) {
            const std::string suffix = (ims.i.size() > 1) ? " +" : "";
            ImGui::Text("%s%s", i_front->character_name.c_str(), suffix.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s%s", i_front->location.c_str(), suffix.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", i_front->item->model_id);
            ImGui::TableNextColumn();
            style.ButtonTextAlign = ImVec2(0.f, 0.5f);
            const auto& description_one_line = ims.GetDescription();
            clicked = ImGui::Button(description_one_line.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([ms = &ims]() {
                    OnItemTooltip(ms);
                });
            }
            ImGui::TableNextColumn();
        }
        else {
            const auto pos = ImGui::GetCursorPos();
            const auto it = i_front->item;
            // 仅在绘制时获取并缓存图标；指针稳定，因此每个显示的物品一次
            if (!it->texture) {
                const auto player = GW::Agents::GetControlledCharacter();
                it->texture = Resources::GetItemImage(it->model_file_id, it->interaction, it->dyes, player && player->GetIsFemale());
            }
            if (it->texture && *(it->texture))
                clicked = ImGui::IconButton(nullptr, *(it->texture), button_size, ImGuiButtonFlags_None, button_size);
            else
                clicked = ImGui::Button("？？？", button_size);

            if (ims.quantity > 1) {
                const auto rect_min = ImGui::GetItemRectMin();
                char qty_buf[8];
                snprintf(qty_buf, sizeof(qty_buf), "%d", ims.quantity);
                ImGui::GetWindowDrawList()->AddText({rect_min.x + item_spacing, rect_min.y}, color_quantity, qty_buf);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([ms = &ims]() {
                    OnItemTooltip(ms);
                });
            }
        }

        ImGui::PopStyleColor(style_count);
        ImGui::PopID();

        if (clicked) {
            ItemPath p;
            p.account = i_front->account->uuid;
            p.character = i_front->character_name;
            p.hero_id = i_front->hero_id;
            p.bag_id = i_front->bag_id;
            p.slot = i_front->slot;
            OnAccountInventoryItemClicked(p, ImGui::IsKeyDown(ImGuiMod_Ctrl));
        }
    };

    if (settings.detailed_view) {
        ImGuiListClipper clipper;
        clipper.Begin(item_count, ImGui::GetTextLineHeightWithSpacing());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                render_item(i);
            }
        }
        clipper.End();
        ImGui::EndTable();
    }
    else {
        ImGui::EndTable(); // 结束过滤/排序头表
        const int cols = std::max(1, (int)(inner_width / (button_height + item_spacing)));
        const int row_count = (item_count + cols - 1) / cols;

        ImGui::BeginChild("###itemgrid", ImVec2(inner_width, std::max(ImGui::GetContentRegionAvail().y, ITEMS_TABLE_MIN_HEIGHT)));

        const float cell_size = button_height + item_spacing;

        int rendered_cells = 0;

        ImGuiListClipper clipper;
        clipper.Begin(row_count, cell_size);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                for (int col = 0; col < cols; col++) {
                    const int idx = row * cols + col;
                    if (idx >= item_count) break;
                    ImGui::SetCursorPos({col * cell_size, row * cell_size});
                    render_item(idx);
                    rendered_cells++;
                }
            }
        }
        clipper.End();

        ImGui::EndChild();
        // ImGui::Text("已渲染：%d / %d", rendered_cells, item_count);
    }

    ImGui::End();
}

void AccountInventoryWindow::DrawSettingsInternal()
{
    auto font_scale = ImGui::FontScale();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ImGui::Text("账户库存显示所有玩家、英雄和仓库库存的合并视图。");
    if (ImGui::Button("收集所有库存")) {
        visible = true;
        ImGui::ConfirmDialog("为了加载所有可用物品，这将遍历\n所有角色和所有英雄。\n如果您有很多角色，这将需要几分钟。\n确定要继续吗？", [](bool result, void*) {
            if (result) AccountInventoryWindow::Instance().GatherAllInventories();
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("删除账户库存")) {
        show_delete_note = true;
        accounts.clear();
        inventory_lookup.clear();
        item_refs.clear();
        inventory_sorted.clear();
        slot_rows.clear();
        free_slots_sorted.clear();
        for (auto it = ini_by_character.begin(); it != ini_by_character.end(); ++it) {
            inventory_dirty.insert(it->first);
        }
        SaveToFiles(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("删除所有库存")) {
        show_delete_note = true;
        LoadFromFiles(false); // 首先重新加载所有内容，以便我们知道当前磁盘上的所有库存文件
        accounts.clear();
        inventory_lookup.clear();
        item_refs.clear();
        inventory_sorted.clear();
        slot_rows.clear();
        free_slots_sorted.clear();
        SaveToFiles(true); // accounts 为空 -> 删除每个已知的库存文件
    }
    ImGui::Checkbox("###account_inventory_detailed_view", &settings.detailed_view);
    ImGui::SameLine();
    ImGui::Text("详细视图 - 在详细列表和图标网格视图之间切换。");
    ImGui::Checkbox("###account_inventory_merge_stacks", &settings.merge_stacks);
    ImGui::SameLine();
    ImGui::Text("合并堆叠 - 合并相同物品的多个堆叠，包括不可堆叠物品。");
    ImGui::Checkbox("###account_inventory_hide_other_accounts", &settings.hide_other_accounts);
    ImGui::SameLine();
    ImGui::Text("隐藏其他账户 - 隐藏不属于当前活动账户的物品。");
    ImGui::Checkbox("###account_inventory_hide_equipment", &settings.hide_equipment);
    ImGui::SameLine();
    ImGui::Text("隐藏装备 - 隐藏当前装备或属于武器套装的物品。");
    ImGui::Checkbox("###account_inventory_hide_equipment_pack", &settings.hide_equipment_pack);
    ImGui::SameLine();
    ImGui::Text("隐藏装备包 - 隐藏装备包的内容。");
    ImGui::Checkbox("###account_inventory_hide_hero_armor", &settings.hide_hero_armor);
    ImGui::SameLine();
    ImGui::Text("隐藏英雄护甲 - 隐藏英雄穿戴的护甲。");
    ImGui::Checkbox("###account_inventory_hide_unclaimed_items", &settings.hide_unclaimed_items);
    ImGui::SameLine();
    ImGui::Text("隐藏未认领物品 - 隐藏未认领物品窗口中的物品。");
    ImGui::PopTextWrapPos();
    if (show_delete_note) {
        // 我们可以在此处禁用此模块自身，如果 ToolboxSettings 的 ModuleToggle.enabled 是 ToolboxModule 的一部分的话
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f * font_scale, 0.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("删除库存中", &show_delete_note)) {
            ImGui::TextWrapped("请确保在 工具箱设置 -> 窗口 中禁用账户库存以阻止其重新收集库存数据。");
            if (ImGui::Button("确定")) {
                show_delete_note = false;
            }
        }
        ImGui::End();
    }
}

void AccountInventoryWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();
    needs_sorting = true;
    // 仅从文件加载外部物品。允许用户重新加载活动账户的库存数据可能导致临时不一致
    LoadFromFiles(true);
}

void AccountInventoryWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    SaveToFiles(false);
}
