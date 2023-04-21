#include "stdafx.h"

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameEntities/Agent.h>

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


namespace {
    ImVec4 ItemBlue = ImColor(153, 238, 255).Value;
    ImVec4 ItemPurple = ImColor(187, 137, 237).Value;
    ImVec4 ItemGold = ImColor(255, 204, 86).Value;

    const char* bag_names[5] = {
        "None",
        "Backpack",
        "Belt Pouch",
        "Bag 1",
        "Bag 2"
    };

    bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }
    std::unordered_map<uint32_t, std::pair<uint16_t,clock_t>> pending_moves; // { bag_idx | slot, { quantity_to_move,move_started_at} }
    uint16_t get_pending_move(uint32_t bag_idx, uint32_t slot) {
        const uint32_t bag_slot = (uint32_t)bag_idx << 16 | slot;
        const auto found = pending_moves.find(bag_slot);
        if (found == pending_moves.end())
            return 0;
        if (TIMER_DIFF(found->second.second) > 3000) // 3 second timeout incase of hanging moves
            return 0;
        return found->second.first;
    }
    void set_pending_move(uint32_t bag_idx, uint32_t slot, uint16_t quantity_to_move) {
        const uint32_t bag_slot = bag_idx << 16 | slot;
        pending_moves[bag_slot] = { (uint16_t)(get_pending_move(bag_idx, slot) + quantity_to_move), TIMER_INIT() };
    }
    void clear_pending_move(GW::Constants::Bag bag_idx, uint32_t slot) {
        const uint32_t bag_slot = (uint32_t)bag_idx << 16 | slot;
        pending_moves.erase(bag_slot);
    }
    void clear_pending_move(uint32_t item_id) {
        const auto item = GW::Items::GetItemById(item_id);
        if (item && item->bag) {
            return clear_pending_move(item->bag->bag_id, item->slot);
        }
    }

    // GW Client doesn't actually know max material storage size for the account.
// We can make a guess by checking how many materials are currently in storage.
    uint16_t MaxSlotSize(GW::Constants::Bag bag_idx) {
        uint16_t slot_size = 250u;
        switch (bag_idx) {
        case GW::Constants::Bag::Material_Storage:
            GW::Bag* bag = GW::Items::GetBag(bag_idx);
            if (!bag || !bag->items.valid() || !bag->items_count)
                return slot_size;
            const GW::Item* b_item = nullptr;
            for (size_t i = GW::Constants::MaterialSlot::Bone; i < GW::Constants::MaterialSlot::N_MATS; i++) {
                b_item = bag->items[i];
                if (!b_item || b_item->quantity <= slot_size)
                    continue;
                while (b_item->quantity > slot_size) {
                    slot_size += 250u;
                }
            }
            break;
        }
        return slot_size;
    }
    uint16_t MaxSlotSize(uint32_t bag_idx) {
        return MaxSlotSize(static_cast<GW::Constants::Bag>(bag_idx));
    }

    uint16_t move_materials_to_storage(GW::Item* item) {
        ASSERT(item && item->quantity);
        ASSERT(item->GetIsMaterial());

        const int islot = GW::Items::GetMaterialSlot(item);
        if (islot < 0 || (int)GW::Constants::N_MATS <= islot) return 0;
        const uint32_t slot = static_cast<uint32_t>(islot);
        const uint16_t max_in_slot = MaxSlotSize(GW::Constants::Bag::Material_Storage);
        uint16_t available = max_in_slot;
        const GW::Item* b_item = GW::Items::GetItemBySlot(GW::Constants::Bag::Material_Storage, slot + 1);
        if (b_item) {
            if (b_item->quantity >= max_in_slot)
                return 0;
            available -= b_item->quantity;
        }
        const uint16_t pending_move = get_pending_move((uint32_t)GW::Constants::Bag::Material_Storage, slot);
        available -= pending_move;
        const uint16_t will_move = std::min<uint16_t>(item->quantity, available);
        if (will_move) {
            set_pending_move((uint32_t)GW::Constants::Bag::Material_Storage, slot, will_move);
            GW::Items::MoveItem(item, GW::Constants::Bag::Material_Storage, slot, will_move);
        }
        return will_move;
    }

    // From bag_first to bag_last (included) i.e. [bag_first, bag_last]
    // Returns the amount moved
    uint16_t complete_existing_stack(GW::Item* item, size_t bag_first, size_t bag_last, uint16_t quantity = 1000u) {
        if (!item->GetIsStackable()) return 0;
        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        for (size_t bag_i = bag_first; bag_i <= bag_last; bag_i++) {
            GW::Bag* bag = GW::Items::GetBag(bag_i);
            if (!bag) continue;
            const uint16_t max_slot_size = MaxSlotSize(bag_i);
            size_t slot = bag->find2(item);
            while (slot != GW::Bag::npos) {
                const GW::Item* b_item = bag->items[slot];
                // b_item can be null in the case of birthday present for instance.

                if (b_item != nullptr && b_item->quantity < max_slot_size) {
                    uint16_t availaible = max_slot_size - get_pending_move(bag_i, slot) - b_item->quantity;
                    const uint16_t will_move = std::min<uint16_t>(availaible, remaining);
                    if (will_move > 0) {
                        GW::Items::MoveItem(item, b_item, will_move);
                        set_pending_move(bag_i, slot, will_move);
                        remaining -= will_move;
                    }
                    if (remaining == 0)
                        return to_move;
                }
                slot = bag->find2(item, slot + 1);
            }
        }
        return to_move - remaining;
    }

    uint16_t move_to_first_empty_slot(GW::Item* item, size_t bag_first, size_t bag_last, uint16_t quantity = 1000u) {
        quantity = std::min<uint16_t>(item->quantity, quantity);
        for (size_t bag_i = bag_first; bag_i <= bag_last; bag_i++) {
            GW::Bag* bag = GW::Items::GetBag(bag_i);
            if (!bag) continue;
            const uint16_t max_slot_size = MaxSlotSize(bag_i);
            size_t slot = bag->find1(0);
            // The reason why we test if the slot has no item is because birthday present have ModelId == 0
            while (slot != GW::Bag::npos) {
                if (bag->items[slot] == nullptr) {
                    uint16_t available = max_slot_size - get_pending_move(bag_i, slot);
                    const uint16_t will_move = std::min<uint16_t>(quantity, available);
                    if (will_move > 0) {
                        set_pending_move(bag_i, slot, will_move);
                        GW::Items::MoveItem(item, bag, slot, will_move);
                        return will_move;
                    }
                }
                slot = bag->find1(0, slot + 1);
            }
        }
        return 0;
    }

    uint16_t move_item_to_storage_page(GW::Item* item, size_t page, uint16_t quantity = 1000u) {
        ASSERT(item && item->quantity);
        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        if (page == static_cast<size_t>(GW::Constants::StoragePane::Material_Storage)) {
            if (!item->GetIsMaterial())
                return 0;
            remaining -= move_materials_to_storage(item);
        }

        if (static_cast<size_t>(GW::Constants::StoragePane::Storage_14) < page)
            return item->quantity - remaining;

        const size_t storage1 = static_cast<size_t>(GW::Constants::Bag::Storage_1);
        const size_t bag_index = storage1 + page;
        ASSERT(GW::Items::GetBag(bag_index));

        // if the item is stackable we try to complete stack that already exist in the current storage page
        if (remaining)
            remaining -= complete_existing_stack(item, bag_index, bag_index, remaining);
        if (remaining)
            remaining -= move_to_first_empty_slot(item, bag_index, bag_index, remaining);
        return to_move - remaining;
    }

    uint16_t move_item_to_storage(GW::Item* item, uint16_t quantity = 1000u) {
        ASSERT(item && item->quantity);
        const uint16_t to_move = std::min<uint16_t>(item->quantity,quantity);
        uint16_t remaining = to_move;
        const bool is_storage_open = GW::Items::GetIsStorageOpen();
        if (remaining && is_storage_open && item->GetIsMaterial() && GameSettings::GetSettingBool("move_materials_to_current_storage_pane")) {
            const size_t current_storage = GW::Items::GetStoragePage();
            remaining -= move_item_to_storage_page(item, current_storage, remaining);
        }
        if(remaining && item->GetIsMaterial())
            remaining -= move_materials_to_storage(item);

        if (remaining && is_storage_open && GameSettings::GetSettingBool("move_item_to_current_storage_pane")) {
            const size_t current_storage = GW::Items::GetStoragePage();
            remaining -= move_item_to_storage_page(item, current_storage, remaining);
        }


        const size_t storage1 = static_cast<size_t>(GW::Constants::Bag::Storage_1);
        const size_t storage14 = static_cast<size_t>(GW::Constants::Bag::Storage_14);

        if (remaining)
            remaining -= complete_existing_stack(item, storage1, storage14, remaining);
        while (remaining) {
            const uint16_t moved = move_to_first_empty_slot(item, storage1, storage14, remaining);
            if (!moved)
                break;
            remaining -= moved;
        }

        return to_move - remaining;
    }

    uint16_t move_item_to_inventory(GW::Item* item, uint16_t quantity = 1000u) {
        ASSERT(item && item->quantity);

        const size_t backpack = static_cast<size_t>(GW::Constants::Bag::Backpack);
        const size_t bag2 = static_cast<size_t>(GW::Constants::Bag::Bag_2);

        const uint16_t to_move = std::min<uint16_t>(item->quantity, quantity);
        uint16_t remaining = to_move;
        // If item is stackable, try to complete similar stack
        remaining -= complete_existing_stack(item, backpack, bag2, remaining);
        while (remaining) {
            const uint16_t moved = move_to_first_empty_slot(item, backpack, bag2, remaining);
            if (!moved)
                break;
            remaining -= moved;
        }

        return to_move - remaining;
    }

    std::vector<InventoryManager::Item*> filter_items(GW::Constants::Bag from, GW::Constants::Bag to, const std::function<bool(InventoryManager::Item*)> cmp, uint32_t limit = 0) {
        std::vector<InventoryManager::Item*> out;
        const size_t bag_first = static_cast<size_t>(from);
        const size_t bag_last = static_cast<size_t>(to);
        for (size_t bag_i = bag_first; bag_i <= bag_last; bag_i++) {
            GW::Bag* bag = GW::Items::GetBag(bag_i);
            if (!bag) continue;
            for (size_t slot = 0; slot < bag->items.size(); slot++) {
                const auto item = static_cast<InventoryManager::Item*>(bag->items[slot]);
                if (!cmp(item))
                    continue;
                out.push_back(item);
                if (out.size() == limit)
                    return out;
            }
        }
        return out;
    }
    uint16_t count_items(GW::Constants::Bag from, GW::Constants::Bag to, std::function<bool(InventoryManager::Item*)> cmp) {
        const auto items = filter_items(from, to, std::move(cmp));
        uint16_t out = 0;
        for (const auto item : items) {
            out += item->quantity;
        }
        return out;
    }

    const GW::Array<GW::TradeContext::Item>* GetPlayerTradeItems() {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
            return nullptr;
        const GW::TradeContext* c = GW::GetGameContext()->trade;
        if (!c || !c->GetIsTradeInitiated())
            return nullptr;
        return &c->player.items;
    }

    void store_all_materials() {
        const std::vector<InventoryManager::Item*> items = filter_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, [](GW::Item* item) {
            return item && item->GetIsMaterial();
        });
        for (const auto& item : items) {
            move_item_to_storage(item);
        }
        pending_moves.clear();
    }
    void store_all_tomes() {
        const std::vector<InventoryManager::Item*> items = filter_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, [](InventoryManager::Item* item) {
            return item && item->IsTome();
            });
        for (const auto& item : items) {
            move_item_to_storage(item);
        }
        pending_moves.clear();
    }
    void move_all_item(InventoryManager::Item* like_item) {
        ASSERT(like_item && like_item->bag);
        const auto is_same_item = [like_item](InventoryManager::Item* cmp) {
            return cmp && InventoryManager::IsSameItem(like_item, cmp);
        };
        if (like_item->bag->IsInventoryBag()) {
            const std::vector<InventoryManager::Item*> items = filter_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, is_same_item);
            for (const auto& item : items) {
                move_item_to_storage(item);
            }
        }
        else {
            const std::vector<InventoryManager::Item*> items = filter_items(GW::Constants::Bag::Material_Storage, GW::Constants::Bag::Storage_14, is_same_item);
            for (const auto& item : items) {
                move_item_to_inventory(item);
            }
        }
        pending_moves.clear();
    }
    void store_all_upgrades() {
        const std::vector<InventoryManager::Item*> items = filter_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, [](InventoryManager::Item* item) {
            return item && item->type == 8;
            });
        for (const auto& item : items) {
            move_item_to_storage(item);
        }
        pending_moves.clear();
    }


    // Move a whole stack into/out of storage
    uint16_t move_item(GW::Item* item, uint16_t quantity = 1000u) {
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


        // @Fix:
        //  There is a bug in gw that doesn't "save" if the material storage
        //  (or anniversary storage in the case when the player bought all other storage)
        //  so we cannot know if they are the storage selected.
        //  Sol: The solution is to patch the value 7 -> 9 at 0040E851 (EB 20 33 C0 BE 06 [-5])
        // @Cleanup: Bad
        if (item->model_file_id == 0x0002f301) {
            Log::Error("Ctrl+click doesn't work with birthday presents yet");
            return 0;
        }
        const bool is_inventory_item = item->bag->IsInventoryBag();
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

    uint32_t requesting_quote_type = 0;

    GW::UI::WindowPosition* inventory_bags_window_position = nullptr;

    typedef void(__fastcall* AddItemRowToWindow_pt)(void* ecx, void* edx, uint32_t frame, uint32_t item_id);
    AddItemRowToWindow_pt AddItemRowToWindow_Func = nullptr;
    AddItemRowToWindow_pt RetAddItemRowToWindow = nullptr;

    // x, y, z, w; Top, right, bottom, left
    struct Rect {
        float top, right, bottom, left;
        bool contains(const GW::Vec2f& pos) {
            return (pos.x > left
                && pos.x < right
                && pos.y > top
                && pos.y < bottom);
        }
        Rect() { top = right = bottom = left = 0.0f; }
        Rect(float _x, float _y, float _z, float _w) { top = _x; right = _y; bottom = _z; left = _w; }
    };

    Rect& operator*=(Rect& lhs, float rhs) {
        lhs.top *= rhs;
        lhs.bottom *= rhs;
        lhs.left *= rhs;
        lhs.right *= rhs;
        return lhs;
    }

    Rect operator*(float lhs, Rect rhs) {
        rhs *= lhs;
        return rhs;
    }


    const Rect GetGWWindowPadding() {
        Rect gw_window_padding = { 33.f, 14.f, 14.f, 18.f };
        gw_window_padding *= GuiUtils::GetGWScaleMultiplier();
        return gw_window_padding;
    }
    // Size of a single inv slot on-screen (includes 1px right padding, 2px bottom padding)
    const GW::Vec2f GetInventorySlotSize() {
        GW::Vec2f inventory_slot_size = { 41.f, 50.f };
        inventory_slot_size *= GuiUtils::GetGWScaleMultiplier();
        return inventory_slot_size;
    }

    // Width of scrollbar on gw windows.
    const float gw_scrollbar_width = 20.f;
    uint32_t pending_item_move_for_trade = 0;

    bool GetMousePosition(GW::Vec2f& pos) {
        const ImVec2 imgui_pos = ImGui::GetIO().MousePos;
        pos.x = (float)imgui_pos.x;
        pos.y = (float)imgui_pos.y;
        return true;
    }

    bool IsTradeWindowOpen() {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
            return false;
        const GW::TradeContext* c = GW::GetGameContext()->trade;
        return c && c->GetIsTradeInitiated();
    }

    GW::HookEntry on_offer_item_hook;
    clock_t check_context_menu_position = 0;
    DWORD is_right_clicking = 0;
    DWORD mouse_moved = 0;

    struct PreMoveItemStruct {
        uint32_t item_id = 0;
        uint32_t bag_id = 0;
        uint32_t slot = 0;
        bool prompt_split_stack = false;
    };

    void prompt_split_stack(GW::Item* item) {
        PreMoveItemStruct details;
        details.item_id = item->item_id;
        // Doesn't matter where the prompt is asking to move to, as long as its not the same slot; we're going to override later.
        details.bag_id = static_cast<uint32_t>(GW::Constants::Bag::Backpack);// empty_bag_id;
        details.slot = 0;// empty_slot;
        if (item->bag->index == details.bag_id && item->slot == details.slot)
            details.slot++;
        details.prompt_split_stack = true;
        GW::UI::SendUIMessage(GW::UI::UIMessage::kMoveItem, &details);
        //OnPreMoveItem(7, &details);
        InventoryManager::Instance().stack_prompt_item_id = item->item_id;
    }


    int CountInventoryBagSlots() {
        int slots = 0;
        const GW::Bag* bag = nullptr;
        for (size_t bag_index = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_index < static_cast<size_t>(GW::Constants::Bag::Equipment_Pack); bag_index++) {
            bag = GW::Items::GetBag(bag_index);
            if (!bag) continue;
            slots += bag->items.m_size;
        }
        return slots;
    }

    uint32_t right_clicked_item = 0;

} // namespace
void InventoryManager::OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
    auto& instance = Instance();
    switch (message_id) {
    case GW::UI::UIMessage::kItemUpdated: {
        clear_pending_move((uint32_t)wparam);
    } break;
        // About to request a quote for an item
    case GW::UI::UIMessage::kSendMerchantRequestQuote: {
        requesting_quote_type = 0;
        if (instance.pending_transaction.in_progress() || !ImGui::IsKeyDown(ImGuiKey_ModCtrl) || MaterialsWindow::Instance().GetIsInProgress()) {
            return;
        }
        requesting_quote_type = *(uint32_t*)wparam;
    } break;
        // About to move an item
    case GW::UI::UIMessage::kSendMoveItem: {
        const uint32_t* packet = (uint32_t*)wparam;
        const uint32_t item_id = packet[0];
        const uint32_t quantity = packet[1];

        if (item_id != instance.stack_prompt_item_id) {
            instance.stack_prompt_item_id = 0;
            return;
        }
        instance.stack_prompt_item_id = 0;
        status->blocked = true;
        move_item(GW::Items::GetItemById(item_id), (uint16_t)quantity);
    } break;
        // Quote for item has been received
    case GW::UI::UIMessage::kQuotedItemPrice: {
        if (!requesting_quote_type)
            return;
        if (instance.pending_transaction.in_progress())
            return;
        instance.pending_cancel_transaction = true;
        const uint32_t* packet = (uint32_t*)wparam;
        const uint32_t item_id = packet[0];
        const uint32_t price = packet[1];
        const GW::Item* requested_item = GW::Items::GetItemById(item_id);
        if (!requested_item)
            return;
        instance.pending_transaction.type = requesting_quote_type;
        instance.pending_transaction.item_id = item_id;
        instance.pending_transaction.price = price;
        requesting_quote_type = 0;
        instance.show_transact_quantity_popup = true;
    } break;
        // Map left; cancel all actions
    case GW::UI::UIMessage::kMapChange: {
        instance.CancelAll();
    } break;
        // Item moved; clear prompt
    case GW::UI::UIMessage::kMoveItem: {
        instance.stack_prompt_item_id = 0;
    } break;
    }
}

void InventoryManager::Initialize() {
    ToolboxUIElement::Initialize();
    GW::Items::RegisterItemClickCallback(&ItemClick_Entry, ItemClickCallback);

    GW::UI::UIMessage message_id_hooks[] = {
        GW::UI::UIMessage::kSendMoveItem,
        GW::UI::UIMessage::kSendMerchantRequestQuote,
        GW::UI::UIMessage::kQuotedItemPrice,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMoveItem,
        GW::UI::UIMessage::kSendUseItem,
        GW::UI::UIMessage::kItemUpdated
    };
    for (const auto message_id : message_id_hooks) {
        GW::UI::RegisterUIMessageCallback(&ItemClick_Entry, message_id, OnUIMessage);
    }

    GW::Trade::RegisterOfferItemCallback(&on_offer_item_hook, OnOfferTradeItem);

    inventory_bags_window_position = GW::UI::GetWindowPosition(GW::UI::WindowID::WindowID_InventoryBags);

    AddItemRowToWindow_Func = reinterpret_cast<AddItemRowToWindow_pt>(GW::Scanner::Find(
        "\x83\xc4\x04\x80\x78\x04\x06\x0f\x84\xd3\x00\x00\x00\x6a\x02\xff\x37", nullptr, -0x10));
    if (AddItemRowToWindow_Func) {
        GW::Hook::CreateHook(AddItemRowToWindow_Func, OnAddItemToWindow, reinterpret_cast<void**>(&RetAddItemRowToWindow));
        GW::Hook::EnableHooks(AddItemRowToWindow_Func);
    }
}

void InventoryManager::Terminate()
{
    ToolboxUIElement::Terminate();
    ClearPotentialItems();
}

// Hide unsellable items from merchant
void InventoryManager::OnAddItemToWindow(void* ecx, void* edx, uint32_t frame, uint32_t item_id) {
    GW::Hook::EnterHook();
    const auto item = GW::Items::GetItemById(item_id);
    bool block_item = false;
    if (item) {
        if (Instance().hide_unsellable_items && !item->value) {
            block_item = true;
        }
        if (Instance().hide_from_merchant_items.contains(item->model_id)) {
            block_item = true;
        }
    }

    if (!block_item) {
        RetAddItemRowToWindow(ecx, edx, frame, item_id);
    }
    GW::Hook::LeaveHook();
}
void InventoryManager::OnOfferTradeItem(GW::HookStatus* status, uint32_t item_id, uint32_t quantity) {
    if (ImGui::IsKeyDown(ImGuiKey_ModShift) || !Instance().trade_whole_stacks)
        return; // Default behaviour; prompt user for amount
    if (quantity == 0) {
        const GW::Item* item = GW::Items::GetItemById(item_id);
        if (item && item->quantity > 1) {
            status->blocked = true;
            GW::Trade::OfferItem(item_id, item->quantity);
        }
    }
}
bool InventoryManager::WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
    // GW Deliberately makes a WM_MOUSEMOVE event right after right button is pressed.
    // Does this to "hide" the cursor when looking around.
    switch (message) {
    case WM_INPUT: {
        if (!(is_right_clicking && !mouse_moved && GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT && lParam))
            break; // Not raw input
        UINT dwSize = sizeof(RAWINPUT);
        BYTE lpb[sizeof(RAWINPUT)];
        ASSERT(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize);

        const RAWINPUT* raw = (RAWINPUT*)lpb;
        if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 && raw->data.mouse.lLastX && raw->data.mouse.lLastY) {
            // If its a relative mouse move, process the action
            mouse_moved = 1;
        }
    } break;

    case WM_RBUTTONDOWN: {
        is_right_clicking = 1;
        mouse_moved = 0;
        const auto item = GW::Items::GetHoveredItem();
        right_clicked_item = item ? item->item_id : 0;
    } break;
    case WM_RBUTTONUP:
        // 100ms delay allows gw to reset cursor position, otherwise the context menu will show in the middle of the screen
        check_context_menu_position = mouse_moved != 1 ? TIMER_INIT() + 50 : 0;
        is_right_clicking = mouse_moved = 0;
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
            const Item* item = static_cast<Item*>(GW::Items::GetHoveredItem());
            GW::Bag* bag = item ? item->bag : nullptr;
            if (!bag)
                break;
            if (static_cast<GW::Constants::Bag>(bag->index + 1) == GW::Constants::Bag::Material_Storage) {
                // Item in material storage pane clicked - spoof a click event
                GW::HookStatus status;
                ItemClickCallback(&status, message == WM_LBUTTONDOWN ? 7 : 8, item->slot, bag);
                return false;
            }
        } break;
    }
    return false;
}
void InventoryManager::SaveSettings(ToolboxIni* ini) {
    ToolboxUIElement::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(only_use_superior_salvage_kits), only_use_superior_salvage_kits);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_rare_mats), salvage_rare_mats);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_backpack), bags_to_salvage_from[GW::Constants::Bag::Backpack]);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_belt_pouch), bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_bag_1), bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
    ini->SetBoolValue(Name(), VAR_NAME(salvage_from_bag_2), bags_to_salvage_from[GW::Constants::Bag::Bag_2]);
    ini->SetBoolValue(Name(), VAR_NAME(trade_whole_stacks), trade_whole_stacks);
    ini->SetBoolValue(Name(), VAR_NAME(wiki_link_on_context_menu), wiki_link_on_context_menu);
    ini->SetBoolValue(Name(), VAR_NAME(hide_unsellable_items), hide_unsellable_items);
    ini->SetBoolValue(Name(), VAR_NAME(change_secondary_for_tome), change_secondary_for_tome);

    GuiUtils::MapToIni(ini, Name(), VAR_NAME(hide_from_merchant_items), hide_from_merchant_items);
}
void InventoryManager::LoadSettings(ToolboxIni* ini) {
    ToolboxUIElement::LoadSettings(ini);
    only_use_superior_salvage_kits = ini->GetBoolValue(Name(), VAR_NAME(only_use_superior_salvage_kits), only_use_superior_salvage_kits);
    salvage_rare_mats = ini->GetBoolValue(Name(), VAR_NAME(salvage_rare_mats), salvage_rare_mats);
    bags_to_salvage_from[GW::Constants::Bag::Backpack] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_backpack), bags_to_salvage_from[GW::Constants::Bag::Backpack]);
    bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_belt_pouch), bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
    bags_to_salvage_from[GW::Constants::Bag::Bag_1] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_bag_1), bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
    bags_to_salvage_from[GW::Constants::Bag::Bag_2] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_bag_2), bags_to_salvage_from[GW::Constants::Bag::Bag_2]);
    trade_whole_stacks = ini->GetBoolValue(Name(), VAR_NAME(trade_whole_stacks), trade_whole_stacks);
    wiki_link_on_context_menu = ini->GetBoolValue(Name(), VAR_NAME(wiki_link_on_context_menu), wiki_link_on_context_menu);
    hide_unsellable_items = ini->GetBoolValue(Name(), VAR_NAME(hide_unsellable_items), hide_unsellable_items);
    change_secondary_for_tome = ini->GetBoolValue(Name(), VAR_NAME(change_secondary_for_tome), change_secondary_for_tome);

    hide_from_merchant_items =
        GuiUtils::IniToMap<std::map<uint32_t, std::string>>(ini, Name(), VAR_NAME(hide_from_merchant_items));
}
void InventoryManager::ClearSalvageSession(GW::HookStatus* status, void *)
{
    if (status)
        status->blocked = true;
    Instance().current_salvage_session.salvage_item_id = 0;
}
void InventoryManager::CancelSalvage()
{
    DetachSalvageListeners();
    ClearSalvageSession();
    potential_salvage_all_items.clear();
    is_salvaging = has_prompted_salvage = is_salvaging_all = false;
    pending_salvage_item.item_id = 0;
    pending_salvage_kit.item_id = 0;
    salvage_all_type = SalvageAllType::None;
    salvaged_count = 0;
    context_item.item_id = 0;
    pending_cancel_salvage = false;
}
void InventoryManager::CancelTransaction()
{
    DetachTransactionListeners();
    ClearTransactionSession();
    is_transacting = has_prompted_transaction = false;
    pending_transaction_amount = 0;
    pending_cancel_transaction = false;
    pending_transaction.retries = 0;
}
void InventoryManager::ClearTransactionSession(GW::HookStatus* status, void*)
{
    if (status)
        status->blocked = true;
    Instance().pending_transaction.setState(PendingTransaction::State::None);
}
void InventoryManager::CancelIdentify()
{
    is_identifying = is_identifying_all = false;
    pending_identify_item.item_id = 0;
    pending_identify_kit.item_id = 0;
    identify_all_type = IdentifyAllType::None;
    identified_count = 0;
    context_item.item_id = 0;
}
void InventoryManager::CancelAll()
{
    ClearPotentialItems();
    CancelSalvage();
    CancelIdentify();
    CancelTransaction();
}
void InventoryManager::AttachSalvageListeners() {
    if (salvage_listeners_attached)
        return;
    GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GW::Packet::StoC::SalvageSessionCancel::STATIC_HEADER, &ClearSalvageSession);
    GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GW::Packet::StoC::SalvageSessionDone::STATIC_HEADER, &ClearSalvageSession);
    GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GW::Packet::StoC::SalvageSessionItemKept::STATIC_HEADER, &ClearSalvageSession);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SalvageSessionSuccess>(
        &salvage_hook_entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::SalvageSessionSuccess*) {
            ClearSalvageSession(status);
            GW::Items::SalvageSessionDone();
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SalvageSession>(
        &salvage_hook_entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::SalvageSession* packet) {
            current_salvage_session = *packet;
            status->blocked = true;
        });
    salvage_listeners_attached = true;
}
void InventoryManager::AttachTransactionListeners() {
    if (transaction_listeners_attached)
        return;
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TransactionDone>(&salvage_hook_entry, [this](GW::HookStatus* status, GW::Packet::StoC::TransactionDone*) {
        pending_transaction_amount--;
        status->blocked = true;
        //Log::Info("Transacted item; %d to go", pending_transaction_amount);
        Instance().pending_transaction.setState(PendingTransaction::State::Pending);
        });
    GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GAME_SMSG_TRANSACTION_REJECT, [this](GW::HookStatus* status, void*) {
        if (!pending_transaction.in_progress())
            return;
        pending_cancel_transaction = true;
        Log::WarningW(L"Trader rejected transaction");
        status->blocked = true;
        return;
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&salvage_hook_entry, [this](GW::HookStatus* status, GW::Packet::StoC::QuotedItemPrice* packet) {
        if (pending_transaction.item_id != packet->itemid) {
            pending_cancel_transaction = true;
            return;
        }
        pending_transaction.price = packet->price;
        pending_transaction.setState(PendingTransaction::State::Quoted);
        status->blocked = true;
        });
    transaction_listeners_attached = true;
}
void InventoryManager::DetachTransactionListeners() {
    if (!transaction_listeners_attached)
        return;
    GW::StoC::RemoveCallback(GW::Packet::StoC::TransactionDone::STATIC_HEADER, &salvage_hook_entry);
    GW::StoC::RemoveCallback(GAME_SMSG_TRANSACTION_REJECT, &salvage_hook_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::QuotedItemPrice::STATIC_HEADER, &salvage_hook_entry);
    transaction_listeners_attached = false;
}
void InventoryManager::DetachSalvageListeners() {
    if (!salvage_listeners_attached)
        return;
    GW::StoC::RemoveCallback(GW::Packet::StoC::SalvageSessionCancel::STATIC_HEADER, &salvage_hook_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::SalvageSessionDone::STATIC_HEADER, &salvage_hook_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::SalvageSession::STATIC_HEADER, &salvage_hook_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::SalvageSessionItemKept::STATIC_HEADER, &salvage_hook_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::SalvageSessionSuccess::STATIC_HEADER, &salvage_hook_entry);
    salvage_listeners_attached = false;
}
void InventoryManager::IdentifyAll(IdentifyAllType type) {
    if (type != identify_all_type) {
        CancelIdentify();
        is_identifying_all = true;
        identify_all_type = type;
    }
    if (!is_identifying_all || is_identifying)
        return;
    // Get next item to identify
    Item* unid = GetNextUnidentifiedItem();
    if (!unid) {
        //Log::Info("Identified %d items", identified_count);
        CancelIdentify();
        return;
    }
    Item *kit = context_item.item();
    if (!kit || !kit->IsIdentificationKit()) {
        CancelIdentify();
        Log::Warning("The identification kit was consumed");
        return;
    }
    Identify(unid, kit);
}
void InventoryManager::ContinueIdentify() {
    is_identifying = false;
    if (!IsMapReady()) {
        CancelIdentify();
        return;
    }
    if (pending_identify_item.item_id)
        identified_count++;
    if (is_identifying_all)
        IdentifyAll(identify_all_type);
}
void InventoryManager::ContinueSalvage() {
    is_salvaging = false;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        CancelSalvage();
        return;
    }
    if (!IsMapReady()) {
        pending_cancel_salvage = true;
    }
    const Item* current_item = pending_salvage_item.item();
    if (current_item && current_salvage_session.salvage_item_id != 0) {
        // Popup dialog for salvage; salvage materials and cycle.
        ClearSalvageSession();
        GW::Items::SalvageMaterials();
        pending_salvage_at = (clock() / CLOCKS_PER_SEC);
        is_salvaging = true;
        return;
    }
    if (pending_salvage_item.item_id)
        salvaged_count++;
    if (current_item && current_item->quantity == pending_salvage_item.quantity) {
        CancelSalvage();
        Log::Error("Salvage flagged as complete, but item still exists in slot %d/%d", current_item->bag->index+1, current_item->slot+1);
        return;
    }
    if (pending_cancel_salvage) {
        CancelSalvage();
        return;
    }
    if (is_salvaging_all)
        SalvageAll(salvage_all_type);
}
void InventoryManager::ContinueTransaction() {
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
            Log::Info("Transaction complete");
            CancelTransaction();
            return;
        }
        Log::Log("PendingTransaction pending, ask for quote\n");
        pending_transaction.setState(PendingTransaction::State::Quoting);
        auto packet = pending_transaction.quote();
        AttachTransactionListeners();
        GW::Merchant::RequestQuote((GW::Merchant::TransactionType)packet.type, { 0, packet.item_give_count,packet.item_give_ids }, { 0, packet.item_recv_count,packet.item_recv_ids });
    }break;
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
            return;
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
        auto packet = pending_transaction.transact();
        AttachTransactionListeners();
        GW::Merchant::TransactItems((GW::Merchant::TransactionType)packet.type,
            packet.gold_give, { packet.item_give_count, packet.item_give_ids, packet.item_give_quantities },
            packet.gold_recv, { packet.item_recv_count, packet.item_recv_ids, packet.item_recv_quantities });
    }break;
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
            return;
        }
        break;
    default:
        // Anything else, cancel the transaction.
        CancelTransaction();
        return;
    }
}
void InventoryManager::SalvageAll(SalvageAllType type) {
    if (type != salvage_all_type) {
        CancelSalvage();
        salvage_all_type = type;
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
    if (!is_salvaging_all || is_salvaging)
        return;
    if (pending_cancel_salvage) {
        CancelSalvage();
        return;
    }
    Item *kit = context_item.item();
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
    const auto available_slot = GetAvailableInventorySlot();
    if (!available_slot.first) {
        CancelSalvage();
        Log::Warning("No more space in inventory");
        return;
    }
    PotentialItem* ref = *potential_salvage_all_items.begin();
    if (!ref->proceed) {
        delete ref;
        potential_salvage_all_items.erase(potential_salvage_all_items.begin());
        return; // User wants to skip this item; continue to next frame.
    }
    Item* item = ref->item();
    if (!item || !item->bag || !item->bag->IsInventoryBag()) {
        delete ref;
        potential_salvage_all_items.erase(potential_salvage_all_items.begin());
        return; // Item has moved or been consumed since prompt.
    }

    Salvage(item, kit);
}
void InventoryManager::Salvage(Item* item, Item* kit) {
    if (!item || !kit)
        return;
    if (!item->IsSalvagable() || !kit->IsSalvageKit())
        return;
    if (!(pending_salvage_item.set(item) && pending_salvage_kit.set(kit)))
        return;
    AttachSalvageListeners();
    GW::Items::SalvageStart(pending_salvage_kit.item_id, pending_salvage_item.item_id);
    pending_salvage_at = (clock() / CLOCKS_PER_SEC);
    is_salvaging = true;
}
void InventoryManager::Identify(Item* item, Item* kit) {
    if (!item || !kit)
        return;
    if (item->GetIsIdentified() || !kit->IsIdentificationKit())
        return;
    if(!(pending_identify_item.set(item) && pending_identify_kit.set(kit)))
        return;
    GW::Items::IdentifyItem(pending_identify_kit.item_id, pending_identify_item.item_id);
    pending_identify_at = (clock() / CLOCKS_PER_SEC);
    is_identifying = true;
}
void InventoryManager::FetchPotentialItems() {
    Item* found = nullptr;
    if (salvage_all_type != SalvageAllType::None) {
        ClearPotentialItems();
        while ((found = GetNextUnsalvagedItem(context_item.item(), found)) != nullptr) {
            auto pi = new PotentialItem();
            pi->set(found);
            potential_salvage_all_items.push_back(pi);
        }
    }
}
InventoryManager::Item* InventoryManager::GetNextUnidentifiedItem(Item* start_after_item) {
    size_t start_bag = static_cast<size_t>(GW::Constants::Bag::Backpack);
    size_t start_position = 0;
    if (start_after_item) {
        for (size_t i = 0; i < start_after_item->bag->items.size(); i++) {
            if (start_after_item->bag->items[i] == start_after_item) {
                start_position = i + 1;
                start_bag = static_cast<size_t>(start_after_item->bag->index + 1);
                if (start_position >= start_after_item->bag->items.size()) {
                    start_position = 0;
                    start_bag++;
                }
                break;
            }
        }
    }
    const size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack);
    size_t items_found = 0;
    Item* item = nullptr;
    for (size_t bag_i = start_bag; bag_i <= end_bag; bag_i++) {
        GW::Bag* bag = GW::Items::GetBag(bag_i);
        if (!bag) continue;
        GW::ItemArray& items = bag->items;
        items_found = 0;
        for (size_t i = 0; i < items.size() && items_found < bag->items_count; i++) {
            item = static_cast<Item*>(items[i]);
            if (!item)
                continue;
            items_found++;
            if (item->GetIsIdentified())
                continue;
            if (item->IsGreen() || item->type == static_cast<uint8_t>(GW::Constants::ItemType::Minipet))
                continue;
            switch (identify_all_type) {
            case IdentifyAllType::All:
                return item;
            case IdentifyAllType::Blue:
                if (item->IsBlue())
                    return item;
                break;
            case IdentifyAllType::Purple:
                if (item->IsPurple())
                    return item;
                break;
            case IdentifyAllType::Gold:
                if (item->IsGold())
                    return item;
                break;
            default:
                break;
            }
        }
    }
    return nullptr;
}
InventoryManager::Item* InventoryManager::GetNextUnsalvagedItem(Item* kit, Item* start_after_item) {
    size_t start_bag = static_cast<size_t>(GW::Constants::Bag::Backpack);
    size_t start_position = 0;
    if (start_after_item) {
        for (size_t i = 0; i < start_after_item->bag->items.size(); i++) {
            if (start_after_item->bag->items[i] == start_after_item) {
                start_position = i + 1;
                start_bag = static_cast<size_t>(start_after_item->bag->index + 1);
                if (start_position >= start_after_item->bag->items.size()) {
                    start_position = 0;
                    start_bag++;
                }
                break;
            }
        }
    }
    const size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);
    size_t items_found = 0;
    Item* item = nullptr;
    for (size_t bag_i = start_bag; bag_i <= end_bag; bag_i++) {
        if (!bags_to_salvage_from[static_cast<GW::Constants::Bag>(bag_i)])
            continue;
        GW::Bag* bag = GW::Items::GetBag(bag_i);
        if (!bag) continue;
        GW::ItemArray& items = bag->items;
        items_found = 0;
        for (size_t i = (bag_i == start_bag ? start_position : 0); i < items.size() && items_found < bag->items_count; i++) {
            item = static_cast<Item*>(items[i]);
            if (!item)
                continue;
            items_found++;
            if (!item->value)
                continue; // No value usually means no salvage.
            if (!item->IsSalvagable())
                continue;
            if (item->IsWeaponSetItem())
                continue;
            if (item->IsRareMaterial() && !salvage_rare_mats)
                continue; // Don't salvage rare mats
            if (item->IsArmor() || item->customized)
                continue; // Don't salvage armor, or customised weapons.
            if (item->IsBlue() && !item->GetIsIdentified() && (kit && kit->IsLesserKit()))
                continue; // Note: lesser kits cant salvage blue unids - Guild Wars bug/feature
            const GW::Constants::Rarity rarity = item->GetRarity();
            switch (rarity) {
            case GW::Constants::Rarity::Gold:
                if (!item->GetIsIdentified()) continue;
                if (salvage_all_type < SalvageAllType::GoldAndLower) continue;
                return item;
            case GW::Constants::Rarity::Purple:
                if (!item->GetIsIdentified()) continue;
                if (salvage_all_type < SalvageAllType::PurpleAndLower) continue;
                return item;
            case GW::Constants::Rarity::Blue:
                if (!item->GetIsIdentified()) continue;
                if (salvage_all_type < SalvageAllType::BlueAndLower) continue;
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
std::pair<GW::Bag*, uint32_t> InventoryManager::GetAvailableInventorySlot(GW::Item* like_item) {
    GW::Item* existing_stack = GetAvailableInventoryStack(like_item, true);
    if (existing_stack)
        return { existing_stack->bag, existing_stack->slot };
    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) return { nullptr,0 };
    size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);
    Item* im_item = static_cast<Item*>(like_item);
    if(im_item && (im_item->IsWeapon() || im_item->IsArmor()))
        end_bag = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack);
    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx <= end_bag; bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items.valid()) continue;
        for (size_t slot = 0; slot < bag->items.size(); slot++) {
            if (!bag->items[slot])
                return { bag, slot };
        }
    }
    return { nullptr,0 };
}

uint16_t InventoryManager::RefillUpToQuantity(uint16_t wanted_quantity, const std::vector<uint32_t>& model_ids)
{
    uint16_t moved = 0;
    for (const auto model_id : model_ids) {
        const auto is_same_item = [model_id](const InventoryManager::Item* cmp) {
            return cmp && cmp->model_id == model_id;
        };
        const auto amount_in_inventory = count_items(GW::Constants::Bag::Backpack, GW::Constants::Bag::Equipment_Pack, is_same_item);
        if (amount_in_inventory >= wanted_quantity)
            continue; // Already got enough
        uint16_t to_move = wanted_quantity - amount_in_inventory;
        const auto amount_in_storage = count_items(GW::Constants::Bag::Material_Storage, GW::Constants::Bag::Storage_14, is_same_item);
        if (amount_in_storage < to_move) {
            // @Enhancement: Make this warning optional? Its more annoying than anything else if you're using it as a hotkey and you run out, so disabled for now.
            // Log::Warning("Only able to withdraw %d of %d items with model id %d", amount_in_inventory + amount_in_storage, wanted_quantity, model_id);
        }
        const auto storage_items = filter_items(GW::Constants::Bag::Material_Storage, GW::Constants::Bag::Storage_14, is_same_item);
        for (const auto item : storage_items) {
            const auto this_move = move_item_to_inventory(item,to_move);
            moved += this_move;
            to_move -= this_move;
            if (to_move < 1)
                break; // All items moved ok
        }
        // NB: No error messages are here because we don't know the exact reason for the failed move item - could be that another module is already transferring the item, so technically its not a problem
    }
    return moved;
}

GW::Item* InventoryManager::GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack) {
    if (!like_item || static_cast<Item*>(like_item)->IsStackable())
        return nullptr;
    GW::Item* best_item = nullptr;
    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) return best_item;
    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx < static_cast<size_t>(GW::Constants::Bag::Equipment_Pack); bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items_count || !bag->items.valid()) continue;
        for (GW::Item* item : bag->items) {
            if (!item || like_item->item_id == item->item_id || !IsSameItem(like_item, item) || item->quantity == 250)
                continue;
            if (entire_stack && 250 - item->quantity < like_item->quantity)
                continue;
            if (!best_item || item->quantity < best_item->quantity)
                best_item = item;
        }
    }
    return best_item;
}
bool InventoryManager::IsSameItem(const GW::Item* item1, const GW::Item* item2) {
    // TODO: Special cases for minipets and kits (dunno whats up with minipets, but kits have uses in mod struct)
    return item1 && item2
        && (!item1->model_id || item1->model_id == item2->model_id)
        && (!item1->model_file_id || item1->model_file_id == item2->model_file_id)
        && (!item1->mod_struct_size || item1->mod_struct_size == item2->mod_struct_size)
        && (!item1->interaction || item1->interaction == item2->interaction)
        && (!item1->mod_struct_size || memcmp(item1->mod_struct,item2->mod_struct,item1->mod_struct_size * sizeof(item1->mod_struct[0])) == 0);
}
bool InventoryManager::IsPendingIdentify() {
    if (!pending_identify_kit.item_id || !pending_identify_item.item_id)
        return false;
    Item* current_kit = pending_identify_kit.item();
    if (current_kit && current_kit->GetUses() == pending_identify_kit.uses)
        return true;
    const Item* current_item = pending_identify_item.item();
    if (current_item && !current_item->GetIsIdentified())
        return true;
    return false;
}
bool InventoryManager::IsPendingSalvage() {
    if (!pending_salvage_kit.item_id || !pending_salvage_item.item_id)
        return false;
    if (current_salvage_session.salvage_item_id)
        return false;
    Item* current_kit = pending_salvage_kit.item();
    if (current_kit && current_kit->GetUses() == pending_salvage_kit.uses)
        return true;
    const Item* current_item = pending_salvage_item.item();
    if (current_item && current_item->quantity == pending_salvage_item.quantity)
        return true;
    return false;
}
void InventoryManager::DrawSettingInternal() {
    ImGui::TextDisabled("This module is responsible for extra item functions via ctrl+click, right click or double click");
    ImGui::Checkbox("Hide unsellable items from merchant window", &hide_unsellable_items);
    ImGui::Checkbox("Move whole stacks into trade by default", &trade_whole_stacks);
    ImGui::ShowHelp("Shift drag to prompt for amount, drag without shift to move the whole stack into trade");
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
    ImGui::Text("Salvage from:");
    ImGui::ShowHelp("Only ticked bags will be checked for salvagable items");
    ImGui::Checkbox("Backpack", &bags_to_salvage_from[GW::Constants::Bag::Backpack]);
    ImGui::SameLine();
    ImGui::Checkbox("Belt Pouch", &bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
    ImGui::SameLine();
    ImGui::Checkbox("Bag 1", &bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
    ImGui::SameLine();
    ImGui::Checkbox("Bag 2", &bags_to_salvage_from[GW::Constants::Bag::Bag_2]);

    ImGui::Separator();
    ImGui::Text("Hide items from merchant sell window:");
    ImGui::BeginChild("hide_from_merchant_items", ImVec2(0.0F, hide_from_merchant_items.size() * 25.0F));
    for (const auto& [item_id, item_name] : hide_from_merchant_items) {
        ImGui::PushID(static_cast<int>(item_id));
        ImGui::Text("%s (%d)", item_name.c_str(), item_id);
        ImGui::SameLine();
        const bool clicked = ImGui::Button(" X ");
        ImGui::PopID();
        if (clicked) {
            Log::Info("Removed Item %s with ID (%d)", item_name.c_str(), item_id);
            hide_from_merchant_items.erase(item_id);
            break;
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
    bool submitted = false;
    ImGui::Text("Add new item from merchant window:");
    static int new_item_id;
    static char buf[50];
    ImGui::InputText("Item Name", buf, 50);
    ImGui::InputInt("Item Model ID", &new_item_id);
    submitted |= ImGui::Button("Add");
    if (submitted && new_item_id > 0) {
        const auto new_id = static_cast<uint32_t>(new_item_id);
        if (!hide_from_merchant_items.contains(new_id)) {
            hide_from_merchant_items[new_id] = std::string(buf);
            Log::Info("Added Item %s with ID (%d)", buf, new_id);
            std::ranges::fill(buf, '\0');
            new_item_id = 0;
        }
    }
}
void InventoryManager::Update(float) {
    if (check_context_menu_position && TIMER_DIFF(check_context_menu_position) > 0) {
        const auto item = right_clicked_item ? GW::Items::GetItemById(right_clicked_item) : nullptr;
        const auto bag = item ? item->bag : nullptr;
        if (bag) {
            // Item right clicked - spoof a click event
            GW::HookStatus status;
            ItemClickCallback(&status, 999, item->slot, bag);
        }
        check_context_menu_position = 0;
    }

    if (pending_item_move_for_trade) {
        Item* item = reinterpret_cast<Item*>(GW::Items::GetItemById(pending_item_move_for_trade));
        if (!item) {
            pending_item_move_for_trade = 0;
            return;
        }
        if (item->bag && item->bag->IsInventoryBag()) {
            if (item->CanOfferToTrade())
                GW::Trade::OfferItem(item->item_id);
            pending_item_move_for_trade = 0;
        }
    }
    if (is_salvaging) {
        if (IsPendingSalvage()) {
            if ((clock() / CLOCKS_PER_SEC) - pending_salvage_at > 5) {
                is_salvaging = is_salvaging_all = false;
                Log::Warning("Failed to salvage item in slot %d/%d", pending_salvage_item.bag, pending_salvage_item.slot);
            }
        }
        else {
            // Salvage complete
            ContinueSalvage();
        }
    }
    if (is_identifying) {
        if (IsPendingIdentify()) {
            if ((clock() / CLOCKS_PER_SEC) - pending_identify_at > 5) {
                is_identifying = is_identifying_all = false;
                Log::Warning("Failed to identify item in slot %d/%d", pending_identify_item.bag, pending_identify_item.slot);
            }
        }
        else {
            // Identify complete
            ContinueIdentify();
        }
    }
    if (is_identifying_all)
        IdentifyAll(identify_all_type);
    if (is_salvaging_all)
        SalvageAll(salvage_all_type);
    if (pending_transaction.in_progress())
        ContinueTransaction();

};
void InventoryManager::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (!GW::Map::GetIsMapLoaded())
        return;
    DrawPendingTomeUsage();
#if 0
    DrawInventoryOverlay();
#endif

    static bool check_all_items = true;
    static bool show_inventory_context_menu = false;
    DrawItemContextMenu(show_item_context_menu);
    show_item_context_menu = false;



    /*
    *   Cog icon on inventory bags window
    */
#if 0
    bool show_options_on_inventory = false;
    if (show_options_on_inventory && inventory_bags_window_position && inventory_bags_window_position->visible()) {
        float uiscale_multiply = GuiUtils::GetGWScaleMultiplier();
        // NB: Use case to define GW::Vec4f ?
        GW::Vec2f x = inventory_bags_window_position->xAxis(uiscale_multiply);
        GW::Vec2f y = inventory_bags_window_position->yAxis(uiscale_multiply);
        // Clamp
        ImVec4 rect(x.x, y.x, x.y, y.y);
        ImVec4 viewport(0, 0, (float)GW::Render::GetViewportWidth(), (float)GW::Render::GetViewportHeight());
        // GW Clamps windows to viewport; we need to do the same.
        GuiUtils::ClampRect(rect, viewport);
        // Left placement
        GW::Vec2f internal_offset(
            (rect.z - rect.x) - 48.f * uiscale_multiply ,
            0.f
        );
        ImVec2 calculated_pos = ImVec2(rect.x + internal_offset.x, rect.y + internal_offset.y);
        ImGui::SetNextWindowPos(calculated_pos);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(0, 0, 0, 0).Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor(0, 0, 0, 0).Value);
        const char* context_menu_id = "Inventory Context Menu###inv_context";
        if (ImGui::Begin("Inventory bags context", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
            const char* btn_id = u8"\uf013###inv_bags_context_btn";
            show_inventory_context_menu = ImGui::Button(btn_id);
        }
        ImGui::End();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(4);
        if (show_inventory_context_menu)
            ImGui::OpenPopup(context_menu_id);
        if (ImGui::BeginPopup(context_menu_id)) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
                if (ImGui::Button("Store Materials")) {
                    Log::Info("TODO: Store materials");
                }
            }
            ImGui::EndPopup();
        }

    }
#endif




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
                    Item* item = pending_transaction.item();
                    if (item) {
                        // Set initial transaction amount to be the entire stack
                        pending_transaction_amount = item->quantity;
                        if (item->GetIsMaterial() && !item->IsRareMaterial()
                            && (GW::Merchant::TransactionType)pending_transaction.type == GW::Merchant::TransactionType::TraderSell) {
                            pending_transaction_amount = static_cast<int>(floor(pending_transaction_amount / 10));
                        }
                    }
                }
                else {
                    pending_transaction_amount = static_cast<int>(floor(GW::Items::GetGoldAmountOnCharacter() / pending_transaction.price));
                }
            }
            // Prompt user for amount
            ImGui::Text(pending_transaction.selling() ? "Enter quantity to sell:" : "Enter quantity to buy:");
            if (ImGui::InputInt("###transacting_quantity", &pending_transaction_amount, 1, 1)) {
                if (pending_transaction_amount < 1)
                    pending_transaction_amount = 1;
            }
            const ImGuiID id = ImGui::GetID("###transacting_quantity");
            if (show_transact_quantity_popup) {
                ImGui::SetFocusID(id, ImGui::GetCurrentWindow());
            }
            show_transact_quantity_popup = false;
            bool begin_transaction = (ImGui::GetFocusID() == id && ImGui::IsKeyPressed(ImGuiKey_Enter));
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
        } break;
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
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                pending_cancel_salvage = true;
                ImGui::CloseCurrentPopup();
            }
        }
        else {
            // Are you sure prompt; at this point we've already got the list of items via FetchPotentialItems()
            ImGui::Text("You're about to salvage %d item%s:", potential_salvage_all_items.size(), potential_salvage_all_items.size() == 1 ? "" : "s");
            ImGui::TextDisabled("Untick an item to skip salvaging");
            static const std::regex sanitiser("<[^>]+>");
            static const std::wregex wsanitiser(L"<[^>]+>");
            static const std::wstring wiki_url(L"https://wiki.guildwars.com/wiki/");
            const float& font_scale = ImGui::GetIO().FontGlobalScale;
            const float wiki_btn_width = 50.0f * font_scale;
            static float longest_item_name_length = 280.0f * font_scale;
            PotentialItem* pi;
            Item* item;
            const GW::Bag* bag = nullptr;
            bool has_items_to_salvage = false;
            if (ImGui::Checkbox("Select All", &check_all_items)) {
                for (size_t i = 0; i < potential_salvage_all_items.size(); i++) {
                    potential_salvage_all_items[i]->proceed = check_all_items;
                }
            }
            for(size_t i=0;i< potential_salvage_all_items.size();i++) {
                pi = potential_salvage_all_items[i];
                if (!pi) continue;
                item = pi->item();
                if (!item) continue;
                if (bag != item->bag) {
                    if (!item->bag || item->bag->index > 3)
                        continue;
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
                ImGui::Checkbox(pi->name.string().c_str(),&pi->proceed);
                const float item_name_length = ImGui::CalcTextSize(pi->name.string().c_str(), nullptr, true).x;
                longest_item_name_length = item_name_length > longest_item_name_length ? item_name_length : longest_item_name_length;
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", pi->desc.string().c_str());
                }
                ImGui::SameLine(longest_item_name_length + wiki_btn_width);
                if (ImGui::Button("Wiki", ImVec2(wiki_btn_width, 0))) {
                    GuiUtils::SearchWiki(pi->wiki_name.wstring());
                }
                ImGui::PopID();
                has_items_to_salvage |= pi->proceed;
            }
            ImGui::Text("\n\nAre you sure?");
            ImVec2 btn_width = ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x, 0);
            if (has_items_to_salvage) {
                btn_width.x /= 2;
                if (ImGui::Button("OK", btn_width)) {
                    is_salvaging_all = true;
                }
                ImGui::SameLine();
            }
            if (ImGui::Button("Cancel", btn_width)) {
                CancelSalvage();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}
bool InventoryManager::DrawItemContextMenu(bool open) {
    const char* context_menu_id = "Item Context Menu";
    const auto has_context_menu = [&](Item* item) {
        if (!item)
            return false;
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
            return true;
        if (wiki_link_on_context_menu)
            return true;
        return item->IsIdentificationKit() || item->IsSalvageKit();
    };
    Item* context_item_actual = context_item.item();
    if (!context_item.item_id || !has_context_menu(context_item_actual)) {
        context_item.item_id = 0;
        return false;
    }


    // NB: Be really careful here; a window has to be open for at LEAST 1 frame before closing it again.

    // If it is closed before the draw ends, the you'll get a crash on g.NavWindow being NULL.
    // Either don't open it at all, or wait a frame before closing it.
    if(open)
        ImGui::OpenPopup(context_menu_id);
    if (!ImGui::BeginPopup(context_menu_id))
        return false;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
    const ImVec2 size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
    /*IDirect3DTexture9** tex = Resources::GetItemImage(context_item.wiki_name.wstring());
    if (tex && *tex) {
        const float text_height = ImGui::CalcTextSize(" ").y;
        ImGui::Image(*tex, ImVec2(text_height, text_height));
        ImGui::SameLine();
    }*/
    ImGui::Text(context_item.name.string().c_str());
    ImGui::Separator();
    // Shouldn't really fetch item() every frame, but its only when the menu is open and better than risking a crash
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        if (ImGui::Button(context_item_actual->bag->IsInventoryBag() ? "Store Item" : "Withdraw Item", size)) {
            ImGui::CloseCurrentPopup();
            move_item(context_item_actual);
            goto end_popup;
        }
        if (context_item_actual->GetIsMaterial() && context_item_actual->bag->IsInventoryBag()) {
            if (ImGui::Button("Store All Materials", size)) {
                ImGui::CloseCurrentPopup();
                store_all_materials();
                goto end_popup;
            }
        }
        else if (context_item_actual->IsTome() && context_item_actual->bag->IsInventoryBag()) {
            if (ImGui::Button("Store All Tomes", size)) {
                ImGui::CloseCurrentPopup();
                store_all_tomes();
                goto end_popup;
            }
        }
        else if (context_item_actual->type == 8 && context_item_actual->bag->IsInventoryBag()) {
            if (ImGui::Button("Store All Upgrades", size)) {
                ImGui::CloseCurrentPopup();
                store_all_upgrades();
                goto end_popup;
            }
        }
        char buf[128];
        if (context_item_actual->bag->IsInventoryBag()) {
            snprintf(buf, 128, "Store All %s", context_item.plural_item_name.string().c_str());
        }
        else {
            snprintf(buf, 128, "Withdraw All %s", context_item.plural_item_name.string().c_str());
        }
        if (ImGui::Button(buf, size)) {
            ImGui::CloseCurrentPopup();
            move_all_item(context_item_actual);
            goto end_popup;
        }

    }
    if (context_item_actual->IsIdentificationKit()) {
        IdentifyAllType type = IdentifyAllType::None;
        if (ImGui::Button("Identify All Items", size))
            type = IdentifyAllType::All;
        ImGui::PushStyleColor(ImGuiCol_Text, ItemBlue);
        if (ImGui::Button("Identify All Blue Items", size))
            type = IdentifyAllType::Blue;
        ImGui::PushStyleColor(ImGuiCol_Text, ItemPurple);
        if (ImGui::Button("Identify All Purple Items", size))
            type = IdentifyAllType::Purple;
        ImGui::PushStyleColor(ImGuiCol_Text, ItemGold);
        if (ImGui::Button("Identify All Gold Items", size))
            type = IdentifyAllType::Gold;
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
    else if (context_item_actual->IsSalvageKit()) {
        SalvageAllType type = SalvageAllType::None;
        if (ImGui::Button("Salvage All White Items", size))
            type = SalvageAllType::White;
        ImGui::PushStyleColor(ImGuiCol_Text, ItemBlue);
        if (ImGui::Button("Salvage All Blue & Lesser Items", size))
            type = SalvageAllType::BlueAndLower;
        ImGui::PushStyleColor(ImGuiCol_Text, ItemPurple);
        if (ImGui::Button("Salvage All Purple & Lesser Items", size))
            type = SalvageAllType::PurpleAndLower;
        ImGui::PushStyleColor(ImGuiCol_Text, ItemGold);
        if (ImGui::Button("Salvage All Gold & Lesser Items", size))
            type = SalvageAllType::GoldAndLower;
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
    if (wiki_link_on_context_menu && ImGui::Button("Guild Wars Wiki", size)) {
        ImGui::CloseCurrentPopup();
        GuiUtils::SearchWiki(context_item.wiki_name.wstring());
        goto end_popup;
    }
    end_popup:
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndPopup();
    return true;
}
void InventoryManager::ItemClickCallback(GW::HookStatus* status, uint32_t type, uint32_t slot, GW::Bag* bag) {
    InventoryManager& im = InventoryManager::Instance();
    Item* item = nullptr;
    switch (type) {
    case 7:   // Left click + ctrl
        if (!ImGui::IsKeyDown(ImGuiKey_ModCtrl))
            return;
        break;
    case 8: // Double click - add to trade window if available
        if (!IsTradeWindowOpen())
            return;
        status->blocked = true;
        item = static_cast<Item*>(GW::Items::GetItemBySlot(bag, slot + 1));
        if (!item || !item->CanOfferToTrade())
            return;
        if (!item->bag->IsInventoryBag()) {
            const uint16_t moved = move_to_first_empty_slot(item, (size_t)GW::Constants::Bag::Backpack, (size_t)GW::Constants::Bag::Bag_2);
            if (!moved) {
                Log::ErrorW(L"Failed to move item to inventory for trading");
                return;
            }
        }
        pending_item_move_for_trade = item->item_id;
        return;
    case 999: // Right click (via GWToolbox)
        if (!Instance().right_click_context_menu_in_explorable && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
            return;
        if (!Instance().right_click_context_menu_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
            return;
        break;
    default:
        return;
    }

    if (bag) {
        item = static_cast<Item*>(GW::Items::GetItemBySlot(bag, slot + 1));
    }
    else {
        item = static_cast<Item*>(GW::Items::GetHoveredItem());
    }

    if (!item) return;
    const bool is_inventory_item = item->bag->IsInventoryBag();
    const bool is_storage_item = item->bag->IsStorageBag() || item->bag->IsMaterialStorage();
    if (!is_inventory_item && !is_storage_item) return;

    const bool show_context_menu = item->IsIdentificationKit() || item->IsSalvageKit() || type == 999;

    if (show_context_menu) {
        // Context menu applies
        if (im.context_item.item_id == item->item_id && im.show_item_context_menu)
            return; // Double looped.
        if (!im.context_item.set(item))
            return;
        im.show_item_context_menu = true;
        status->blocked = true;
        return;
    } else if (type == 7 
        && ImGui::IsKeyDown(ImGuiKey_ModCtrl) 
        && GameSettings::GetSettingBool("move_item_on_ctrl_click")
        && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        // Move item on ctrl click
        if (ImGui::IsKeyDown(ImGuiKey_ModShift) && item->quantity > 1)
            prompt_split_stack(item);
        else
            move_item(item);
    }
}
void InventoryManager::ClearPotentialItems()
{
    for (const PotentialItem* item : potential_salvage_all_items) {
        delete item;
    }
    potential_salvage_all_items.clear();
}

// Change secondary if using a tome
void InventoryManager::OnUseItem(GW::HookStatus* status, void* packet) {
    const uint32_t header = *(uint32_t*)packet;
    if (header != GAME_CMSG_ITEM_USE)
        return;
    const uint32_t item_id = ((uint32_t*)packet)[1];
    auto& instance = Instance();
    if (instance.tome_pending_stage != PendingTomeUseStage::None)
        return;
    const GW::Item* item = GW::Items::GetItemById(item_id);
    if (!item) return;
    GW::Constants::Profession profession_needed = GW::Constants::Profession::None;
    switch (item->model_id) {
    case 21786:case 21796:
        profession_needed = GW::Constants::Profession::Assassin;
        break;
    case 21787:case 21797:
        profession_needed = GW::Constants::Profession::Mesmer;
        break;
    case 21788:case 21798:
        profession_needed = GW::Constants::Profession::Necromancer;
        break;
    case 21789:case 21799:
        profession_needed = GW::Constants::Profession::Elementalist;
        break;
    case 21790:case 21800:
        profession_needed = GW::Constants::Profession::Monk;
        break;
    case 21791:case 21801:
        profession_needed = GW::Constants::Profession::Warrior;
        break;
    case 21792:case 21802:
        profession_needed = GW::Constants::Profession::Ranger;
        break;
    case 21793:case 21803:
        profession_needed = GW::Constants::Profession::Dervish;
        break;
    case 21794:case 21804:
        profession_needed = GW::Constants::Profession::Ritualist;
        break;
    case 21795:case 21805:
        profession_needed = GW::Constants::Profession::Paragon;
        break;
    }
    if (profession_needed != GW::Constants::Profession::None) {
        instance.tome_pending_profession = (uint32_t)profession_needed;
        instance.tome_pending_item_id = item_id;
        instance.tome_pending_stage = PendingTomeUseStage::PromptUser;
        status->blocked = true;
    }
}

// Run every frame; if we're pending aa change of secondary profession to use a tome, check and action.
void InventoryManager::DrawPendingTomeUsage() {
    if (tome_pending_stage == PendingTomeUseStage::None)
        return;
    constexpr const char* popup_label = "Change secondary profession?###change-secondary";
    if (tome_pending_timeout && TIMER_INIT() > tome_pending_timeout) {
        Log::Error("Timeout reached trying to change profession for tome use");
        goto cancel;
    }
    if (tome_pending_stage == PendingTomeUseStage::PromptUser) {
        const GW::AgentLiving* player = GW::Agents::GetPlayerAsAgentLiving();
        if (player && (player->secondary == tome_pending_profession || player->primary == tome_pending_profession)) {
            tome_pending_stage = PendingTomeUseStage::UseItem;
            return;
        }
        const auto& d = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({ d.x * .5f,d.y * .5f }, 0, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup(popup_label);
        tome_pending_stage = PendingTomeUseStage::AwaitPromptReply;
    }
    if (ImGui::BeginPopupModal(popup_label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Changing your secondary profession will change your skills and attributes.\nDo you want to continue?");
        if (ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            tome_pending_stage = PendingTomeUseStage::ChangeProfession;
            tome_pending_timeout = TIMER_INIT() + 3000;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing())
            ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }
    if (tome_pending_stage == PendingTomeUseStage::AwaitPromptReply) {
        // Popup not drawn; cancelled.
        goto cancel;
    }


    switch (tome_pending_stage) {
        case PendingTomeUseStage::ChangeProfession: {
            const GW::AgentLiving* player = GW::Agents::GetPlayerAsAgentLiving();
            if (!player)
                return;
            if (player && player->secondary == tome_pending_profession) {
                tome_pending_stage = PendingTomeUseStage::UseItem;
                return;
            }
            auto* world = GW::GetWorldContext();
            if (!world) {
                goto cancel;
            }
            GW::Array< GW::ProfessionState>& profession_unlocks_array = world->party_profession_states;
            const GW::ProfessionState* found = nullptr;
            for (size_t i = 0; !found && profession_unlocks_array.valid() && i < profession_unlocks_array.size(); i++) {
                if (profession_unlocks_array[i].agent_id == player->agent_id)
                    found = &profession_unlocks_array[i];
            }
            if (!found) {
                Log::Error("Player's profession info not found in world->party_profession_states");
                goto cancel;
            }
            if (((found->unlocked_professions >> tome_pending_profession) & 1) != 1) {
                // Skip to use item error; profession isn't unlocked
                tome_pending_stage = PendingTomeUseStage::UseItem;
                return;
            }
            GW::PlayerMgr::ChangeSecondProfession((GW::Constants::Profession)tome_pending_profession);
            tome_pending_stage = PendingTomeUseStage::AwaitProfession;
            return;
        }



        case PendingTomeUseStage::AwaitProfession: {
            const GW::AgentLiving* player = GW::Agents::GetPlayerAsAgentLiving();
            if (player && player->secondary == tome_pending_profession) {
                tome_pending_stage = PendingTomeUseStage::UseItem;
            }
            return;
        }
        case PendingTomeUseStage::UseItem: {
            const GW::Item* item = GW::Items::GetItemById(tome_pending_item_id);
            if (item) {
                GW::Items::UseItem(item);
            }
            goto cancel;
        }
    }
cancel:
    tome_pending_stage = PendingTomeUseStage::None;
    tome_pending_timeout = 0;
    tome_pending_item_id = 0;
    tome_pending_profession = 0;
}

bool InventoryManager::Item::IsWeaponSetItem()
{
    if (!IsWeapon())
        return false;
    const GW::ItemContext* c = GW::GetItemContext();
    if (!c || !c->inventory)
        return false;
    const GW::WeaponSet *weapon_sets = c->inventory->weapon_sets;
    for (size_t i = 0; i < 4; i++) {
        if (weapon_sets[i].offhand == this)
            return true;
        if (weapon_sets[i].weapon == this)
            return true;
    }
    return false;
}

bool InventoryManager::Item::IsOfferedInTrade() {
    auto* player_items = GetPlayerTradeItems();
    if (!player_items)
        return false;
    for (auto& player_item : *player_items) {
        if (player_item.item_id == item_id)
            return true;
    }
    return false;
}
bool InventoryManager::Item::CanOfferToTrade() {
    auto* player_items = GetPlayerTradeItems();
    if (!player_items)
        return false;
    return IsTradable() && IsTradeWindowOpen() && !IsOfferedInTrade() && player_items->size() < 7;
}

bool InventoryManager::Item::IsSalvagable()
{
    if (item_formula == 0x5da)
        return false;
    if (IsUsable() || IsGreen())
        return false; // Non-salvagable flag set
    if (!bag)
        return false;
    if (!bag->IsInventoryBag() && !bag->IsStorageBag())
        return false;
    if (bag->index + 1 == static_cast<uint32_t>(GW::Constants::Bag::Equipment_Pack))
        return false;
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
    if (IsWeapon() || IsArmor())
        return true;
    return false;
}
bool InventoryManager::Item::IsWeapon()
{
    switch ((GW::Constants::ItemType)type) {
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
bool InventoryManager::Item::IsArmor()
{
    switch ((GW::Constants::ItemType)type) {
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
GW::ItemModifier *InventoryManager::Item::GetModifier(uint32_t identifier)
{
    for (size_t i = 0; i < mod_struct_size; i++) {
        GW::ItemModifier *mod = &mod_struct[i];
        if (mod->identifier() == identifier)
            return mod;
    }
    return nullptr;
}
// InventoryManager::Item definitions

uint32_t InventoryManager::Item::GetUses()
{
    const GW::ItemModifier* mod = GetModifier(0x2458);
    return mod ? mod->arg2() : quantity;
}
bool InventoryManager::Item::IsSalvageKit()
{
    return IsLesserKit() || IsExpertSalvageKit(); // || IsPerfectSalvageKit();
}
bool InventoryManager::Item::IsTome() {
    const GW::ItemModifier* mod = GetModifier(0x2788);
    const uint32_t use_id = mod ? mod->arg2() : 0;
    return use_id > 15 && use_id < 36;
}
bool InventoryManager::Item::IsIdentificationKit()
{
    const GW::ItemModifier *mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 1;
}
bool InventoryManager::Item::IsLesserKit()
{
    const GW::ItemModifier *mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 3;
}
bool InventoryManager::Item::IsExpertSalvageKit()
{
    const GW::ItemModifier *mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 2;
}
bool InventoryManager::Item::IsPerfectSalvageKit()
{
    const GW::ItemModifier *mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 6;
}
bool InventoryManager::Item::IsRareMaterial()
{
    const GW::ItemModifier *mod = GetModifier(0x2508);
    return mod && mod->arg1() > 11;
}
GW::Constants::Rarity InventoryManager::Item::GetRarity()
{
    if (IsGreen())
        return GW::Constants::Rarity::Green;
    if (IsGold())
        return GW::Constants::Rarity::Gold;
    if (IsPurple())
        return GW::Constants::Rarity::Purple;
    if (IsBlue())
        return GW::Constants::Rarity::Blue;
    return GW::Constants::Rarity::White;
}
bool InventoryManager::PendingItem::set(InventoryManager::Item *item)
{
    item_id = 0;
    if (!item || !item->item_id || !item->bag)
        return false;
    item_id = item->item_id;
    slot = item->slot;
    quantity = item->quantity;
    uses = item->GetUses();
    bag = static_cast<GW::Constants::Bag>(item->bag->index + 1);
    name.reset(item->complete_name_enc ? item->complete_name_enc : item->name_enc);
    // NB: This doesn't work for inscriptions; gww doesn't have a page per inscription.
    wiki_name.reset(item->name_enc);
    wiki_name.language(GW::Constants::TextLanguage::English);
    wiki_name.wstring(); // Trigger decode; this isn't done any other time
    wchar_t plural_item_name_wc[128];
    swprintf(plural_item_name_wc, 128, L"\xa3d\x10a\xa35\x101\x200\x10a%s\x1\x1", item->name_enc);
    plural_item_name.reset(plural_item_name_wc);
    desc.reset(item->info_string);
    return true;
}

InventoryManager::Item *InventoryManager::PendingItem::item()
{
    if (!item_id)
        return nullptr;
    Item *item = static_cast<Item *>(GW::Items::GetItemBySlot(bag, slot + 1));
    return item && item->item_id == item_id ? item : nullptr;
}

InventoryManager::Item* InventoryManager::PendingTransaction::item()
{
    return static_cast<Item*>(GW::Items::GetItemById(item_id));
}
InventoryManager::CtoS_QuoteItem InventoryManager::PendingTransaction::quote()
{
    CtoS_QuoteItem q;
    q.type = type;
    if (selling()) {
        q.gold_recv = 0;
        q.item_give_count = 1;
        q.item_give_ids[0] = item_id;
    }
    else {
        q.gold_give = 0;
        q.item_recv_count = 1;
        q.item_recv_ids[0] = item_id;
    }
    return q;
}
InventoryManager::TransactItems InventoryManager::PendingTransaction::transact()
{
    TransactItems q;
    q.type = type;
    if (selling()) {
        q.gold_recv = price;
        q.item_give_count = 1;
        q.item_give_ids[0] = item_id;
        q.item_give_quantities[0] = 1;
    }
    else {
        q.gold_give = price;
        q.item_recv_count = 1;
        q.item_recv_ids[0] = item_id;
        q.item_recv_quantities[0] = 1;
    }
    return q;
}

bool InventoryManager::PendingTransaction::selling()
{
    return (GW::Merchant::TransactionType)type == GW::Merchant::TransactionType::MerchantSell
        || (GW::Merchant::TransactionType)type == GW::Merchant::TransactionType::TraderSell;
}

void InventoryManager::PendingItem::PluralEncString::sanitise() {
    if (sanitised)
        return;
    GuiUtils::EncString::sanitise();
    if (sanitised) {
        static const std::wregex plural(L"256 ");
        decoded_ws = std::regex_replace(decoded_ws, plural, L"");
    }
}
