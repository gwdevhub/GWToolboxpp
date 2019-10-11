#include "stdafx.h"
#include "Pcons.h"
#include <mutex>
#include <d3dx9tex.h>

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>
#include <GWCA\GameContainers\GamePos.h>

#include <GWCA\GameEntities\Agent.h>
#include <GWCA\GameEntities\Skill.h>

#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\CtoSMgr.h>

#include <logger.h>
#include "GuiUtils.h"
#include <Modules\Resources.h>
#include <Windows\PconsWindow.h>
#include <Widgets\AlcoholWidget.h>

using namespace GW::Constants;

float Pcon::size = 46.0f;
int Pcon::pcons_delay = 5000;
int Pcon::lunar_delay = 500;
bool Pcon::disable_when_not_found = true;
bool Pcon::refill_if_below_threshold = false;
DWORD Pcon::player_id = 0;
Color Pcon::enabled_bg_color = Colors::ARGB(102, 0, 255, 0);

DWORD Pcon::alcohol_level = 0;
bool Pcon::suppress_drunk_effect = false;
bool Pcon::suppress_drunk_text = false;
bool Pcon::suppress_drunk_emotes = false;
bool Pcon::suppress_lunar_skills = false;

static std::mutex reserve_slot_mutex;

// 22 is the highest bag index. 25 is the most slots in any single bag.
std::vector<std::vector<clock_t>> Pcon::reserved_bag_slots(22, std::vector<clock_t>(25));

// ================================================
Pcon::Pcon(const char* chatname,
    const char* ininame,
    const wchar_t* filename,
    WORD res_id,
    ImVec2 uv0_, ImVec2 uv1_, int threshold_,
    const char* desc_)
    : chat(chatname), ini(ininame), threshold(threshold_), 
	uv0(uv0_), uv1(uv1_), texture(nullptr),
	enabled(false), quantity(0), timer(TIMER_INIT()) {
	if (desc_)
		desc = desc_;
	Resources::Instance().LoadTextureAsync(&texture, Resources::GetPath(L"img/pcons", filename), res_id);
}
Pcon::~Pcon() {
	if (refill_thread.joinable())
		refill_thread.join();
}
// Resets pcon counters so it needs to recalc number and refill.
void Pcon::ResetCounts() {
	refill_attempted = !enabled;
	pcon_quantity_checked = false;
}
void Pcon::SetEnabled(bool b) {
	if (enabled == b) return;
	enabled = b;
	ResetCounts();
}
void Pcon::Draw(IDirect3DDevice9* device) {
	if (texture == nullptr) return;
	ImVec2 pos = ImGui::GetCursorPos();
	ImVec2 s(size, size);
	ImVec4 bg = enabled ? ImColor(enabled_bg_color) : ImVec4(0, 0, 0, 0);
	ImVec4 tint(1, 1, 1, 1);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, 0, bg, tint)) {
		SetEnabled(!enabled);
	}
	ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && desc.size())
        ImGui::SetTooltip(desc.c_str());

	ImFont* f = GuiUtils::GetFont(GuiUtils::f20);
	ImGui::PushFont(f);
	ImVec4 color;
	if (quantity == 0) color = ImVec4(1, 0, 0, 1);
	else if (quantity < threshold) color = ImVec4(1, 1, 0, 1);
	else color = ImVec4(0, 1, 0, 1);

	ImVec2 nextPos = ImGui::GetCursorPos();

	ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
	ImGui::TextColored(ImVec4(0, 0, 0, 1), "%d", quantity);
	ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
	ImGui::TextColored(color, "%d", quantity);
	ImGui::PopFont();

	if (maptype == GW::Constants::InstanceType::Outpost && PconsWindow::Instance().show_storage_quantity) {
		f = GuiUtils::GetFont(GuiUtils::f16);
		ImGui::PushFont(f);
		ImGui::SetCursorPos(ImVec2(pos.x + 3, nextPos.y - f->FontSize));
		ImGui::TextColored(ImVec4(0, 0, 0, 1), "%d", quantity_storage);
		ImGui::SetCursorPos(ImVec2(pos.x + 2, nextPos.y - f->FontSize - 1));
		ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1), "%d", quantity_storage);
		ImGui::PopFont();
	}
	
	ImGui::SetCursorPos(pos);
	ImGui::Dummy(ImVec2(size, size));
}
void Pcon::Update(int delay) {
	if (mapid != GW::Map::GetMapID() || maptype != GW::Map::GetInstanceType()) { // Map changed; reset vars
		mapid = GW::Map::GetMapID();
		maptype = GW::Map::GetInstanceType();
		ResetCounts();
	}
    if(maptype == GW::Constants::InstanceType::Loading)
        return;
	if (enabled && PconsWindow::Instance().GetEnabled()) {
		if (delay < 0) delay = Pcon::pcons_delay;
		player = GW::Agents::GetPlayer();
		// === Use item if possible ===
		// NOTE: Only fails CanUseByEffect() if we've found an effects array for this map before.
		if (player != nullptr
			&& !player->GetIsDead()
			&& (player_id == 0 || player->agent_id == player_id)
			&& TIMER_DIFF(timer) > delay
			&& CanUseByInstanceType()
			&& CanUseByEffect()) {

			bool used = false;
			int qty = CheckInventory(&used);
			AfterUsed(used, qty);
		}
	}


	if (!pcon_quantity_checked) {

        if (!refill_attempted) {
            Refill();
            return; // Not tried to refill yet
        }
		pcon_quantity_checked = true;
		int qty = CheckInventory();
		if (qty < 0) return; // Inventory pointers not ready
		quantity = qty;
		if (maptype == GW::Constants::InstanceType::Outpost) {
			quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
		}
		if (enabled && PconsWindow::Instance().GetEnabled() && maptype == GW::Constants::InstanceType::Outpost) {
			// Only warn user of low pcon count if is enabled and we're in an outpost.
			if (quantity == 0) {
				Log::Error("No more %s items found", chat);
			}
			else if (quantity < threshold) {
				Log::Warning("Low on %s", chat);
			}
		}
	}
}
bool Pcon::ReserveSlotForMove(int bagIndex, int slot) { // Should NOT be used directly, supposed to be used with a mutex.
    std::lock_guard<std::mutex> lk(reserve_slot_mutex); // Lock to reserve the slot once found - pcons check on different threads!
	if (IsSlotReservedForMove(bagIndex, slot)) {
		printf("Slot %d %d, %d is LOCKED\n", bagIndex, slot, reserved_bag_slots.at(bagIndex).at(slot));
		return false;
	}
	reserved_bag_slots.at(bagIndex).at(slot) = TIMER_INIT();
	printf("locking %d %d, %d\n", bagIndex, slot, reserved_bag_slots.at(bagIndex).at(slot));
	return true;
}
bool Pcon::UnreserveSlotForMove(int bagIndex, int slot) { // Should NOT be used directly, supposed to be used with a mutex.
    std::lock_guard<std::mutex> lk(reserve_slot_mutex); // Lock to reserve the slot once found - pcons check on different threads!
    reserved_bag_slots.at(bagIndex).at(slot) = 0;
	printf("unlocking %d %d, %d\n", bagIndex, slot, reserved_bag_slots.at(bagIndex).at(slot));
    return true;
}
bool Pcon::IsSlotReservedForMove(int bagIndex, int slot) {
    //std::lock_guard<std::mutex> lk(reserve_slot_mutex); // Lock to reserve the slot once found - pcons check on different threads!
	clock_t slot_reserved_at = reserved_bag_slots.at(bagIndex).at(slot);
    return slot_reserved_at && TIMER_DIFF(slot_reserved_at) < 3000; // 1000ms is reasonable for CtoS then StoC
}
void Pcon::AfterUsed(bool used, int qty) {
	if (qty >= 0) { // if not, bag was undefined and we just ignore everything
		quantity = qty;
		if (used) {
			timer = TIMER_INIT();
			if (quantity == 0) { // if we just used the last one
				mapid = GW::Map::GetMapID();
				maptype = GW::Map::GetInstanceType();
				Log::Warning("Just used the last %s", chat);
				if (disable_when_not_found) enabled = false;
			}
		}
		else {
			// we should have used but didn't find the item
			if (disable_when_not_found) enabled = false;
			if (mapid != GW::Map::GetMapID()
				|| maptype != GW::Map::GetInstanceType()) { // only yell at the user once
				mapid = GW::Map::GetMapID();
				maptype = GW::Map::GetInstanceType();
				Log::Error("Cannot find %s", chat);
			}
		}
	}
}
GW::Item* Pcon::FindVacantStackOrSlotInInventory(GW::Item* likeItem) { // Scan bags, find an incomplete stack, or otherwise an empty slot.
	GW::Bag** bags = GW::Items::GetBagArray();
	if (bags == nullptr) return nullptr;
	int emptySlotIdx = -1;
	GW::Bag* emptyBag = nullptr;
	
	for (int bagIndex = static_cast<int>(GW::Constants::Bag::Bag_2); bagIndex > 0; --bagIndex) { // Work from last bag to first; pcons at bottom of inventory
		GW::Bag* bag = bags[bagIndex];
		if (bag == nullptr) continue;	// No bag, skip
		GW::ItemArray items = bag->items;
		if (!items.valid()) continue;	// No item array, skip
		for (size_t i = items.size(); i > 0; i--) { // Work from last slot to first; pcons at bottom of inventory
			int slotIndex = i - 1;
			GW::Item* item = items[slotIndex];
			if (!item || item == nullptr) {
                // Reserve this slot for later
				if (!emptyBag && ReserveSlotForMove(bag->index, slotIndex)) {
					emptySlotIdx = slotIndex;
					emptyBag = bag;
				}
				continue;
			}
			if (!likeItem)
				continue; // Only compare with existing items if we have something to compare to.
			if (likeItem->mod_struct_size != item->mod_struct_size || likeItem->model_id != item->model_id)
				continue; // This is not the same item - apples and pears.
			if (likeItem->item_id == item->item_id || item->quantity >= 250)
				continue; // Comparing against yourself, or this item is already a full stack.
            if (ReserveSlotForMove(bag->index, item->slot)) {
                if (emptySlotIdx >= 0) // Unlock the empty slot.
                    UnreserveSlotForMove(emptyBag->index, emptySlotIdx);
                return item;	// Found a stack with space.
            }
		}
	}
	if (!emptyBag) return nullptr;
	GW::Item* item = new GW::Item(); // Create a "fake" item...
	item->bag = emptyBag; // ...that belongs in the empty bag/slot we found...
	item->slot = emptySlotIdx;
	item->quantity = 0; // ...with 250 available slots.
    item->item_id = 0; // item_id to 0 for comparison
	return item;
}
// Blocking - run on a separate thread! Waits for the item in this bag/slot to change e.g. waiting for a MoveItem request
GW::Item* Pcon::WaitForSlotUpdate(GW::Bag* bag, int slot, uint32_t timeout_seconds) {
    if (bag == nullptr)
        return nullptr;
    time_t start = time(nullptr);
    int prevQty = 0, prevId = 0;
    if (bag->items[slot]) {
        prevQty = bag->items[slot]->quantity;
        prevId = bag->items[slot]->item_id;
    }
    size_t ms_sleep = 150;
	size_t i = 0;
    uint32_t timeout_ms = timeout_seconds * 1000;
    while(i < timeout_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
		i += 150;
        GW::Item* curItem = bag->items[slot];
        if (!curItem) {
            if (prevId)
                return nullptr; // Item removed.
            continue;
        }
        else {
			if (curItem->item_id != prevId || curItem->quantity != prevQty) {
				return curItem; // Item swapped for another or qty changed
			}
        }
    }
    return nullptr;
}
// Similar to GW::Items::MoveItem, except this returns amount moved and uses the split stack header when needed.
// Most of this logic should be integrated back into GWCA repo, but I've written it here for GWToolbox
int Pcon::MoveItem(GW::Item *item, GW::Bag *bag, int slot, int quantity = 0) {
	if (slot < 0) return 0;
	if (!item || !bag) return 0;
	if (bag->items.size() < (unsigned)slot) return 0;
	if (quantity < 1) quantity = item->quantity;
	if (quantity > item->quantity) quantity = item->quantity;
	GW::Item* destItem = bag->items.valid() ? bag->items[slot] : nullptr;
	int originalQuantity = destItem ? destItem->quantity : 0;
	int vacantQuantity = 250 - originalQuantity;
	if (quantity > 1 && vacantQuantity < quantity) quantity = vacantQuantity;
	if (quantity == item->quantity) { // Move the whole thing.
		GW::CtoS::SendPacket(0x10, CtoGS_MSGMoveItem, item->item_id, bag->bag_id, slot);
	}
	else { // Split stack
		GW::CtoS::SendPacket(0x14, CtoGS_MSGSplitStack, item->item_id, quantity, bag->bag_id, slot);
	}
	return quantity;
}
// Blocking function - waits for item moves etc, dont run on main thread. False if not ready (i.e. keep trying)
bool Pcon::RefillBlocking() {
    // Simple function to check for return or not.
    if (!enabled || !PconsWindow::Instance().GetEnabled() || maptype != GW::Constants::InstanceType::Outpost) {
        return true; // This Pcon disabled, all pcons disabled, or not in outpost.
    }
    if (!refill_if_below_threshold || quantity >= threshold) {
        return true; // Not allowed to auto refill, or above the threshold already.
    }
	// Wait until inventory is ready.
	uint32_t timeout_ms = 5000, i = 0;
	while (i < timeout_ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
		i += 150;
		if (maptype != GW::Constants::InstanceType::Outpost)
			continue;
		if (!GW::Items::GetBagArray())
			continue;
		break;
	}
	//printf("Begin %s\n", chat);
    int points_needed = threshold - CheckInventory(); // quantity is actually points e.g. 20 grog = 60 quantity
    int points_moved = 0;
    int items_moved = 0;
    int quantity_to_move = 0;
    if (points_needed < 1)
        return true;
    GW::Bag** bags = GW::Items::GetBagArray();
    if (bags == nullptr)
        return false;
    for (int bagIndex = static_cast<int>(GW::Constants::Bag::Storage_1); bagIndex <= static_cast<int>(GW::Constants::Bag::Storage_14); ++bagIndex) {
        if (points_needed < 1) return true;
        GW::Bag* storageBag = bags[bagIndex];
        if (storageBag == nullptr) continue;	// No bag, skip
        GW::ItemArray storageItems = storageBag->items;
        if (!storageItems.valid()) continue;	// No item array, skip
        for (size_t i = 0; i < storageItems.size(); i++) {
            if (points_needed < 1) return true;
            GW::Item* storageItem = storageItems[i];
            if (storageItem == nullptr) continue; // No item, skip
            int points_per_item = QuantityForEach(storageItem);
            if (points_per_item < 1) continue; // This is not the pcon you're looking for...
            GW::Item* inventoryItem = FindVacantStackOrSlotInInventory(storageItem); // Now find a slot in inventory to move them to.
            if (inventoryItem == nullptr)
                return true; // No space for more pcons in inventory.
            quantity_to_move = (int)ceil((float)points_needed / (float)points_per_item);
            if (quantity_to_move > storageItem->quantity)		quantity_to_move = storageItem->quantity;
            // Get slot of bag moving into
            GW::ItemArray invBagItems = inventoryItem->bag->items;
            int slot_to = inventoryItem->slot;
            GW::Bag* bag_to = inventoryItem->bag;
            int qty_before = inventoryItem->quantity;
            MoveItem(storageItem, bag_to, slot_to, quantity_to_move);
            GW::Item* updatedItem = WaitForSlotUpdate(bag_to, slot_to); // NOTE: This is blocking, dont run on main thread
            UnreserveSlotForMove(bag_to->index, slot_to);
            if (!updatedItem) {
                printf("Failed to wait for slot move\n");
                return false;
            }
			int this_moved = (updatedItem->quantity - qty_before) * points_per_item;
			points_needed -= this_moved;
            printf("Moved %d points for %s to %d %d successfully\n", this_moved, chat, updatedItem->bag->index, updatedItem->slot);
        }
    }
	return true;
}
void Pcon::Refill() {
    if (refilling)
        return; // Still going
    refilling = true;
    refill_thread = std::thread([&] {
		//printf("Refilling %s\n", chat);
		quantity = CheckInventory();
		quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
		RefillBlocking();
		quantity = CheckInventory();
		quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
        //printf("Refilled %s\n", chat);
        refill_attempted = true;
        refilling = false;
    });
    refill_thread.detach();
}
int Pcon::CheckInventory(bool* used, int* used_qty_ptr,int from_bag, int to_bag) const {
	int count = 0;
	int used_qty = 0;
	GW::Bag** bags = GW::Items::GetBagArray();
	if (bags == nullptr) return -1;
	for (int bagIndex = from_bag; bagIndex <= to_bag; ++bagIndex) {
		GW::Bag* bag = bags[bagIndex];
		if (bag == nullptr) continue;	// No bag, skip
		GW::ItemArray items = bag->items;
		if (!items.valid()) continue;	// No item array, skip
		for (size_t i = 0; i < items.size(); i++) {
			GW::Item* item = items[i];
			if (item == nullptr) continue; // No item, skip
			int qtyea = QuantityForEach(item);
			if (qtyea < 1) continue; // This is not the pcon you're looking for...
			if (used != nullptr && !*used) {
				GW::Items::UseItem(item);
				*used = true;
				used_qty = qtyea;
			}
			count += qtyea * item->quantity;
		}
	}
	if (used_qty_ptr) *used_qty_ptr = used_qty;
	return count - used_qty;
}
bool Pcon::CanUseByInstanceType() const {
	return maptype == GW::Constants::InstanceType::Explorable;
}
void Pcon::LoadSettings(CSimpleIni* inifile, const char* section) {
	char buf_active[256];
	char buf_threshold[256];
	char buf_visible[256];
	snprintf(buf_active, 256, "%s_active", ini);
	snprintf(buf_threshold, 256, "%s_threshold", ini);
	snprintf(buf_visible, 256, "%s_visible", ini);
	enabled = inifile->GetBoolValue(section, buf_active);
	threshold = inifile->GetLongValue(section, buf_threshold, threshold);
	visible = inifile->GetBoolValue(section, buf_visible, true);
}
void Pcon::SaveSettings(CSimpleIni* inifile, const char* section) {
	char buf_active[256];
	char buf_threshold[256];
	char buf_visible[256];
	snprintf(buf_active, 256, "%s_active", ini);
	snprintf(buf_threshold, 256, "%s_threshold", ini);
	snprintf(buf_visible, 256, "%s_visible", ini);
	inifile->SetBoolValue(section, buf_active, enabled);
	inifile->SetLongValue(section, buf_threshold, threshold);
	inifile->SetBoolValue(section, buf_visible, visible);
}

// ================================================
int PconGeneric::QuantityForEach(const GW::Item* item) const {
	if (item->model_id == (DWORD)itemID) return 1;
	return 0;
}
bool PconGeneric::CanUseByEffect() const {
	DWORD player_id = GW::Agents::GetPlayerId();
	if (!player_id) return false;  // player doesn't exist?

	GW::AgentEffectsArray AgEffects = GW::Effects::GetPartyEffectArray();
	if (!AgEffects.valid()) return !map_has_effects_array; // When the player doesn't have any effect, the array is not created.

	GW::EffectArray *effects = NULL;
	for (size_t i = 0; i < AgEffects.size() && !effects; i++) {
		if (AgEffects[i].agent_id == player_id)
			effects = &AgEffects[i].effects;
	}

	if (!effects || !effects->valid())
		return !map_has_effects_array; // When the player doesn't have any effect, the array is not created.

	for (DWORD i = 0; i < effects->size(); i++) {
		if (effects->at(i).skill_id == (DWORD)effectID)
			return effects->at(i).GetTimeRemaining() < 1000;
	}
	return true;
}

// ================================================
bool PconCons::CanUseByEffect() const {
	if (!PconGeneric::CanUseByEffect()) return false;
	if (!GW::PartyMgr::GetIsPartyLoaded()) return false;

	GW::MapAgentArray mapAgents = GW::Agents::GetMapAgentArray();
	if (!mapAgents.valid()) return false;

	int n_players = GW::Agents::GetAmountOfPlayersInInstance();
	for (int i = 1; i <= n_players; ++i) {
		DWORD currentPlayerAgID = GW::Agents::GetAgentIdByLoginNumber(i);
		if (currentPlayerAgID <= 0) return false;
		if (currentPlayerAgID >= mapAgents.size()) return false;
		if (mapAgents[currentPlayerAgID].GetIsDead()) return false;
	}

	return true;
}

void PconRefiller::Draw(IDirect3DDevice9* device) {
    if (maptype == GW::Constants::InstanceType::Explorable)
        return; // Don't draw in explorable areas - this is only for refilling in an outpost!
    Pcon::Draw(device);
}

// ================================================
bool PconCity::CanUseByInstanceType() const {
	return maptype == GW::Constants::InstanceType::Outpost;
}
bool PconCity::CanUseByEffect() const {
	GW::Agent* player = GW::Agents::GetPlayer();
	if (player == nullptr) return false;
	if (player->move_x == 0.0f && player->move_y == 0.0f) return false;

	GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
	if (!effects.valid()) return !map_has_effects_array; // When the player doesn't have any effect, the array is not created.

	for (DWORD i = 0; i < effects.size(); i++) {
		if (effects[i].GetTimeRemaining() < 1000) continue;
		if (effects[i].skill_id == (DWORD)SkillID::Sugar_Rush_short
			|| effects[i].skill_id == (DWORD)SkillID::Sugar_Rush_medium
			|| effects[i].skill_id == (DWORD)SkillID::Sugar_Rush_long
			|| effects[i].skill_id == (DWORD)SkillID::Sugar_Jolt_short
			|| effects[i].skill_id == (DWORD)SkillID::Sugar_Jolt_long) {
			return false; // already on
		}
	}
	return true;
}
int PconCity::QuantityForEach(const GW::Item* item) const {
	switch (item->model_id) {
	case ItemID::CremeBrulee:
	case ItemID::ChocolateBunny:
	case ItemID::Fruitcake:
	case ItemID::SugaryBlueDrink:
	case ItemID::RedBeanCake:
	case ItemID::JarOfHoney:
	case ItemID::KrytalLokum:
	case ItemID::MandragorRootCake:
		return 1;
	default:
		return 0;
	}
}

// ================================================
bool PconAlcohol::CanUseByEffect() const {
	return AlcoholWidget::Instance().GetAlcoholLevel() <= 1;
}
int PconAlcohol::QuantityForEach(const GW::Item* item) const {
	switch (item->model_id) {
	case ItemID::Eggnog:
	case ItemID::DwarvenAle:
	case ItemID::HuntersAle:
	case ItemID::Absinthe:
	case ItemID::WitchsBrew:
	case ItemID::Ricewine:
	case ItemID::ShamrockAle:
	case ItemID::Cider:
		return 1;
	case ItemID::Grog:
	case ItemID::SpikedEggnog:
	case ItemID::AgedDwarvenAle:
	case ItemID::AgedHuntersAle:
	case ItemID::FlaskOfFirewater:
	case ItemID::KrytanBrandy:
		return 5;
	case ItemID::Keg: {
		GW::ItemModifier *mod = item->mod_struct;
		if (mod == nullptr) return 5; // we don't think this will ever happen

		for (DWORD i = 0; i < item->mod_struct_size; i++) {
			if (mod->identifier() == 581) {
				return mod->arg3() * 5;
			}
			mod++;
		}
		return 5; // this should never happen, but we keep it as a fallback
	}
	default:
		return 0;
	}
}
void PconAlcohol::ForceUse() {
	GW::Agent* player = GW::Agents::GetPlayer();
	if (player != nullptr
		&& !player->GetIsDead()
		&& (player_id == 0 || player->agent_id == player_id)) {
		bool used = false;
		int used_qty = 0;
		int qty = CheckInventory(&used, &used_qty);
		if (used_qty == 1) {
			bool used2 = false;
			qty = CheckInventory(&used2, &used_qty); // use another!
		}

		AfterUsed(used, qty);
	}
}

// ================================================
void PconLunar::Update(int delay) {
	Pcon::Update(Pcon::lunar_delay);
}
int PconLunar::QuantityForEach(const GW::Item* item) const {
	switch (item->model_id) {
	case ItemID::LunarPig:
	case ItemID::LunarRat:
	case ItemID::LunarOx:
	case ItemID::LunarTiger:
	case ItemID::LunarDragon:
	case ItemID::LunarHorse:
	case ItemID::LunarRabbit:
	case ItemID::LunarSheep:
	case ItemID::LunarSnake:
	case ItemID::LunarMonkey:
	case ItemID::LunarRooster:
	case ItemID::LunarDog:
		return 1;
	default:
		return 0;
	}
}
bool PconLunar::CanUseByEffect() const {
	DWORD player_id = GW::Agents::GetPlayerId();
	if (!player_id) return false;  // player doesn't exist?

	GW::AgentEffectsArray AgEffects = GW::Effects::GetPartyEffectArray();
	if (!AgEffects.valid()) return !map_has_effects_array; // When the player doesn't have any effect, the array is not created.

	GW::EffectArray *effects = NULL;
	for (size_t i = 0; i < AgEffects.size() && !effects; i++) {
		if (AgEffects[i].agent_id == player_id)
			effects = &AgEffects[i].effects;
	}

	if (!effects || !effects->valid())
		return !map_has_effects_array; // When the player doesn't have any effect, the array is not created.

	for (DWORD i = 0; i < effects->size(); i++) {
		if (effects->at(i).skill_id == (DWORD)GW::Constants::SkillID::Lunar_Blessing)
			return false; // already on
	}
	return true;
}
