#include "stdafx.h"
#include <fileapi.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
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

#define memeq(a, b) (memcmp((a), (b), sizeof(*(a))) == 0)


namespace {

    constexpr float ITEMS_TABLE_MIN_HEIGHT = 220.f;
    constexpr int CHEST_ARMOR_INVENTORY_SLOT = 2;
    constexpr clock_t ADD_HERO_TIMEOUT = 500;
    constexpr clock_t SAVE_HERO_TIMEOUT = 500;
    constexpr clock_t MAP_LOADED_DELAYED_TIMEOUT = 400;
    constexpr clock_t SAVE_DIRTY_INVENTORIES_TIMEOUT = 1000;

const char* HERO_NAME[] = {
        "(Player)",
        "Norgu",
        "Goren",
        "Tahlkora",
        "Master Of Whispers",
        "Acolyte Jin",
        "Koss",
        "Dunkoro",
        "Acolyte Sousuke",
        "Melonni",
        "Zhed Shadowhoof",
        "General Morgahn",
        "Margrid The Sly",
        "Zenmai",
        "Olias",
        "Razah",
        "MOX",
        "Keiran Thackeray",
        "Jora",
        "Pyre Fierceshot",
        "Anton",
        "Livia",
        "Hayda",
        "Kahmu",
        "Gwen",
        "Xandra",
        "Vekk",
        "Ogden",
        "Mercenary Hero 1",
        "Mercenary Hero 2",
        "Mercenary Hero 3",
        "Mercenary Hero 4",
        "Mercenary Hero 5",
        "Mercenary Hero 6",
        "Mercenary Hero 7",
        "Mercenary Hero 8",
        "Miku",
        "Zei Ri",
        "Devona",
        "Ghost of Althea"
    };


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

    UUID GetAccountGuid() {
        const auto account_uuid = GW::AccountMgr::GetPortalAccountUuid();
        return account_uuid ? *account_uuid : TextUtils::ConvertWStringToGuid(GW::AccountMgr::GetAccountEmail());
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

    std::string GetIniID(const GUID& account, const std::string& character)
    {
        if (character == "(Chest)") {
            return TextUtils::GuidToString(&account);
        }
        else {
            return character;
        }
    }

    void RightAlignText(const std::string& text)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x - ImGui::GetScrollX());
        ImGui::Text("%s", text.c_str());
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

    enum RerollStage { None, NextCharacter, WaitForCharacterLoad, DoSaveHeroes, DoneCharacterLoad, WaitForHeroLoad, DoneHeroLoad, DoRestoreHeroes, RerollToItem, WaitForHeroWithItem };

    struct InventoryItem {
        // identifying attributes
        GUID account{};
        std::string character{};
        GW::Constants::HeroID hero_id = (GW::Constants::HeroID)0;
        GW::Constants::Bag bag_id = (GW::Constants::Bag)0;
        uint32_t slot{};

        uint32_t model_id{};
        uint32_t model_file_id{};
        uint32_t interaction{};
        uint16_t quantity{};
        uint8_t equipped{};
        std::wstring description{}; // output of AsyncDecodeStr(ShorthandItemDescription)

        uint32_t item_id{};
        // caches, do not serialize
        IDirect3DTexture9** texture; // output of GetItemImage
        std::string location{};     // (Player) <Storage Pane> or <Hero Name>

        void CopyKeyTo(InventoryItem* i)
        {
            i->account = account;
            i->character = character;
            i->hero_id = hero_id;
            i->bag_id = bag_id;
            i->slot = slot;
        }
    };

struct MergeStack;

    struct ItemCompare {
        ImGuiTableSortSpecs* sort_specs{};
        UUID current_account{};
        bool operator()(const MergeStack& lms, const MergeStack& rms) const;
        bool operator()(InventoryItem* l, InventoryItem* r) const;
    };

    struct MergeStack {
        uint16_t quantity;
        std::string description;
        std::set<InventoryItem*, ItemCompare> i;
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
                        delta = l->character.compare(r->character);
                        break;
                    case ItemColumnID_Location:
                        delta = l->location.compare(r->location);
                        break;
                    case ItemColumnID_ModelID:
                        delta = l->model_id - r->model_id;
                        break;
                    case ItemColumnID_Description:
                        delta = lms.description.compare(rms.description);
                        break;
                }
                if (delta != 0) return delta * sort_direction < 0;
            }
        }
        // fallback
        if (delta == 0) delta = l->character.compare(r->character);
        if (delta == 0) delta = l->location.compare(r->location);
        if (delta == 0) delta = (int)l->bag_id - (int)r->bag_id;
        if (delta == 0) delta = l->slot - r->slot;
        if (delta == 0) delta = memcmp(&l->account, &r->account, sizeof(l->account));
        return delta * sort_direction < 0;
    }
    bool ItemCompare::operator()(InventoryItem* l, InventoryItem* r) const
    {
        if (l->account != r->account) {
            // lowest item is the one that can be interacted with. Make sure it is one on this account if there is one
            if (memeq(&l->account, &current_account)) return true;
            if (memeq(&r->account, &current_account)) return false;
        }
        auto lms = MergeStack(l->account, L"");
        lms.quantity = l->quantity;
        lms.i.insert(l);
        auto rms = MergeStack(r->account, L"");
        rms.quantity = r->quantity;
        rms.i.insert(r);
        return this->operator()(lms, rms);
    }
    MergeStack::MergeStack(const UUID& account, const std::wstring& _description) : quantity{}, i(ItemCompare{nullptr, account}) {
        description = TextUtils::WStringToString(_description);
    }

    struct ItemHash {
        using is_transparent = void;
        std::size_t operator()(const std::unique_ptr<InventoryItem>& i) const noexcept { return operator()(std::to_address(i)); }
        std::size_t operator()(const InventoryItem* const* const i) const noexcept { return operator()(*i); }
        std::size_t operator()(const InventoryItem* const i) const noexcept
        {
            std::size_t h1 = std::hash<uint32_t>{}(i->account.Data1) ^ std::hash<uint32_t>{}(i->account.Data2) ^ std::hash<uint32_t>{}(i->account.Data3);
            std::size_t h2 = std::hash<std::string>{}(i->character);
            std::size_t h3 = std::hash<uint32_t>{}(i->hero_id);
            std::size_t h4 = std::hash<uint32_t>{}(static_cast<uint32_t>(i->bag_id));
            std::size_t h5 = std::hash<uint32_t>{}(i->slot);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };

    struct ItemEqual {
        using is_transparent = void;
        bool operator()(const std::unique_ptr<InventoryItem>& l, const std::unique_ptr<InventoryItem>& r) const { return operator()(std::to_address(l), std::to_address(r)); }
        bool operator()(const InventoryItem* const l, const std::unique_ptr<InventoryItem>& r) const { return operator()(l, std::to_address(r)); }
        bool operator()(const InventoryItem* const* const l, const std::unique_ptr<InventoryItem>& r) const { return operator()(*l, std::to_address(r)); }
        bool operator()(const InventoryItem* const l, const InventoryItem* const r) const { return l->hero_id == r->hero_id && l->bag_id == r->bag_id && l->slot == r->slot && l->account == r->account && l->character == r->character; }
    };

    struct CharacterFreeSlots {
        UUID account;
        std::string character{};
        std::string account_representing_character{}; // character name to tell which account a chest inventory belongs to without showing an email address
        uint32_t max_inventory{};
        uint32_t max_equipment{};
        uint32_t occupied_inventory{};
        uint32_t occupied_equipment{};
        bool anniversary_pane_active;
    };

    struct SlotCompare {
        ImGuiTableSortSpecs* sort_specs{};
        bool operator()(const CharacterFreeSlots* const l, const CharacterFreeSlots* const r) const
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

    struct SlotHash {
        using is_transparent = void;
        std::size_t operator()(const std::unique_ptr<CharacterFreeSlots>& i) const noexcept { return operator()(std::to_address(i)); }
        std::size_t operator()(const CharacterFreeSlots* const* const i) const noexcept { return operator()(*i); }
        std::size_t operator()(const CharacterFreeSlots* const i) const noexcept
        {
            const uint64_t* parts = reinterpret_cast<const uint64_t*>(&i->account);
            std::size_t h1 = std::hash<uint64_t>{}(parts[0]) ^ (std::hash<uint64_t>{}(parts[1]) << 1);
            std::size_t h2 = std::hash<std::string>{}(i->character);
            return h1 ^ (h2 << 1);
        }
    };

    struct SlotEqual {
        using is_transparent = void;
        bool operator()(const std::unique_ptr<CharacterFreeSlots>& l, const std::unique_ptr<CharacterFreeSlots>& r) const { return operator()(std::to_address(l), std::to_address(r)); }
        bool operator()(const CharacterFreeSlots* const l, const std::unique_ptr<CharacterFreeSlots>& r) const { return operator()(l, std::to_address(r)); }
        bool operator()(const CharacterFreeSlots* const* const l, const std::unique_ptr<CharacterFreeSlots>& r) const { return operator()(*l, std::to_address(r)); }
        bool operator()(const CharacterFreeSlots* const l, const CharacterFreeSlots* const r) const { return l->account == r->account && l->character == r->character; }
    };

    class InventoryIni : public ToolboxIni {
    public:
        FILETIME last_change_time{};
        GUID account{};
        std::string ini_ID{}; // character name for character/hero inventories, email for xunlai chests
        InventoryIni(std::filesystem::path _location_on_disk) { location_on_disk = _location_on_disk; }
    };

    void OnItemTooltip(const MergeStack* ms);
    void OnInventoryItemClicked(InventoryItem* i, bool move);
    static bool CheckIniDirty(InventoryIni* ini);
    InventoryIni* GetIni(const std::string& ini_ID, const GUID& account);
    std::string ItemToSectionName(InventoryItem* i);
    void LoadFromFiles(bool only_foreign);
    void SaveToFiles(bool include_foreign);
    void SortSlots(ImGuiTableSortSpecs* sort_specs);
    void DescriptionDecode(InventoryItem* i, GW::Item* item);
    void ClearMissingItem(const UUID* account, const std::string& character, const GW::Constants::HeroID hero_id, const GW::Constants::Bag bag_id, const uint32_t slot);
    // state machine for rerolling to items, internal functions
    void StepReroll();
    void SaveHeroes();
    void RestoreHeroes();
    void MoveItem();

    // collective callback hook
    GW::HookEntry OnUIMessage_HookEntry{};
    // main item storage
    std::unordered_set<std::unique_ptr<InventoryItem>, ItemHash, ItemEqual> inventory{};
    // On*SlotCleared send an item_id, but the information which bag and slot it was in
    // is already removed. In order to remove items from inventory without iterating,
    // we keep track of the item_id->InventoryItem mapping
    std::unordered_map<uint32_t, InventoryItem*> inventory_lookup{};
    // sorted/filtered view for display
    std::vector<MergeStack> inventory_sorted{};
    // ini files, 1 per character/chest
    std::unordered_map<std::filesystem::path, std::unique_ptr<InventoryIni>> ini_by_path{};
    std::unordered_map<std::string, InventoryIni*> ini_by_character{};
    // change tracker to reduce writes
    std::unordered_set<std::string> inventory_dirty{};
    // tracking of free inventory slot numbers
    std::unordered_set<std::unique_ptr<CharacterFreeSlots>, SlotHash, SlotEqual> free_slots{};
    // sorted/filtered view for display
    std::set<CharacterFreeSlots*, SlotCompare> free_slots_sorted{};
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
    RerollStage reroll_stage = RerollStage::None;
    std::vector<GW::AvailableCharacterInfo> reroll_char_queue{};
    std::vector<uint32_t> reroll_hero_queue{};
    std::vector<uint32_t> cached_heroes{};
    clock_t add_hero_timer{};
    clock_t save_hero_timer{};
    clock_t map_loaded_delayed_timer{};
    clock_t save_dirty_inventories_timer{};
    bool map_loaded_delayed_trigger = false;
    InventoryItem* item_to_move = nullptr;

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
    
    void SaveHeroes()
    {
        if (!cached_heroes.empty()) return;
        const GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        if (!(party_info && me)) return;
        for (const auto& hero : party_info->heroes) {
            if (hero.owner_player_id != me->login_number) continue;
            cached_heroes.push_back(hero.hero_id);
        }
    }

    void RestoreHeroes()
    {
        GW::PartyMgr::KickAllHeroes();
        if (cached_heroes.empty()) {
            reroll_stage = RerollStage::NextCharacter;
            StepReroll();
            return;
        }
        for (auto hero_id : cached_heroes) {
            GW::PartyMgr::AddHero((GW::Constants::HeroID)hero_id);
        }
    }

    void StepReroll()
    {
        GW::AvailableCharacterInfo next_char{};
        uint32_t next_hero{};
        switch (reroll_stage) {
            case RerollStage::NextCharacter:
                if (reroll_char_queue.empty()) {
                    reroll_stage = RerollStage::None;
                    SaveToFiles(false);
                    return;
                }
                reroll_stage = RerollStage::WaitForCharacterLoad;
                next_char = reroll_char_queue.back();
                reroll_char_queue.pop_back();
                reroll_hero_queue = CompletionWindow::GetCharacterCompletion(next_char.player_name)->heroes;
                if (!RerollWindow::Instance().Reroll(next_char.player_name, false, false, true, false)) {
                    reroll_stage = RerollStage::None;
                }
                return;
            case RerollStage::DoneHeroLoad:
                if (reroll_hero_queue.empty()) {
                    reroll_stage = RerollStage::DoRestoreHeroes;
                    RestoreHeroes();
                    return;
                }
                // fall through
            case RerollStage::DoneCharacterLoad:
                if (reroll_hero_queue.empty()) {
                    reroll_stage = RerollStage::NextCharacter;
                    StepReroll();
                    return;
                }
                reroll_stage = RerollStage::WaitForHeroLoad;
                next_hero = reroll_hero_queue.back();
                reroll_hero_queue.pop_back();
                GW::PartyMgr::KickAllHeroes();
                GW::PartyMgr::AddHero((GW::Constants::HeroID)next_hero);
                add_hero_timer = TIMER_INIT();
                return;
            case RerollStage::RerollToItem:
                reroll_stage = RerollStage::None;
                if (cached_heroes.empty()) {
                    // no cached_heroes means we rerolled for an item on the player character
                    // move the item, but skip any hero related setup
                    MoveItem();
                    return;
                }
                auto hero_id = cached_heroes.front();
                // do not kick heroes, if the one we need is already added
                bool hero_is_present = false;
                for (auto it = bag_ptr_to_hero_id.begin(); it != bag_ptr_to_hero_id.end(); ++it) {
                    if (it->second == hero_id) {
                        hero_is_present = true;
                        break;
                    }
                }
                if (hero_is_present) {
                    // hero is already in the party, just move the item
                    MoveItem();
                }
                else {
                    // MoveItem will be triggered through AddItem, once the item is available
                    reroll_stage = RerollStage::WaitForHeroWithItem;
                    GW::PartyMgr::KickAllHeroes();
                    GW::PartyMgr::AddHero((GW::Constants::HeroID)hero_id);
                    add_hero_timer = TIMER_INIT();
                }
                cached_heroes.clear();
                return;
        }
    }

    void MoveItem()
    {
        if (!item_to_move) return;
        auto it = inventory.find(item_to_move);
        delete item_to_move;
        item_to_move = nullptr;
        if (it == inventory.end()) return;
        auto i = new InventoryItem();
        i->character = (*it)->character;
        i->bag_id = (*it)->bag_id;
        i->slot = (*it)->slot;
        i->model_id = (*it)->model_id;
        i->item_id = (*it)->item_id;

        // can only move from current player or from chest
        if (i->character != GetCurrentPlayerNameS() && !IsChestBag(i->bag_id)) return;

        GW::GameThread::Enqueue([i = i] {
            auto item = GW::Items::GetItemById(i->item_id);
            // plausibilize that our item_id was up to date, this should only be false immediately after loading a new map
            if (item && item->bag && item->slot == i->slot && item->bag->bag_id() == i->bag_id && item->model_id == i->model_id) {
                if (!IsChestBag(i->bag_id)) GW::Items::OpenXunlaiWindow();
                InventoryManager::MoveItem((InventoryManager::Item*)item);
            }
            delete i;
        });
    }

    // jump to location of clicked item, i.e. open chest/add hero/change character
    // with Ctrl: move item to/from chest after jump
    void OnInventoryItemClicked(InventoryItem* i, bool move)
    {
        if (map_loaded_delayed_trigger || reroll_stage != RerollStage::None || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return;
        if (!memeq(&i->account,&current_account)) return;
        if (move) {
            if (IsHeroArmor(i->hero_id, i->slot)) return; // can not unequip hero armor
            item_to_move = new InventoryItem();
            i->CopyKeyTo(item_to_move);
        }
        else if (item_to_move) {
            delete item_to_move;
            item_to_move = nullptr;
        }
        bool item_is_in_chest = IsChestBag(i->bag_id);
        bool item_is_on_current_character = i->character == GetCurrentPlayerNameS();
        bool item_is_on_hero = GW::Constants::HeroID::NoHero != i->hero_id;
        if (item_is_on_hero) {
            cached_heroes.clear();
            cached_heroes.push_back(i->hero_id);
        }

        if (item_is_in_chest && item_to_move) {
            MoveItem();
        }
        else if (item_is_in_chest && !item_to_move) {
            GW::GameThread::Enqueue([bag_id = i->bag_id]() {
                uint32_t pane;
                if (bag_id == GW::Constants::Bag::Material_Storage)
                    pane = (uint32_t)GW::Constants::StoragePane::Material_Storage;
                else
                    pane = (uint32_t)bag_id - (uint32_t)GW::Constants::Bag::Storage_1;
                GW::UI::SetPreference(GW::UI::NumberPreference::StorageBagPage, pane);
                GW::Items::OpenXunlaiWindow();
            });
        }
        else if (item_is_on_current_character && item_is_on_hero) {
            // MoveItem will be triggered through StepReroll if the hero is already present.
            // Otherwise through AddItem, once the heroes inventory has been added by GW
            reroll_stage = RerollStage::RerollToItem;
            StepReroll();
        }
        else if (item_is_on_current_character && !item_is_on_hero && item_to_move) {
            MoveItem();
        }
        else if (!item_is_on_current_character) {
            // MoveItem will be triggered through StepReroll if the item is on a player character or
            // if the corresponding hero is already present.
            // Otherwise through AddItem, once the heroes inventory has been added by GW
            reroll_stage = RerollStage::RerollToItem;
            if (!RerollWindow::Instance().Reroll(TextUtils::StringToWString(i->character).c_str(), false, false, true, false)) {
                // reroll failed, abort
                reroll_stage = RerollStage::None;
                if (item_to_move) {
                    delete item_to_move;
                    item_to_move = nullptr;
                }
            }
        }
    }

    void OnItemTooltip(const MergeStack* ms)
    {

        std::string prev_character{};
        std::string prev_account_representing_character{};
        std::string prev_location{};
        for (auto it = ms->i.begin(); it != ms->i.end(); it++) {
            int style_count = 0;
            bool is_this_account = memeq(&(*it)->account, &current_account);
            if (is_this_account) {
                style_count = 1;
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            std::string account_representing_character;
            CharacterFreeSlots free_slot = CharacterFreeSlots{(*it)->account, (*it)->character};
            if (auto fs_it = free_slots.find(&free_slot); fs_it != free_slots.end()) {
                account_representing_character = (*fs_it)->account_representing_character;
            }
            bool reprint = (*it)->character != prev_character || account_representing_character != prev_account_representing_character;
            if (reprint) {
                std::string suffix = "";
                if (!is_this_account && (*it)->character == "(Chest)" && !account_representing_character.empty()) {
                    suffix = " [" + account_representing_character + "]";
                }
                ImGui::Text("%s%s", (*it)->character.c_str(), suffix.c_str());
            }
            reprint |= (*it)->location != prev_location;
            if (reprint) {
                ImGui::Text("- %s", (*it)->location.c_str());
            }
            ImGui::PopStyleColor(style_count);
            prev_account_representing_character = account_representing_character;
            prev_character = (*it)->character;
            prev_location = (*it)->location;
        }
        ImGui::Separator();
        ImGui::PushTextWrapPos(440.f * ImGui::FontScale());
        ImGui::Text("%s", ms->description.c_str());
        ImGui::PopTextWrapPos();
    }

    void SortSlots(ImGuiTableSortSpecs* sort_specs)
    {

        free_slots_sorted = std::set<CharacterFreeSlots*, SlotCompare>(SlotCompare{sort_specs});
        for (auto& free_slot : free_slots) {
            if (hide_other_accounts && !memeq(&free_slot->account,&current_account)) {
                continue;
            }
            free_slots_sorted.insert(free_slot.get());
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


        std::unordered_map<std::wstring, size_t> merged_stacks{};
        for (auto it = inventory.begin(); it != inventory.end(); ++it) {
            auto i = it->get();
            if (hide_other_accounts && i->account != current_account) continue;
            if (hide_equipment && (i->bag_id == GW::Constants::Bag::Equipped_Items || i->equipped)) continue;
            if (hide_equipment_pack && i->bag_id == GW::Constants::Bag::Equipment_Pack) continue;
            if (hide_hero_armor && IsHeroArmor(i->hero_id, i->slot)) continue;
            if (hide_unclaimed_items && i->bag_id == GW::Constants::Bag::Unclaimed_Items) continue;

            if (!name_filter.empty()) {
                const auto character_check = name_is_lower ? TextUtils::ToLower(i->character) : i->character;
                if (!character_check.contains(name_filter)) continue;
            }
            if (!location_filter.empty()) {
                const auto location_check = location_is_lower ? TextUtils::ToLower(i->location) : i->location;
                if (!location_check.contains(location_filter)) continue;
            }
            if (!model_ID_filter.empty() && model_ID_filter != std::to_string(i->model_id)) continue;
            if (!item_filter_w.empty()) {
                const auto description_check = item_is_lower ? TextUtils::ToLower(i->description) : i->description;
                if (!description_check.contains(item_filter_w)) continue;
            }

            auto merge_id = std::to_wstring(i->model_id) + i->description;
            if (!merge_stacks || !merged_stacks.contains(merge_id)) {
                merged_stacks[merge_id] = inventory_sorted.size();
                inventory_sorted.push_back(MergeStack(current_account, i->description));
            }
            MergeStack* ms = &inventory_sorted[merged_stacks[merge_id]];
            ms->quantity += i->quantity;
            ms->i.insert(i);
        }
        filtered_item_count = inventory_sorted.size();

        if (inventory_sorted.size() > 1) std::sort(inventory_sorted.begin(), inventory_sorted.end(), ItemCompare{sort_specs, current_account});

        if (sort_specs) sort_specs->SpecsDirty = false;
        needs_sorting = false;
    }

    // unique section name for item in ini file
    std::string ItemToSectionName(InventoryItem* i)
    {
        char buf[9];
        std::string out;
        snprintf(buf, sizeof(buf), "%08x", i->hero_id);
        out.append(buf);
        snprintf(buf, sizeof(buf), "%08x", i->bag_id);
        out.append(buf);
        snprintf(buf, sizeof(buf), "%08x", i->slot);
        out.append(buf);
        return out;
    }

    bool CheckIniDirty(InventoryIni* ini)
    {
        FILETIME change_time;
        HANDLE f = CreateFileW(ini->location_on_disk.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) return false;
        bool res = GetFileTime(f, NULL, NULL, &change_time);
        CloseHandle(f);
        if (!res) return false;
        if (CompareFileTime(&change_time, &ini->last_change_time) != 0) {
            ini->last_change_time = change_time;
            return true;
        }
        return false;
    }


    InventoryIni* GetIni(const std::string& ini_ID, const GUID& account)
    {
        if (!ini_by_character.contains(ini_ID)) {
            wchar_t path[MAX_PATH];
            if (0 == GetTempFileNameW(Resources::GetPath(L"inventories").wstring().c_str(), L"inv", 0, path)) {
                Log::Error("Account Inventory: Failed to create inventory ini. Inventory tracking data will be lost.");
                return nullptr;
            }
            auto ini = std::make_unique<InventoryIni>(path);
            ini->ini_ID = ini_ID;
            ini->account = account;
            ini_by_character[ini_ID] = ini.get();
            ini_by_path[path] = std::move(ini);
        }
        return ini_by_character[ini_ID];
    }

    void LoadFromFiles(bool only_foreign)
    {
        ToolboxIni::TNamesDepend entries{};


        Resources::EnsureFolderExists(Resources::GetPath(L"inventories"));
        std::unordered_set<std::filesystem::path> visited;
        if (only_foreign) {
            for (auto it = inventory.begin(); it != inventory.end();) {
                if ((*it)->account != current_account) {
                    it = inventory.erase(it);
                }
                else
                    ++it;
            }
            for (auto it = free_slots.begin(); it != free_slots.end();) {
                if ((*it)->account != current_account) {
                    it = free_slots.erase(it);
                }
                else
                    ++it;
            }
        }
        else {
            inventory_lookup.clear();
        }
        for (const auto& file : std::filesystem::directory_iterator{Resources::GetPath(L"inventories")}) {
            auto path = file.path();
            visited.insert(path);
            if (!ini_by_path.contains(path)) {
                ini_by_path[path] = std::make_unique<InventoryIni>(path);
            }
            auto ini = ini_by_path[path].get();
            if (only_foreign && ini->account == current_account) continue;
            if (CheckIniDirty(ini)) {
                ini->Reset();
                if (ini->LoadFile(path.wstring()) < 0) continue;
            }
            ini->GetAllSections(entries);
            for (const ToolboxIni::Entry& entry : entries) {
                const char* section = entry.pItem;

                // account and character values must exist in both freeslot and item sections
                auto account_str = ini->GetValue(section, "account", "");
                GUID account;
                if (!TextUtils::StringToGuid(account_str, &account)) continue; // Error converting
                auto character = ini->GetValue(section, "character", "");
                if (ini->ini_ID.empty()) {
                    ini->ini_ID = GetIniID(account, character);
                    ini->account = account;
                    ini_by_character[ini->ini_ID] = ini;
                }
                else if (ini->ini_ID != GetIniID(account, character)) {
                    continue;
                }
                if (only_foreign && account == current_account) continue;

                if (std::string_view(section) == "freeslots") {
                    CharacterFreeSlots free_slot;
                    free_slot.account = account;
                    free_slot.character = character;
                    free_slot.account_representing_character = ini->GetValue(section, "account_character", "");
                    free_slot.max_equipment = (int)(ini->GetLongValue(section, "maxequipment", 0));
                    free_slot.max_inventory = (int)(ini->GetLongValue(section, "maxinventory", 0));
                    free_slot.occupied_equipment = (int)(ini->GetLongValue(section, "occupiedequipment", 0));
                    free_slot.occupied_inventory = (int)(ini->GetLongValue(section, "occupiedinventory", 0));
                    free_slot.anniversary_pane_active = ini->GetBoolValue(section, "anniversary_pane_active", false);
                    free_slots.insert(std::make_unique<CharacterFreeSlots>(free_slot));
                    continue;
                }

                auto i = std::make_unique<InventoryItem>();
                ;
                i->account = account;
                i->character = character;
                i->bag_id = (GW::Constants::Bag)(ini->GetLongValue(section, "bagid", 1));
                i->hero_id = (GW::Constants::HeroID)(ini->GetLongValue(section, "heroid", 0));
                i->slot = (uint32_t)(ini->GetLongValue(section, "slot", 0));

                i->model_id = (uint32_t)(ini->GetLongValue(section, "modelid", 0));
                i->model_file_id = (uint32_t)(ini->GetLongValue(section, "modelfileid", 0));
                i->interaction = (uint32_t)(ini->GetLongValue(section, "interaction", 0));
                i->quantity = (uint16_t)(ini->GetLongValue(section, "quantity", 0));
                i->equipped = (uint8_t)(ini->GetLongValue(section, "equipped", 0));
                GuiUtils::IniToArray(ini->GetValue(section, "description", ""), i->description);
                GW::Item item;
                item.model_file_id = i->model_file_id;
                item.interaction = i->interaction;
                i->texture = Resources::GetItemImage(&item);
                i->location = HERO_NAME[i->hero_id];
                if (IsChestBag(i->bag_id)) {
                    i->location = BAG_NAME[(int)(i->bag_id)];
                }
                if (auto it = inventory.find(i); it != inventory.end()) {
                    inventory.erase(it);
                }
                inventory.insert(std::move(i));
            }
        }
        for (auto it = ini_by_path.begin(); it != ini_by_path.end(); ++it) {
            if (!visited.contains(it->first)) it->second->Reset();
        }
        needs_sorting = true;
    }

    void SaveToFiles(bool include_foreign)
    {

        std::unordered_set<std::string> visited;

        for (auto& free_slot : free_slots) {
            auto ini_ID = GetIniID(free_slot->account, free_slot->character);
            if (!inventory_dirty.contains(ini_ID)) continue;
            if (free_slot->account != current_account) continue; // skip foreign items, only update what belongs to us
            auto ini = GetIni(ini_ID, free_slot->account);
            if (!ini) return;
            if (!visited.contains(ini_ID)) {
                ini->Reset();
                visited.insert(ini_ID);
            }
            auto section = "freeslots";
            ini->SetValue(section, "account", TextUtils::GuidToString(&free_slot->account).c_str());
            if (!free_slot->character.empty()) {
                ini->SetValue(section, "character", free_slot->character.c_str());
            }
            if (!free_slot->account_representing_character.empty()) {
                ini->SetValue(section, "account_character", free_slot->account_representing_character.c_str());
            }
            ini->SetLongValue(section, "maxequipment", free_slot->max_equipment);
            ini->SetLongValue(section, "maxinventory", free_slot->max_inventory);
            ini->SetLongValue(section, "occupiedequipment", free_slot->occupied_equipment);
            ini->SetLongValue(section, "occupiedinventory", free_slot->occupied_inventory);
            if (free_slot->anniversary_pane_active) {
                ini->SetBoolValue(section, "anniversary_pane_active", free_slot->anniversary_pane_active);
            }
        }

        for (auto& i : inventory) {
            auto ini_ID = GetIniID(i->account, i->character);
            if (!inventory_dirty.contains(ini_ID)) continue;
            if (i->account != current_account) continue; // skip foreign items, only update what belongs to us
            auto ini = GetIni(ini_ID, i->account);
            if (!ini) return;
            if (!visited.contains(ini_ID)) {
                ini->Reset();
                visited.insert(ini_ID);
            }
            auto section = ItemToSectionName(i.get());
            ini->SetValue(section.c_str(), "account", TextUtils::GuidToString(&i->account).c_str());
            ini->SetValue(section.c_str(), "character", i->character.c_str());
            ini->SetLongValue(section.c_str(), "heroid", (long)i->hero_id);
            ini->SetLongValue(section.c_str(), "bagid", (long)i->bag_id);
            ini->SetLongValue(section.c_str(), "slot", (long)i->slot);
            ini->SetLongValue(section.c_str(), "modelid", (long)i->model_id);
            ini->SetLongValue(section.c_str(), "modelfileid", (long)i->model_file_id);
            ini->SetLongValue(section.c_str(), "interaction", (long)i->interaction);
            ini->SetLongValue(section.c_str(), "quantity", (long)i->quantity);
            ini->SetLongValue(section.c_str(), "equipped", (long)i->equipped);
            std::string ini_desc;
            GuiUtils::ArrayToIni(i->description, &ini_desc);
            ini->SetValue(section.c_str(), "description", ini_desc.c_str());
        }
        for (auto it = ini_by_character.begin(); it != ini_by_character.end(); ++it) {
            auto ini_ID = it->first;
            if (!include_foreign && it->second->account != current_account) continue;
            if (include_foreign || visited.contains(ini_ID)) {
                if (it->second->SaveFile(it->second->location_on_disk.wstring().c_str()) != SI_OK) {
                    Log::Error("Account Inventory: Failed to save inventory ini. Inventory tracking data will be lost.");
                }
            }
            else if (inventory_dirty.contains(ini_ID)) {
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

    void DescriptionDecode(InventoryItem* i, GW::Item* item)
    {
        struct SyncDecode {
            InventoryItem i;
            std::wstring enc;
        };
        struct SyncDecode* sync = new SyncDecode();
        i->CopyKeyTo(&sync->i);
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
                    if (auto it = inventory.find(&sync->i); it != inventory.end()) {
                        (*it)->description = TextUtils::StripTags(s);
                        inventory_dirty.insert(GetIniID((*it)->account, (*it)->character));
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

        auto i = std::make_unique<InventoryItem>();
        i->account = current_account;
        i->bag_id = item->bag->bag_id();
        if (IsChestBag(i->bag_id)) {
            i->character = "(Chest)";
        }
        else {
            i->character = GetCurrentPlayerNameS();
            if (i->character.empty()) {
                i->character = "Unavailable";
            }
        }
        i->slot = item->slot;

        // This is a workaround because I could not find a way to get a hero_id from an item currently equipped on a hero.
        // item->bag->bag_array is a separate array for each hero with only the Equipped_Items bag set, but seemingly no reference back to the hero.
        // The workaround uses the fact that items are added by GW in the order of the respective heroes in the party.
        i->hero_id = GW::Constants::HeroID::NoHero;
        if (i->bag_id == GW::Constants::Bag::Equipped_Items && (GW::Inventory*)item->bag->bag_array != GW::Items::GetInventory()) {
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
                i->hero_id = bag_ptr_to_hero_id[item->bag];
            }
        }
        // END hero_id workaround

        i->model_id = item->model_id;
        i->model_file_id = item->model_file_id;
        i->interaction = item->interaction;
        i->quantity = item->quantity;
        i->equipped = item->equipped;
        i->item_id = item->item_id;
        i->texture = Resources::GetItemImage(item);
        i->location = HERO_NAME[i->hero_id];
        if (IsChestBag(i->bag_id)) {
            i->location = BAG_NAME[(int)(i->bag_id)];
        }

        CharacterFreeSlots free_slot = CharacterFreeSlots{current_account, i->character};
        auto fs_it = free_slots.find(&free_slot);
        if (fs_it != free_slots.end()) {
            if (i->bag_id == GW::Constants::Bag::Equipment_Pack) {
                (*fs_it)->occupied_equipment++;
            }
            else if (BagCanHoldAnything(i->bag_id)) {
                (*fs_it)->occupied_inventory++;
            }
        }
        if (auto it = inventory.find(i); it != inventory.end()) {
            // found
            auto o_item_id = (*it)->item_id;
            // make sure the lookup entry has not already been overwritten by another item being loaded during map load.
            if (inventory_lookup.contains(o_item_id) && inventory_lookup[o_item_id] == it->get()) {
                inventory_lookup.erase(o_item_id);
                // when stacks are split or merged, the source and target stack will be readded by gw without being deleted first.
                // if we already know the item in the source/target slot, the number of occupied spaces did not actually change.
                if (fs_it != free_slots.end()) {
                    if (i->bag_id == GW::Constants::Bag::Equipment_Pack) {
                        (*fs_it)->occupied_equipment--;
                    }
                    else if (BagCanHoldAnything(i->bag_id)) {
                        (*fs_it)->occupied_inventory--;
                    }
                }
            }
            inventory.erase(it);
        }
        auto i_raw = i.get();
        inventory_lookup[item->item_id] = i_raw;
        inventory.insert(std::move(i));
        DescriptionDecode(i_raw, item);
        inventory_dirty.insert(GetIniID(i_raw->account, i_raw->character));
        save_dirty_inventories_timer = TIMER_INIT();
        needs_sorting = true;

        if (item_to_move && GW::Constants::HeroID::NoHero != i_raw->hero_id && ItemEqual{}(item_to_move, i_raw)) {
            // If we had to change characters and item_to_move is on a player character,
            // then we get here during loading before MoveItem can work.
            // In that case StepReroll will take care of moving the item.
            MoveItem();
        }
    }
    bool RemoveItem(uint32_t item_id);
    void ClearMissingItem(const UUID* account, const std::string& character, const GW::Constants::HeroID hero_id, const GW::Constants::Bag bag_id, const uint32_t slot)
    {
        InventoryItem i;
        i.account = *account;
        i.bag_id = bag_id;
        i.character = character;
        i.slot = slot;
        i.hero_id = hero_id;
        if (auto it = inventory.find(&i); it != inventory.end()) {
            // found
            inventory_dirty.insert(GetIniID((*it)->account, (*it)->character));
            save_dirty_inventories_timer = TIMER_INIT();
            RemoveItem((*it)->item_id);
            // Most likely the missing item was still in our inifile but removed ingame.
            // In this case we won't know an item_id, but still want to remove it from inventory
            inventory.erase(it);
        }
    }

    bool RemoveItem(uint32_t item_id)
    {
        if (!inventory_lookup.contains(item_id)) return false;


        auto i = inventory_lookup[item_id];
        CharacterFreeSlots free_slot = CharacterFreeSlots{current_account, i->character};
        if (auto it = free_slots.find(&free_slot); it != free_slots.end()) {
            if (i->bag_id == GW::Constants::Bag::Equipment_Pack) {
                (*it)->occupied_equipment--;
            }
            else if (BagCanHoldAnything(i->bag_id)) {
                (*it)->occupied_inventory--;
            }
        }
        if (auto it = inventory.find(i); it != inventory.end()) {
            inventory.erase(it);
        }
        inventory_lookup.erase(item_id);
        needs_sorting = true;
        inventory_dirty.insert(GetIniID(i->account, i->character));
        save_dirty_inventories_timer = TIMER_INIT();
        return true;
    }

}



void AccountInventoryWindow::Initialize()
{
    ToolboxWindow::Initialize();

    current_account = GetAccountGuid();
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
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, (GW::UI::UIMessage)message_id,
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
    inventory.clear();
    inventory_lookup.clear();
    inventory_sorted.clear();
    while (!hero_bag_generation_order.empty()) hero_bag_generation_order.pop();
    bag_ptr_to_hero_id.clear();
    ini_by_character.clear();
    ini_by_path.clear();
    inventory_dirty.clear();
    free_slots.clear();
    free_slots_sorted.clear();
    reroll_hero_queue.clear();
    reroll_char_queue.clear();
    last_character = "";
    last_available_chars.clear();
    reroll_stage = RerollStage::None;
    map_loaded_delayed_trigger = false;
    show_delete_note = false;
    ToolboxWindow::Terminate();
}

void AccountInventoryWindow::Update(float)
{
    if (map_loaded_delayed_trigger && TIMER_DIFF(map_loaded_delayed_timer) > MAP_LOADED_DELAYED_TIMEOUT) {
        OnMapLoadedDelayed();
    }
    // wait until after OnMapLoadedDelayed to continue rerolling
    if (!map_loaded_delayed_trigger && reroll_stage == RerollStage::DoSaveHeroes && TIMER_DIFF(save_hero_timer) > SAVE_HERO_TIMEOUT) {
        SaveHeroes();
        reroll_stage = RerollStage::DoneCharacterLoad;
        StepReroll();
    }
    if (reroll_stage == RerollStage::WaitForHeroLoad && TIMER_DIFF(add_hero_timer) > ADD_HERO_TIMEOUT) {
        // failed to load hero in time, continue rerolling
        // this happens if mercenary heroes are available but not set up or possibly with koss when he's gone
        reroll_stage = RerollStage::DoneHeroLoad;
        StepReroll();
    }
    if (reroll_stage == RerollStage::WaitForHeroWithItem && TIMER_DIFF(add_hero_timer) > ADD_HERO_TIMEOUT) {
        // failed to load hero in time, forget item_to_move
        if (item_to_move) {
            delete item_to_move;
            item_to_move = nullptr;
        }
        reroll_stage = RerollStage::None;
    }
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
    current_account = GetAccountGuid();
    current_character = GetCurrentPlayerNameS();
    inventory_lookup.clear(); // discard now outdated id caches
    while (!hero_bag_generation_order.empty()) hero_bag_generation_order.pop();
    bag_ptr_to_hero_id.clear();
    std::string characters[] = {"(Chest)", current_character};
    for (auto & character: characters) {
        if (character.empty()) continue;
        CharacterFreeSlots free_slot = CharacterFreeSlots{current_account, character};
        free_slots.insert(std::make_unique<CharacterFreeSlots>(free_slot));
        auto it = free_slots.find(&free_slot);
        CharacterFreeSlots* free_slot_p = it->get();
        if (free_slot_p->account_representing_character.empty()) {
            auto available_characters = GW::AccountMgr::GetAvailableChars();
            if (available_characters->size() > 0) {
                // alphabetically first character name, to be shown in tooltip to distinguish chests from multiple accounts without showing email addresses
                const wchar_t *min = nullptr;
                for (const auto& available_char : *available_characters) {
                    if (!min || wcscmp(available_char.player_name, min) < 0) min = available_char.player_name;
                }
                free_slot_p->account_representing_character = TextUtils::WStringToString(min);
            }
        }
        if (character == current_character) {
            free_slot_p->occupied_equipment = 0;
        }
        free_slot_p->occupied_inventory = 0;
    }
    if (!(reroll_stage == RerollStage::None || reroll_stage == RerollStage::WaitForCharacterLoad || reroll_stage >= RerollStage::RerollToItem)) {
        // map load at an inappropriate time during GatherAllInventories, e.g. due to manual intervention or network issues
        // abort rerolling
        reroll_stage = RerollStage::None;
    }
}

void AccountInventoryWindow::PostMapLoad()
{
    bool character_changed = false;
    map_loaded_delayed_trigger = true;
    map_loaded_delayed_timer = TIMER_INIT();
    current_account = GetAccountGuid();
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
                    auto i = inventory_lookup[item->item_id];
                    if (i->equipped != item->equipped) {
                        i->equipped = item->equipped;
                        inventory_dirty.insert(GetIniID(i->account, i->character));
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
        std::string characters[] = {"(Chest)", current_character};
        for (auto & character: characters) {
            CharacterFreeSlots free_slot = CharacterFreeSlots{current_account, character};
            if (auto it = free_slots.find(&free_slot); it != free_slots.end()) {
                if (character == current_character) {
                    (*it)->max_equipment = max_equipment;
                    (*it)->max_inventory = max_inventory;
                } else {
                    // Since we do not know whether the anniversary storage pane is actually available,
                    // assume it is not, unless there has been at least one item in it at some point.
                    if ((*it)->anniversary_pane_active || last_chest_pane_contains_any_item) {
                        (*it)->anniversary_pane_active = true;
                    } else {
                        max_chest -= 25;
                    }
                    (*it)->max_inventory = max_chest;
                }
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
            for (auto it = inventory.begin(); it != inventory.end();) {
                if (!IsChestBag((*it)->bag_id) && (*it)->account == current_account) {
                    if (auto avail = availableChars.find((*it)->character); avail == availableChars.end()) {
                        // not found
                        inventory_dirty.insert(GetIniID((*it)->account, (*it)->character));
                        CharacterFreeSlots free_slot = CharacterFreeSlots{current_account, (*it)->character};
                        if (auto fs_it = free_slots.find(&free_slot); fs_it != free_slots.end()) {
                            free_slots.erase(fs_it);
                        }
                        inventory_lookup.erase((*it)->item_id);
                        it = inventory.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }
        last_available_chars = availableChars;
    }

    needs_sorting = true;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        SaveToFiles(false); // save inventory in outposts only to avoid impacting gameplay
    }
    switch (reroll_stage) {
        case RerollStage::None:
            break;
        case RerollStage::WaitForCharacterLoad:
            save_hero_timer = TIMER_INIT();
            reroll_stage = RerollStage::DoSaveHeroes;
            break;
        // RerollStage::RerollToItem is handled in OnMapLoadedDelayed as it might call MoveItem,
        // which does not work immediately after loading into a map
    }
}

void AccountInventoryWindow::OnMapLoadedDelayed()
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        // not done loading, retry later
        map_loaded_delayed_timer = TIMER_INIT();
        return;
    }
    map_loaded_delayed_trigger = false;
    if (reroll_stage == RerollStage::RerollToItem) {
        StepReroll();
        return;
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
    reroll_hero_queue.clear();
    reroll_char_queue.clear();
    cached_heroes.clear();
    auto available_characters = GW::AccountMgr::GetAvailableChars();
    for (const auto& available_char : *available_characters) {
        const auto char_select_info = GW::AccountMgr::GetAvailableCharacter(available_char.player_name);
        if (!char_select_info) {
            continue;
        }
        if (wcscmp(available_char.player_name, GW::AccountMgr::GetCurrentPlayerName()) == 0) {
            // process current char last, so we automatically end up back where we started.
            reroll_char_queue.insert(reroll_char_queue.begin(), available_char);
        } else {
            reroll_char_queue.push_back(available_char);
            const auto reroll_to_player_current_map = char_select_info->map_id();
            if (GWToolbox::ShouldDisableToolbox(reroll_to_player_current_map)) {
                const auto charname_str = TextUtils::WStringToString(char_select_info->player_name);
                const auto msg = std::format("{} is currently in {}.\n"
                    "This is an outpost that toolbox won't work in.\n"
                    "All characters must be in outposts where toolbox can work,\n"
                    "e.g. Great Temple of Balthazar.\n\n"
                    "Reroll to {} so you can move it to another outpost?",
                    charname_str, Resources::GetMapName(reroll_to_player_current_map)->string(), charname_str);
                ImGui::ConfirmDialog(msg.c_str(), [](bool result, void*){if (result) AccountInventoryWindow::Instance().OnRerollPromptReply();});
                return;
            }
        }
    }
    reroll_stage = RerollStage::NextCharacter;
    StepReroll();
}

void AccountInventoryWindow::OnRerollPromptReply()
{
    auto next_char = reroll_char_queue.back();
    reroll_char_queue.clear();
    RerollWindow::Instance().Reroll(next_char.player_name, false, false, true, true);
}

void AccountInventoryWindow::OnPartyAddHero(GW::Constants::HeroID hero_id)
{
    HandleHeroBag(hero_id);
    if (reroll_stage == RerollStage::None) return;
    switch (reroll_stage) {
        case RerollStage::WaitForHeroLoad:
            reroll_stage = RerollStage::DoneHeroLoad;
            StepReroll();
            return;
        case RerollStage::DoRestoreHeroes:
            const GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
            if (party_info) {
                if (party_info->heroes.size() < cached_heroes.size()) return;
            }
            cached_heroes.clear();
            reroll_stage = RerollStage::NextCharacter;
            StepReroll();
            return;
    }
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
    float checkbox_max_width = 160.f * font_scale;
    if (memcmp(&style.Colors[ImGuiCol_Button], &cached_button_color, sizeof(ImVec4)) != 0) {
        // Only update colours when theme changes
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

    if (RerollStage::None < reroll_stage && reroll_stage < RerollStage::RerollToItem) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f * font_scale, 0.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Account Inventory loading in progress")) {
            ImGui::TextWrapped("Please do not interrupt inventory loading.");
            if (ImGui::Button("Abort!")) {
                reroll_stage = RerollStage::None;
            }
        }
        ImGui::End();
    }
    if (visible) {
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
                auto free_equipment = free_slot->max_equipment - free_slot->occupied_equipment;
                auto free_inventory = free_slot->max_inventory - free_slot->occupied_inventory;
                auto is_current_account = memeq(&free_slot->account, &current_account);
                bool is_chest = free_slot->character == "(Chest)";
                std::string suffix = "";
                int style_count = 0;
                if (!is_current_account) {
                    style_count = 1;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    if (is_chest && !free_slot->account_representing_character.empty()) {
                        suffix = " [" + free_slot->account_representing_character + "]";
                    }
                }
                ImGui::TableNextColumn();
                if (is_current_account) {
                    if (ImGui::Button(free_slot->character.c_str())) {
                        InventoryItem i;
                        i.account = free_slot->account;
                        i.character = free_slot->character;
                        i.bag_id = is_chest ? GW::Constants::Bag::Storage_1 : GW::Constants::Bag::None;
                        OnInventoryItemClicked(&i, false);
                    }
                }
                else {
                    ImGui::Text("%s%s", free_slot->character.c_str(), suffix.c_str());
                }
                ImGui::TableNextColumn();
                if (free_slot->max_inventory) RightAlignText(std::to_string(free_inventory) + "/");
                ImGui::TableNextColumn();
                if (free_slot->max_inventory) ImGui::Text("%d", free_slot->max_inventory);
                ImGui::TableNextColumn();
                if (free_slot->max_equipment) RightAlignText(std::to_string(free_equipment) + "/");
                ImGui::TableNextColumn();
                if (free_slot->max_equipment) ImGui::Text("%d", free_slot->max_equipment);
                ImGui::PopStyleColor(style_count);
            }
            ImGui::EndTable();
        }

        float items_table_height = std::max(ImGui::GetContentRegionAvail().y, ITEMS_TABLE_MIN_HEIGHT);
        float inner_width = ImGui::GetContentRegionAvail().x - item_spacing;
        ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
        if (detailed_view) {
            flags |= ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg;
        }
        else {
            items_table_height = 2 * ImGui::GetFrameHeight();
            flags |= ImGuiTableFlags_SizingFixedFit;
        }
        if (!ImGui::BeginTable("###itemstable", ItemColumnID_Max, flags, ImVec2(inner_width, items_table_height))) {
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
        ImGui::Text("Filter   %d/%d Items", filtered_item_count, inventory_sorted.size());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGuiTableSortSpecs* item_sort_specs = ImGui::TableGetSortSpecs();
        if (needs_sorting || (item_sort_specs && item_sort_specs->SpecsDirty)) {
            SortAndFilterInventory(item_sort_specs);
        }

        if (!detailed_view) {
            ImGui::EndTable();
            if (!ImGui::BeginTable(
                    "###itemgrid", std::max(1, (int)(inner_width / (3.3f * ImGui::GetTextLineHeight() + item_spacing))), ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY,
                    ImVec2(inner_width, std::max(ImGui::GetContentRegionAvail().y, ITEMS_TABLE_MIN_HEIGHT))
                )) {
                ImGui::End();
                return;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
        }

        int render_index = 0;
        for (auto& ims : inventory_sorted) {
            auto i_front = *(ims.i.begin());

            bool clicked = false;
            ImGui::PushID(++render_index);
            int style_count = 0;
            if (!memeq(&i_front->account, &current_account)) {
                style_count += 1;
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                if (IsChestBag(i_front->bag_id)) {
                    style_count += 3;
                    ImGui::PushStyleColor(ImGuiCol_Button, color_chest_item_foreign);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_chest_item_hovered_foreign);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_chest_item_active_foreign);
                }
                else if (IsOnHero(i_front->hero_id)) {
                    style_count += 3;
                    ImGui::PushStyleColor(ImGuiCol_Button, color_hero_item_foreign);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_hero_item_hovered_foreign);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_hero_item_active_foreign);
                }
                else {
                    style_count += 3;
                    ImGui::PushStyleColor(ImGuiCol_Button, color_item_foreign);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_item_hovered_foreign);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_item_active_foreign);
                }
            }
            else {
                if (IsChestBag(i_front->bag_id)) {
                    style_count += 3;
                    ImGui::PushStyleColor(ImGuiCol_Button, color_chest_item);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_chest_item_hovered);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_chest_item_active);
                }
                else if (IsOnHero(i_front->hero_id)) {
                    style_count += 3;
                    ImGui::PushStyleColor(ImGuiCol_Button, color_hero_item);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_hero_item_hovered);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_hero_item_active);
                }
            }
            if (detailed_view) {
                std::string suffix = (ims.i.size() > 1) ? " +" : "";
                ImGui::Text("%s%s", i_front->character.c_str(), suffix.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s%s", i_front->location.c_str(), suffix.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", i_front->model_id);
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
                const ImVec2 pos = ImGui::GetCursorPos();
                auto w = 3.3f * ImGui::GetTextLineHeight();
                if (i_front->texture && *(i_front->texture)) {
                    clicked = ImGui::IconButton("", *i_front->texture, ImVec2(w, w), ImGuiButtonFlags_None, ImVec2(w, w));
                }
                else {
                    clicked = ImGui::Button("???", ImVec2(w, w));
                }
                if (ims.quantity > 1) {
                    ImGui::SetCursorPos(ImVec2(pos.x + item_spacing, pos.y));
                    ImGui::TextColored(ImVec4(0.98f, 0.97f, 0.6f, 1.f), "%d", ims.quantity);
                    ImGui::SetCursorPos(pos);
                    ImGui::Dummy(ImVec2(w, w));
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip([ms = &ims]() {
                        OnItemTooltip(ms);
                    });
                }
                ImGui::TableNextColumn();
            }
            ImGui::PopStyleColor(style_count);
            ImGui::PopID();

            if (clicked) {
                OnInventoryItemClicked(i_front, ImGui::IsKeyDown(ImGuiMod_Ctrl));
            }
        }
        ImGui::EndTable();
        ImGui::End();
    }
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
        inventory.clear();
        inventory_lookup.clear();
        inventory_sorted.clear();
        free_slots.clear();
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
        inventory.clear();
        inventory_lookup.clear();
        inventory_sorted.clear();
        free_slots.clear();
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
    current_account = GetAccountGuid();
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
