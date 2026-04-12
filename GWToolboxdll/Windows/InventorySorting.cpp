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
        return false;                                                                      \
    }                                                                                      \
    if (pending_cancel) {                                                                  \
        Log::Info("Sorting cancelled");                                                    \
        is_sorting = false;                                                                \
        show_sort_popup = false;                                                           \
        return false;                                                                      \
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

    // Helper: iterate over all slots in a bag range
    template <typename Fn>
    void ForEachItemInBags(GW::Constants::Bag start, GW::Constants::Bag end, Fn&& fn)
    {
        for (auto bag_id = start; bag_id <= end; bag_id = static_cast<GW::Constants::Bag>(std::to_underlying(bag_id) + 1)) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            if (!bag || !bag->items.valid()) continue;
            for (uint32_t slot = 0; slot < bag->items.size(); slot++) {
                GW::Item* item = bag->items[slot];
                if (!item) continue;
                fn(bag, bag_id, slot, item);
            }
        }
    }

    struct SlotExpectation {
        GW::Constants::Bag bag_id;
        uint32_t slot;
        uint32_t expected_value; // item_id or quantity depending on check mode
    };

    enum class ExpectationMode {
        Quantity, // expected_value is quantity, 0 means slot should be empty
        ItemId,   // expected_value is item_id
    };

    // Helper: wait until all slot expectations are met
    bool WaitForExpectations(const std::vector<SlotExpectation>& expectations, ExpectationMode mode, uint32_t timeout_ms)
    {
        for (size_t t = 0; t < timeout_ms && !pending_cancel; t += 50) {
            bool task_done = false;
            bool all_done = true;

            GW::GameThread::Enqueue([&expectations, &all_done, &task_done, mode]() {
                for (const auto& exp : expectations) {
                    GW::Bag* bag = GW::Items::GetBag(exp.bag_id);
                    if (!bag || !bag->items.valid()) {
                        all_done = false;
                        break;
                    }
                    GW::Item* item = bag->items[exp.slot];

                    if (mode == ExpectationMode::Quantity) {
                        if (exp.expected_value == 0) {
                            if (item != nullptr) {
                                all_done = false;
                                break;
                            }
                        }
                        else {
                            if (!item || item->quantity != exp.expected_value) {
                                all_done = false;
                                break;
                            }
                        }
                    }
                    else {
                        if (!item || item->item_id != exp.expected_value) {
                            all_done = false;
                            break;
                        }
                    }
                }
                task_done = true;
            });

            WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Failed to verify slot expectations");

            if (all_done) return true;
            Sleep(50);
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
    bool open = ImGui::CollapsingHeader("Change Storage Inventory Sorting Order", ImGuiTreeNodeFlags_SpanLabelWidth);
    ImGui::SameLine(0.f, 20.f);
    bool sort_inv = false;
    if (ImGui::ConfirmButton("Sort Storage Inventory!", &sort_inv)) {
        Resources::EnqueueWorkerTask([]() {
            SortInventory(GW::Constants::Bag::Storage_1, GW::Constants::Bag::Storage_14);
        });
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
bool InventorySorting::CombineStacks(GW::Constants::Bag start, GW::Constants::Bag end)
{
    ASSERT(!GW::GameThread::IsInGameThread());

    std::vector<GW::Item*> stackable_items;
    std::vector<std::vector<GW::Item*>> groups;
    std::vector<SlotExpectation> expectations;
    bool task_done = false;

    GW::GameThread::Enqueue([&stackable_items, &groups, &task_done, start, end]() {
        ForEachItemInBags(start, end, [&](GW::Bag*, GW::Constants::Bag, uint32_t, GW::Item* item) {
            if (item->GetIsStackable() && item->quantity > 0 && item->quantity < 250) stackable_items.push_back(item);
        });

        std::vector<bool> assigned(stackable_items.size(), false);
        for (size_t i = 0; i < stackable_items.size(); i++) {
            if (assigned[i]) continue;
            std::vector<GW::Item*> group = {stackable_items[i]};
            assigned[i] = true;

            for (size_t j = i + 1; j < stackable_items.size(); j++) {
                if (assigned[j]) continue;
                if (InventoryManager::IsSameItem(stackable_items[i], stackable_items[j])) {
                    group.push_back(stackable_items[j]);
                    assigned[j] = true;
                }
            }

            if (group.size() > 1) groups.push_back(std::move(group));
        }
        task_done = true;
    });

    WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Stack consolidation failed to collect items");
    if (groups.empty() || pending_cancel) return !pending_cancel;

    task_done = false;
    GW::GameThread::Enqueue([&groups, &expectations, &task_done]() {
        for (auto& items : groups) {
            std::sort(items.begin(), items.end(), [](GW::Item* a, GW::Item* b) {
                return a->quantity > b->quantity;
            });

            std::vector<uint32_t> quantities;
            for (auto* item : items)
                quantities.push_back(item->quantity);

            size_t dst = 0;
            size_t src = items.size() - 1;
            while (dst < src) {
                uint32_t space = 250 - quantities[dst];
                if (space == 0) {
                    dst++;
                    continue;
                }

                uint32_t to_move = std::min(space, quantities[src]);
                GW::Items::MoveItem(items[src], items[dst], to_move);

                quantities[dst] += to_move;
                quantities[src] -= to_move;

                if (quantities[src] == 0) src--;
                if (quantities[dst] >= 250) dst++;
            }

            for (size_t i = 0; i < items.size(); i++)
                expectations.push_back({items[i]->bag->bag_id(), items[i]->slot, quantities[i]});
        }
        task_done = true;
    });

    WAIT_FOR_GAME_THREAD_TASK(task_done, 5000, "Stack consolidation failed to issue merge commands");
    if (pending_cancel) return false;

    return WaitForExpectations(expectations, ExpectationMode::Quantity, 5000);
}

bool InventorySorting::StoreMaterials(GW::Constants::Bag start, GW::Constants::Bag end)
{
    ASSERT(!GW::GameThread::IsInGameThread());

    std::vector<SlotExpectation> expectations;
    bool task_done = false;

    GW::GameThread::Enqueue([&task_done, &expectations, start, end]() {
        const uint32_t max_stack = GW::Items::GetMaterialStorageStackSize();
        const auto material_storage_bag = GW::Items::GetBag(GW::Constants::Bag::Material_Storage);
        if (!material_storage_bag) {
            task_done = true;
            return;
        }

        ForEachItemInBags(start, end, [&](GW::Bag*, GW::Constants::Bag bag_id, uint32_t slot, GW::Item* item) {
            if (!item->GetIsMaterial()) return;

            const auto mod = ((InventoryManager::Item*)item)->GetModifier(0x2508);
            const auto mat_slot = mod->arg1();
            if (mat_slot > (uint32_t)GW::Constants::MaterialSlot::JadeiteShard) return;

            const auto mat_item = material_storage_bag->items[mat_slot];
            uint32_t space = max_stack - (mat_item ? mat_item->quantity : 0);
            if (space == 0) return;

            uint32_t to_move = std::min(space, (uint32_t)item->quantity);
            GW::Items::MoveItem(item, GW::Constants::Bag::Material_Storage, mat_slot, to_move);

            uint32_t remaining = item->quantity - to_move;
            expectations.push_back({bag_id, slot, remaining});
        });

        task_done = true;
    });

    WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Store materials failed to issue move commands");
    if (expectations.empty() || pending_cancel) return !pending_cancel;

    return WaitForExpectations(expectations, ExpectationMode::Quantity, 5000);
}

bool InventorySorting::SortInventory(GW::Constants::Bag start, GW::Constants::Bag end)
{
    ASSERT(!GW::GameThread::IsInGameThread());
    if (!StoreMaterials(start, end)) return false;
    if (!CombineStacks(start, end)) return false;

    std::vector<SlotExpectation> expectations;
    bool task_done = false;

    GW::GameThread::Enqueue([&task_done, &expectations, start, end]() {
        if (is_sorting || !IsMapReady()) {
            task_done = true;
            return;
        }

        std::vector<uint32_t> item_ids;
        ForEachItemInBags(start, end, [&](GW::Bag*, GW::Constants::Bag, uint32_t, GW::Item* item) {
            item_ids.push_back(item->item_id);
        });

        std::sort(item_ids.begin(), item_ids.end(), [](uint32_t a_id, uint32_t b_id) {
            GW::Item* item_a = GW::Items::GetItemById(a_id);
            GW::Item* item_b = GW::Items::GetItemById(b_id);
            return ShouldItemComeFirst(item_a, item_b);
        });

        size_t idx = 0;
        for (auto bag_id = start; bag_id <= end; bag_id = static_cast<GW::Constants::Bag>(std::to_underlying(bag_id) + 1)) {
            GW::Bag* bag = GW::Items::GetBag(bag_id);
            if (!bag || !bag->items.valid()) continue;

            for (uint32_t slot = 0; slot < bag->items.size() && idx < item_ids.size(); slot++) {
                GW::Item* item = GW::Items::GetItemById(item_ids[idx]);
                if (item && (item->bag != bag || item->slot != slot)) GW::Items::MoveItem(item, bag_id, slot);
                expectations.push_back({bag_id, slot, item_ids[idx]});
                idx++;
            }
        }

        task_done = true;
    });

    WAIT_FOR_GAME_THREAD_TASK(task_done, 3000, "Sorting failed to issue move commands");
    if (expectations.empty() || pending_cancel) return !pending_cancel;

    return WaitForExpectations(expectations, ExpectationMode::ItemId, 5000);
}
