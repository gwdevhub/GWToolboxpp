#pragma once

#include <GWCA/Utilities/Hook.h>
#include <Utils/ToolboxUtils.h>
#include <ToolboxWindow.h>
#include <mutex>
#include <memory>
#include <Timer.h>

class AccountInventoryWindow : public ToolboxWindow {
    enum ItemColumnID
    {
        ItemColumnID_Character,
        ItemColumnID_Location,
        ItemColumnID_ModelID,
        ItemColumnID_Description,
        ItemColumnID_Max,
    };

    enum SlotColumnID
    {
        SlotColumnID_Character,
        SlotColumnID_Inventory,
        SlotColumnID_InventorySize,
        SlotColumnID_Equipment,
        SlotColumnID_EquipmentSize,
        SlotColumnID_Max,
    };

    enum RerollStage {
        None,
        NextCharacter,
        WaitForCharacterLoad,
        DoSaveHeroes,
        DoneCharacterLoad,
        WaitForHeroLoad,
        DoneHeroLoad,
        DoRestoreHeroes,
        RerollToItem,
        WaitForHeroWithItem
    };

    struct InventoryItem {
        // identifying attributes
        std::wstring account{};
        std::wstring character{};
        uint32_t hero_id{};
        uint32_t bag_id{};
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
        std::wstring location{}; // (Player) <Storage Pane> or <Hero Name>

        void CopyKeyTo(InventoryItem *i) {
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
        std::wstring current_account{};
        // used for order in Draw
        bool operator()(const MergeStack &lms, const MergeStack &rms) const
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
            if (delta == 0) delta = l->bag_id - r->bag_id;
            if (delta == 0) delta = l->slot - r->slot;
            if (delta == 0 && l->account < r->account) delta = -1;
            return delta * sort_direction < 0;
        }
        // used for order inside MergeStack, i.e. for interaction and tooltip
        bool operator()(InventoryItem *l, InventoryItem *r) const
        {
            if (l->account != r->account) {
                // lowest item is the one that can be interacted with. Make sure it is one on this account if there is one
                if (l->account == current_account) return true;
                if (r->account == current_account) return false;
            }
            auto lms = MergeStack(l->account, L"");
            lms.quantity = l->quantity;
            lms.i.insert(l);
            auto rms = MergeStack(r->account, L"");
            rms.quantity = r->quantity;
            rms.i.insert(r);
            return this->operator()(lms, rms);
        }
    };

    struct ItemHash {
        using is_transparent = void;
        std::size_t operator()(const std::unique_ptr<InventoryItem> &i) const noexcept
        {
            return operator()(std::to_address(i));
        }
        std::size_t operator()(InventoryItem const * const * const i) const noexcept
        {
            return operator()(*i);
        }
        std::size_t operator()(InventoryItem const * const i) const noexcept
        {
            std::size_t h1 = std::hash<std::wstring>{}(i->account);
            std::size_t h2 = std::hash<std::wstring>{}(i->character);
            std::size_t h3 = std::hash<uint32_t>{}(i->hero_id);
            std::size_t h4 = std::hash<uint32_t>{}(i->bag_id);
            std::size_t h5 = std::hash<uint32_t>{}(i->slot);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };

    struct ItemEqual {
        using is_transparent = void;
        bool operator()(const std::unique_ptr<InventoryItem> &l, const std::unique_ptr<InventoryItem> &r) const
        {
            return operator()(std::to_address(l), std::to_address(r));
        }
        bool operator()(InventoryItem const * const l, const std::unique_ptr<InventoryItem> &r) const
        {
            return operator()(l, std::to_address(r));
        }
        bool operator()(InventoryItem const * const * const l, const std::unique_ptr<InventoryItem> &r) const
        {
            return operator()(*l, std::to_address(r));
        }
        bool operator()(InventoryItem const * const l, InventoryItem const * const r) const
        {
            return l->hero_id == r->hero_id && l->bag_id == r->bag_id && l->slot == r->slot && l->account == r->account && l->character == r->character;
        }
    };

    struct MergeStack {
        uint16_t quantity;
        std::wstring description;
        std::set<InventoryItem *, ItemCompare> i;

        MergeStack(std::wstring account, std::wstring _description): quantity{}, description(_description), i(ItemCompare{nullptr, account}) {}
    };

    struct CharacterFreeSlots {
        std::wstring account{};
        std::wstring character{};
        std::wstring account_representing_character{}; // character name to tell which account a chest inventory belongs to without showing an email address
        uint32_t max_inventory{};
        uint32_t max_equipment{};
        uint32_t occupied_inventory{};
        uint32_t occupied_equipment{};
        bool anniversary_pane_active;
    };

    struct SlotCompare {
        ImGuiTableSortSpecs* sort_specs{};
        bool operator()(CharacterFreeSlots const * const l, CharacterFreeSlots const * const r) const
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
            if (delta == 0) delta = l->account.compare(r->account);
            return delta * sort_direction < 0;
        }
    };

    struct SlotHash {
        using is_transparent = void;
        std::size_t operator()(const std::unique_ptr<CharacterFreeSlots> &i) const noexcept
        {
            return operator()(std::to_address(i));
        }
        std::size_t operator()(CharacterFreeSlots const * const * const i) const noexcept
        {
            return operator()(*i);
        }
        std::size_t operator()(CharacterFreeSlots const * const i) const noexcept
        {
            std::size_t h1 = std::hash<std::wstring>{}(i->account);
            std::size_t h2 = std::hash<std::wstring>{}(i->character);
            return h1 ^ (h2 << 1);
        }
    };

    struct SlotEqual {
        using is_transparent = void;
        bool operator()(const std::unique_ptr<CharacterFreeSlots> &l, const std::unique_ptr<CharacterFreeSlots> &r) const
        {
            return operator()(std::to_address(l), std::to_address(r));
        }
        bool operator()(CharacterFreeSlots const * const l, const std::unique_ptr<CharacterFreeSlots> &r) const
        {
            return operator()(l, std::to_address(r));
        }
        bool operator()(CharacterFreeSlots const * const * const l, const std::unique_ptr<CharacterFreeSlots> &r) const
        {
            return operator()(*l, std::to_address(r));
        }
        bool operator()(CharacterFreeSlots const * const l, CharacterFreeSlots const * const r) const
        {
            return l->account == r->account && l->character == r->character;
        }
    };

    class InventoryIni : public ToolboxIni {
    public:
        FILETIME last_change_time{};
        std::wstring account{};
        std::wstring ini_ID{}; // character name for character/hero inventories, email for xunlai chests
        InventoryIni(std::filesystem::path _location_on_disk) {
            location_on_disk = _location_on_disk;
        }
    };

    AccountInventoryWindow()
    {
        show_menubutton = can_show_in_main_window;
    }

    void OnInventoryItemClicked(InventoryItem *i, bool move);
    static bool CheckIniDirty(InventoryIni *ini);
    InventoryIni* GetIni(std::wstring ini_ID, std::wstring account);
    std::string ItemToSectionName(InventoryItem *i) const;
    void LoadFromFiles(bool only_foreign);
    void SaveToFiles(bool include_foreign);
    void SortInventory(ImGuiTableSortSpecs* sort_specs);
    void SortSlots(ImGuiTableSortSpecs* sort_specs);
    void DescriptionDecode(InventoryItem *i, GW::Item *item);
    void ClearMissingItem(std::wstring account, std::wstring character, uint32_t hero_id, uint32_t bag_id, uint32_t slot);
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
    std::unordered_map<uint32_t, InventoryItem *> inventory_lookup{};
    // sorted/filtered view for display
    std::vector<MergeStack> inventory_sorted{};
    // ini files, 1 per character/chest
    std::unordered_map<std::filesystem::path, std::unique_ptr<InventoryIni>> ini_by_path{};
    std::unordered_map<std::wstring, InventoryIni*> ini_by_character{};
    // change tracker to reduce writes
    std::unordered_set<std::wstring> inventory_dirty{};
    // tracking of free inventory slot numbers
    std::unordered_set<std::unique_ptr<CharacterFreeSlots>, SlotHash, SlotEqual> free_slots{};
    // sorted/filtered view for display
    std::set<CharacterFreeSlots *, SlotCompare> free_slots_sorted{};
    // tracking for hero_id <-> Equipped_Items bag
    // we rely on hero items being created in the order of heroes in the party
    std::queue<GW::Bag *> hero_bag_generation_order{};
    // there must be a data structure somewhere in the game that already has this mapping
    // but i do not know where it is
    std::unordered_map<GW::Bag *, uint32_t> bag_ptr_to_hero_id{};

    // state between callbacks
    bool initializing = false;
    bool needs_sorting = true;
    bool show_delete_note = false;
    size_t filtered_item_count = 0;
    std::wstring last_character{};
    std::set<std::wstring> last_available_chars{};
    RerollStage reroll_stage = RerollStage::None;
    std::vector<GW::AvailableCharacterInfo> reroll_char_queue{};
    std::vector<uint32_t> reroll_hero_queue{};
    std::vector<uint32_t> cached_heroes{};
    clock_t add_hero_timer{};
    clock_t save_hero_timer{};
    clock_t map_loaded_delayed_timer{};
    clock_t save_dirty_inventories_timer{};
    bool map_loaded_delayed_trigger = false;
    InventoryItem *item_to_move = nullptr;

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
public:
    static AccountInventoryWindow& Instance()
    {
        static AccountInventoryWindow instance;
        return instance;
    }

    [[nodiscard]] const char *Name() const override { return "Account Inventory"; }
    [[nodiscard]] const char *Icon() const override { return ICON_FA_USERS; }

    // callbacks

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;

    void Draw(IDirect3DDevice9 * pDevice) override;
    void DrawSettingsInternal() override;

    void LoadSettings(ToolboxIni * ini) override;
    void SaveSettings(ToolboxIni * ini) override;

    void HandleHeroBag(uint32_t hero_id);
    void GatherAllInventories();
    void OnItemTooltip(const MergeStack *ms, std::wstring description);
    // handle inventory generation during map load
    void PreMapLoad();
    void PostMapLoad();
    // moving items only works after a delay beyond PostMapLoad
    void OnMapLoadedDelayed();
    // state machine for rerolling to items, callbacks.
    // further parts are handled by *MapLoad*
    void OnRerollPromptReply();
    void OnPartyAddHero(uint32_t hero_id);
    // maintain hero_id <-> Equipped_Items bag tracking
    GW::Bag * OnPartyRemoveHero(uint32_t hero_id);

    void AddItem(uint32_t item_id);
    bool RemoveItem(uint32_t item_id);
};
