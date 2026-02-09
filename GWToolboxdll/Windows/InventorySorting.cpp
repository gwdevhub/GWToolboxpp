#include "stdafx.h"

#include <Modules/Resources.h>
#include <Windows/InventorySorting.h>
#include <Modules/InventoryManager.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <GWCA/GameEntities/Item.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>
#include <Utils/ToolboxUtils.h>


namespace {
// Macro to wait for a game thread task to complete with timeout and cancellation checks
#define WAIT_FOR_GAME_THREAD_TASK(task_done_flag, timeout_ms, error_message)               \
    for (size_t i = 0; i < (timeout_ms) && !(task_done_flag) && !pending_cancel; i += 5) { \
        Sleep(5);                                                                          \
    }                                                                                      \
    if (!(task_done_flag)) {                                                               \
        Log::Warning(error_message);                                                       \
        is_sorting = false;                                                                \
        show_sort_popup = false;                                                           \
        return;                                                                            \
    }                                                                                      \
    if (pending_cancel) {                                                                  \
        Log::Info("Sorting cancelled");                                                    \
        is_sorting = false;                                                                \
        show_sort_popup = false;                                                           \
        return;                                                                            \
    }

    // Helper function to check if map is ready
    bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }

    // ImGui colors for item rarities
    const ImVec4 ItemBlue = ImColor(153, 238, 255).Value;
    const ImVec4 ItemPurple = ImColor(187, 137, 237).Value;
    const ImVec4 ItemGold = ImColor(255, 204, 86).Value;

    // State variables
    bool show_sort_popup = false;
    bool is_sorting = false;
    bool pending_cancel = false;
    size_t items_sorted_count = 0;

    // Sort order configuration
    std::vector<GW::Constants::ItemType> sort_order;

    /**
     * Gets the sort priority for an item (lower = higher priority).
     */
    uint32_t GetItemSortPriority(GW::Item* item)
    {
        if (!item) return 0xFFFFFFFF; // Max value for items that don't exist

        // TODO: Check if item is cons, return OrderType::Cons sort order
        // TODO: Check if item is alcohol, return OrderType::Alcohol sort order

        // TODO: Add custom sorting my model id

        size_t priority_by_type = 0;
        for (priority_by_type; priority_by_type < sort_order.size(); priority_by_type++) {
            if (std::to_underlying(sort_order[priority_by_type]) == std::to_underlying(item->type))
                break;
        }

        // Fisrt 8 bits are priority, then next 8 bits is item type, then 16 bits are model_id
        return (static_cast<uint32_t>(priority_by_type & 0xFF) << 24) | (item->model_file_id & 0xffFFFF);
    }

    /**
     * Compares two items for sorting purposes.
     * Returns true if item_a should come before item_b.
     */
    bool ShouldItemComeFirst(GW::Item* item_a, GW::Item* item_b)
    {
        if (!item_a || !item_b) return false;

        const uint32_t priority_a = GetItemSortPriority(item_a);
        const uint32_t priority_b = GetItemSortPriority(item_b);

        return priority_a < priority_b;
    }

    /**
     * Draws the inventory sorting progress popup with cancel button.
     */
    void DrawSortInventoryPopup();

    /**
     * Resets the sort order to default values.
     */
    void ResetSortOrder()
    {
        sort_order = {GW::Constants::ItemType::Salvage,    GW::Constants::ItemType::Materials_Zcoins,
                      GW::Constants::ItemType::Trophy,     GW::Constants::ItemType::Dye,
                      GW::Constants::ItemType::Rune_Mod,   GW::Constants::ItemType::CC_Shards,
                      GW::Constants::ItemType::Usable,     GW::Constants::ItemType::Key,
                      GW::Constants::ItemType::Axe,        GW::Constants::ItemType::Sword,
                      GW::Constants::ItemType::Hammer,     GW::Constants::ItemType::Bow,
                      GW::Constants::ItemType::Staff,      GW::Constants::ItemType::Wand,
                      GW::Constants::ItemType::Offhand,    GW::Constants::ItemType::Daggers,
                      GW::Constants::ItemType::Scythe,     GW::Constants::ItemType::Spear,
                      GW::Constants::ItemType::Shield,     GW::Constants::ItemType::Headpiece,
                      GW::Constants::ItemType::Chestpiece, GW::Constants::ItemType::Leggings,
                      GW::Constants::ItemType::Boots,      GW::Constants::ItemType::Gloves};
    }

    void DrawSortInventoryPopup()
    {
        if (show_sort_popup) {
            ImGui::OpenPopup("Sort Inventory");
        }

        if (ImGui::BeginPopupModal("Sort Inventory", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!is_sorting) {
                // Sorting has completed, close the popup
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            ImGui::TextUnformatted("Sorting inventory by type...");
            ImGui::Text("Items sorted: %zu", items_sorted_count);

            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("This may take a while depending on the number of items.");
            ImGui::TextDisabled("You can cancel at any time.");

            ImGui::Spacing();

            const float button_width = 120.0f;
            const float window_width = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((window_width - button_width) * 0.5f);

            if (ImGui::Button("Cancel", ImVec2(button_width, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                pending_cancel = true;
                InventorySorting::CancelSort();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    /**
     * Finds the next item that needs to be moved and returns its target position.
     * Returns true if an item needs moving, false if all remaining items are sorted.
     */
    struct MoveTarget {
        uint32_t item_id = 0;
        GW::Constants::Bag target_bag = GW::Constants::Bag::None;
        uint32_t target_slot = 0;
        bool needs_move = false;
    };

    /**
     * Finds the next item that needs to be moved.
     * Updates start iterator to point to the item that needs moving.
     * Returns true if an item needs moving, false if all items are sorted.
     * Fills target_out with the destination bag and slot if an item needs moving.
     */
    bool GetNextItemThatNeedsMoving(std::vector<uint32_t>::const_iterator& start, const std::vector<uint32_t>::const_iterator& end, MoveTarget* target_out)
    {
        while (start != end) {
            const auto check_item_id = *start;
            GW::Item* check_item = GW::Items::GetItemById(check_item_id);
            if (!check_item) {
                ++start;
                continue;
            }
            const auto check_item_priority = GetItemSortPriority(check_item);

            // Scan through bags to find where this item currently is and if it needs moving
            for (auto bag_id = GW::Constants::Bag::Storage_1; bag_id <= GW::Constants::Bag::Storage_14; bag_id = static_cast<GW::Constants::Bag>(std::to_underlying(bag_id) + 1)) {
                GW::Bag* bag = GW::Items::GetBag(bag_id);
                if (!bag || !bag->items.valid()) {
                    continue;
                }

                for (size_t slot = 0; slot < bag->items.size(); slot++) {
                    GW::Item* slot_item = bag->items[slot];

                    // Empty slot found - check_item should move here
                    if (!slot_item) {
                        if (target_out) {
                            target_out->target_bag = bag_id;
                            target_out->target_slot = static_cast<uint32_t>(slot);
                        }
                        return true;
                    }

                    // Found our item - it's in the correct position relative to everything before it
                    if (slot_item->item_id == check_item_id) {
                        ++start;
                        return GetNextItemThatNeedsMoving(start, end, target_out);
                    }

                    // Found an item with lower priority - check_item should go before it
                    if (GetItemSortPriority(slot_item) > check_item_priority) {
                        if (target_out) {
                            target_out->target_bag = bag_id;
                            target_out->target_slot = static_cast<uint32_t>(slot);
                        }
                        return true;
                    }
                }
            }

            // Item not found (was deleted/merged), skip it
            ++start;
        }

        return false;
    }
} // namespace

void InventorySorting::Initialize()
{
    ToolboxUIElement::Initialize();

    // Initialize default sort order
    ResetSortOrder();
}

void InventorySorting::LoadSettings(ToolboxIni* ini)
{
    ToolboxUIElement::LoadSettings(ini);

    // Load sort order from ini
    sort_order.clear();

    const size_t sort_order_count = ini->GetLongValue(Name(), "sort_order_count", 0);
    if (sort_order_count > 0) {
        // Load custom sort order
        for (size_t i = 0; i < sort_order_count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "sort_order_%zu", i);
            const int type_value = ini->GetLongValue(Name(), key, -1);
            if (type_value >= 0) {
                sort_order.push_back(static_cast<GW::Constants::ItemType>(type_value));
            }
        }
    }

    // If loading failed or no custom order, use default
    if (sort_order.empty()) {
        ResetSortOrder();
    }
}

void InventorySorting::SaveSettings(ToolboxIni* ini)
{
    ToolboxUIElement::SaveSettings(ini);

    // Save sort order to ini
    ini->SetLongValue(Name(), "sort_order_count", static_cast<long>(sort_order.size()));

    for (size_t i = 0; i < sort_order.size(); i++) {
        char key[32];
        snprintf(key, sizeof(key), "sort_order_%zu", i);
        ini->SetLongValue(Name(), key, static_cast<long>(std::to_underlying(sort_order[i])));
    }
}

void InventorySorting::Draw(IDirect3DDevice9*)
{
    DrawSortInventoryPopup();
}

void InventorySorting::DrawSettingsInternal()
{
    ImGui::PushID("inventory_sorting_settings");
    bool open = ImGui::CollapsingHeader("Change Storage Inventory Sorting Order", ImGuiTreeNodeFlags_SpanTextWidth);
    ImGui::SameLine(0.f, 20.f);
    bool sort_inv = false;
    if (ImGui::ConfirmButton("Sort Storage Inventory!", &sort_inv)) {
        SortInventoryByType();
    }
    if (open) {
        ImGui::Indent();

        ImGui::TextUnformatted("Sort Order Configuration");
        ImGui::TextDisabled("Drag items to reorder priority (top = higher priority)");
        ImGui::Spacing();

        // Drag and drop reordering for sort order
        for (size_t i = 0; i < sort_order.size(); i++) {
            const auto type = sort_order[i];
            const char* type_name = GW::Items::GetItemTypeName(type);

            ImGui::PushID(static_cast<int>(i));
            ImGui::Selectable(type_name, false, ImGuiSelectableFlags_None);

            // Drag and drop source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("SORT_ORDER_ITEM", &i, sizeof(i));
                ImGui::TextUnformatted(type_name);
                ImGui::EndDragDropSource();
            }

            // Drag and drop target
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SORT_ORDER_ITEM")) {
                    const size_t payload_i = *static_cast<const size_t*>(payload->Data);
                    // Swap items
                    if (payload_i != i && payload_i < sort_order.size()) {
                        std::swap(sort_order[payload_i], sort_order[i]);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        bool reset = false;
        if (ImGui::ConfirmButton("Reset to Default Order", &reset)) {
            ResetSortOrder();
        }

        ImGui::Unindent();
    }

    ImGui::PopID();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void InventorySorting::CancelSort()
{
    is_sorting = false;
    show_sort_popup = false;
    pending_cancel = false;
    items_sorted_count = 0;
}

void InventorySorting::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent(
        "Inventory Settings", ICON_FA_BOXES,
        [this](const std::string&, const bool is_showing) {
            if (is_showing) {
                DrawSettingsInternal();
            }
        },
        1.2f
    );
}

void InventorySorting::SortInventoryByType()
{
    if (is_sorting) {
        Log::Warning("Inventory sort already in progress");
        return;
    }

    if (!IsMapReady()) {
        Log::Warning("Cannot sort inventory while map is loading");
        return;
    }

    // Show popup and set flags
    show_sort_popup = true;
    is_sorting = true;
    items_sorted_count = 0;
    pending_cancel = false;

    Log::Info("Starting inventory sort...");

    // Start the sorting process on a worker thread
    Resources::EnqueueWorkerTask([]() {
        std::vector<uint32_t> item_ids;

        // ====== PHASE 1: Collect all item IDs from inventory bags ======
        bool task_done = false;
        GW::GameThread::Enqueue([&item_ids, &task_done]() {
            for (auto bag_id = GW::Constants::Bag::Storage_1; bag_id <= GW::Constants::Bag::Storage_14; bag_id = static_cast<GW::Constants::Bag>(std::to_underlying(bag_id) + 1)) {
                GW::Bag* bag = GW::Items::GetBag(bag_id);
                if (!bag || !bag->items.valid()) {
                    continue;
                }

                // Collect item IDs from this bag
                for (auto item : bag->items) {
                    if (!item) continue;
                    item_ids.push_back(item->item_id);
                }
            }
            // Sort item IDs by priority
            std::sort(item_ids.begin(), item_ids.end(), [](uint32_t a_id, uint32_t b_id) {
                GW::Item* item_a = GW::Items::GetItemById(a_id);
                GW::Item* item_b = GW::Items::GetItemById(b_id);
                return ShouldItemComeFirst(item_a, item_b);
            });
            task_done = true;
        });

        WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Sorting failed to collect inventory item info");

        if (pending_cancel) {
            Log::Info("Sorting cancelled");
            is_sorting = false;
            show_sort_popup = false;
            return;
        }

        // ====== PHASE 2: Find and move items that are out of place ======
        auto current_iter = item_ids.begin();
        const auto end_iter = item_ids.end();

        while (!pending_cancel) {
            // Find the next item that needs moving
            MoveTarget move_target;
            bool needs_move = false;
            task_done = false;

            GW::GameThread::Enqueue([&needs_move, &move_target, &current_iter, &end_iter, &task_done]() {
                needs_move = GetNextItemThatNeedsMoving(current_iter, end_iter, &move_target);
                task_done = true;
            });

            WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Sorting failed to find next item to move");

            if (!needs_move) {
                // All remaining items are in correct positions
                break;
            }

            const uint32_t item_id_to_move = *current_iter;

            // Move the item to the target position
            task_done = false;
            GW::GameThread::Enqueue([&task_done, item_id_to_move, move_target]() {
                GW::Item* item = GW::Items::GetItemById(item_id_to_move);
                if (item) {
                    GW::Items::MoveItem(item, move_target.target_bag, move_target.target_slot);
                }
                task_done = true;
            });

            WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Sorting failed to issue move command");

            // Wait for move to complete
            bool move_complete = false;
            const uint32_t timeout_ms = 3000;

            for (size_t j = 0; j < timeout_ms && !pending_cancel; j += 20) {
                task_done = false;
                GW::GameThread::Enqueue([&move_complete, &task_done, item_id_to_move, move_target]() {
                    GW::Item* item = GW::Items::GetItemById(item_id_to_move);

                    // Item might have been merged into a stack and no longer exists
                    if (!item) {
                        move_complete = true;
                        task_done = true;
                        return;
                    }

                    // Check if item reached target position
                    if (item->bag && item->bag->bag_id() == move_target.target_bag && item->slot == move_target.target_slot) {
                        move_complete = true;
                    }

                    task_done = true;
                });

                WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Sorting failed to verify item move");

                if (move_complete) {
                    items_sorted_count++;
                    break;
                }

                Sleep(20);
            }

            if (pending_cancel) {
                Log::Info("Sorting cancelled");
                is_sorting = false;
                show_sort_popup = false;
                return;
            }

            if (!move_complete) {
                Log::Warning("Sorting failed to move item %u to bag %d slot %u", item_id_to_move, std::to_underlying(move_target.target_bag), move_target.target_slot);
                is_sorting = false;
                show_sort_popup = false;
                return;
            }
        }

        // ====== PHASE 3: Completion ======
        Log::Info("Inventory sorting complete! %zu items sorted.", items_sorted_count);
        is_sorting = false;
        show_sort_popup = false;
    });
}
