#include "stdafx.h"

#include <GWCA/Packets/Opcodes.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Context/ItemContext.h>
#include <GWCA/Context/TradeContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/MerchantMgr.h>
#include <GWCA/Managers/TradeMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Timer.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <Modules/InventoryManager.h>
#include <Modules/GameSettings.h>

#include <Windows/MaterialsWindow.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/ArmoryWindow.h>
#include <Windows/GWMarketWindow.h>

#include <GWCA/GameEntities/Frame.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>

namespace {
    InventoryManager& Instance()
    {
        return InventoryManager::Instance();
    }

    ImVec4 ItemBlue = ImColor(153, 238, 255).Value;
    ImVec4 ItemPurple = ImColor(187, 137, 237).Value;
    ImVec4 ItemGold = ImColor(255, 204, 86).Value;

    bool trade_whole_stacks = false;
    bool move_to_trade_on_double_click = true;
    bool move_to_trade_on_alt_click = false;
    bool salvage_all_on_ctrl_click = false;
    bool identify_all_on_ctrl_click = false;
    bool auto_reuse_salvage_kit = false;
    bool auto_reuse_id_kit = false;

    const char* bag_names[5] = {
        "None",
        "Backpack",
        "Belt Pouch",
        "Bag 1",
        "Bag 2"
    };

        bool show_item_context_menu = false;
    bool is_identifying = false;
    bool is_identifying_all = false;
    bool is_salvaging = false;
    bool is_salvaging_all = false;
    bool has_prompted_salvage = false;
    bool show_salvage_all_popup = true;
    bool salvage_listeners_attached = false;
    bool only_use_superior_salvage_kits = false;

    bool hide_unsellable_items = false;
    bool hide_weapon_sets_and_customized_items = false;
    bool hide_golds_from_merchant = false;


    std::map<uint32_t, std::string> hide_from_merchant_items{}; // This should be the same in functionality to block_from_being_salvaged, but players are using it now :(
    std::map<std::wstring, std::string> block_from_being_salvaged{};

    bool salvage_rare_mats = false;
    bool salvage_nicholas_items = true;
    bool show_transact_quantity_popup = false;
    bool transaction_listeners_attached = false;

    bool wiki_link_on_context_menu = false;
    bool right_click_context_menu_in_explorable = true;
    bool right_click_context_menu_in_outpost = true;

    std::map<GW::Constants::Bag, bool> bags_to_salvage_from{};

    size_t identified_count = 0;
    size_t salvaged_count = 0;

    GW::HookEntry on_map_change_entry;
    GW::HookEntry salvage_hook_entry;
    GW::HookEntry transaction_hook_entry;
    GW::HookEntry ItemClick_Entry;

    uint32_t stack_prompt_item_id = 0;
    int pending_transaction_amount = 0;
    bool pending_cancel_transaction = false;
    bool is_transacting = false;
    bool has_prompted_transaction = false;

    clock_t pending_salvage_at = 0;
    clock_t pending_identify_at = 0;

    struct PendingItem {
        uint32_t item_id = 0;
        uint32_t slot = 0;
        GW::Constants::Bag bag = (GW::Constants::Bag)0;
        uint32_t uses = 0;
        uint32_t quantity = 0;
        bool set(const InventoryManager::Item* item = nullptr);
        GuiUtils::EncString* name = nullptr;
        GuiUtils::EncString* desc = nullptr;
        GuiUtils::EncString* wiki_name = nullptr;
        GuiUtils::EncString* single_item_name = nullptr;

        class PluralEncString : public GuiUtils::EncString {
        protected:
            void sanitise() override;
        };

        PluralEncString* plural_item_name = nullptr;

        InventoryManager::Item* item() const;
        PendingItem()
        {
            single_item_name = new GuiUtils::EncString{};
            plural_item_name = new PluralEncString{};
            name = new GuiUtils::EncString{};
            desc = new GuiUtils::EncString{};
            wiki_name = new GuiUtils::EncString{};
        }
        ~PendingItem()
        {
            single_item_name->Release();
            plural_item_name->Release();
            name->Release();
            desc->Release();
            wiki_name->Release();
        }
    };
    struct PotentialItem : PendingItem {
        bool proceed = true;
    };

    PendingItem context_item;
    bool pending_cancel_salvage = false;
    InventoryManager::IdentifyAllType identify_all_type = InventoryManager::IdentifyAllType::None;
    InventoryManager::SalvageAllType salvage_all_type = InventoryManager::SalvageAllType::None;

        struct PendingTransaction {
        enum State : uint8_t { None, Prompt, Pending, Quoting, Quoted, Transacting } state = None;

        GW::Merchant::TransactionType type = (GW::Merchant::TransactionType)0;
        uint32_t price = 0;
        uint32_t item_id = 0;
        clock_t state_timestamp = 0;
        uint8_t retries = 0;

        void setState(const State _state)
        {
            state = _state;
            state_timestamp = clock();
        }
        InventoryManager::Item* item() const;
        bool in_progress() const { return state > Prompt; }
        bool selling();
    };



    PendingItem pending_identify_item;
    PendingItem pending_identify_kit;
    PendingItem pending_salvage_item;
    PendingItem pending_salvage_kit;
    PendingTransaction pending_transaction;

    bool wcseq(const wchar_t* a, const wchar_t* b) {
        return a && b && wcscmp(a, b) == 0;
    }

    bool GetIsProfessionUnlocked(GW::Constants::Profession prof)
    {
        const auto world = GW::GetWorldContext();
        const auto player = GW::PlayerMgr::GetPlayerByID();
        if (!(world && player)) {
            return false;
        }
        GW::Array<GW::ProfessionState>& profession_unlocks_array = world->party_profession_states;
        const GW::ProfessionState* found = nullptr;
        for (size_t i = 0; !found && profession_unlocks_array.valid() && i < profession_unlocks_array.size(); i++) {
            if (profession_unlocks_array[i].agent_id == player->agent_id) {
                found = &profession_unlocks_array[i];
            }
        }
        return found && (found->unlocked_professions >> std::to_underlying(prof) & 1) == 1;
    }

    bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }

    struct PendingMove {
        uint32_t item_id;
        uint16_t quantity;
        clock_t started_at;
    };

    bool IsUnid(const GW::Item* item)
    {
        const auto inv_item = static_cast<const InventoryManager::Item*>(item);
        return inv_item && inv_item->CanBeIdentified() && inv_item->GetRarity() == GW::Constants::Rarity::Gold;
    }

    std::unordered_map<uint32_t, PendingMove> pending_moves; // { bag_idx | slot, { quantity_to_move,move_started_at} }
    uint16_t get_pending_move(const GW::Constants::Bag bag_id, const uint32_t slot)
    {
        const uint32_t bag_slot = ((size_t)bag_id) << 16 | slot;
        const auto found = pending_moves.find(bag_slot);
        if (found == pending_moves.end()) {
            return 0;
        }
        if (TIMER_DIFF(found->second.started_at) > 3000) // 3 second timeout incase of hanging moves
        {
            return 0;
        }
        return found->second.quantity;
    }
    GW::Item* get_pending_move_item(const GW::Constants::Bag bag_id, const uint32_t slot) {
        const uint32_t bag_slot = ((size_t)bag_id) << 16 | slot;
        const auto found = pending_moves.find(bag_slot);
        if (found == pending_moves.end()) {
            return nullptr;
        }
        if (TIMER_DIFF(found->second.started_at) > 3000) // 3 second timeout incase of hanging moves
        {
            return nullptr;
        }
        return GW::Items::GetItemById(found->second.item_id);
    }

    void set_pending_move(const GW::Constants::Bag bag_id, const uint32_t slot, const uint16_t quantity_to_move, const uint32_t item_id)
    {
        const uint32_t bag_slot = ((size_t)bag_id) << 16 | slot;
        pending_moves[bag_slot] = {
            item_id,
            static_cast<uint16_t>(get_pending_move(bag_id, slot) + quantity_to_move),
            TIMER_INIT()
        };
    }

    void clear_pending_move(GW::Constants::Bag bag_idx, const uint32_t slot)
    {
        const uint32_t bag_slot = std::to_underlying(bag_idx) << 16 | slot;
        pending_moves.erase(bag_slot);
    }

    void clear_pending_move(const uint32_t item_id)
    {
        const auto item = GW::Items::GetItemById(item_id);
        if (item && item->bag) {
            return clear_pending_move(item->bag->bag_id(), item->slot);
        }
    }

    uint16_t MaxSlotSize(const GW::Bag* bag)
    {
        return bag && bag->bag_type == GW::Constants::BagType::MaterialStorage ? (uint16_t)GW::Items::GetMaterialStorageStackSize() : 250;
    }

    uint16_t move_materials_to_storage(const GW::Item* item)
    {
        const auto material_slot = GW::Items::GetMaterialSlot(item);
        if (material_slot == GW::Constants::MaterialSlot::Count) {
            return 0;
        }
        const uint32_t slot = std::to_underlying(material_slot);
        const auto materials_pane = GW::Items::GetBag(GW::Constants::Bag::Material_Storage);
        uint16_t space_available = MaxSlotSize(materials_pane);
        const GW::Item* existing_stack_in_storage = GW::Items::GetItemBySlot(materials_pane, slot + 1);
        if (existing_stack_in_storage) {
            ASSERT(existing_stack_in_storage->quantity <= space_available); // Could trigger if we failed to get the correct max slot size for materials
            space_available -= existing_stack_in_storage->quantity;
        }
        const uint16_t pending_move = get_pending_move(GW::Constants::Bag::Material_Storage, slot);
        space_available -= pending_move;
        const uint16_t will_move = std::min<uint16_t>(item->quantity, space_available);
        if (will_move) {
            set_pending_move(GW::Constants::Bag::Material_Storage, slot, will_move, item->item_id);
            GW::Items::MoveItem(item, GW::Constants::Bag::Material_Storage, slot, will_move);
        }
        return will_move;
    }

    size_t find_same_item_in_bag(const GW::Bag* bag, const GW::Item* item, const size_t start_slot = 0) {
        if (!bag) return GW::Bag::npos;
        const auto& items = bag->items;
        for (size_t i = start_slot; i < items.size(); i++) {
            if (!item) {
                if (items[i])
                    continue;
                return i;
            }
            if (item != items[i] && Instance().IsSameItem(item, items[i]))
                return i;
        }
        return GW::Bag::npos;
    }

    // From bag_first to bag_last (included) i.e. [bag_first, bag_last]
    // Returns the amount moved
    uint16_t complete_existing_stack(const GW::Item* item, const GW::Constants::Bag bag_first, const GW::Constants::Bag bag_last, const uint16_t quantity = 1000u)
    {
        if (!item->GetIsStackable()) {
            return 0;
        }
        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        for (auto bag_id = bag_first; bag_id <= bag_last; bag_id++) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            if (!bag) {
                continue;
            }
            const uint16_t max_slot_size = MaxSlotSize(bag);
            size_t slot = find_same_item_in_bag(bag,item);
            while (slot != GW::Bag::npos) {
                const GW::Item* b_item = bag->items[slot];
                // b_item can be null in the case of birthday present for instance.

                if (b_item != nullptr && b_item->quantity < max_slot_size) {
                    uint16_t availaible = max_slot_size - get_pending_move(bag_id, slot) - b_item->quantity;
                    const uint16_t will_move = std::min<uint16_t>(availaible, remaining);
                    if (will_move > 0) {
                        GW::Items::MoveItem(item, b_item, will_move);
                        set_pending_move(bag_id, slot, will_move, item->item_id);
                        remaining -= will_move;
                    }
                    if (remaining == 0) {
                        return to_move;
                    }
                }
                slot = find_same_item_in_bag(bag,item,slot + 1);
            }
        }
        return to_move - remaining;
    }

    uint16_t move_to_first_empty_slot(const GW::Item* item, const GW::Constants::Bag bag_first, const GW::Constants::Bag bag_last, uint16_t quantity = 1000u)
    {
        quantity = std::min<uint16_t>(item->quantity, quantity);
        const auto stackable = item->GetIsStackable();
        for (auto bag_id = bag_first; bag_id <= bag_last; bag_id++) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            if (!bag) {
                continue;
            }
            const uint16_t max_slot_size = MaxSlotSize(bag);
            size_t slot = (size_t)-1;
            
            do {
                slot = find_same_item_in_bag(bag, nullptr,slot + 1);
                if (slot == GW::Bag::npos)
                    break; // No empty slot found
                if (bag->items[slot] != nullptr)
                    break; // Slot isn't empty(?)
                const auto pending_amount_for_slot = get_pending_move(bag_id, slot);
                if (pending_amount_for_slot) {
                    // Slot is currently empty, but we're pending a move for something into it
                    if (!stackable)
                        continue; // This item isn't stackable
                    if (!Instance().IsSameItem(item, get_pending_move_item(bag_id, slot)))
                        continue; // The pending item isn't the same as this item
                }
                uint16_t available = max_slot_size - pending_amount_for_slot;
                const uint16_t will_move = std::min<uint16_t>(quantity, available);
                if (will_move > 0) {
                    set_pending_move(bag_id, slot, will_move, item->item_id);
                    GW::Items::MoveItem(item, bag, slot, will_move);
                    return will_move;
                }

            } while (true);
        }
        return 0;
    }
    // page is 0 based from storage 1. Note that if material storage is on focus, 0 is returned.
    uint16_t move_item_to_storage_page(const GW::Item* item, const GW::Constants::StoragePane page, const uint16_t quantity = 1000u)
    {
        ASSERT(item && item->quantity);
        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        if (page == GW::Constants::StoragePane::Material_Storage) {
            if (!item->GetIsMaterial()) {
                return 0;
            }
            remaining -= move_materials_to_storage(item);
        }

        if (GW::Constants::StoragePane::Storage_14 < page) {
            return to_move - remaining;
        }

        const GW::Constants::Bag bag_id = (GW::Constants::Bag)((size_t)GW::Constants::Bag::Storage_1 + (size_t)page);
        // if the item is stackable we try to complete stack that already exist in the current storage page
        if (remaining) {
            remaining -= complete_existing_stack(item, bag_id, bag_id, remaining);
        }
        if (remaining) {
            remaining -= move_to_first_empty_slot(item, bag_id, bag_id, remaining);
        }
        return to_move - remaining;
    }

    uint16_t move_item_to_storage(const GW::Item* item, const uint16_t quantity = 1000u)
    {
        ASSERT(item && item->quantity);
        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        const bool is_storage_open = GW::Items::GetIsStorageOpen();
        if (remaining && is_storage_open && item->GetIsMaterial() && GameSettings::GetSettingBool("move_materials_to_current_storage_pane")) {
            remaining -= move_item_to_storage_page(item, GW::Items::GetStoragePage(), remaining);
        }
        if (remaining && item->GetIsMaterial()) {
            remaining -= move_materials_to_storage(item);
        }

        if (remaining && is_storage_open && GameSettings::GetSettingBool("move_item_to_current_storage_pane")) {
            remaining -= move_item_to_storage_page(item, GW::Items::GetStoragePage(), remaining);
        }

        if (remaining) {
            remaining -= complete_existing_stack(item, GW::Constants::Bag::Storage_1, GW::Constants::Bag::Storage_14, remaining);
        }
        while (remaining) {
            const uint16_t moved = move_to_first_empty_slot(item, GW::Constants::Bag::Storage_1, GW::Constants::Bag::Storage_14, remaining);
            if (!moved) {
                break;
            }
            remaining -= moved;
        }

        return to_move - remaining;
    }

    uint16_t move_item_to_inventory(const GW::Item* item, const uint16_t quantity = 1000u)
    {
        ASSERT(item && item->quantity);

        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        // If item is stackable, try to complete similar stack
        remaining -= complete_existing_stack(item, GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, remaining);
        while (remaining) {
            const uint16_t moved = move_to_first_empty_slot(item, GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, remaining);
            if (!moved) {
                break;
            }
            remaining -= moved;
        }

        return to_move - remaining;
    }

    std::vector<InventoryManager::Item*> filter_items(GW::Constants::Bag from, GW::Constants::Bag to, const std::function<bool(InventoryManager::Item*)>& cmp, const uint32_t limit = 0)
    {
        std::vector<InventoryManager::Item*> out;
        for (auto bag_id = from; bag_id <= to; bag_id++) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            if (!bag) {
                continue;
            }
            for (size_t slot = 0; slot < bag->items.size(); slot++) {
                const auto item = static_cast<InventoryManager::Item*>(bag->items[slot]);
                if (!cmp(item)) {
                    continue;
                }
                out.push_back(item);
                if (out.size() == limit) {
                    return out;
                }
            }
        }
        return out;
    }
    std::vector<InventoryManager::Item*> filter_storage(const std::function<bool(InventoryManager::Item*)>& cmp, const uint32_t limit = 0)
    {
        return filter_items(GW::Constants::Bag::Material_Storage, GW::Constants::Bag::Storage_14, cmp, limit);
    }
    std::vector<InventoryManager::Item*> filter_inventory(const std::function<bool(InventoryManager::Item*)>& cmp, const uint32_t limit = 0)
    {
        return filter_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, cmp, limit);
    }
    uint16_t count_items(const GW::Constants::Bag from, const GW::Constants::Bag to, std::function<bool(InventoryManager::Item*)> cmp)
    {
        const auto items = filter_items(from, to, std::move(cmp));
        uint16_t out = 0;
        for (const auto item : items) {
            out += item->quantity;
        }
        return out;
    }

    const GW::Array<GW::TradeItem>* GetPlayerTradeItems()
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
            return nullptr;
        }
        const GW::TradeContext* c = GW::GetGameContext()->trade;
        if (!c || !c->GetIsTradeInitiated()) {
            return nullptr;
        }
        return &c->player.items;
    }

    void store_items(const std::vector<InventoryManager::Item*>& items) {
        for (const auto item : items) {
            move_item_to_storage(item);
        }
        pending_moves.clear();
    }
    void withdraw_items(const std::vector<InventoryManager::Item*>& items) {
        for (const auto item : items) {
            move_item_to_inventory(item);
        }
        pending_moves.clear();
    }

    void store_all_materials()
    {
        store_items(filter_inventory([](const GW::Item* item) {
            return item && item->GetIsMaterial();
        }));
    }
    void store_all_unids() {
        store_items(filter_inventory(IsUnid));
    }
    void withdraw_all_unids() {
        withdraw_items(filter_storage(IsUnid));
    }

    void store_all_tomes()
    {
        store_items(filter_inventory([](const InventoryManager::Item* item) {
            return item && item->IsTome();
        }));
    }

    void move_all_item(InventoryManager::Item* like_item)
    {
        ASSERT(like_item && like_item->bag);
        const auto is_same_item = [like_item](const InventoryManager::Item* cmp) {
            return cmp && InventoryManager::IsSameItem(like_item, cmp);
        };
        if (like_item->bag->IsInventoryBag()) {
            store_items(filter_inventory(is_same_item));
        }
        else {
            withdraw_items(filter_storage(is_same_item));
        }
    }

    void store_all_upgrades()
    {
        store_items(filter_inventory([](const InventoryManager::Item* item) {
            return item && item->type == GW::Constants::ItemType::Rune_Mod;
        }));
    }
    void store_all_dyes()
    {
        store_items(filter_inventory([](const InventoryManager::Item* item) {
            return item && item->type == GW::Constants::ItemType::Dye;
            }));
    }
    void withdraw_all_dyes()
    {
        withdraw_items(filter_storage([](const InventoryManager::Item* item) {
            return item && item->type == GW::Constants::ItemType::Dye;
        }));
    }
    void withdraw_all_tomes()
    {
        withdraw_items(filter_storage([](const InventoryManager::Item* item) {
            return item && item->IsTome();
            }));
    }
    void store_all_nicholas_items() {
        store_items(filter_inventory([](const InventoryManager::Item* item) {
            return item && DailyQuests::GetNicholasItemInfo(item->name_enc);
        }));
    }

    void consume_all(InventoryManager::Item* like_item) {
        for (uint16_t i = 0; like_item && i < like_item->GetUses(); i++) {
            GW::Items::UseItem(like_item);
        }
    }

    bool get_next_bag_slot(const InventoryManager::Item* item, GW::Constants::Bag* bag_id_out, size_t* slot_out)
    {
        if (!(item && item->bag)) return false;
        const auto bag = item->bag;
        const auto slot_item = bag->items[item->slot];
        ASSERT(slot_item == item);
        size_t slot = item->slot + 1;
        auto bag_id = bag->bag_id();
        if (slot >= bag->items.size()) {
            bag_id = GW::Constants::Bag::Max;
            slot = 0;
            for (auto it = static_cast<GW::Constants::Bag>(std::to_underlying(bag->bag_id()) + 1); it < GW::Constants::Bag::Max; ++it) {
                if (GW::Items::GetBag(it)) {
                    bag_id = it;
                    break;
                }
            }
        }
        if (bag_id != GW::Constants::Bag::Max) {
            *bag_id_out = bag_id;
            *slot_out = slot;
            return true;
        }
        return true;
    }

    // Move a whole stack into/out of storage
    uint16_t move_item(const InventoryManager::Item* item, const uint16_t quantity = 1000u)
    {
        // Expected behaviors
        //  When clicking on item in inventory
        //   case storage close (or move_item_to_current_storage_pane = false):
        //    - If the item is a material, it look if it can move it to the material page.
        //    - If the item is stackable, search in all the storage if there is already similar items and completes the stack
        //    - If not everything was moved, move the remaining in the first empty slot of the storage.
        //   case storage open:
        //    - If the item is a material, it look if it can move it to the material page.
        //    - If the item is stackable, search for incomplete stacks in the current storage page and completes them
        //    - If not everything was moved, move the remaining in the first empty slot of the current page.

        // @Cleanup: Bad
        if (item->model_file_id == 0x0002f301) {
            Log::Error("Ctrl+click doesn't work with birthday presents yet");
            return 0;
        }
        const bool is_inventory_item = item->IsInventoryItem();
        uint16_t remaining = std::min<uint16_t>(item->quantity, quantity);
        if (is_inventory_item) {
            remaining -= move_item_to_storage(item, remaining);
        }
        else {
            remaining -= move_item_to_inventory(item, remaining);
        }
        pending_moves.clear();
        return remaining;
    }

    GW::Merchant::TransactionType requesting_quote_type = (GW::Merchant::TransactionType)0;

    GW::UI::WindowPosition* inventory_bags_window_position = nullptr;

    using AddItemRowToWindow_pt = void(__fastcall*)(void* ecx, void* edx, uint32_t frame, uint32_t item_id);
    AddItemRowToWindow_pt AddItemRowToWindow_Func = nullptr;
    AddItemRowToWindow_pt RetAddItemRowToWindow = nullptr;

    uint32_t pending_item_move_for_trade = 0;

    std::vector<PotentialItem*> potential_salvage_all_items{}; // List of items that would be processed if user confirms Salvage All


    bool IsTradeWindowOpen()
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
            return false;
        }
        const GW::TradeContext* c = GW::GetGameContext()->trade;
        return c && c->GetIsTradeInitiated();
    }

    GW::HookEntry on_offer_item_hook;
    bool change_secondary_for_tome = true;

    void prompt_split_stack(const GW::Item* item)
    {
        GW::GameThread::Enqueue([item] {
            GW::UI::UIPacket::kMoveItem packet = {.item_id = item->item_id, .to_bag_index = item->bag->index, .to_slot = item->slot, .prompt = 1u};
            if (SendUIMessage(GW::UI::UIMessage::kMoveItem, &packet)) {
                stack_prompt_item_id = item->item_id;
            }
        });
    }

    enum PendingTomeUseStage {
        None,
        PromptUser,
        AwaitPromptReply,
        ChangeProfession,
        AwaitProfession,
        UseItem
    } tome_pending_stage;

    uint32_t pending_destroy_item_id = 0;
    clock_t pending_destroy_item_timeout = 0;

    void DrawPendingDestroyItem() {
        const auto popup_id = "##destroy_item_prompt";
        if (!pending_destroy_item_id)
            return;
        const auto item = GW::Items::GetItemById(pending_destroy_item_id);
        if (!(item && item->bag)) {
            pending_destroy_item_id = 0;
            ImGui::ClosePopup(popup_id);
            return;
        }
        if (!ImGui::IsPopupOpen(popup_id)) {
            ImGui::OpenPopup(popup_id);
        }
        if (ImGui::BeginPopupModal(popup_id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Sure you want to destroy this item?");
            if (ImGui::Button("Destroy", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                GW::Items::DestroyItem(pending_destroy_item_id);
                pending_destroy_item_id = 0;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                pending_destroy_item_id = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }
    }

    GW::Constants::Profession tome_pending_profession;
    time_t tome_pending_timeout = 0;
    uint32_t tome_pending_item_id = 0;

    // Run every frame; if we're pending aa change of secondary profession to use a tome, check and action.
    void DrawPendingTomeUsage()
    {
        if (tome_pending_stage == None) {
            return;
        }
        const GW::AgentLiving* player = GW::Agents::GetControlledCharacter();
        constexpr auto popup_label = "Change secondary profession?###change-secondary";
        if (tome_pending_timeout && TIMER_INIT() > tome_pending_timeout) {
            Log::Error("Timeout reached trying to change profession for tome use");
            goto cancel;
        }
        if (!player) {
            return;
        }
        if (tome_pending_stage == PromptUser) {
            if (player->secondary == static_cast<uint8_t>(tome_pending_profession) || player->primary == static_cast<uint8_t>(tome_pending_profession)) {
                tome_pending_stage = UseItem;
                return;
            }
            const auto& d = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos({d.x * .5f, d.y * .5f}, 0, ImVec2(0.5f, 0.5f));
            ImGui::OpenPopup(popup_label);
            tome_pending_stage = AwaitPromptReply;
        }
        if (ImGui::BeginPopupModal(popup_label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Changing your secondary profession will change your skills and attributes.\nDo you want to continue?");
            if (ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                tome_pending_stage = ChangeProfession;
                tome_pending_timeout = TIMER_INIT() + 3000;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }
        if (tome_pending_stage == AwaitPromptReply) {
            // Popup not drawn; cancelled.
            goto cancel;
        }

        switch (tome_pending_stage) {
            case ChangeProfession: {
                if (player->secondary == static_cast<uint8_t>(tome_pending_profession)) {
                    tome_pending_stage = UseItem;
                    return;
                }
                if (!GetIsProfessionUnlocked(tome_pending_profession)) {
                    tome_pending_stage = UseItem;
                    return;
                }
                GW::GameThread::Enqueue([] {
                    GW::PlayerMgr::ChangeSecondProfession(tome_pending_profession);
                    });
                tome_pending_stage = AwaitProfession;
                return;
            }
            case AwaitProfession: {
                if (player->secondary == static_cast<uint8_t>(tome_pending_profession)) {
                    tome_pending_stage = UseItem;
                }
                return;
            }
            case UseItem: {
                GW::Items::UseItem(GW::Items::GetItemById(tome_pending_item_id));
            }
        }
    cancel:
        tome_pending_stage = None;
        tome_pending_timeout = 0;
        tome_pending_item_id = 0;
        tome_pending_profession = GW::Constants::Profession::None;
    }

    void OnUseItem(GW::HookStatus* status, const uint32_t item_id)
    {
        if (tome_pending_stage != None) {
            return;
        }
        if (!change_secondary_for_tome) {
            return;
        }
        const auto item = GW::Items::GetItemById(item_id);
        if (!item) {
            return;
        }
        auto profession_needed = GW::Constants::Profession::None;
        switch (item->model_id) {
            case 21786:
            case 21796:
                profession_needed = GW::Constants::Profession::Assassin;
                break;
            case 21787:
            case 21797:
                profession_needed = GW::Constants::Profession::Mesmer;
                break;
            case 21788:
            case 21798:
                profession_needed = GW::Constants::Profession::Necromancer;
                break;
            case 21789:
            case 21799:
                profession_needed = GW::Constants::Profession::Elementalist;
                break;
            case 21790:
            case 21800:
                profession_needed = GW::Constants::Profession::Monk;
                break;
            case 21791:
            case 21801:
                profession_needed = GW::Constants::Profession::Warrior;
                break;
            case 21792:
            case 21802:
                profession_needed = GW::Constants::Profession::Ranger;
                break;
            case 21793:
            case 21803:
                profession_needed = GW::Constants::Profession::Dervish;
                break;
            case 21794:
            case 21804:
                profession_needed = GW::Constants::Profession::Ritualist;
                break;
            case 21795:
            case 21805:
                profession_needed = GW::Constants::Profession::Paragon;
                break;
        }
        if (profession_needed != GW::Constants::Profession::None) {
            tome_pending_profession = profession_needed;
            tome_pending_item_id = item_id;
            tome_pending_stage = PromptUser;
            status->blocked = true;
        }
    }

    uint32_t merchant_list_tab = 0;

    struct ButtonPress {
        uint32_t frame_id; // Child offset of the button
        clock_t added = 0;
        ButtonPress(uint32_t frame_id) : frame_id(frame_id) {
            added = TIMER_INIT();
        }
    };
    std::queue<ButtonPress> queued_button_presses;

    // Cycle through queued buttons, trigger as necessary
    void ProcessQueuedButtonPresses() {
        while (queued_button_presses.size()) {
            auto todo = queued_button_presses.front();
            if (TIMER_DIFF(todo.added) > 1000) {
                queued_button_presses.pop();
                continue;
            }
            if (GW::UI::ButtonClick(GW::UI::GetFrameById(todo.frame_id))) {
                queued_button_presses.pop();
            }
        }
    }


    bool CanBulkConsume(InventoryManager::Item* item) {
        if (!(item && item->IsUsable()))
            return false;
        auto mod = item->GetModifier(0x2788);
        switch (mod ? mod->arg2() : 0) {
        case 0xF1: // Double click to drink. Excessive alcohol consumption...
        case 0x1: // Double click to use. Excessive alcohol consumption...
        case 0x8: // Double click to use. Eating excessive sweets...
        case 0x24: //Double click to use in town
            return true;
        }

        mod = item->GetModifier(0x25e8);
        switch (mod ? mod->arg1() : 0) {
        case 0x7: // Nicholas keg quote
            return true;
        }
        switch (item->model_file_id) {
        case 0x26858: // Squash serum
            return true;
        }
        return false;
    }

    GW::UI::UIInteractionCallback UICallback_ChooseQuantityPopup_Func = nullptr, UICallback_ChooseQuantityPopup_Ret = nullptr;
    
    // When choose quantity popup is shown, automatically accept with max amount, unless shift is held
    void __cdecl OnChooseQuantityPopupUIMessage(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        UICallback_ChooseQuantityPopup_Ret(message, wParam, lParam);

        if(!(message->message_id == GW::UI::UIMessage::kInitFrame
            && trade_whole_stacks
            && !ImGui::IsKeyDown(ImGuiMod_Shift)))
            return GW::Hook::LeaveHook();
        const auto frame = GW::UI::GetFrameById(message->frame_id);
        if(!frame || frame->child_offset_id != 0x2) 
            return GW::Hook::LeaveHook(); // This isn't the prompt for trade stacks
        const auto max_btn = GW::UI::GetChildFrame(frame,4);
        const auto ok_btn = GW::UI::GetChildFrame(frame, 3);
        if (max_btn && ok_btn) {
            queued_button_presses.push(max_btn->frame_id);
            queued_button_presses.push(ok_btn->frame_id);
        }
        GW::Hook::LeaveHook();
    }

    void __fastcall OnAddItemToWindow(void* ecx, void* edx, const uint32_t frame, const uint32_t item_id)
    {
        GW::Hook::EnterHook();
        if (merchant_list_tab == 0xb) {
            const auto item = reinterpret_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));

            if (item && item->IsHiddenFromMerchants()) {
                GW::Hook::LeaveHook();
                return;
            }
        }
        RetAddItemRowToWindow(ecx, edx, frame, item_id);
        GW::Hook::LeaveHook();
    }

    void UpdateQuoteHelpText()
    {
        auto SetFrameText = [](GW::UI::Frame* frame) {
            const auto quote_help_text = (GW::TextLabelFrame*)frame;
            if (!quote_help_text) return;
            const auto current_text = quote_help_text->GetEncodedLabel();
            if (!(current_text && wcslen(current_text) < 10)) return;
            std::wstring new_text = std::format(L"{}\x2\x108\x107{}\x1", current_text, L"\n\nNeed to buy or sell in bulk? Hold Ctrl when clicking \"Request Quote\" to choose a quantity.");
            quote_help_text->SetLabel(new_text.c_str());
        };

        auto frame = GW::UI::GetFrameByLabel(L"BtnSell");
        if (frame) frame = GW::UI::GetChildFrame(frame->relation.GetParent(), 0x8);
        SetFrameText(frame);

        frame = GW::UI::GetFrameByLabel(L"BtnBuy");
        if (frame) frame = GW::UI::GetChildFrame(frame->relation.GetParent(), 0x15);
        SetFrameText(frame);
    }

    void ClearPotentialItems()
    {
        for (const PotentialItem* item : potential_salvage_all_items) {
            delete item;
        }
        potential_salvage_all_items.clear();
    }
    void AttachTransactionListeners()
    {
        if (transaction_listeners_attached) {
            return;
        }
        GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GAME_SMSG_TRANSACTION_REJECT, [](GW::HookStatus*, void*) {
            if (!pending_transaction.in_progress()) {
                return;
            }
            pending_cancel_transaction = true;
            Log::WarningW(L"Trader rejected transaction");
        });
        transaction_listeners_attached = true;
    }
    void DetachTransactionListeners()
    {
        if (!transaction_listeners_attached) {
            return;
        }
        GW::StoC::RemoveCallback(GW::Packet::StoC::TransactionDone::STATIC_HEADER, &salvage_hook_entry);
        GW::StoC::RemoveCallback(GAME_SMSG_TRANSACTION_REJECT, &salvage_hook_entry);
        GW::StoC::RemoveCallback(GW::Packet::StoC::QuotedItemPrice::STATIC_HEADER, &salvage_hook_entry);
        transaction_listeners_attached = false;
    }
    void ClearTransactionSession()
    {
        pending_transaction.setState(PendingTransaction::State::None);
    }
    void CancelTransaction()
    {
        DetachTransactionListeners();
        ClearTransactionSession();
        is_transacting = has_prompted_transaction = false;
        pending_transaction_amount = 0;
        pending_cancel_transaction = false;
        pending_transaction.retries = 0;
    }
    void CancelSalvage()
    {
        potential_salvage_all_items.clear();
        is_salvaging = has_prompted_salvage = is_salvaging_all = false;
        pending_salvage_item.item_id = 0;
        pending_salvage_kit.item_id = 0;
        salvage_all_type = InventoryManager::SalvageAllType::None;
        salvaged_count = 0;
        context_item.item_id = 0;
        pending_cancel_salvage = false;
        GW::Items::SalvageSessionCancel(); // NB: Don't care about failure
    }
    void CancelIdentify()
    {
        is_identifying = is_identifying_all = false;
        pending_identify_item.item_id = 0;
        pending_identify_kit.item_id = 0;
        identify_all_type = InventoryManager::IdentifyAllType::None;
        identified_count = 0;
        context_item.item_id = 0;
    }
    void CancelAll()
    {
        ClearPotentialItems();
        CancelSalvage();
        CancelIdentify();
        CancelTransaction();
    }
    
    InventoryManager::Item* GetNextUnidentifiedItem(const InventoryManager::Item* start_after_item = nullptr)
    {
        size_t start_slot = 0;
        auto start_bag_id = GW::Constants::Bag::Backpack;
        if (start_after_item) {
            get_next_bag_slot(start_after_item, &start_bag_id, &start_slot);
        }

        for (auto bag_id = start_bag_id; bag_id <= GW::Constants::Bag::Equipment_Pack; bag_id++) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            size_t slot = start_slot;
            start_slot = 0;
            if (!bag) {
                continue;
            }
            GW::ItemArray& items = bag->items;
            for (size_t i = slot, size = items.size(); i < size; i++) {
                const auto item = static_cast<InventoryManager::Item*>(items[i]);
                if (!(item && item->CanBeIdentified())) {
                    continue;
                }
                switch (identify_all_type) {
                    case InventoryManager::IdentifyAllType::All:
                        return item;
                    case InventoryManager::IdentifyAllType::Blue:
                        if (item->IsBlue()) {
                            return item;
                        }
                        break;
                    case InventoryManager::IdentifyAllType::Purple:
                        if (item->IsPurple()) {
                            return item;
                        }
                        break;
                    case InventoryManager::IdentifyAllType::Gold:
                        if (item->IsGold()) {
                            return item;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        return nullptr;
    }
    
    void Identify(const InventoryManager::Item* item, const InventoryManager::Item* kit)
    {
        if (!item || !kit) {
            return;
        }
        if (item->GetIsIdentified() || !kit->IsIdentificationKit()) {
            return;
        }
        if (!(pending_identify_item.set(item) && pending_identify_kit.set(kit))) {
            return;
        }
        GW::Items::IdentifyItem(pending_identify_kit.item_id, pending_identify_item.item_id);
        pending_identify_at = clock() / CLOCKS_PER_SEC;
        is_identifying = true;
    }

    void IdentifyAll(const InventoryManager::IdentifyAllType type)
    {
        if (type != identify_all_type) {
            CancelIdentify();
            is_identifying_all = true;
            identify_all_type = type;
        }
        if (!is_identifying_all || is_identifying) {
            return;
        }
        // Get next item to identify
        const auto unid = GetNextUnidentifiedItem();
        if (!unid) {
            // Log::Info("Identified %d items", identified_count);
            CancelIdentify();
            return;
        }
        const auto kit = context_item.item();
        if (!kit || !kit->IsIdentificationKit()) {
            CancelIdentify();
            Log::Warning("The identification kit was consumed");
            return;
        }
        Identify(unid, kit);
    }

    void ContinueIdentify()
    {
        is_identifying = false;
        if (!IsMapReady()) {
            CancelIdentify();
            return;
        }
        if (pending_identify_item.item_id) {
            identified_count++;
        }
        if (is_identifying_all) {
            IdentifyAll(identify_all_type);
        }
    }
    GW::HookEntry OnPostUIMessage_HookEntry;
    GW::HookEntry OnPreUIMessage_HookEntry;

    struct LastKitUsed {
        uint32_t item_id = 0;
        clock_t used_at = 0;
        uint32_t uses = 0;
    };
    LastKitUsed last_salvage_kit_used = { 0 };
    LastKitUsed last_id_kit_used = {0};



    void OnPreUIMessage(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
            case GW::UI::UIMessage::kVendorWindow: {
                merchant_list_tab = *static_cast<uint32_t*>(wparam);
            } break;
            // About to request a quote for an item
            case GW::UI::UIMessage::kSendMerchantRequestQuote: {
                const auto packet = (GW::UI::UIPacket::kSendMerchantRequestQuote*)wparam;
                requesting_quote_type = (GW::Merchant::TransactionType)0;
                if (pending_transaction.in_progress() || !ImGui::IsKeyDown(ImGuiMod_Ctrl) || MaterialsWindow::Instance().GetIsInProgress()) {
                    return;
                }
                requesting_quote_type = packet->type;
                CancelTransaction();
                pending_transaction.type = requesting_quote_type;
                pending_transaction.item_id = packet->item_id;
                pending_transaction.price = 0;
                show_transact_quantity_popup = true;
                status->blocked = true;
            } break;
            // About to move an item
            case GW::UI::UIMessage::kSendMoveItem: {
                const auto packet = (GW::UI::UIPacket::kSendMoveItem*)wparam;

                if (packet->item_id != stack_prompt_item_id) {
                    stack_prompt_item_id = 0;
                    return;
                }
                stack_prompt_item_id = 0;
                status->blocked = true;
                move_item((InventoryManager::Item*)GW::Items::GetItemById(packet->item_id), static_cast<uint16_t>(packet->quantity));
            } break;
            // Quote for item has been received
            case GW::UI::UIMessage::kVendorQuote: {
                auto& transaction = pending_transaction;
                if (!transaction.in_progress()) {
                    return;
                }
                const auto packet = (GW::UI::UIPacket::kVendorQuote*)wparam;

                if (transaction.item_id != packet->item_id) {
                    Log::ErrorW(L"Received quote for item %u, but expected %u", packet->item_id, transaction.item_id);
                    CancelTransaction();
                    return;
                }
                transaction.price = packet->price;
                transaction.setState(PendingTransaction::State::Quoted);
            } break;
            case GW::UI::UIMessage::kVendorTransComplete: {
                auto& transaction = pending_transaction;
                if (!transaction.in_progress()) {
                    return;
                }
                pending_transaction_amount--;
                transaction.setState(PendingTransaction::State::Pending);
            } break;
            // Map left; cancel all actions
            case GW::UI::UIMessage::kMapChange: {
                CancelAll();
            } break;
            // Item moved; clear prompt
            case GW::UI::UIMessage::kMoveItem: {
                stack_prompt_item_id = 0;
            } break;
            case GW::UI::UIMessage::kSendUseItem: {
                OnUseItem(status, (uint32_t)wparam);
            } break;
            case GW::UI::UIMessage::kSalvageItem: {
                const auto packet = (GW::UI::UIPacket::kUseKitOnItem*)wparam;
                const auto item = GW::Items::GetItemById(packet->item_id);
                if (item && item->name_enc && *item->name_enc && block_from_being_salvaged.contains(item->name_enc)) status->blocked = true;
            } break;
        }
    }

    void OnPostUIMessage(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (status->blocked)
            return;
        switch (message_id) {
            case GW::UI::UIMessage::kVendorWindow: {
                UpdateQuoteHelpText();
            } break;
            case GW::UI::UIMessage::kPreStartSalvage: {
                const auto kit = (InventoryManager::Item*)GW::Items::GetItemById(((uint32_t*)wparam)[1]);
                if (auto_reuse_salvage_kit && kit) {
                    last_salvage_kit_used = {.item_id = kit->item_id, .used_at = TIMER_INIT(), .uses = kit->GetUses()};
                }
            } break;
            case GW::UI::UIMessage::kIdentifyItem: {
                const auto kit = (InventoryManager::Item*)GW::Items::GetItemById(((uint32_t*)wparam)[1]);
                if (!(auto_reuse_id_kit && kit)) break;
                const auto item = (InventoryManager::Item*)GW::Items::GetItemById(((uint32_t*)wparam)[0]);
                if (!(item && item->CanBeIdentified()) && kit && kit->GetUses()) {
                    //  Run next frame to allow current cursor to be cleared otherwise it'll assert
                    GW::GameThread::Enqueue([kit]() {
                        GW::Items::UseItem(kit);
                    });
                    break;
                }
                last_id_kit_used = {.item_id = kit->item_id, .used_at = TIMER_INIT(), .uses = kit->GetUses()};
            } break;
            case GW::UI::UIMessage::kItemUpdated: {
                const auto packet = (GW::UI::UIPacket::kItemUpdated*)wparam;
                clear_pending_move(packet->item_id);
                if (packet->item_id == last_salvage_kit_used.item_id 
                    && last_salvage_kit_used.used_at != 0 && TIMER_DIFF(last_salvage_kit_used.used_at) < 500) {
                        const auto updated_kit = (InventoryManager::Item*)GW::Items::GetItemById(packet->item_id);
                    if (updated_kit && updated_kit->GetUses() != last_salvage_kit_used.uses) {
                        last_salvage_kit_used = {0};
                        GW::Items::UseItem(updated_kit) || (Log::Log("Failed to re-use kit automatically"), false);
                    }
                }
                if (packet->item_id == last_id_kit_used.item_id && last_id_kit_used.used_at != 0 && TIMER_DIFF(last_id_kit_used.used_at) < 500) {
                    const auto updated_kit = (InventoryManager::Item*)GW::Items::GetItemById(packet->item_id);
                    if (updated_kit && updated_kit->GetUses() != last_id_kit_used.uses) {
                        last_id_kit_used = {0};
                        GW::Items::UseItem(updated_kit) || (Log::Log("Failed to re-use kit automatically"), false);
                    }
                }

            } break;
        }
    }

    void DrawMerchantHiddenItemsSettings()
    {
        // Two-column layout for merchant items using tables
        if (ImGui::BeginTable("merchant_settings_table", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Hidden from Merchant", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Ignored when Salvaging", ImGuiTableColumnFlags_WidthStretch);

            // Header row
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Hidden items from merchant sell window:");
            ImGui::SameLine();
            ImGui::TextDisabled("(Click on an item to remove it)");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Ignored items when salvaging:");
            ImGui::SameLine();
            ImGui::TextDisabled("(Click on an item to remove it)");

            // Content row
            ImGui::TableNextRow();

            // Left column: Hidden items list
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            const float list_height = 100.f;
            ImGui::BeginChild("hide_from_merchant_items", ImVec2(0.0F, list_height));

            const float wrap_width = ImGui::GetContentRegionAvail().x;
            for (const auto& [item_id, item_name] : hide_from_merchant_items) {
                ImGui::PushID(static_cast<int>(item_id));

                const auto button_label = std::format("{} | X", item_name);

                // Calculate if this button would exceed available width
                const ImVec2 button_size = ImGui::CalcTextSize(button_label.c_str());
                const float button_width = button_size.x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const float cursor_x = ImGui::GetCursorPosX();

                // Wrap to next line if button would go past the edge
                if (cursor_x + button_width > wrap_width && cursor_x > 0.0f) {
                    ImGui::NewLine();
                }

                bool clicked = false;
                clicked = ImGui::ConfirmButton(button_label.c_str(), &clicked);

                if (clicked) {
                    Log::Flash("Removed Item %s with ID (%d)", item_name.c_str(), item_id);
                    hide_from_merchant_items.erase(item_id);
                    ImGui::PopID();
                    break;
                }

                ImGui::SameLine();
                ImGui::PopID();
            }

            ImGui::EndChild();
            ImGui::Text("To add an item to this list, right click the item from your inventory and select 'Hide this when selling'");

            // Right column: Salvage ignore list
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::BeginChild("block_from_being_salvaged", ImVec2(0.0F, list_height));

            const float wrap_width2 = ImGui::GetContentRegionAvail().x;
            for (const auto& it : block_from_being_salvaged) {
                ImGui::PushID(&it);

                const auto button_label = std::format("{} | X", it.second);

                // Calculate if this button would exceed available width
                const ImVec2 button_size = ImGui::CalcTextSize(button_label.c_str());
                const float button_width = button_size.x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const float cursor_x = ImGui::GetCursorPosX();

                // Wrap to next line if button would go past the edge
                if (cursor_x + button_width > wrap_width2 && cursor_x > 0.0f) {
                    ImGui::NewLine();
                }

                bool clicked = false;
                clicked = ImGui::ConfirmButton(button_label.c_str(), &clicked);

                if (clicked) {
                    Log::Flash("Removed %s", it.second.c_str());
                    block_from_being_salvaged.erase(it.first);
                    ImGui::PopID();
                    break;
                }

                ImGui::SameLine();
                ImGui::PopID();
            }

            ImGui::EndChild();
            ImGui::Text("To add an item to this list, right click the item from your inventory and select 'Ignore this when salvaging'");

            ImGui::EndTable();
        }
    }


    void FetchPotentialItems()
    {
        const InventoryManager::Item* found = nullptr;
        if (salvage_all_type != InventoryManager::SalvageAllType::None) {
            ClearPotentialItems();
            while ((found = InventoryManager::GetNextUnsalvagedItem(context_item.item(), found)) != nullptr) {
                const auto pi = new PotentialItem();
                pi->set(found);
                potential_salvage_all_items.push_back(pi);
            }
        }
    }

    void Salvage(InventoryManager::Item* item, const InventoryManager::Item* kit)
    {
        if (!item || !kit) {
            return;
        }
        if (!item->IsSalvagable() || !kit->IsSalvageKit()) {
            return;
        }
        if (!(pending_salvage_item.set(item) && pending_salvage_kit.set(kit))) {
            return;
        }
        GW::Items::SalvageStart(pending_salvage_kit.item_id, pending_salvage_item.item_id);
        pending_salvage_at = TIMER_INIT();
        is_salvaging = true;
    }

    void SalvageAll(const InventoryManager::SalvageAllType type)
    {
        if (type != salvage_all_type) {
            CancelSalvage();
            salvage_all_type = type;
        }
        const auto available_slot = InventoryManager::GetAvailableInventorySlot();
        if (!available_slot.first) {
            CancelSalvage();
            Log::Warning("No more space in inventory");
            return;
        }
        if (!has_prompted_salvage) {
            salvage_all_type = type;
            FetchPotentialItems();
            if (!potential_salvage_all_items.size()) {
                CancelSalvage();
                Log::Warning("No items to salvage");
                return;
            }
            show_salvage_all_popup = true;
            has_prompted_salvage = true;
            return;
        }
        if (!is_salvaging_all || is_salvaging) {
            return;
        }
        const auto kit = context_item.item();
        if (!kit || !kit->IsSalvageKit()) {
            CancelSalvage();
            Log::Warning("The salvage kit was consumed");
            return;
        }
        if (!potential_salvage_all_items.size()) {
            Log::Info("Salvaged %d items", salvaged_count);
            CancelSalvage();
            return;
        }
        const PotentialItem* ref = *potential_salvage_all_items.begin();
        if (!ref->proceed) {
            delete ref;
            potential_salvage_all_items.erase(potential_salvage_all_items.begin());
            return; // User wants to skip this item; continue to next frame.
        }
        auto item = ref->item();
        if (!item || !item->bag || !item->bag->IsInventoryBag()) {
            delete ref;
            potential_salvage_all_items.erase(potential_salvage_all_items.begin());
            return; // Item has moved or been consumed since prompt.
        }

        Salvage(item, kit);
    }

    bool IsPendingSalvage()
    {
        if (!pending_salvage_kit.item_id || !pending_salvage_item.item_id) {
            return false;
        }
        const auto current_kit = pending_salvage_kit.item();
        if (current_kit && current_kit->GetUses() == pending_salvage_kit.uses) {
            return true;
        }
        const auto current_item = pending_salvage_item.item();
        if (current_item && current_item->quantity == pending_salvage_item.quantity) {
            return true;
        }
        return false;
    }

    void ContinueSalvage()
    {
        if (pending_cancel_salvage) {
            CancelSalvage();
            return;
        }
        if (TIMER_DIFF(pending_salvage_at) > 5000) {
            Log::Warning("Failed to salvage item in slot %d/%d", pending_salvage_item.bag, pending_salvage_item.slot);
            CancelSalvage();
            return;
        }
        if (!IsMapReady()) {
            CancelSalvage();
            return;
        }
        if (IsPendingSalvage()) {
            const auto salvage_info = GW::Items::GetSalvageSessionInfo();
            if (salvage_info) {
                if (salvage_info->item_id != pending_salvage_item.item_id) {
                    Log::Warning("Unexpected salvage item prompt - different item id!");
                    CancelSalvage();
                    return;
                }
                if (!GW::Items::SalvageMaterials()) {
                    Log::Warning("GW::Items::SalvageMaterials failure");
                    CancelSalvage();
                    return;
                }
                pending_salvage_at = TIMER_INIT();
            }
            // Auto accept "you can only salvage materials with a lesser salvage kit"
            GW::UI::ButtonClick(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"), 0x6, 0x6d, 0x6));
            return;
        }
        is_salvaging = false;
        if (pending_salvage_item.item_id) {
            salvaged_count++;
        }
        if (is_salvaging_all) {
            SalvageAll(salvage_all_type);
        }
        else {
            CancelSalvage();
        }
    }

    void ContinueTransaction()
    {
        if (!IsMapReady()) {
            pending_cancel_transaction = true;
        }

        switch (pending_transaction.state) {
            case PendingTransaction::State::Pending: {
                if (pending_cancel_transaction) {
                    CancelTransaction();
                    return;
                }
                // Check if we need any more of this item; send quote if yes, complete if no.
                if (pending_transaction_amount <= 0) {
                    Log::Flash("Transaction complete");
                    CancelTransaction();
                    return;
                }
                Log::Log("PendingTransaction pending, ask for quote\n");
                pending_transaction.setState(PendingTransaction::State::Quoting);
                if (!RequestQuote(pending_transaction.type, pending_transaction.item_id)) {
                    Log::ErrorW(L"Failed to request quote");
                    CancelTransaction();
                    return;
                }
            } break;
            case PendingTransaction::State::Quoting:
                // Check for timeout having asked for a quote.
                if (TIMER_DIFF(pending_transaction.state_timestamp) > 1000) {
                    if (pending_transaction.retries > 0) {
                        Log::ErrorW(L"Timeout waiting for item quote");
                        CancelTransaction();
                        return;
                    }
                    pending_transaction.retries++;
                    pending_transaction.setState(PendingTransaction::State::Pending);
                }
                break;
            case PendingTransaction::State::Quoted: {
                pending_transaction.retries = 0;
                if (pending_cancel_transaction) {
                    CancelTransaction();
                    return;
                }
                Log::Log("PendingTransaction quoted %d, moving to buy/sell\n", pending_transaction.price);
                // Got a quote; begin transaction
                pending_transaction.setState(PendingTransaction::State::Transacting);
                if (!GW::Merchant::TransactItems()) {
                    Log::ErrorW(L"Failed to transact items");
                    CancelTransaction();
                    return;
                }
            } break;
            case PendingTransaction::State::Transacting:
                // Check for timeout having agreed to buy or sell
                if (TIMER_DIFF(pending_transaction.state_timestamp) > 1000) {
                    if (pending_transaction.retries > 0) {
                        Log::ErrorW(L"Timeout waiting for item sell/buy");
                        CancelTransaction();
                        return;
                    }
                    pending_transaction.retries++;
                    pending_transaction.setState(PendingTransaction::State::Pending);
                }
                break;
            default:
                // Anything else, cancel the transaction.
                CancelTransaction();
        }
    }

    bool IsPendingIdentify()
    {
        if (!pending_identify_kit.item_id || !pending_identify_item.item_id) {
            return false;
        }
        const auto current_kit = pending_identify_kit.item();
        if (current_kit && current_kit->GetUses() == pending_identify_kit.uses) {
            return true;
        }
        const auto current_item = pending_identify_item.item();
        if (current_item && !current_item->GetIsIdentified()) {
            return true;
        }
        return false;
    }

} // namespace

void InventoryManager::Initialize()
{
    ToolboxUIElement::Initialize();

    bags_to_salvage_from = {
        {GW::Constants::Bag::Backpack, true},
        {GW::Constants::Bag::Belt_Pouch, true},
        {GW::Constants::Bag::Bag_1, true},
        {GW::Constants::Bag::Bag_2, true}
    };

    GW::Items::RegisterItemClickCallback(&ItemClick_Entry, ItemClickCallback);

    GW::UI::UIMessage message_id_hooks[] = {
        GW::UI::UIMessage::kSendMoveItem,
        GW::UI::UIMessage::kSendMerchantRequestQuote,
        GW::UI::UIMessage::kVendorTransComplete,
        GW::UI::UIMessage::kVendorQuote,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMoveItem,
        GW::UI::UIMessage::kSendUseItem,
        GW::UI::UIMessage::kItemUpdated,
        GW::UI::UIMessage::kVendorWindow,        
        GW::UI::UIMessage::kPreStartSalvage,
        GW::UI::UIMessage::kIdentifyItem,        
        GW::UI::UIMessage::kSalvageItem
    };
    for (const auto message_id : message_id_hooks) {
        RegisterUIMessageCallback(&OnPreUIMessage_HookEntry, message_id, OnPreUIMessage,-0x8000);
        RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, message_id, OnPostUIMessage, 0x8000);
    }


    inventory_bags_window_position = GetWindowPosition(GW::UI::WindowID::WindowID_InventoryBags);

    AddItemRowToWindow_Func = reinterpret_cast<AddItemRowToWindow_pt>(GW::Scanner::Find(
        "\x83\xc4\x04\x80\x78\x04\x06\x0f\x84\xd3\x00\x00\x00\x6a\x02\xff\x37", nullptr, -0x10));
    if (AddItemRowToWindow_Func) {
        GW::Hook::CreateHook((void**)&AddItemRowToWindow_Func, OnAddItemToWindow, reinterpret_cast<void**>(&RetAddItemRowToWindow));
        GW::Hook::EnableHooks(AddItemRowToWindow_Func);
    }

    UICallback_ChooseQuantityPopup_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GmItemSplit.cpp", "inventorySlot", 0, 0));
    if (UICallback_ChooseQuantityPopup_Func) {
        GW::Hook::CreateHook((void**)&UICallback_ChooseQuantityPopup_Func, OnChooseQuantityPopupUIMessage, reinterpret_cast<void**>(&UICallback_ChooseQuantityPopup_Ret));
        GW::Hook::EnableHooks(UICallback_ChooseQuantityPopup_Func);
    }

#if _DEBUG
    ASSERT(AddItemRowToWindow_Func);
    ASSERT(UICallback_ChooseQuantityPopup_Func);
#endif

}

void InventoryManager::Terminate()
{
    ToolboxUIElement::Terminate();
    ClearPotentialItems();
    GW::Items::RemoveItemClickCallback(&ItemClick_Entry);
    GW::UI::RemoveUIMessageCallback(&OnPreUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
    GW::Hook::RemoveHook(AddItemRowToWindow_Func);
    GW::Hook::RemoveHook(UICallback_ChooseQuantityPopup_Func);
    block_from_being_salvaged.clear();

}

bool InventoryManager::WndProc(const UINT message, const WPARAM, const LPARAM)
{
    // GW Deliberately makes a WM_MOUSEMOVE event right after right button is pressed.
    // Does this to "hide" the cursor when looking around.
    switch (message) {
        case WM_GW_RBUTTONCLICK: {
            const auto item = GW::Items::GetHoveredItem();
            if (!item) break;
            GW::GameThread::Enqueue([item]() {
                // Item right clicked - spoof a click event
                GW::HookStatus status;
                GW::UI::UIPacket::kMouseAction packet = {
                    .child_offset_id = item->slot + 2u, 
                    .current_state = (GW::UI::UIPacket::ActionState)999,
                };
                ItemClickCallback(&status, &packet, item);
                });
        } break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            const auto item = GW::Items::GetHoveredItem();
            const auto bag = item ? item->bag : nullptr;
            if (bag && bag->IsMaterialStorage()) {
                // Item in material storage pane clicked - spoof a click event
                GW::HookStatus status;
                GW::UI::UIPacket::kMouseAction packet = {.child_offset_id = item->slot + 2u, .current_state = message == WM_LBUTTONDOWN ? GW::UI::UIPacket::ActionState::MouseUp : GW::UI::UIPacket::ActionState::MouseDoubleClick};
                ItemClickCallback(&status, &packet, item);
                return false;
            }
        }
        break;
    }
    return false;
}

uint16_t InventoryManager::CountItemsByName(const wchar_t* name_enc)
{
    return count_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Storage_14, [name_enc](InventoryManager::Item* item) {
        return item && item->name_enc && wcscmp(item->name_enc, name_enc) == 0;
        });
}

void InventoryManager::SaveSettings(ToolboxIni* ini)
{
    ToolboxUIElement::SaveSettings(ini);
    SAVE_BOOL(only_use_superior_salvage_kits);
    SAVE_BOOL(salvage_rare_mats);
    SAVE_BOOL(salvage_nicholas_items);
    SAVE_BOOL(trade_whole_stacks);
    SAVE_BOOL(wiki_link_on_context_menu);
    SAVE_BOOL(hide_unsellable_items);
    SAVE_BOOL(hide_golds_from_merchant);
    SAVE_BOOL(hide_weapon_sets_and_customized_items);
    SAVE_BOOL(change_secondary_for_tome);
    SAVE_BOOL(right_click_context_menu_in_outpost);
    SAVE_BOOL(right_click_context_menu_in_explorable);
    SAVE_BOOL(move_to_trade_on_double_click);
    SAVE_BOOL(move_to_trade_on_alt_click);
    SAVE_BOOL(salvage_all_on_ctrl_click);
    SAVE_BOOL(identify_all_on_ctrl_click);
    SAVE_BOOL(auto_reuse_salvage_kit);
    SAVE_BOOL(auto_reuse_id_kit);

    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_backpack), bags_to_salvage_from[GW::Constants::Bag::Backpack]);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_belt_pouch), bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_bag_1), bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_bag_2), bags_to_salvage_from[GW::Constants::Bag::Bag_2]);

    GuiUtils::MapToIni(ini, Name(), VAR_NAME(hide_from_merchant_items), hide_from_merchant_items);
    GuiUtils::MapToIni(ini, Name(), VAR_NAME(block_from_being_salvaged), block_from_being_salvaged);
}

void InventoryManager::LoadSettings(ToolboxIni* ini)
{
    ToolboxUIElement::LoadSettings(ini);
    LOAD_BOOL(only_use_superior_salvage_kits);
    LOAD_BOOL(salvage_rare_mats);
    LOAD_BOOL(salvage_nicholas_items);
    LOAD_BOOL(trade_whole_stacks);
    LOAD_BOOL(wiki_link_on_context_menu);
    LOAD_BOOL(hide_golds_from_merchant);
    LOAD_BOOL(hide_unsellable_items);
    LOAD_BOOL(hide_weapon_sets_and_customized_items);
    LOAD_BOOL(change_secondary_for_tome);
    LOAD_BOOL(right_click_context_menu_in_outpost);
    LOAD_BOOL(right_click_context_menu_in_explorable);
    LOAD_BOOL(move_to_trade_on_double_click);
    LOAD_BOOL(move_to_trade_on_alt_click);
    LOAD_BOOL(salvage_all_on_ctrl_click);
    LOAD_BOOL(identify_all_on_ctrl_click);
    LOAD_BOOL(auto_reuse_salvage_kit);
    LOAD_BOOL(auto_reuse_id_kit);

    bags_to_salvage_from[GW::Constants::Bag::Backpack] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_backpack), bags_to_salvage_from[GW::Constants::Bag::Backpack]);
    bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_belt_pouch), bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
    bags_to_salvage_from[GW::Constants::Bag::Bag_1] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_bag_1), bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
    bags_to_salvage_from[GW::Constants::Bag::Bag_2] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_bag_2), bags_to_salvage_from[GW::Constants::Bag::Bag_2]);

    hide_from_merchant_items = GuiUtils::IniToMap<std::map<uint32_t, std::string>>(ini, Name(), VAR_NAME(hide_from_merchant_items));
    block_from_being_salvaged = GuiUtils::IniToMap<std::map<std::wstring, std::string>>(ini, Name(), VAR_NAME(block_from_being_salvaged));
}

InventoryManager::Item* InventoryManager::GetNextUnsalvagedItem(const Item* kit, const Item* start_after_item)
{
    size_t start_slot = 0;
    auto start_bag_id = GW::Constants::Bag::Backpack;
    if (start_after_item) {
        get_next_bag_slot(start_after_item, &start_bag_id, &start_slot);
    }
    for (auto bag_id = start_bag_id; bag_id <= GW::Constants::Bag::Bag_2; bag_id++) {
        size_t slot = start_slot;
        start_slot = 0;
        if (!bags_to_salvage_from[bag_id]) {
            continue;
        }
        GW::Bag* bag = GW::Items::GetBag(bag_id);
        if (!bag) {
            continue;
        }
        GW::ItemArray& items = bag->items;
        for (size_t i = slot, size = items.size(); i < size; i++) {
            const auto item = static_cast<Item*>(items[i]);
            if (!item) {
                continue;
            }
            if (!item->value) {
                continue; // No value usually means no salvage.
            }
            if (!item->IsSalvagable()) {
                continue;
            }
            if (item->equipped) {
                continue;
            }
            if (item->IsRareMaterial() && !salvage_rare_mats) {
                continue; // Don't salvage rare mats
            }
            if (item->IsArmor() || item->customized) {
                continue; // Don't salvage armor, or customised weapons.
            }
            if (item->IsBlue() && !item->GetIsIdentified() && (kit && kit->IsLesserKit())) {
                continue; // Note: lesser kits cant salvage blue unids - Guild Wars bug/feature
            }
            if (DailyQuests::GetNicholasItemInfo(item->name_enc) && !salvage_nicholas_items) {
                continue; // Don't salvage nicholas items
            }
            const GW::Constants::Rarity rarity = item->GetRarity();
            switch (rarity) {
                case GW::Constants::Rarity::Gold:
                    if (!item->GetIsIdentified()) {
                        continue;
                    }
                    if (salvage_all_type < SalvageAllType::GoldAndLower) {
                        continue;
                    }
                    return item;
                case GW::Constants::Rarity::Purple:
                    if (!item->GetIsIdentified()) {
                        continue;
                    }
                    if (salvage_all_type < SalvageAllType::PurpleAndLower) {
                        continue;
                    }
                    return item;
                case GW::Constants::Rarity::Blue:
                    if (!item->GetIsIdentified()) {
                        continue;
                    }
                    if (salvage_all_type < SalvageAllType::BlueAndLower) {
                        continue;
                    }
                    return item;
                case GW::Constants::Rarity::White:
                    return item;
                default:
                    break;
            }
        }
    }
    return nullptr;
}

std::pair<GW::Bag*, uint32_t> InventoryManager::GetAvailableInventorySlot(GW::Item* like_item)
{
    GW::Item* existing_stack = GetAvailableInventoryStack(like_item, true);
    if (existing_stack) {
        return {existing_stack->bag, existing_stack->slot};
    }
    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) {
        return {nullptr, 0};
    }
    size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);
    const auto im_item = static_cast<Item*>(like_item);
    if (im_item && (im_item->IsWeapon() || im_item->IsArmor())) {
        end_bag = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack);
    }
    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx <= end_bag; bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items.valid()) {
            continue;
        }
        for (size_t slot = 0; slot < bag->items.size(); slot++) {
            if (!bag->items[slot]) {
                return {bag, slot};
            }
        }
    }
    return {nullptr, 0};
}

uint16_t InventoryManager::RefillUpToQuantity(const uint16_t wanted_quantity, const std::vector<uint32_t>& model_ids)
{
    uint16_t moved = 0;
    for (const auto model_id : model_ids) {
        const auto is_same_item = [model_id](const Item* cmp) {
            return cmp && cmp->model_id == model_id;
        };
        const auto amount_in_inventory = count_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Equipment_Pack, is_same_item);
        if (amount_in_inventory >= wanted_quantity) {
            continue; // Already got enough
        }
        uint16_t to_move = wanted_quantity - amount_in_inventory;
        const auto amount_in_storage = count_items(GW::Constants::Bag::Material_Storage, GW::Constants::Bag::Storage_14, is_same_item);
        if (amount_in_storage < to_move) {
            // @Enhancement: Make this warning optional? Its more annoying than anything else if you're using it as a hotkey and you run out, so disabled for now.
            // Log::Warning("Only able to withdraw %d of %d items with model id %d", amount_in_inventory + amount_in_storage, wanted_quantity, model_id);
        }
        const auto storage_items = filter_items(GW::Constants::Bag::Material_Storage, GW::Constants::Bag::Storage_14, is_same_item);
        for (const auto item : storage_items) {
            const auto this_move = move_item_to_inventory(item, to_move);
            moved += this_move;
            to_move -= this_move;
            if (to_move < 1) {
                break; // All items moved ok
            }
        }
        // NB: No error messages are here because we don't know the exact reason for the failed move item - could be that another module is already transferring the item, so technically its not a problem
    }
    return moved;
}

uint16_t InventoryManager::StoreItems(uint16_t quantity, const std::vector<unsigned>& model_ids)
{
    uint16_t moved = 0;
    for (const auto model_id : model_ids) {
        const auto is_same_item = [model_id](const Item* cmp) {
            return cmp && cmp->model_id == model_id;
        };
        const auto inventory_items = filter_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, is_same_item);
        uint16_t to_move = quantity;
        for (const auto item : inventory_items) {
            const auto this_move = move_item_to_storage(item, to_move);
            moved += this_move;
            to_move -= this_move;
            if (to_move < 1) {
                break; // All items moved ok
            }
        }
    }
    return moved;
}

GW::Item* InventoryManager::GetAvailableInventoryStack(GW::Item* like_item, const bool entire_stack)
{
    if (!like_item || static_cast<Item*>(like_item)->IsStackable()) {
        return nullptr;
    }
    GW::Item* best_item = nullptr;
    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) {
        return best_item;
    }
    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx < static_cast<size_t>(GW::Constants::Bag::Equipment_Pack); bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items_count || !bag->items.valid()) {
            continue;
        }
        for (GW::Item* item : bag->items) {
            if (!item || like_item->item_id == item->item_id || !IsSameItem(like_item, item) || item->quantity == 250) {
                continue;
            }
            if (entire_stack && 250 - item->quantity < like_item->quantity) {
                continue;
            }
            if (!best_item || item->quantity < best_item->quantity) {
                best_item = item;
            }
        }
    }
    return best_item;
}

bool InventoryManager::IsSameItem(const GW::Item* item1, const GW::Item* item2)
{
    return item1 && item2
        && (!item1->model_file_id || item1->model_file_id == item2->model_file_id)
        && (item1->type != GW::Constants::ItemType::Dye || memcmp(&item1->dye,&item2->dye,sizeof(item1->dye)) == 0)
        && ::wcseq(item1->name_enc, item2->name_enc);
}

void InventoryManager::DrawSettingsInternal()
{
    ImGui::TextDisabled("This module is responsible for extra item functions via ctrl+click, right click or double click");
    ImGui::Checkbox("Hide unsellable items from merchant window", &hide_unsellable_items);
    ImGui::Checkbox("Hide weapon sets and customized items from merchant window", &hide_weapon_sets_and_customized_items);
    ImGui::Checkbox("Hide gold items from merchant window", &hide_golds_from_merchant);
    ImGui::Checkbox("Move whole stacks by default", &trade_whole_stacks);
    ImGui::ShowHelp("Shift drag to prompt for amount, drag without shift to move the whole stack without any item quantity prompts");
    ImGui::TextUnformatted("Move items to trade on:");
    ImGui::ShowHelp("When trading with another player, you normally have to drag an item from inventory to the trade window. Enable an option below to make it easier.");
    ImGui::Indent();
    if (ImGui::Checkbox("Double Click", &move_to_trade_on_double_click) && move_to_trade_on_alt_click) move_to_trade_on_alt_click = false;
    ImGui::SameLine();
    if (ImGui::Checkbox("Alt+Click", &move_to_trade_on_alt_click) && move_to_trade_on_alt_click) move_to_trade_on_double_click = false;
    ImGui::Unindent();
    ImGui::Checkbox("Show 'Guild Wars Wiki' link on item context menu", &wiki_link_on_context_menu);
    ImGui::Checkbox("Prompt to change secondary profession when using a tome", &change_secondary_for_tome);
    ImGui::Text("Right click an item to open context menu in:");
    ImGui::Indent();
    ImGui::Checkbox("Exporable Area", &right_click_context_menu_in_explorable);
    ImGui::SameLine();
    ImGui::Checkbox("Outpost", &right_click_context_menu_in_outpost);
    ImGui::Unindent();
    ImGui::Text("Salvage All options:");
    ImGui::SameLine();
    ImGui::TextDisabled("Note: Salvage All will only salvage items that are identified.");
    ImGui::Checkbox("Salvage Rare Materials", &salvage_rare_mats);
    ImGui::ShowHelp("Untick to skip salvagable rare materials when checking for salvagable items");
    ImGui::SameLine();
    ImGui::Checkbox("Salvage Nicholas Items", &salvage_nicholas_items);
    ImGui::ShowHelp("Untick to skip items that Nicholas the Traveller collects when checking for salvagable items");
    ImGui::Text("Salvage from:");
    ImGui::ShowHelp("Only ticked bags will be checked for salvagable items");
    ImGui::Checkbox("Backpack", &bags_to_salvage_from[GW::Constants::Bag::Backpack]);
    ImGui::SameLine();
    ImGui::Checkbox("Belt Pouch", &bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
    ImGui::SameLine();
    ImGui::Checkbox("Bag 1", &bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
    ImGui::SameLine();
    ImGui::Checkbox("Bag 2", &bags_to_salvage_from[GW::Constants::Bag::Bag_2]);
    ImGui::Checkbox("Salvage All with Control+Click", &salvage_all_on_ctrl_click);
    ImGui::ShowHelp("Control+Click a salvage kit to open the Salvage All window");
    ImGui::Checkbox("Identify All with Control+Click", &identify_all_on_ctrl_click);
    ImGui::ShowHelp("Control+Click an identification kit to identify all items with it");
    ImGui::Checkbox("Auto re-use salvage kit", &auto_reuse_salvage_kit);
    ImGui::ShowHelp("When a salvage kit is used up immediately by salvaging without a popup,\ncheck this box to 're-use' the kit ready for the next item.");
    ImGui::Checkbox("Auto re-use identification kit", &auto_reuse_id_kit);
    ImGui::ShowHelp("When a identification kit is used up immediately by identifying an item,\ncheck this box to 're-use' the kit ready for the next item.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawMerchantHiddenItemsSettings();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void InventoryManager::Update(float)
{
    ProcessQueuedButtonPresses();

    if (pending_item_move_for_trade) {
        const auto item = reinterpret_cast<Item*>(GW::Items::GetItemById(pending_item_move_for_trade));
        if (!item) {
            pending_item_move_for_trade = 0;
            return;
        }
        if (item->bag && item->bag->IsInventoryBag()) {
            if (item->CanOfferToTrade()) {
                GW::Trade::OfferItem(item->item_id);
            }
            pending_item_move_for_trade = 0;
        }
    }
    if (is_salvaging) {
        ContinueSalvage();
    }
    if (is_identifying) {
        if (IsPendingIdentify()) {
            if (clock() / CLOCKS_PER_SEC - pending_identify_at > 5) {
                is_identifying = is_identifying_all = false;
                Log::Warning("Failed to identify item in slot %d/%d", pending_identify_item.bag, pending_identify_item.slot);
            }
        }
        else {
            // Identify complete
            ContinueIdentify();
        }
    }
    if (is_identifying_all) {
        IdentifyAll(identify_all_type);
    }
    if (is_salvaging_all) {
        SalvageAll(salvage_all_type);
    }
    if (pending_transaction.in_progress()) {
        ContinueTransaction();
    }
};

void InventoryManager::Draw(IDirect3DDevice9*)
{
    if (!GW::Map::GetIsMapLoaded()) {
        return;
    }
    DrawPendingTomeUsage();
    DrawPendingDestroyItem();
#if 0
    DrawInventoryOverlay();
#endif

    static bool check_all_items = true;
    static bool show_inventory_context_menu = false;
    DrawItemContextMenu(show_item_context_menu);
    show_item_context_menu = false;

    if (show_transact_quantity_popup) {
        ImGui::OpenPopup("Transaction quantity");
        pending_transaction.setState(PendingTransaction::State::Prompt);
    }
    if (ImGui::BeginPopupModal("Transaction quantity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        switch (pending_transaction.state) {
            case PendingTransaction::State::None:
                // Transaction has just completed, progress window still open - close it now.
                pending_cancel_transaction = true;
                ImGui::CloseCurrentPopup();
                break;
            case PendingTransaction::State::Prompt: {
                if (show_transact_quantity_popup) {
                    pending_transaction_amount = 1;
                    if (pending_transaction.selling()) {
                        const Item* item = pending_transaction.item();
                        if (item) {
                            // Set initial transaction amount to be the entire stack
                            pending_transaction_amount = item->quantity;
                            if (item->GetIsMaterial() && !item->IsRareMaterial() && pending_transaction.type == GW::Merchant::TransactionType::TraderSell) {
                                pending_transaction_amount = static_cast<int>(floor(pending_transaction_amount / 10));
                            }
                        }
                    }
                }
                // Prompt user for amount
                ImGui::Text(pending_transaction.selling() ? "Enter quantity to sell:" : "Enter quantity to buy:");
                if (ImGui::InputInt("###transacting_quantity", &pending_transaction_amount, 1, 1)) {
                    if (pending_transaction_amount < 1) {
                        pending_transaction_amount = 1;
                    }
                }
                const ImGuiID id = ImGui::GetID("###transacting_quantity");
                if (show_transact_quantity_popup) {
                    ImGui::SetFocusID(id, ImGui::GetCurrentWindow());
                }
                show_transact_quantity_popup = false;
                bool begin_transaction = ImGui::GetFocusID() == id && ImGui::IsKeyPressed(ImGuiKey_Enter);
                begin_transaction |= ImGui::Button("Continue");
                if (begin_transaction) {
                    pending_cancel_transaction = false;
                    pending_transaction.setState(PendingTransaction::State::Pending);
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    pending_cancel_transaction = true;
                    ImGui::CloseCurrentPopup();
                }
            }
            break;
            default:
                // Anything else is in progress.
                ImGui::Text(pending_transaction.selling() ? "Selling items..." : "Buying items...");
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    pending_cancel_transaction = true;
                    ImGui::CloseCurrentPopup();
                }
                break;
        }
        ImGui::EndPopup();
    }
    if (show_salvage_all_popup) {
        ImGui::OpenPopup("Salvage All?");
        show_salvage_all_popup = false;
        check_all_items = true;
    }
    if (ImGui::BeginPopupModal("Salvage All?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!is_salvaging_all && salvage_all_type == SalvageAllType::None) {
            // Salvage has just completed, progress window still open - close it now.
            CancelSalvage();
            ImGui::CloseCurrentPopup();
        }
        else if (is_salvaging_all) {
            // Salvage in progress
            ImGui::Text("Salvaging items...");
            if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                pending_cancel_salvage = true;
                ImGui::CloseCurrentPopup();
            }
        }
        else {
            // Are you sure prompt; at this point we've already got the list of items via FetchPotentialItems()
            ImGui::Text("You're about to salvage %d item%s:", potential_salvage_all_items.size(), potential_salvage_all_items.size() == 1 ? "" : "s");
            ImGui::TextDisabled("Untick an item to skip salvaging");
            const float& font_scale = ImGui::GetIO().FontGlobalScale;
            const float wiki_btn_width = 50.0f * font_scale;
            static float longest_item_name_length = 280.0f * font_scale;
            const GW::Bag* bag = nullptr;
            bool has_items_to_salvage = false;
            if (ImGui::Checkbox("Select All", &check_all_items)) {
                for (size_t i = 0; i < potential_salvage_all_items.size(); i++) {
                    potential_salvage_all_items[i]->proceed = check_all_items;
                }
            }
            for (size_t i = 0; i < potential_salvage_all_items.size(); i++) {
                PotentialItem* pi = potential_salvage_all_items[i];
                if (!pi) {
                    continue;
                }
                const Item* item = pi->item();
                if (!item) {
                    continue;
                }
                if (bag != item->bag) {
                    if (!item->bag || item->bag->index > 3) {
                        continue;
                    }
                    bag = item->bag;
                    ImGui::Text(bag_names[item->bag->index + 1]);
                }
                switch (item->GetRarity()) {
                    case GW::Constants::Rarity::Blue:
                        ImGui::PushStyleColor(ImGuiCol_Text, ItemBlue);
                        break;
                    case GW::Constants::Rarity::Purple:
                        ImGui::PushStyleColor(ImGuiCol_Text, ItemPurple);
                        break;
                    case GW::Constants::Rarity::Gold:
                        ImGui::PushStyleColor(ImGuiCol_Text, ItemGold);
                        break;
                    default:
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
                        break;
                }
                ImGui::PushID(static_cast<int>(pi->item_id));
                ImGui::Checkbox(pi->name->string().c_str(), &pi->proceed);
                const float item_name_length = ImGui::CalcTextSize(pi->name->string().c_str(), nullptr, true).x;
                longest_item_name_length = item_name_length > longest_item_name_length ? item_name_length : longest_item_name_length;
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", pi->desc->string().c_str());
                }
                ImGui::SameLine(longest_item_name_length + wiki_btn_width);
                pi->wiki_name->wstring();
                if (ImGui::Button("Wiki", ImVec2(wiki_btn_width, 0))) {
                    GuiUtils::SearchWiki(pi->wiki_name->wstring());
                }
                ImGui::PopID();
                has_items_to_salvage |= pi->proceed;
            }
            ImGui::Text("\n\nAre you sure?");
            auto btn_width = ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x, 0);
            if (has_items_to_salvage) {
                btn_width.x /= 2;
                if (ImGui::Button("OK", btn_width) || ImGui::IsKeyDown(ImGuiKey_Space) || ImGui::IsKeyDown(ImGuiKey_Enter)) {
                    is_salvaging_all = true;
                }
                ImGui::SameLine();
            }
            // Pressing [Escape] when no item is selected results in the window being closed.
            // Or pressing [Escape] when some items are selected results in everything being unselected.
            if (ImGui::Button("Cancel", btn_width) || ImGui::IsKeyPressed(ImGuiKey_Escape) && !has_items_to_salvage) {
                CancelSalvage();
                ImGui::CloseCurrentPopup();
            }
            else if (has_items_to_salvage && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                check_all_items = false;
                has_items_to_salvage = false;
                for (size_t i = 0; i < potential_salvage_all_items.size(); i++) {
                    potential_salvage_all_items[i]->proceed = false;
                }
            }
        }
        ImGui::EndPopup();
    }
}

bool InventoryManager::DrawItemContextMenu(const bool open)
{
    const auto context_menu_id = "Item Context Menu";
    const auto has_context_menu = [&](const Item* item) {
        if (!item) {
            return false;
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            return true;
        }
        if (wiki_link_on_context_menu) {
            return true;
        }
        return item->IsIdentificationKit() || item->IsSalvageKit();
    };
    auto context_item_actual = context_item.item();
    if (!context_item.item_id || !has_context_menu(context_item_actual)) {
        context_item.item_id = 0;
        return false;
    }

    // NB: Be really careful here; a window has to be open for at LEAST 1 frame before closing it again.

    // If it is closed before the draw ends, the you'll get a crash on g.NavWindow being NULL.
    // Either don't open it at all, or wait a frame before closing it.
    if (open) {
        ImGui::OpenPopup(context_menu_id);
    }
    if (!ImGui::BeginPopup(context_menu_id)) {
        return false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
    const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
    /*IDirect3DTexture9** tex = Resources::GetItemImage(context_item.wiki_name.wstring());
    if (tex && *tex) {
        const float text_height = ImGui::CalcTextSize(" ").y;
        ImGui::Image(*tex, ImVec2(text_height, text_height));
        ImGui::SameLine();
    }*/
    ImGui::Text(context_item.name->string().c_str());
    ImGui::Separator();
    const auto bag = context_item_actual->bag;
    bool can_use_storage = GW::Items::CanAccessXunlaiChest();
    // Shouldn't really fetch item() every frame, but its only when the menu is open and better than risking a crash
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        if (bag && can_use_storage && ImGui::Button(context_item_actual->IsInventoryItem() ? "Store Item" : "Withdraw Item", size)) {
            ImGui::CloseCurrentPopup();
            move_item(context_item_actual);
            goto end_popup;
        }
        char c_all_label[128];
        *c_all_label = 0;
        snprintf(c_all_label, _countof(c_all_label), "Consume All %s", context_item.plural_item_name->string().c_str());
        if (CanBulkConsume(context_item_actual) && ImGui::Button(c_all_label, size)) {
            ImGui::CloseCurrentPopup();
            const auto msg = std::format("Are you sure you want to consume all {} {}(s)?", context_item_actual->GetUses(), context_item.plural_item_name->string());
            ImGui::ConfirmDialog(msg.c_str(), [](bool result, void* wparam) {
                if(result)
                    consume_all((InventoryManager::Item *)GW::Items::GetItemById((uint32_t)wparam));
                }, (void*)context_item_actual->item_id);
            
            goto end_popup;
        }
        char move_all_label[128];
        *move_all_label = 0;
        if (can_use_storage && context_item_actual->IsInventoryItem()) {
            if (IsUnid(context_item_actual)) {
                if (ImGui::Button("Store All Unids", size)) {
                    ImGui::CloseCurrentPopup();
                    store_all_unids();
                    goto end_popup;
                }
            }
            if (context_item_actual->GetIsMaterial()) {
                if (ImGui::Button("Store All Materials", size)) {
                    ImGui::CloseCurrentPopup();
                    store_all_materials();
                    goto end_popup;
                }
            }
            else if (context_item_actual->IsTome()) {
                if (ImGui::Button("Store All Tomes", size)) {
                    ImGui::CloseCurrentPopup();
                    store_all_tomes();
                    goto end_popup;
                }
            }
            else if (context_item_actual->type == GW::Constants::ItemType::Rune_Mod) {
                if (ImGui::Button("Store All Upgrades", size)) {
                    ImGui::CloseCurrentPopup();
                    store_all_upgrades();
                    goto end_popup;
                }
            }
            else if (context_item_actual->type == GW::Constants::ItemType::Dye) {
                if (ImGui::Button("Store All Dyes", size)) {
                    ImGui::CloseCurrentPopup();
                    store_all_dyes();
                    goto end_popup;
                }
            }
            if (DailyQuests::GetNicholasItemInfo(context_item_actual->name_enc)) {
                if (ImGui::Button("Store All Nicholas Items", size)) {
                    ImGui::CloseCurrentPopup();
                    store_all_nicholas_items();
                    goto end_popup;
                }
            }
            snprintf(move_all_label, _countof(move_all_label), "Store All %s", context_item.plural_item_name->string().c_str());
        }
        if (can_use_storage && context_item_actual->IsStorageItem()) {
            if (IsUnid(context_item_actual)) {
                if (ImGui::Button("Withdraw All Unids", size)) {
                    ImGui::CloseCurrentPopup();
                    withdraw_all_unids();
                    goto end_popup;
                }
            }
            if (context_item_actual->type == GW::Constants::ItemType::Dye) {
                if (ImGui::Button("Withdraw All Dyes", size)) {
                    ImGui::CloseCurrentPopup();
                    withdraw_all_dyes();
                    goto end_popup;
                }
            }
            if (context_item_actual->IsTome()) {
                if (ImGui::Button("Withdraw All Tomes", size)) {
                    ImGui::CloseCurrentPopup();
                    withdraw_all_tomes();
                    goto end_popup;
                }
            }
            snprintf(move_all_label, _countof(move_all_label), "Withdraw All %s", context_item.plural_item_name->string().c_str());
        }
        if (*move_all_label && ImGui::Button(move_all_label, size)) {
            ImGui::CloseCurrentPopup();
            move_all_item(context_item_actual);
            goto end_popup;
        }
    }
    if (bag && context_item_actual->IsIdentificationKit()) {
        auto type = IdentifyAllType::None;
        if (ImGui::Button("Identify All Items", size)) {
            type = IdentifyAllType::All;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ItemBlue);
        if (ImGui::Button("Identify All Blue Items", size)) {
            type = IdentifyAllType::Blue;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ItemPurple);
        if (ImGui::Button("Identify All Purple Items", size)) {
            type = IdentifyAllType::Purple;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ItemGold);
        if (ImGui::Button("Identify All Gold Items", size)) {
            type = IdentifyAllType::Gold;
        }
        ImGui::PopStyleColor(3);
        if (type != IdentifyAllType::None) {
            ImGui::CloseCurrentPopup();
            CancelIdentify();
            if (context_item.set(context_item_actual)) {
                is_identifying_all = true;
                identify_all_type = type;
                IdentifyAll(type);
            }
            goto end_popup;
        }
    }
    if (bag && context_item_actual->IsSalvageKit()) {
        auto type = SalvageAllType::None;
        if (ImGui::Button("Salvage All White Items", size)) {
            type = SalvageAllType::White;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ItemBlue);
        if (ImGui::Button("Salvage All Blue & Lesser Items", size)) {
            type = SalvageAllType::BlueAndLower;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ItemPurple);
        if (ImGui::Button("Salvage All Purple & Lesser Items", size)) {
            type = SalvageAllType::PurpleAndLower;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ItemGold);
        if (ImGui::Button("Salvage All Gold & Lesser Items", size)) {
            type = SalvageAllType::GoldAndLower;
        }
        ImGui::PopStyleColor(3);
        if (type != SalvageAllType::None) {
            ImGui::CloseCurrentPopup();
            CancelSalvage();
            if (context_item.set(context_item_actual)) {
                salvage_all_type = type;
                SalvageAll(type);
            }
            goto end_popup;
        }
    }
    if (bag) {
        const auto btn_text = std::format("Destroy {}", context_item.name->string());
        if (ImGui::Button(btn_text.c_str())) {
            ImGui::CloseCurrentPopup();
            pending_destroy_item_id = context_item.item_id;
            goto end_popup;
        }
    }


    context_item.wiki_name->wstring();
    if (wiki_link_on_context_menu && ImGui::Button("Guild Wars Wiki", size)) {
        ImGui::CloseCurrentPopup();
        GuiUtils::SearchWiki(context_item.wiki_name->wstring());
    }
    if (ArmoryWindow::CanPreviewItem(context_item.item())) {
        if (ImGui::Button("Preview Item", size)) {
            ImGui::CloseCurrentPopup();
            ArmoryWindow::PreviewItem(context_item.item());
            goto end_popup;
        }
    }
    if (GWMarketWindow::CanSellItem(context_item.item())) {
        if (ImGui::Button("Sell Item on Market", size)) {
            ImGui::CloseCurrentPopup();
            GWMarketWindow::AddItemToSell(context_item.item());
            goto end_popup;
        }
    }
    if (context_item_actual->name_enc && *context_item_actual->name_enc && context_item_actual->IsSalvagable(true,false)) {
        context_item.single_item_name->string(); // Pre-decode
        const auto identifier = context_item_actual->name_enc;
        bool flagged = block_from_being_salvaged.contains(identifier);
        if (ImGui::Button(!flagged ? "Hide this when salvaging" : "Show this when salvaging", size)) {
            ImGui::CloseCurrentPopup();
            if (flagged) {
                block_from_being_salvaged.erase(identifier);
            }
            else {
                block_from_being_salvaged[identifier] = context_item.single_item_name->string();
            }
            
            goto end_popup;
        }
    }
    if (context_item_actual->model_id && context_item_actual->value) {
        context_item.single_item_name->string(); // Pre-decode
        const auto identifier = context_item_actual->model_id;
        bool flagged = hide_from_merchant_items.contains(identifier);
        if (ImGui::Button(!flagged ? "Hide this when selling" : "Show this when selling", size)) {
            ImGui::CloseCurrentPopup();
            if (flagged) {
                hide_from_merchant_items.erase(identifier);
            }
            else {
                hide_from_merchant_items[identifier] = context_item.single_item_name->string();
            }
            goto end_popup;
        }
    }
    end_popup:
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndPopup();
    return true;
}

void InventoryManager::ItemClickCallback(GW::HookStatus* status, GW::UI::UIPacket::kMouseAction* action, GW::Item* gw_item)
{
#pragma warning(push)
#pragma warning(disable : 4063) // case 'value' is not a valid value for switch of enum

    auto item = (InventoryManager::Item*)gw_item;
    if (!item) return;
    switch (action->current_state) {
        case GW::UI::UIPacket::ActionState::MouseClick:
        case GW::UI::UIPacket::ActionState::MouseUp: // Left click
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                // Get any hovered item in order to get info about it for Ctrl+Click shortcuts.
                // May be null, in which case said shortcuts are ignored.

                if (item && identify_all_on_ctrl_click && item->IsIdentificationKit() && ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                    // Ctrl+Click on identification kit: Identify all items
                    ImGui::CloseCurrentPopup();
                    CancelIdentify();
                    if (context_item.set(item)) {
                        IdentifyAllType identify_type = IdentifyAllType::All;
                        is_identifying_all = true;
                        identify_all_type = identify_type;
                        IdentifyAll(identify_type);
                    }
                    return;
                }
                else if (item && salvage_all_on_ctrl_click && item->IsSalvageKit() && ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                    // Ctrl+Click on salvage kit: Open salvage all window
                    ImGui::CloseCurrentPopup();
                    CancelSalvage();
                    if (context_item.set(item)) {
                        SalvageAllType salvage_type = SalvageAllType::GoldAndLower;
                        salvage_all_type = salvage_type;
                        SalvageAll(salvage_type);
                    }
                    return;
                }
                else if (GameSettings::GetSettingBool("move_item_on_ctrl_click") && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
                    // Ctrl+Click: Move item to inventory/chest
                    if (ImGui::IsKeyDown(ImGuiMod_Shift) && item->quantity > 1) {
                        prompt_split_stack(item);
                    }
                    else {
                        move_item(item);
                    }
                    return;
                }
            }
            if (ImGui::IsKeyDown(ImGuiMod_Alt) && move_to_trade_on_alt_click && IsTradeWindowOpen()) {
                // Alt+Click: Add to trade window if available
                if (!item || !item->CanOfferToTrade()) {
                    return;
                }
                if (!item->bag->IsInventoryBag()) {
                    const uint16_t moved = move_to_first_empty_slot(item, GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2);
                    if (!moved) {
                        Log::ErrorW(L"Failed to move item to inventory for trading");
                        return;
                    }
                }
                pending_item_move_for_trade = item->item_id;
            }
            return;
        case GW::UI::UIPacket::ActionState::MouseDoubleClick: // Double click
            if (move_to_trade_on_double_click && IsTradeWindowOpen()) {
                status->blocked = true;
                // Alt+Click: Add to trade window if available
                if (!item || !item->CanOfferToTrade()) {
                    return;
                }
                if (!item->bag->IsInventoryBag()) {
                    const uint16_t moved = move_to_first_empty_slot(item, GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2);
                    if (!moved) {
                        Log::ErrorW(L"Failed to move item to inventory for trading");
                        return;
                    }
                }
                pending_item_move_for_trade = item->item_id;
            }
            return;
        case static_cast<GW::UI::UIPacket::ActionState>(999u): // Right click (via GWToolbox)
            if (!right_click_context_menu_in_explorable && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                return;
            }
            if (!right_click_context_menu_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
                return;
            }
            if (!item) {
                return;
            }

            // Context menu applies
            if (context_item.item_id == item->item_id && show_item_context_menu) {
                return; // Double looped.
            }
            if (!context_item.set(item)) {
                return;
            }
            show_item_context_menu = true;
            status->blocked = true;
            return;
        default:
            return;
    }
#pragma warning(pop)
}

bool InventoryManager::Item::IsOfferedInTrade() const
{
    return GW::Trade::IsItemOffered(item_id) != nullptr;
}

bool InventoryManager::Item::CanOfferToTrade() const
{
    auto* player_items = GetPlayerTradeItems();
    if (!player_items) {
        return false;
    }
    return IsTradable() && IsTradeWindowOpen() && !IsOfferedInTrade() && player_items->size() < 7;
}

bool InventoryManager::Item::CanBeIdentified() const
{
    if (GetIsIdentified()) return false;
    if (IsSalvagable(false)) return true;
    switch (type) {
        case GW::Constants::ItemType::Bundle:
        case GW::Constants::ItemType::Usable:
        case GW::Constants::ItemType::Quest_Item:
        case GW::Constants::ItemType::Storybook:
            return false;
    }
    if (IsWeapon() || IsArmor()) return true;
    if (IsGreen()) return false;
    switch (model_file_id) {
        case 0x44CAC:
            return false;
    }
    return true;
}

bool InventoryManager::Item::IsOldSchool() const
{
    // Not OS if inscribable (Nightfall/EotN) or not salvagable
    if (GetIsInscribable() || !IsSalvagable(false)) return false;

    // OS off-hands (wand/focus/shield) have 2 inherent mods, no upgrade slots
    switch (type) {
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Offhand:
            return !IsUpgradable();
    }

    // Other OS weapons have 1 inherent + 1 suffix slot (so they ARE upgradable)
    return IsWeapon() && !IsPrefixUpgradable();
}

bool InventoryManager::Item::IsSalvagable(bool check_bag, bool check_blocked_from_being_salvaged) const
{
    if (item_formula == 0x5da) {
        return false;
    }
    if (IsUsable() || IsGreen()) {
        return false; // Non-salvagable flag set
    }
    if (check_bag && !bag) {
        return false;
    }
    if (check_bag && !bag->IsInventoryBag() && !bag->IsStorageBag()) {
        return false;
    }
    if (check_bag && bag->index + 1 == std::to_underlying(GW::Constants::Bag::Equipment_Pack)) {
        return false;
    }
    if (check_blocked_from_being_salvaged && name_enc && *name_enc && block_from_being_salvaged.contains(name_enc)) {
        return false;
    }
    switch (static_cast<GW::Constants::ItemType>(type)) {
        case GW::Constants::ItemType::Trophy:
            return GetRarity() == GW::Constants::Rarity::White && info_string && is_material_salvageable;
        case GW::Constants::ItemType::Salvage:
        case GW::Constants::ItemType::CC_Shards:
            return true;
        case GW::Constants::ItemType::Materials_Zcoins:
            return is_material_salvageable != 0;
        default:
            break;
    }
    if (IsWeapon() || IsArmor()) {
        return true;
    }
    return false;
}

bool InventoryManager::Item::IsWeapon() const
{
    switch (static_cast<GW::Constants::ItemType>(type)) {
        case GW::Constants::ItemType::Axe:
        case GW::Constants::ItemType::Sword:
        case GW::Constants::ItemType::Shield:
        case GW::Constants::ItemType::Scythe:
        case GW::Constants::ItemType::Bow:
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Staff:
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Daggers:
        case GW::Constants::ItemType::Hammer:
        case GW::Constants::ItemType::Spear:
            return true;
        default:
            return false;
    }
}

bool InventoryManager::Item::IsArmor() const
{
    switch (static_cast<GW::Constants::ItemType>(type)) {
        case GW::Constants::ItemType::Headpiece:
        case GW::Constants::ItemType::Chestpiece:
        case GW::Constants::ItemType::Leggings:
        case GW::Constants::ItemType::Boots:
        case GW::Constants::ItemType::Gloves:
            return true;
        default:
            return false;
    }
}

bool InventoryManager::Item::IsHiddenFromMerchants() const
{
    if (hide_unsellable_items && !value) {
        return true;
    }
    if (hide_weapon_sets_and_customized_items && (customized || equipped)) {
        return true;
    }
    if (hide_from_merchant_items.contains(model_id)) {
        return true;
    }
    if (hide_golds_from_merchant && GetRarity() == GW::Constants::Rarity::Gold) {
        return true;
    }
    return false;
}

GW::ItemModifier* InventoryManager::Item::GetModifier(const uint32_t identifier) const
{
    for (size_t i = 0; i < mod_struct_size; i++) {
        GW::ItemModifier* mod = &mod_struct[i];
        if (mod->identifier() == identifier) {
            return mod;
        }
    }
    return nullptr;
}

// InventoryManager::Item definitions

uint32_t InventoryManager::Item::GetUses() const
{
    const GW::ItemModifier* mod = GetModifier(0x2458);
    return (mod ? mod->arg2() : 1) * quantity;
}

bool InventoryManager::Item::IsSalvageKit() const
{
    return IsLesserKit() || IsExpertSalvageKit(); // || IsPerfectSalvageKit();
}

bool InventoryManager::Item::IsTome() const
{
    const GW::ItemModifier* mod = GetModifier(0x2788);
    const uint32_t use_id = mod ? mod->arg() : 0;
    return use_id > 15 && use_id < 36;
}

bool InventoryManager::Item::IsIdentificationKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 1;
}

bool InventoryManager::Item::IsLesserKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 3;
}

bool InventoryManager::Item::IsExpertSalvageKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 2;
}

bool InventoryManager::Item::IsPerfectSalvageKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 6;
}

bool InventoryManager::Item::IsRareMaterial() const
{
    const GW::ItemModifier* mod = GetModifier(0x2508);
    return mod && mod->arg1() > 11;
}
bool InventoryManager::Item::IsInventoryItem() const
{
    return bag && (bag->IsInventoryBag() || bag->bag_type == GW::Constants::BagType::Equipped);
}
bool InventoryManager::Item::IsStorageItem() const
{
    return bag && (bag->IsStorageBag() || bag->IsMaterialStorage());
}

GW::Constants::Rarity InventoryManager::Item::GetRarity() const
{
    return GW::Items::GetRarity(this);
}

bool PendingItem::set(const InventoryManager::Item* item)
{
    item_id = 0;
    if (!item || !item->item_id) {
        return false;
    }
    item_id = item->item_id;
    slot = item->slot;
    quantity = item->quantity;
    uses = item->GetUses();
    bag = item->bag ? item->bag->bag_id() : GW::Constants::Bag::None;
    name->reset(item->complete_name_enc ? item->complete_name_enc : item->name_enc);
    single_item_name->reset(item->single_item_name);
    // NB: This doesn't work for inscriptions; gww doesn't have a page per inscription.
    wiki_name->reset(ItemDescriptionHandler::GetItemEncNameWithoutMods(item).c_str());
    wiki_name->language(GW::Constants::Language::English);
    const auto plural_item_enc = std::format(L"\xa35\x101\x100\x10a{}\x1", item->name_enc);
    plural_item_name->reset(plural_item_enc.c_str());
    desc->reset(ItemDescriptionHandler::GetItemDescription(item).c_str());
    return true;
}

InventoryManager::Item* PendingItem::item() const
{
    if (!item_id) {
        return nullptr;
    }
    InventoryManager::Item* item = nullptr;
    if (bag == GW::Constants::Bag::None) {
        // i.e. merchant item
        item = static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));
    }
    else {
        item = static_cast<InventoryManager::Item*>(GW::Items::GetItemBySlot(GW::Items::GetBag(bag), slot + 1));
    }
    return item && item->item_id == item_id ? item : nullptr;
}

InventoryManager::Item* PendingTransaction::item() const
{
    return static_cast<InventoryManager::Item*>(GW::Items::GetItemById(item_id));
}

bool PendingTransaction::selling()
{
    return type == GW::Merchant::TransactionType::MerchantSell || type == GW::Merchant::TransactionType::TraderSell;
}

void PendingItem::PluralEncString::sanitise()
{
    if (sanitised) {
        return;
    }
    EncString::sanitise();
    if (sanitised) {
        decoded_ws = decoded_ws.substr(2);
    }
}
