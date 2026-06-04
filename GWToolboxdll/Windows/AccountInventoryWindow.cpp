#include "stdafx.h"
#include <atomic>
#include <bit>
#include <deque>
#include <iterator>
#include <mutex>
#include <optional>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>

#include <Logger.h>
#include <GWToolbox.h>
#include <Utils/TextUtils.h>

#include <Modules/Resources.h>
#include <Modules/InventoryManager.h>
#include <Windows/CompletionWindow.h>
#include <Windows/RerollWindow.h>
#include <Windows/AccountInventoryWindow.h>

#include <Utils/ToolboxUtils.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/PlayerMgr.h>

#define memeq(a, b) (memcmp((a), (b), sizeof(*(a))) == 0)


namespace {

    // Based on boost::hash_combine
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

    const char* BAG_NAME[] = {"",          "Backpack",  "Belt Pouch", "Bag 1",     "Bag 2",     "Equipment Pack", "Material Storage", "Unclaimed Items", "Storage 1",  "Storage 2",  "Storage 3",     "Storage 4",
                          "Storage 5", "Storage 6", "Storage 7",  "Storage 8", "Storage 9", "Storage 10",     "Storage 11",       "Storage 12",      "Storage 13", "Storage 14", "Equipped Items"};
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
                return 25; // Storage_1 through Storage_14
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

    // Identity of an inventory ini. There is one ini file per account; every
    // character/hero/chest of that account is stored together in it (disambiguated
    // by per-item sections), so the id is just the account GUID. The character
    // arg is kept for call-site convenience but no longer affects the id.
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

    

    // Intrinsic item data only. Its account/character/hero/bag/slot are implied by
    // where it sits in the hierarchy (Account -> chest/characters -> bags -> items).
    struct Item {
        uint32_t model_id{};
        uint32_t model_file_id{};
        uint32_t interaction{};
        uint16_t quantity{};
        uint8_t equipped{};
        std::wstring description{}; // output of AsyncDecodeStr(ShorthandItemDescription)
        uint32_t item_id{};         // live-session only; 0 for items loaded from disk
        IDirect3DTexture9** texture = nullptr; // cache (GetItemImage), not serialized
    };

    // Free-slot occupancy, folded out of the old CharacterFreeSlots into the nodes.
    struct FreeSlotInfo {
        bool known = false;         // true once we have free-slot data for this node
        uint32_t max_inventory{};
        uint32_t max_equipment{};
        uint32_t occupied_inventory{};
        uint32_t occupied_equipment{};
    };

    struct Bag {
        GW::Constants::Bag bag_id{};
        std::unordered_map<uint32_t /*slot*/, Item> items; // node-stable: raw Item* stay valid
    };
    struct Hero {
        GW::Constants::HeroID hero_id{};
        Bag bag; // single Equipped_Items bag
    };
    struct Character {
        std::string name;
        std::unordered_map<GW::Constants::Bag, Bag> bags;
        std::unordered_map<GW::Constants::HeroID, Hero> heroes;
        FreeSlotInfo free_slots;
    };
    struct Account {
        GUID uuid{};
        std::unordered_map<GW::Constants::Bag, Bag> chest;        // 15 chest panels
        std::unordered_map<std::string, Character> characters;
        std::string account_representing_character;               // tooltip helper
        bool anniversary_pane_active = false;
        FreeSlotInfo chest_free_slots;                            // equipment unused for chest
    };

    // Ephemeral flattened view of one item, rebuilt by traversing the hierarchy.
    // Carries the denormalized fields the display/sort/tooltip need.
    struct ItemRef {
        Account* account = nullptr;   // never null
        Character* character = nullptr; // null for chest items
        Hero* hero = nullptr;         // null unless on a hero
        GW::Constants::Bag bag_id{};
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        uint32_t slot{};
        Item* item = nullptr;
        std::string character_name;   // "(Chest)" | Character::name
        std::string location;         // "(Player)" | hero name | BAG_NAME[bag]
    };

    // A storage path: enough to locate (or re-locate) an item in the hierarchy.
    struct ItemPath {
        GUID account{};
        std::string character;        // "(Chest)" or player name
        GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
        GW::Constants::Bag bag_id{};
        uint32_t slot{};
    };

    // Resolved live item: where it lives + the owning bag (so it can be erased).
    struct ItemLoc {
        Account* account = nullptr;
        Character* character = nullptr; // null for chest items
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
        std::set<ItemRef*, ItemCompare> i;
        MergeStack(const UUID& account, const std::wstring& _description);
        std::string GetDescription() {
            std::string build_desc = description;
            if (quantity > 1) build_desc = std::to_string(quantity) + " " + build_desc;
            auto description_one_line = TextUtils::ctre_regex_replace<L"\n", L" - ">(build_desc);
            return description_one_line;
        }
    };
    bool ItemCompare::operator()(const MergeStack& lms, const MergeStack& rms) const
    {
        int sort_direction = 1;
        int delta = 0;
        if (rms.i.size() == 0) return false;
        if (lms.i.size() == 0) return true;
        auto l = *(lms.i.begin());
        auto r = *(rms.i.begin());
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
        // fallback
        if (delta == 0) delta = l->character_name.compare(r->character_name);
        if (delta == 0) delta = l->location.compare(r->location);
        if (delta == 0) delta = (int)l->bag_id - (int)r->bag_id;
        if (delta == 0) delta = l->slot - r->slot;
        if (delta == 0) delta = memcmp(&l->account->uuid, &r->account->uuid, sizeof(l->account->uuid));
        return delta * sort_direction < 0;
    }
    bool ItemCompare::operator()(ItemRef* l, ItemRef* r) const
    {
        if (l->account->uuid != r->account->uuid) {
            // lowest item is the one that can be interacted with. Make sure it is one on this account if there is one
            if (memeq(&l->account->uuid, &current_account)) return true;
            if (memeq(&r->account->uuid, &current_account)) return false;
        }
        auto lms = MergeStack(l->account->uuid, L"");
        lms.quantity = l->item->quantity;
        lms.i.insert(l);
        auto rms = MergeStack(r->account->uuid, L"");
        rms.quantity = r->item->quantity;
        rms.i.insert(r);
        return this->operator()(lms, rms);
    }
    MergeStack::MergeStack(const UUID& account, const std::wstring& _description) : quantity{}, i(ItemCompare{nullptr, account}) {
        description = TextUtils::WStringToString(_description);
    }

    // One row in the Free Slots table, built from a Character (or an Account's chest).
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
            // fallback
            if (delta == 0) delta = l->character.compare(r->character);
            if (delta == 0) delta = memcmp(&l->account,&r->account,sizeof(r->account));
            return delta * sort_direction < 0;
        }
    };

    class InventoryIni : public ToolboxIni {
    public:
        std::filesystem::file_time_type last_change_time{};
        bool is_loaded = false;
        GUID account{};
        std::string ini_ID{}; // character name for character/hero inventories, email for xunlai chests
        InventoryIni(std::filesystem::path _location_on_disk) { location_on_disk = _location_on_disk; }
    };

    struct RawIniEntry {
        bool is_freeslot = false;
        GUID account{};
        std::string character;
        GW::Constants::HeroID hero_id{};
        GW::Constants::Bag bag_id{};
        uint32_t slot = 0, model_id = 0, model_file_id = 0, interaction = 0;
        uint16_t quantity = 0;
        uint8_t equipped = 0;
        std::wstring description;
        std::string account_representing_character;
        uint32_t max_equipment = 0, max_inventory = 0, occupied_equipment = 0, occupied_inventory = 0;
        bool anniversary_pane_active = false;
    };

    void OnItemTooltip(const MergeStack* ms);
    void OnAccountInventoryItemClicked(const ItemPath& path, bool move);
    static bool CheckIniDirty(InventoryIni* ini, std::filesystem::file_time_type write_time);
    InventoryIni* GetIni(const std::string& ini_ID, const GUID& account);
    void LoadFromFiles(bool only_foreign);
    void SaveToFiles(bool include_foreign);
    void SortSlots(ImGuiTableSortSpecs* sort_specs);
    void DescriptionDecode(const ItemPath& path, GW::Item* item);
    void ClearMissingItem(const UUID* account, const std::string& character, const GW::Constants::HeroID hero_id, const GW::Constants::Bag bag_id, const uint32_t slot);

    // collective callback hook
    GW::HookEntry OnUIMessage_HookEntry{};
    // main item storage: the account hierarchy, keyed by canonical GUID string.
    std::unordered_map<std::string, Account> accounts{};
    // On*SlotCleared send only an item_id, so we keep item_id -> resolved location
    // for live items to remove them without traversing the hierarchy.
    std::unordered_map<uint32_t, ItemLoc> inventory_lookup{};
    // sorted/filtered view for display + its backing ItemRef store (stable addresses)
    std::deque<ItemRef> item_refs{};
    std::vector<MergeStack> inventory_sorted{};
    // ini files, 1 per character/chest
    std::unordered_map<std::filesystem::path, std::unique_ptr<InventoryIni>> ini_by_path{};
    std::unordered_map<std::string, InventoryIni*> ini_by_character{};
    // change tracker to reduce writes
    std::unordered_set<std::string> inventory_dirty{};
    // sorted/filtered view for the Free Slots table + its backing rows
    std::vector<SlotRow> slot_rows{};
    std::set<SlotRow*, SlotCompare> free_slots_sorted{};
    // tracking for hero_id <-> Equipped_Items bag
    // we rely on hero items being created in the order of heroes in the party
    std::queue<GW::Bag*> hero_bag_generation_order{};
    // there must be a data structure somewhere in the game that already has this mapping
    // but i do not know where it is
    std::unordered_map<GW::Bag*, GW::Constants::HeroID> bag_ptr_to_hero_id{};

    // state between callbacks
    bool initializing = false;
    bool needs_sorting = true;
    bool needs_filter = true;
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
        void Begin() { 
            if (current_stage != Stage::None) return;
            Set(Stage::Start);
        }
        void Update();
        bool Cancel(const char* err = 0) { 
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

    // config options
    bool detailed_view = false;
    bool merge_stacks = false;
    bool hide_other_accounts = false;
    bool hide_equipment = false;
    bool hide_equipment_pack = false;
    bool hide_hero_armor = false;
    bool hide_unclaimed_items = false;

    // input buffers
    inline static const size_t BUFFER_SIZE = 128;
    char name_filter_buf[BUFFER_SIZE]{};
    char location_filter_buf[BUFFER_SIZE]{};
    char model_ID_filter_buf[BUFFER_SIZE]{};
    char item_filter_buf[BUFFER_SIZE]{};

    GUID current_account;
    std::string current_character;

    // member variables
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
        const GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        if (!(party_info && me)) return {};
        std::vector<GW::Constants::HeroID> hero_ids;
        for (const auto& hero : party_info->heroes) {
            if (hero.owner_player_id != me->login_number) continue;
            hero_ids.push_back(hero.hero_id);
        }
        return hero_ids;
    }

    // jump to location of clicked item, i.e. open chest/add hero/change character
    // with Ctrl: move item to/from chest after jump
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
        // Only one tooltip is visible at a time; cache aggregates and recompute only when the hovered item changes.
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
                if (!is_this_account && (*it)->character_name == "(Chest)" && !account_representing_character.empty()) {
                    suffix = " [" + account_representing_character + "]";
                }
                ImGui::Text("%s%s: %u", (*it)->character_name.c_str(), suffix.c_str(), char_totals[char_key(*it, account_representing_character)]);
            }
            reprint |= (*it)->location != prev_location;
            if (reprint) {
                ImGui::Text("- %s: %u", (*it)->location.c_str(), loc_totals[loc_key(*it, account_representing_character)]);
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

    // ===== hierarchy routing / lookup helpers =====

    std::string AccountKey(const GUID& account) { return TextUtils::GuidToString(&account); }

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

    // Resolve (creating Account/Character/Hero/Bag as needed) the bag a storage path
    // lives in. Encapsulates the IsChestBag / IsOnHero routing.
    Bag& GetOrCreateBag(const GUID& account, const std::string& character,
                        GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id)
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

    Item& GetOrCreateItem(const GUID& account, const std::string& character,
                          GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id, uint32_t slot)
    {
        return GetOrCreateBag(account, character, hero_id, bag_id).items[slot];
    }

    // Find-only resolution: returns where an item lives, or nullopt if any level is missing.
    std::optional<ItemLoc> FindItemLoc(const GUID& account, const std::string& character,
                                       GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id, uint32_t slot)
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

    // Free-slot record for an (account, character) pair. "(Chest)" -> the account's
    // chest record. Returns nullptr unless the record is known.
    FreeSlotInfo* FindFreeSlots(const GUID& account, const std::string& character)
    {
        Account* acc = FindAccount(account);
        if (!acc) return nullptr;
        if (character == "(Chest)")
            return acc->chest_free_slots.known ? &acc->chest_free_slots : nullptr;
        const auto it = acc->characters.find(character);
        if (it == acc->characters.end() || !it->second.free_slots.known) return nullptr;
        return &it->second.free_slots;
    }
    FreeSlotInfo& GetOrCreateFreeSlots(const GUID& account, const std::string& character)
    {
        Account& acc = GetOrCreateAccount(account);
        if (character == "(Chest)") {
            acc.chest_free_slots.known = true;
            return acc.chest_free_slots;
        }
        Character& ch = acc.characters[character];
        ch.name = character;
        ch.free_slots.known = true;
        return ch.free_slots;
    }

    // Rebuild the flattened ItemRef view from the whole hierarchy (all accounts).
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
                    r.character_name = "(Chest)";
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
                        r.location = "(Player)";
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
        // Build one row per character (with known free-slot data) and one per account
        // chest, into a stable backing vector, then sort pointers into it.
        slot_rows.clear();
        for (auto& [key, acc] : accounts) {
            if (acc.chest_free_slots.known) {
                SlotRow row;
                row.account = acc.uuid;
                row.character = "(Chest)";
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
            if (hide_other_accounts && !memeq(&row.account, &current_account)) {
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
        std::unordered_map<std::wstring, size_t> merged_stacks{};
        for (auto& r : item_refs) {
            if (hide_other_accounts && r.account->uuid != current_account) continue;
            if (hide_equipment && (r.bag_id == GW::Constants::Bag::Equipped_Items || r.item->equipped)) continue;
            if (hide_equipment_pack && r.bag_id == GW::Constants::Bag::Equipment_Pack) continue;
            if (hide_hero_armor && IsHeroArmor(r.hero_id, r.slot)) continue;
            if (hide_unclaimed_items && r.bag_id == GW::Constants::Bag::Unclaimed_Items) continue;

            if (!name_filter.empty()) {
                const auto character_check = name_is_lower ? TextUtils::ToLower(r.character_name) : r.character_name;
                if (!character_check.contains(name_filter)) continue;
            }
            if (!location_filter.empty()) {
                const auto location_check = location_is_lower ? TextUtils::ToLower(r.location) : r.location;
                if (!location_check.contains(location_filter)) continue;
            }
            if (!model_ID_filter.empty() && model_ID_filter != std::to_string(r.item->model_id)) continue;
            if (!item_filter_w.empty()) {
                const auto description_check = item_is_lower ? TextUtils::ToLower(r.item->description) : r.item->description;
                if (!description_check.contains(item_filter_w)) continue;
            }

            auto merge_id = std::to_wstring(r.item->model_id) + r.item->description;
            if (!merge_stacks || !merged_stacks.contains(merge_id)) {
                merged_stacks[merge_id] = inventory_sorted.size();
                inventory_sorted.push_back(MergeStack(current_account, r.item->description));
            }
            MergeStack* ms = &inventory_sorted[merged_stacks[merge_id]];
            ms->quantity += r.item->quantity;
            ms->i.insert(&r);
        }
        filtered_item_count = inventory_sorted.size();

        if (inventory_sorted.size() > 1) std::sort(inventory_sorted.begin(), inventory_sorted.end(), ItemCompare{sort_specs, current_account});

        if (sort_specs) sort_specs->SpecsDirty = false;
        needs_sorting = false;
    }

    // unique section name for item in ini file. The account's characters/heroes/
    // chest now share one ini, so the character is part of the key to keep two
    // characters' identical hero/bag/slot from colliding onto the same section.
    std::string ItemToSectionName(const std::string& character, GW::Constants::HeroID hero_id, GW::Constants::Bag bag_id, uint32_t slot)
    {
        char buf[9];
        std::string out = character;
        out.push_back('.');
        snprintf(buf, sizeof(buf), "%08x", hero_id);
        out.append(buf);
        snprintf(buf, sizeof(buf), "%08x", bag_id);
        out.append(buf);
        snprintf(buf, sizeof(buf), "%08x", slot);
        out.append(buf);
        return out;
    }

    bool CheckIniDirty(InventoryIni* ini, std::filesystem::file_time_type write_time)
    {
        if (write_time != ini->last_change_time) {
            ini->last_change_time = write_time;
            ini->is_loaded = false;
            return true;
        }
        return false;
    }


    // Deterministic on-disk location for an account's inventory ini. One file per
    // account, named by the account GUID - e.g. "inventories/tmp<account-uuid>.tmp".
    std::filesystem::path AccountIniPath(const GUID& account)
    {
        const auto name = L"tmp" + TextUtils::StringToWString(TextUtils::GuidToString(&account)) + L".tmp";
        return Resources::GetPath(L"inventories", name);
    }

    // True only for a canonical inventory filename, tmp<account-uuid>.tmp. The GUID
    // is parsed and the name rebuilt to reject anything else (stray files, the old
    // "inv####.tmp" temp names, trailing junk, wrong-case hex).
    bool IsInventoryIniFilename(const std::filesystem::path& path)
    {
        if (path.extension() != L".tmp") return false;
        const std::string stem = path.stem().string();
        if (!stem.starts_with("tmp")) return false;
        GUID guid{};
        if (!TextUtils::StringToGuid(stem.substr(3), &guid)) return false;
        return AccountIniPath(guid).filename() == path.filename();
    }

    InventoryIni* GetIni(const std::string& ini_ID, const GUID& account)
    {
        if (const auto found = ini_by_character.find(ini_ID); found != ini_by_character.end()) {
            return found->second;
        }
        // First time we touch this account this session: bind to its canonical file,
        // reusing the InventoryIni already created for that path if one exists.
        const auto path = AccountIniPath(account);
        InventoryIni* ini;
        if (const auto existing = ini_by_path.find(path); existing != ini_by_path.end()) {
            ini = existing->second.get();
        }
        else {
            Resources::EnsureFolderExists(Resources::GetPath(L"inventories"));
            auto owned = std::make_unique<InventoryIni>(path);
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
        std::vector<RawIniEntry> entries;
        bool success = false;
    };

    struct BatchLoadState {
        std::mutex mutex;
        std::vector<LoadResult> results;
        std::atomic<int> tasks_remaining{0};
    };

    RawIniEntry ParseFreeSlotSection(const ToolboxIni& ini, const char* section, const GUID& account, const std::string& character)
    {
        RawIniEntry e;
        e.is_freeslot = true;
        e.account = account;
        e.character = character;
        e.account_representing_character = ini.GetValue(section, "account_character", "");
        e.max_equipment       = (uint32_t)ini.GetLongValue(section, "maxequipment",      0);
        e.max_inventory       = (uint32_t)ini.GetLongValue(section, "maxinventory",      0);
        e.occupied_equipment  = (uint32_t)ini.GetLongValue(section, "occupiedequipment", 0);
        e.occupied_inventory  = (uint32_t)ini.GetLongValue(section, "occupiedinventory", 0);
        e.anniversary_pane_active = ini.GetBoolValue(section, "anniversary_pane_active", false);
        return e;
    }

    RawIniEntry ParseItemSection(const ToolboxIni& ini, const char* section, const GUID& account, const std::string& character)
    {
        RawIniEntry e;
        e.account = account;
        e.character = character;
        e.bag_id        = (GW::Constants::Bag)    ini.GetLongValue(section, "bagid",      1);
        e.hero_id       = (GW::Constants::HeroID) ini.GetLongValue(section, "heroid",     0);
        e.slot          = (uint32_t)ini.GetLongValue(section, "slot",        0);
        e.model_id      = (uint32_t)ini.GetLongValue(section, "modelid",     0);
        e.model_file_id = (uint32_t)ini.GetLongValue(section, "modelfileid", 0);
        e.interaction   = (uint32_t)ini.GetLongValue(section, "interaction", 0);
        e.quantity      = (uint16_t)ini.GetLongValue(section, "quantity",    0);
        e.equipped      = (uint8_t) ini.GetLongValue(section, "equipped",    0);
        GuiUtils::IniToArray(ini.GetValue(section, "description", ""), e.description);
        return e;
    }

    LoadResult LoadIniFile(const std::filesystem::path& path, bool only_foreign, const GUID& account)
    {
        LoadResult r;
        r.path = path;
        ToolboxIni temp_ini;
        r.success = temp_ini.LoadFile(path) == SI_OK;
        if (!r.success) return r;

        TNamesDepend sections;
        temp_ini.GetAllSections(sections);
        r.entries.reserve(sections.size());

        for (const auto& sec : sections) {
            const char* section = sec.pItem;
            GUID entry_account;
            if (!TextUtils::StringToGuid(temp_ini.GetValue(section, "account", ""), &entry_account)) continue;
            const std::string character = temp_ini.GetValue(section, "character", "");
            if (only_foreign && entry_account == account) continue;

            // Free-slot sections are named "<character>.freeslots" (or just
            // "freeslots" in older files); item sections always end in hex digits.
            if (std::string_view(section).ends_with("freeslots"))
                r.entries.push_back(ParseFreeSlotSection(temp_ini, section, entry_account, character));
            else
                r.entries.push_back(ParseItemSection(temp_ini, section, entry_account, character));
        }
        return r;
    }

    void ApplyFreeSlotEntry(const RawIniEntry& e)
    {
        Account& acc = GetOrCreateAccount(e.account);
        if (!e.account_representing_character.empty())
            acc.account_representing_character = e.account_representing_character;
        FreeSlotInfo* fs;
        if (e.character == "(Chest)") {
            acc.chest_free_slots.known = true;
            acc.anniversary_pane_active = e.anniversary_pane_active;
            fs = &acc.chest_free_slots;
        }
        else {
            Character& ch = acc.characters[e.character];
            ch.name = e.character;
            ch.free_slots.known = true;
            fs = &ch.free_slots;
        }
        fs->max_equipment      = e.max_equipment;
        fs->max_inventory      = e.max_inventory;
        fs->occupied_equipment = e.occupied_equipment;
        fs->occupied_inventory = e.occupied_inventory;
    }

    void ApplyItemEntry(const RawIniEntry& e)
    {
        Item& item = GetOrCreateItem(e.account, e.character, e.hero_id, e.bag_id, e.slot);
        item.model_id      = e.model_id;
        item.model_file_id = e.model_file_id;
        item.interaction   = e.interaction;
        item.quantity      = e.quantity;
        item.equipped      = e.equipped;
        item.description   = e.description;
        GW::Item gw_item;
        gw_item.model_file_id = item.model_file_id;
        gw_item.interaction   = item.interaction;
        item.texture = Resources::GetItemImage(&gw_item);
    }

    void ApplyLoadResult(const LoadResult& r)
    {
        const auto it = ini_by_path.find(r.path);
        auto* ini = it != ini_by_path.end() ? it->second.get() : nullptr;
        if (!ini || !r.success) return;

        for (const auto& e : r.entries) {
            const auto entry_ini_id = GetIniID(e.account, e.character);
            if (ini->ini_ID.empty()) {
                ini->ini_ID = entry_ini_id;
                ini->account = e.account;
                ini_by_character[ini->ini_ID] = ini;
            }
            else if (ini->ini_ID != entry_ini_id) {
                continue;
            }
            if (e.is_freeslot)
                ApplyFreeSlotEntry(e);
            else
                ApplyItemEntry(e);
        }
        ini->is_loaded = true;
    }

    void LoadFromFiles(bool only_foreign)
    {
        Resources::EnsureFolderExists(Resources::GetPath(L"inventories"));
        std::unordered_set<std::filesystem::path> visited;

        if (only_foreign) {
            for (auto& [path, ini] : ini_by_path) {
                if (ini->account != current_account)
                    ini->is_loaded = false;
            }
            // drop all foreign accounts (items + free slots); they get reloaded below
            for (auto it = accounts.begin(); it != accounts.end();)
                it = it->second.uuid != current_account ? accounts.erase(it) : std::next(it);
        }
        else {
            inventory_lookup.clear();
        }

        std::vector<std::filesystem::path> to_load;
        for (const auto& file : std::filesystem::directory_iterator{Resources::GetPath(L"inventories")}) {
            const auto path = file.path();
            if (!IsInventoryIniFilename(path)) continue; // ignore legacy/unrelated files
            visited.insert(path);
            if (!ini_by_path.contains(path))
                ini_by_path[path] = std::make_unique<InventoryIni>(path);
            auto* ini = ini_by_path[path].get();
            if (only_foreign && ini->account == current_account) continue;
            const bool dirty = CheckIniDirty(ini, file.last_write_time());
            if (!dirty && ini->is_loaded) continue;
            if (dirty) ini->Reset();
            to_load.push_back(path);
        }

        for (auto& [path, ini] : ini_by_path) {
            if (!visited.contains(path)) {
                ini->Reset();
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
                    std::lock_guard lock(state->mutex);
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

    void SaveToFiles(bool include_foreign)
    {

        std::unordered_set<std::string> visited;

        // Only the current account is ever written; foreign accounts are read-only.
        Account* acc = FindAccount(current_account);
        const std::string ini_ID = GetIniID(current_account, "");
        if (acc && inventory_dirty.contains(ini_ID)) {
            InventoryIni* ini = nullptr;
            const auto ensure = [&]() -> InventoryIni* {
                if (!ini) {
                    ini = GetIni(ini_ID, current_account);
                    ini->Reset();
                    visited.insert(ini_ID);
                }
                return ini;
            };
            const auto account_str = TextUtils::GuidToString(&current_account);

            // --- free slots: one section per character, plus one for the chest ---
            const auto write_free_slots = [&](const std::string& character, const FreeSlotInfo& fs, bool anniversary) {
                InventoryIni* f = ensure();
                // Per-character section, suffixed with "freeslots" so the loader can
                // tell free-slot sections from item sections by name.
                const std::string section_name = character + ".freeslots";
                const char* section = section_name.c_str();
                f->SetValue(section, "account", account_str.c_str());
                if (!character.empty()) {
                    f->SetValue(section, "character", character.c_str());
                }
                if (!acc->account_representing_character.empty()) {
                    f->SetValue(section, "account_character", acc->account_representing_character.c_str());
                }
                f->SetLongValue(section, "maxequipment", fs.max_equipment);
                f->SetLongValue(section, "maxinventory", fs.max_inventory);
                f->SetLongValue(section, "occupiedequipment", fs.occupied_equipment);
                f->SetLongValue(section, "occupiedinventory", fs.occupied_inventory);
                if (anniversary) {
                    f->SetBoolValue(section, "anniversary_pane_active", true);
                }
            };
            if (acc->chest_free_slots.known)
                write_free_slots("(Chest)", acc->chest_free_slots, acc->anniversary_pane_active);
            for (auto& [name, ch] : acc->characters)
                if (ch.free_slots.known)
                    write_free_slots(ch.name, ch.free_slots, false);

            // --- items ---
            const auto write_item = [&](const std::string& character, GW::Constants::HeroID hero_id,
                                        GW::Constants::Bag bag_id, uint32_t slot, const Item& item) {
                InventoryIni* f = ensure();
                auto section = ItemToSectionName(character, hero_id, bag_id, slot);
                f->SetValue(section.c_str(), "account", account_str.c_str());
                f->SetValue(section.c_str(), "character", character.c_str());
                f->SetLongValue(section.c_str(), "heroid", (long)hero_id);
                f->SetLongValue(section.c_str(), "bagid", (long)bag_id);
                f->SetLongValue(section.c_str(), "slot", (long)slot);
                f->SetLongValue(section.c_str(), "modelid", (long)item.model_id);
                f->SetLongValue(section.c_str(), "modelfileid", (long)item.model_file_id);
                f->SetLongValue(section.c_str(), "interaction", (long)item.interaction);
                f->SetLongValue(section.c_str(), "quantity", (long)item.quantity);
                f->SetLongValue(section.c_str(), "equipped", (long)item.equipped);
                std::string ini_desc;
                GuiUtils::ArrayToIni(item.description, &ini_desc);
                f->SetValue(section.c_str(), "description", ini_desc.c_str());
            };
            for (auto& [bag_id, bag] : acc->chest)
                for (auto& [slot, item] : bag.items)
                    write_item("(Chest)", GW::Constants::HeroID::NoHero, bag_id, slot, item);
            for (auto& [name, ch] : acc->characters) {
                for (auto& [bag_id, bag] : ch.bags)
                    for (auto& [slot, item] : bag.items)
                        write_item(ch.name, GW::Constants::HeroID::NoHero, bag_id, slot, item);
                for (auto& [hero_id, hero] : ch.heroes)
                    for (auto& [slot, item] : hero.bag.items)
                        write_item(ch.name, hero_id, hero.bag.bag_id, slot, item);
            }
        }
        for (auto it = ini_by_character.begin(); it != ini_by_character.end(); ++it) {
            const auto& cur_ini_ID = it->first;
            if (!include_foreign && it->second->account != current_account) continue;
            if (include_foreign || visited.contains(cur_ini_ID)) {
                if (it->second->SaveFile(it->second->location_on_disk.wstring().c_str()) != SI_OK) {
                    Log::Error("Account Inventory: Failed to save inventory ini. Inventory tracking data will be lost.");
                }
            }
            else if (inventory_dirty.contains(cur_ini_ID)) {
                // dirty but not visited means there are no more items in this inventory. clean up its ini
                it->second->Reset();
            }
        }
        // separate loop because deleting from ini_by_character in the above loop gets really unreadable
        for (auto it = ini_by_path.begin(); it != ini_by_path.end();) {
            if (it->second->IsEmpty()) {
                DeleteFileW(it->first.wstring().c_str());
                ini_by_character.erase(it->second->ini_ID);
                it = ini_by_path.erase(it);
            }
            else
                ++it;
        }
        inventory_dirty.clear();
    }

    void DescriptionDecode(const ItemPath& path, GW::Item* item)
    {
        struct SyncDecode {
            ItemPath path;
            std::wstring enc;
        };
        struct SyncDecode* sync = new SyncDecode();
        sync->path = path;
        switch (item->type) {
            case GW::Constants::ItemType::Headpiece:
            case GW::Constants::ItemType::Boots:
            case GW::Constants::ItemType::Chestpiece:
            case GW::Constants::ItemType::Gloves:
            case GW::Constants::ItemType::Leggings:
                // ShorthandItemDescription includes item name for these
                break;
            default:
                // Default to single_item_name so merge_stacks can combine stacks of single and multiple items.
                if (item->single_item_name) {
                    sync->enc += item->single_item_name;
                }
                else if (item->complete_name_enc) {
                    sync->enc += item->complete_name_enc;
                }
                else if (item->name_enc) {
                    sync->enc += item->name_enc;
                }
        }
        if (item->info_string) {
            auto shorthand_description = ToolboxUtils::ShorthandItemDescription(item);
            // If item info_string starts with "Value:", ShorthandItemDescription doesn't filter the "Value:" part out.
            // Since "Value:" is typically at the end of the description, there is nothing left that we care about anyway.
            // Add description only if it does not start with "Value:".
            if (shorthand_description.find(L"\xA3E\x10A\xA8A\x10A\xA59\x1\x10B") != 0) {
                if (!sync->enc.empty()) {
                    sync->enc += L"\x2\x102\x2";
                }
                sync->enc += shorthand_description;
            }
        }
        GW::GameThread::Enqueue([sync] {
            GW::UI::AsyncDecodeStr(
                sync->enc.c_str(),
                [](void* param, const wchar_t* s) {
                    auto sync = (struct SyncDecode*)param;
                    const auto& p = sync->path;
                    // Re-resolve: a map load between enqueue and callback may have
                    // replaced or removed the item, so never capture a raw Item*.
                    if (auto loc = FindItemLoc(p.account, p.character, p.hero_id, p.bag_id, p.slot)) {
                        loc->item->description = TextUtils::StripTags(s);
                        inventory_dirty.insert(GetIniID(p.account, p.character));
                        save_dirty_inventories_timer = TIMER_INIT();
                        needs_sorting = true;
                    }
                    delete sync;
                },
                sync
            );
        });
    }

    void AddItem(uint32_t item_id)
    {
        auto item = GW::Items::GetItemById(item_id);
        if (!(item && item->bag)) return;

        // gather information for this items storage location, i.e.:
        // account, player character, hero, bag, slot within bag

        ItemPath path;
        path.account = current_account;
        path.bag_id = item->bag->bag_id();
        if (IsChestBag(path.bag_id)) {
            path.character = "(Chest)";
        }
        else {
            path.character = GetCurrentPlayerNameS();
            if (path.character.empty()) {
                path.character = "Unavailable";
            }
        }
        path.slot = item->slot;

        // This is a workaround because I could not find a way to get a hero_id from an item currently equipped on a hero.
        // item->bag->bag_array is a separate array for each hero with only the Equipped_Items bag set, but seemingly no reference back to the hero.
        // The workaround uses the fact that items are added by GW in the order of the respective heroes in the party.
        path.hero_id = GW::Constants::HeroID::NoHero;
        if (path.bag_id == GW::Constants::Bag::Equipped_Items && (GW::Inventory*)item->bag->inventory != GW::Items::GetInventory()) {
            // If we are loaded on a map when this module gets initialized, we will visit items in an arbitrary order
            // and therefore we are unable to guess which hero an item belongs to.
            // In this case we can add items on heroes only once we load into a new map or if the heroes are
            // removed and added again.
            if (initializing) return;
            // Outside of initialization, we get here when a hero is added or when a map is loaded.
            // In both cases items are added in the order of the respective heroes in the party.
            if (!bag_ptr_to_hero_id.contains(item->bag)) {
                // Queue hero bag for later.
                // Items will be added through HandleHeroBag once GW created the hero.
                if (item->slot == CHEST_ARMOR_INVENTORY_SLOT) {
                    hero_bag_generation_order.push(item->bag);
                }
                return;
            }
            else {
                path.hero_id = bag_ptr_to_hero_id[item->bag];
            }
        }
        // END hero_id workaround

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
            // make sure the lookup entry has not already been overwritten by another item being loaded during map load.
            if (const auto found = inventory_lookup.find(o_item_id);
                found != inventory_lookup.end() && found->second.item == existing->item) {
                inventory_lookup.erase(found);
                // when stacks are split or merged, the source and target stack will be readded by gw without being deleted first.
                // if we already know the item in the source/target slot, the number of occupied spaces did not actually change.
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
        it.texture = Resources::GetItemImage(item);

        ItemLoc loc;
        loc.account = &acc;
        loc.character = IsChestBag(path.bag_id) ? nullptr : &acc.characters[path.character];
        loc.bag_id = path.bag_id;
        loc.slot = path.slot;
        loc.bag = &bag;
        loc.item = &it;
        inventory_lookup[item->item_id] = loc;

        DescriptionDecode(path, item);
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
        // Live items go through RemoveItem (drops the lookup + occupancy). Items only
        // present in the ini (item_id 0) aren't in the lookup, so erase them directly.
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
        const std::string character = loc.character ? loc.character->name : "(Chest)";
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

}



void AccountInventoryWindow::Initialize()
{
    ToolboxWindow::Initialize();

    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kItemUpdated,
        GW::UI::UIMessage::kEquipmentSlotUpdated,
        GW::UI::UIMessage::kInventorySlotUpdated,
        GW::UI::UIMessage::kEquipmentSlotCleared,
        GW::UI::UIMessage::kInventorySlotCleared,
        GW::UI::UIMessage::kPartyAddHero,
        GW::UI::UIMessage::kPartyRemoveHero,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kLogout
    };
    for (auto message_id : ui_messages) {
        RegisterUIMessageCallback(&OnUIMessage_HookEntry, (GW::UI::UIMessage)message_id,
            [this] (GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
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
                        const auto hero = ((struct GW::HeroPartyMember **)wparam)[1];
                        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
                        if (!me || !hero) break;
                        if (hero->owner_player_id != me->login_number) break;
                        OnPartyAddHero(((GW::Constants::HeroID*)wparam)[7]);
                        break;
                    }
                    case GW::UI::UIMessage::kPartyRemoveHero: {
                        OnPartyRemoveHero(((GW::Constants::HeroID*)wparam)[3]);
                        break;
                    }
                    case GW::UI::UIMessage::kMapChange:
                        PreMapLoad();
                        break;
                    case GW::UI::UIMessage::kMapLoaded:
                        PostMapLoad();
                        break;
                    case GW::UI::UIMessage::kLogout: {
                        // prepare for potentially changing accounts.
                        SaveToFiles(false);
                        LoadFromFiles(true);
                        show_delete_note = false;
                        // can not reset reroll_stage here, since reroll trigger kLogout.
                        // instead we check whether accounts changed during PreMapLoad.
                        break;
                    }
                }
            }
        );
    }
    initializing = true;
    LoadFromFiles(false);
    auto ic = GW::GetItemContext();
    if (ic) {
        // fake a map load to clear missing items and remove deleted characters.
        PreMapLoad();
        for (auto const &i: ic->item_array) {
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
    while (!hero_bag_generation_order.empty()) hero_bag_generation_order.pop();
    bag_ptr_to_hero_id.clear();
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
        Log::Warning("InventoryScanner: timeout at stage %d", current_stage);
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
                if (GWToolbox::ShouldDisableToolbox(available_char.map_id()) || available_char.is_pvp()) 
                    continue;
                if (original_player == available_char.player_name)
                    continue;
                reroll_char_queue.push_back(available_char.player_name);
            }
            Set(InventoryScanner::Stage::NextCharacter);
        } break;
        case InventoryScanner::Stage::NextCharacter: {
            if (reroll_char_queue.empty()) {
                // Restore original heroes
                for (auto hero_id : original_player_heroes) {
                    GW::PartyMgr::AddHero(hero_id);
                }
                SaveToFiles(false);
                Cancel();
                Log::Info("Inventory scan completed");
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
            // Grab unlocked hero ids
            const auto w = GW::GetWorldContext();
            const auto h = w ? &w->hero_info : nullptr;
            if (h) {
                for (auto& hero : *h) {
                    if (ToolboxUtils::IsHeroUnlocked(hero.hero_id))
                        queued_hero_ids.push_back(hero.hero_id);
                }
            }
            GW::PartyMgr::LeaveParty();
            Set(InventoryScanner::Stage::WaitForEmptyParty);
        } break;
        case InventoryScanner::Stage::WaitForEmptyParty: {
            const auto party = GW::PartyMgr::GetPartyInfo();
            if (!(party && party->GetPartySize() == 1)) break; // Party not empty

            // Add any queued heroes that need checking
            if (queued_hero_ids.empty()) {
                // Checked all heroes for this character, restore original heroes
                for (auto hero_id : original_heroes) {
                    GW::PartyMgr::AddHero(hero_id);
                }
                Set(InventoryScanner::Stage::DoRestoreHeroes);
                break;
            }
            const auto map_info = GW::Map::GetMapInfo();
            const auto max_heroes_per_batch = std::min(7u,map_info && map_info->max_party_size > 1 ? map_info->max_party_size - 1 : 1);

            heroes_pending_load.clear();
            while (!queued_hero_ids.empty() && heroes_pending_load.size() < max_heroes_per_batch) {
                const auto next_hero = queued_hero_ids.back();
                queued_hero_ids.pop_back();
                if (!GW::PartyMgr::AddHero(next_hero)) {
                    Cancel("Failed to add hero");
                    break;
                }
                heroes_pending_load.push_back(next_hero);
            }
            Set(InventoryScanner::Stage::WaitForHeroLoad);
        } break;
        case InventoryScanner::Stage::WaitForHeroLoad: {
            auto waiting = GetPartyHeroIDs() != heroes_pending_load;
            for (auto hero_id : heroes_pending_load) {
                if (!GW::Items::GetHeroInventory(hero_id)) {
                    waiting = true;
                    break;
                }
            }
            if (waiting) break;
            // Assume at this stage that our code has added the hero's inventory to our list?
            GW::PartyMgr::KickAllHeroes();
            Set(InventoryScanner::Stage::WaitForEmptyParty);
        } break;
        case InventoryScanner::Stage::DoRestoreHeroes: {
            if (GetPartyHeroIDs() == original_heroes) 
                Set(InventoryScanner::Stage::NextCharacter);
        } break;
    }
}

void ItemReroller::Update() {
    if (current_stage == ItemReroller::Stage::None) return;
    if (TIMER_DIFF(stage_set_at) > 10000) {
        Log::Warning("ItemReroller: timeout at stage %d", current_stage);
        Cancel();
        
    }

    const auto is_map_loaded = GW::Map::GetIsMapLoaded() && !GW::UI::IsLoadingScreenShown();
    if (!is_map_loaded) return;
    switch (current_stage) {
        case ItemReroller::Stage::Start: {
            if (!memeq(&item.account, &current_account)) {
                Cancel("Item belongs to another account");
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
                if (!(hero_agent && GW::PartyMgr::IsAgentInParty(hero_agent->agent_id))) 
                    break;
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

                // can only move from current player or from chest
                if (item.character != GetCurrentPlayerNameS() && !IsChestBag(item.bag_id)) {
                    Cancel();
                    break;
                }
                InventoryManager::MoveItem((InventoryManager::Item*)GW::Items::GetItemById(loc->item->item_id));
            }
            Cancel();
            // Done.            
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
        } else {
            save_dirty_inventories_timer = 0;
        }
    }
}

void AccountInventoryWindow::PreMapLoad()
{
    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();
    inventory_lookup.clear(); // discard now outdated id caches
    while (!hero_bag_generation_order.empty()) hero_bag_generation_order.pop();
    bag_ptr_to_hero_id.clear();

    Account& acc = GetOrCreateAccount(current_account);
    if (acc.account_representing_character.empty()) {
        auto available_characters = GW::AccountMgr::GetAvailableChars();
        if (available_characters->size() > 0) {
            // alphabetically first character name, to be shown in tooltip to distinguish chests from multiple accounts without showing email addresses
            const wchar_t *min = nullptr;
            for (const auto& available_char : *available_characters) {
                if (!min || wcscmp(available_char.player_name, min) < 0) min = available_char.player_name;
            }
            acc.account_representing_character = TextUtils::WStringToString(min ? min : L"");
        }
    }
    std::string characters[] = {"(Chest)", current_character};
    for (auto & character: characters) {
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

    const GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
    const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
    if (party_info && me) {
        for (const auto &hero: party_info->heroes) {
            if (hero.owner_player_id != me->login_number) continue;
            HandleHeroBag(hero.hero_id);
        }
    }

    // clear empty slots in case inventory was changed without toolbox running.
    // update item->equipped flags.
    // track inventory size in order to display number of free slots
    if (gw_inventory) {
        uint32_t max_chest = 0;
        uint32_t max_equipment = 0;
        uint32_t max_inventory = 0;
        bool last_chest_pane_contains_any_item = false;
        for (uint32_t j = 1; j < _countof(gw_inventory->bags); ++j) {
            auto bag = gw_inventory->bags[j];
            const auto bag_id = static_cast<GW::Constants::Bag>(j);
            const auto character = IsChestBag(bag_id) ? "(Chest)"  : current_character;
            if (!bag) {
                // clear slots in case a previously present bag was removed
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
                    // item->equipped is never set when an item triggers InventorySlotUpdated on map load.
                    // manually check and reapply after every map load.
                    if (const auto found = inventory_lookup.find(item->item_id); found != inventory_lookup.end()) {
                        ItemLoc& loc = found->second;
                        if (loc.item->equipped != item->equipped) {
                            loc.item->equipped = item->equipped;
                            const std::string ch_name = loc.character ? loc.character->name : "(Chest)";
                            inventory_dirty.insert(GetIniID(loc.account->uuid, ch_name));
                        }
                    }
                } else {
                    ClearMissingItem(&current_account, character, GW::Constants::HeroID::NoHero, bag_id, slot);
                }
            }
            if (bag_id == GW::Constants::Bag::Equipment_Pack) {
                max_equipment = std::size(bag->items);
            } else if (BagCanHoldAnything(bag_id)) {
                if (IsChestBag(bag_id)) {
                    max_chest += std::size(bag->items);
                } else {
                    max_inventory += std::size(bag->items);
                }
            }
        }
        if (Account* acc = FindAccount(current_account)) {
            if (const auto it = acc->characters.find(current_character);
                it != acc->characters.end() && it->second.free_slots.known) {
                it->second.free_slots.max_equipment = max_equipment;
                it->second.free_slots.max_inventory = max_inventory;
            }
            if (acc->chest_free_slots.known) {
                // Since we do not know whether the anniversary storage pane is actually available,
                // assume it is not, unless there has been at least one item in it at some point.
                if (acc->anniversary_pane_active || last_chest_pane_contains_any_item) {
                    acc->anniversary_pane_active = true;
                } else {
                    max_chest -= 25;
                }
                acc->chest_free_slots.max_inventory = max_chest;
            }
        }
    }

    // erase deleted characters
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
                    // character was deleted in-game: drop the whole node (its bags,
                    // heroes and free slots) and any live lookup entries pointing into it.
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
        SaveToFiles(false); // save inventory in outposts only to avoid impacting gameplay
    }
}

void AccountInventoryWindow::HandleHeroBag(GW::Constants::HeroID hero_id)
{
    if (initializing) return;
    // If two parties are joined in a way s.t. our hero is in a different position afterwards,
    // it gets added again, so we readd the hero bag we already know.
    // Otherwise the heroes bag should have been added to hero_bag_generation_order before this,
    // since hero bags are never empty.
    GW::Bag * bag = OnPartyRemoveHero(hero_id);
    if (!bag) {
        ASSERT(!hero_bag_generation_order.empty());
        bag = hero_bag_generation_order.front();
        hero_bag_generation_order.pop();
    }
    bag_ptr_to_hero_id[bag] = hero_id;
    for (uint32_t slot = 0; slot < std::size(bag->items); ++slot) {
        auto item = bag->items[slot];
        if (!item) ClearMissingItem(&current_account, current_character, hero_id, GW::Constants::Bag::Equipped_Items, slot);
        else AddItem(item->item_id);
    }
}

void AccountInventoryWindow::GatherAllInventories()
{
    inventory_scan.Begin();
}

void AccountInventoryWindow::OnPartyAddHero(GW::Constants::HeroID hero_id)
{
    HandleHeroBag(hero_id);
}

GW::Bag* AccountInventoryWindow::OnPartyRemoveHero(GW::Constants::HeroID hero_id)
{
    GW::Bag * bag = nullptr;
    for (auto it = bag_ptr_to_hero_id.begin(); it != bag_ptr_to_hero_id.end();) {
        if (it->second == hero_id) {
            bag = it->first;
            it = bag_ptr_to_hero_id.erase(it);
        } else {
            ++it;
        }
    }
    return bag;
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
        if (ImGui::Begin("Account Inventory loading in progress")) {
            ImGui::TextWrapped("Please do not interrupt inventory loading.");
            if (ImGui::Button("Abort!")) {
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

    // view related settings
    ImGui::Checkbox("Detailed View", &detailed_view);
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("Merge Stacks", &merge_stacks)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("Hide other Accounts", &hide_other_accounts)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("Hide Equipment", &hide_equipment)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("Hide Equipment Packs", &hide_equipment_pack)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("Hide Hero Armor", &hide_hero_armor)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkbox_max_width) ImGui::NewLine();
    if (ImGui::Checkbox("Hide unclaimed Items", &hide_unclaimed_items)) needs_sorting = true;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < 110.f * font_scale) ImGui::NewLine();
    if (ImGui::Button("Gather Inventories")) {
        ImGui::ConfirmDialog("In order to load all available items, this will cycle\nthrough all characters and all heroes.\nThis will take a few minutes if you have many characters.\nAre you sure?", [](bool result, void*) {
            if (result) AccountInventoryWindow::Instance().GatherAllInventories();
        });
    }

    const auto color_disabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    if (ImGui::CollapsingHeader("Free Slots")) {
        if (!ImGui::BeginTable("###freeslots", SlotColumnID_Max, ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::End();
            return;
        }
        ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_WidthFixed, 0.f, SlotColumnID_Character);
        ImGui::TableSetupColumn("Inventory", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed, 0.f, SlotColumnID_Inventory);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, SlotColumnID_InventorySize);
        ImGui::TableSetupColumn("Equipment", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, SlotColumnID_Equipment);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, SlotColumnID_EquipmentSize);
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
            const bool is_chest = free_slot->character == "(Chest)";
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
    if (detailed_view) {
        flags |= ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg;
    }
    else {
        flags |= ImGuiTableFlags_SizingFixedFit;
    }

    // filter/sort header table
    if (!ImGui::BeginTable("###itemstable", ItemColumnID_Max, flags, ImVec2(inner_width, detailed_view ? items_table_height : 2 * ImGui::GetFrameHeight()))) {
        ImGui::End();
        return;
    }
    ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_Character);
    ImGui::TableSetupColumn("Location / Hero", ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_Location);
    ImGui::TableSetupColumn("Model ID", ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_ModelID);
    ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, 0.f, ItemColumnID_Description);
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
    ImGui::Text("Filter   %d/%d Items", filtered_item_count, item_refs.size());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGuiTableSortSpecs* item_sort_specs = ImGui::TableGetSortSpecs();
    if (needs_sorting || (item_sort_specs && item_sort_specs->SpecsDirty)) {
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

        if (detailed_view) {
            const std::string suffix = (ims.i.size() > 1) ? " +" : "";
            ImGui::Text("%s%s", i_front->character_name.c_str(), suffix.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s%s", i_front->location.c_str(), suffix.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", i_front->item->model_id);
            ImGui::TableNextColumn();
            style.ButtonTextAlign = ImVec2(0.f, 0.5f);
            const auto description_one_line = ims.GetDescription();
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
            if (i_front->item->texture && *(i_front->item->texture))
                clicked = ImGui::IconButton(nullptr, *i_front->item->texture, button_size, ImGuiButtonFlags_None, button_size);
            else
                clicked = ImGui::Button("???", button_size);

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

    if (detailed_view) {
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
        ImGui::EndTable(); // end the filter/sort header table
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
        //ImGui::Text("Rendered: %d / %d", rendered_cells, item_count);
    }

    ImGui::End();
}

void AccountInventoryWindow::DrawSettingsInternal()
{
    auto font_scale = ImGui::FontScale();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ImGui::Text("Account Inventory shows a combined view of all player, hero and storage inventories.");
    if (ImGui::Button("Gather Inventories")) {
        visible = true;
        ImGui::ConfirmDialog("In order to load all available items, this will cycle\nthrough all characters and all heroes.\nThis will take a few minutes if you have many characters.\nAre you sure?", [](bool result, void*){if (result) AccountInventoryWindow::Instance().GatherAllInventories();});
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Account Inventory")) {
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
    if (ImGui::Button("Delete All Inventories")) {
        show_delete_note = true;
        LoadFromFiles(false); // reload everything first, so we are aware of all inventory inis currently on disk
        accounts.clear();
        inventory_lookup.clear();
        item_refs.clear();
        inventory_sorted.clear();
        slot_rows.clear();
        free_slots_sorted.clear();
        for (auto it = ini_by_path.begin(); it != ini_by_path.end(); ++it) {
            it->second->Reset();
        }
        SaveToFiles(true);
    }
    ImGui::Checkbox("###account_inventory_detailed_view", &detailed_view);
    ImGui::SameLine();
    ImGui::Text("Detailed View - Toggle between detailed list and icon grid view.");
    ImGui::Checkbox("###account_inventory_merge_stacks", &merge_stacks);
    ImGui::SameLine();
    ImGui::Text("Merge Stacks - Combine multiple of the same item, including non-stackable items.");
    ImGui::Checkbox("###account_inventory_hide_other_accounts", &hide_other_accounts);
    ImGui::SameLine();
    ImGui::Text("Hide other Accounts - Hide item which do not belong to the currently active account.");
    ImGui::Checkbox("###account_inventory_hide_equipment", &hide_equipment);
    ImGui::SameLine();
    ImGui::Text("Hide Equipment - Hide items currently equipped or part of a weapon set.");
    ImGui::Checkbox("###account_inventory_hide_equipment_pack", &hide_equipment_pack);
    ImGui::SameLine();
    ImGui::Text("Hide Equipment Packs - Hide contents of equipment packs.");
    ImGui::Checkbox("###account_inventory_hide_hero_armor", &hide_hero_armor);
    ImGui::SameLine();
    ImGui::Text("Hide Hero Armor - Hide armor worn by heroes.");
    ImGui::Checkbox("###account_inventory_hide_unclaimed_items", &hide_unclaimed_items);
    ImGui::SameLine();
    ImGui::Text("Hide unclaimed Items - Hide items from the unclaimed items window.");
    ImGui::PopTextWrapPos();
    if (show_delete_note) {
        // we could just disable this module ourselves, if ToolboxSettings' ModuleToggle.enabled was part of ToolboxModule
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f * font_scale, 0.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Deleting Inventories", &show_delete_note)) {
            ImGui::TextWrapped("Make sure to disable Account Inventory in Toolbox Settings -> Windows to stop it from regathering inventory data.");
            if (ImGui::Button("Ok")) {
                show_delete_note = false;
            }
        }
        ImGui::End();
    }
}

void AccountInventoryWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    current_account = GW::AccountMgr::GetAccountUuid();
    current_character = GetCurrentPlayerNameS();

    LOAD_BOOL(detailed_view);
    LOAD_BOOL(merge_stacks);
    LOAD_BOOL(hide_other_accounts);
    LOAD_BOOL(hide_equipment);
    LOAD_BOOL(hide_equipment_pack);
    LOAD_BOOL(hide_hero_armor);
    LOAD_BOOL(hide_unclaimed_items);
    needs_sorting = true;
    // only LoadFromFiles foreign items here. allowing the user to reload inventory data of the active account, may cause temporary inconsistencies
    LoadFromFiles(true);
}

void AccountInventoryWindow::SaveSettings(ToolboxIni* ini)
{
    SAVE_BOOL(hide_unclaimed_items);
    SAVE_BOOL(hide_hero_armor);
    SAVE_BOOL(hide_equipment_pack);
    SAVE_BOOL(hide_equipment);
    SAVE_BOOL(hide_other_accounts);
    SAVE_BOOL(merge_stacks);
    SAVE_BOOL(detailed_view);
    ToolboxWindow::SaveSettings(ini);
    SaveToFiles(false);
}
