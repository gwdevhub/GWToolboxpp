#include "stdafx.h"
#include "Pcons.h"
#include <mutex>
#include <d3dx9tex.h>

#include <GWCA\Constants\Constants.h>

#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\CharContext.h>

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


namespace {
	GW::EffectArray* GetEffects() {
		GW::Agent* player = GW::Agents::GetPlayer();
		if (!player) return nullptr;  // player doesn't exist?

		GW::AgentEffectsArray AgEffects = GW::Effects::GetPartyEffectArray();
		// @Remark:
		// If the agent doesn't have any effects, the effect array is not created.
		// We also know that the player exist, so the effect can be created.
		if (!AgEffects.valid()) return nullptr;

		GW::EffectArray* effects = NULL;
		for (size_t i = 0; i < AgEffects.size(); i++) {
			if (AgEffects[i].agent_id == player->agent_id && AgEffects[i].effects.valid()) {
				return &AgEffects[i].effects;
			}
		}

		// That's a bit of an odd cases, but we choose to take no risk
		// and not pop the pcons.
		return nullptr;
	}
}
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
bool Pcon::pcons_by_character = true;
static std::mutex reserve_slot_mutex;

// 22 is the highest bag index. 25 is the most slots in any single bag.
std::vector<std::vector<clock_t>> Pcon::reserved_bag_slots(22, std::vector<clock_t>(25));

// ================================================
Pcon::Pcon(const char* chatname,
	const char* abbrevname,
    const char* ininame,
    const wchar_t* filename_, WORD res_id_,
    ImVec2 uv0_, ImVec2 uv1_, int threshold_,
    const char* desc_)
    : chat(chatname), abbrev(abbrevname), ini(ininame), filename(filename_), res_id(res_id_), uv0(uv0_), uv1(uv1_), threshold(threshold_), timer(TIMER_INIT()) {
    enabled = settings_by_charname[L"default"] = new bool(false);
	if (desc_) desc = desc_;
}
Pcon::~Pcon() {
	if (refill_thread.joinable())
		refill_thread.join();
    for (auto c : settings_by_charname) {
        delete c.second;
    }
    settings_by_charname.clear();
}
// Resets pcon counters so it needs to recalc number and refill.
void Pcon::ResetCounts() {
	refill_attempted = false;
	pcon_quantity_checked = false;
	for (auto i : reserved_bag_slots) {
		i.clear();
	}
}
void Pcon::SetEnabled(bool b) {
	if (*enabled == b) return;
	*enabled = b;
	ResetCounts();
}
wchar_t* Pcon::SetPlayerName() {
    GW::GameContext* g = GW::GameContext::instance();
    if (!g || !g->character || !g->character->player_name)
        return nullptr;
    enabled = GetSettingsByName(pcons_by_character ? g->character->player_name : L"default");
    return g->character->player_name;
}
void Pcon::Draw(IDirect3DDevice9* device) {
	if (texture == nullptr) return;
	ImVec2 pos = ImGui::GetCursorPos();
	ImVec2 s(size, size);
	ImVec4 bg = IsEnabled() ? ImColor(enabled_bg_color) : ImVec4(0, 0, 0, 0);
	ImVec4 tint(1, 1, 1, 1);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, 0, bg, tint)) {
        Toggle();
	}
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered() && desc.size())
		ImGui::SetTooltip(desc.c_str());
	if (maptype != GW::Constants::InstanceType::Loading) {
		ImFont* f = GuiUtils::GetFont(GuiUtils::f20);
		ImVec2 nextPos = ImGui::GetCursorPos();
		ImGui::PushFont(f);
		ImVec4 color;
		if (quantity == 0) color = ImVec4(1, 0, 0, 1);
		else if (quantity < threshold) color = ImVec4(1, 1, 0, 1);
		else color = ImVec4(0, 1, 0, 1);


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
	}
	
	ImGui::SetCursorPos(pos);
	ImGui::Dummy(ImVec2(size, size));
}
void Pcon::Initialize() {
	Resources::Instance().LoadTextureAsync(&texture, Resources::GetPath(L"img/pcons", filename), res_id);
}
void Pcon::Update(int delay) {
	if (mapid != GW::Map::GetMapID() || maptype != GW::Map::GetInstanceType()) { // Map changed; reset vars
		mapid = GW::Map::GetMapID();
		maptype = GW::Map::GetInstanceType();
        SetPlayerName();
		ResetCounts();
        refill_attempted = maptype != GW::Constants::InstanceType::Outpost;
	}
    if (maptype == GW::Constants::InstanceType::Loading)
        return;
    // Refill pcons if needed.
    if (!refill_attempted) {
        Refill();
        return;
    }
    // Check pcon count in inventory
    if (!pcon_quantity_checked) {
        int qty = CheckInventory();
        if (qty < 0) return; // Inventory pointers not ready
        quantity = qty;
        if (maptype == GW::Constants::InstanceType::Outpost) {
            quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
            if (IsEnabled() && PconsWindow::Instance().GetEnabled()) {
                // Only warn user of low pcon count if is enabled and we're in an outpost.
                if (quantity == 0) {
                    Log::Error("No more %s items found", chat);
                }
                else if (quantity < threshold) {
                    Log::Warning("Low on %s", chat);
                }
            }
        }
        pcon_quantity_checked = true;
    }
    // === Use item if possible ===
    if (IsEnabled() && PconsWindow::Instance().GetEnabled()) {
        if (delay < 0) delay = Pcon::pcons_delay;
        player = GW::Agents::GetPlayer();
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

}
GW::Bag* Pcon::GetBag(uint32_t bag_id) {
	GW::Bag** bags = GW::Items::GetBagArray();
	if (!bags) return nullptr;
	for (size_t i = 1; i < static_cast<int>(GW::Constants::BagMax); i++) {
		GW::Bag* bag = bags[i];
		if (!bag) continue;
		if (bag->bag_id == bag_id)
			return bag;
	}
	return nullptr;
}
bool Pcon::ReserveSlotForMove(int bagIndex, int slot) { // Should NOT be used directly, supposed to be used with a mutex.
    std::lock_guard<std::mutex> lk(reserve_slot_mutex); // Lock to reserve the slot once found - pcons check on different threads!
	if (IsSlotReservedForMove(bagIndex, slot)) {
		//printf("Slot %d %d, %d is LOCKED\n", bagIndex, slot, reserved_bag_slots.at(bagIndex).at(slot));
		return false;
	}
	reserved_bag_slots.at(bagIndex).at(slot) = TIMER_INIT();
	//printf("locking %d %d, %d\n", bagIndex, slot, reserved_bag_slots.at(bagIndex).at(slot));
	return true;
}
bool Pcon::UnreserveSlotForMove(int bagIndex, int slot) { // Should NOT be used directly, supposed to be used with a mutex.
    std::lock_guard<std::mutex> lk(reserve_slot_mutex); // Lock to reserve the slot once found - pcons check on different threads!
    reserved_bag_slots.at(bagIndex).at(slot) = 0;
	//printf("unlocking %d %d, %d\n", bagIndex, slot, reserved_bag_slots.at(bagIndex).at(slot));
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
				if (disable_when_not_found) SetEnabled(false);
			}
		}
		else {
			// we should have used but didn't find the item
			if (disable_when_not_found) SetEnabled(false);
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
	
	for (size_t bagIndex = static_cast<size_t>(GW::Constants::Bag::Bag_2); bagIndex > 0; --bagIndex) { // Work from last bag to first; pcons at bottom of inventory
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
GW::Item* Pcon::MoveItem(GW::Item* item, GW::Bag* bag, int slot, int quantity, uint32_t timeout_seconds) {
	if (slot < 0) return 0;
	if (!item || !bag) return 0;
	if (bag->items.size() < (unsigned)slot) return 0;
	if (quantity < 1) quantity = item->quantity;
	if (quantity > item->quantity) quantity = item->quantity;
	GW::Item* destItem = bag->items.valid() ? bag->items[slot] : nullptr;
	int originalQuantity = destItem ? destItem->quantity : 0;
	int originalId = destItem ? destItem->item_id : 0;
	int vacantQuantity = 250 - originalQuantity;
	if (quantity > 1 && vacantQuantity < quantity) quantity = vacantQuantity;
	int bag_id = bag->bag_id;
	if (quantity == item->quantity) { // Move the whole thing.
		GW::CtoS::SendPacket(0x10, GAME_CMSG_ITEM_MOVE, item->item_id, bag->bag_id, slot);
	}
	else { // Split stack
		GW::CtoS::SendPacket(0x14, GAME_CMSG_ITEM_SPLIT_STACK, item->item_id, quantity, bag->bag_id, slot);
	}
	if (!timeout_seconds)
		return destItem; // No wait.
	size_t i = 0;
	uint32_t timeout_ms = timeout_seconds * 1000;
	while (i < timeout_ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
		i += 150;
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
			return nullptr; // Map changed.
		GW::Bag* nbag = GetBag(bag_id);
		if (!nbag || !nbag->items.valid())
			continue;
		GW::Item* curItem = nbag->items[slot];
        if (!curItem) 
            continue; // Item not moved yet
        if (curItem->item_id != originalId || curItem->quantity != originalQuantity)
            return curItem; // Item swapped for another or qty changed
	}
	return nullptr;
}
// Blocking function - waits for item moves etc, dont run on main thread. False if not ready (i.e. keep trying)
bool Pcon::RefillBlocking() {
    auto refillable = [this]() {
		return IsEnabled() && refill_if_below_threshold
			&& GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
			&& PconsWindow::Instance().GetEnabled();
    };
    // Simple function to check for return or not.
    if (!refillable()) return true;
	printf("Refilling %s\n", chat);
	quantity = CheckInventory();
	if (quantity >= threshold) return true;
	quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
    int points_needed = threshold - quantity; // quantity is actually points e.g. 20 grog = 60 quantity
    int points_moved = 0;
    int items_moved = 0;
    int quantity_to_move = 0;
    if (points_needed < 1)
        return true;
    GW::Bag** bags = GW::Items::GetBagArray();
    if (bags == nullptr)
        return false;
    for (size_t bagIndex = static_cast<size_t>(GW::Constants::Bag::Storage_1); bagIndex <= static_cast<size_t>(GW::Constants::Bag::Storage_14); ++bagIndex) {
        if (points_needed < 1) return true;
        GW::Bag* storageBag = bags[bagIndex];
        if (storageBag == nullptr) continue;	// No bag, skip
        GW::ItemArray storageItems = storageBag->items;
        if (!storageItems.valid()) continue;	// No item array, skip
        for (size_t i = 0; i < storageItems.size() && storageItems.valid(); i++) {
            if (points_needed < 1) return true;
            GW::Item* storageItem = storageItems[i];
            if (storageItem == nullptr) continue; // No item, skip
            int points_per_item = QuantityForEach(storageItem);
            if (points_per_item < 1) continue; // This is not the pcon you're looking for...
            GW::Item* inventoryItem = FindVacantStackOrSlotInInventory(storageItem); // Now find a slot in inventory to move them to.
			if (inventoryItem == nullptr) {
				printf("No more space for %s", chat);
				return true; // No space for more pcons in inventory.
			}
            quantity_to_move = (int)ceil((float)points_needed / (float)points_per_item);
            if (quantity_to_move > storageItem->quantity)		quantity_to_move = storageItem->quantity;
            // Get slot of bag moving into
            GW::ItemArray invBagItems = inventoryItem->bag->items;
            int slot_to = inventoryItem->slot;
            GW::Bag* bag_to = inventoryItem->bag;
            int qty_before = inventoryItem->quantity;
			if(inventoryItem->quantity == 0)
				delete inventoryItem; // Empty slot was returned; free memory here.
            if (!refillable() || quantity >= threshold) return true;
			// This next statement blocks until move completes.
			GW::Item* updatedItem = MoveItem(storageItem, bag_to, slot_to, quantity_to_move, 3);
			UnreserveSlotForMove(bag_to->index, slot_to);
            if (!refillable()) return true;
            if (!updatedItem) {
                printf("Failed to wait for slot move\n");
                return false;
            }
			int this_moved = (updatedItem->quantity - qty_before) * points_per_item;
			points_needed -= this_moved;
			quantity += this_moved;
			quantity_storage -= this_moved;
            printf("Moved %d points for %s to %d %d successfully\n", this_moved, chat, updatedItem->bag->index, updatedItem->slot);
            if (!refillable() || quantity >= threshold) return true;
        }
    }
	return true;
}
void Pcon::Refill() {
    if (refilling)
        return; // Still going
    refilling = true;
	if (refill_thread.joinable())
		refill_thread.join();
    refill_thread = std::thread([this]() {
		RefillBlocking();
		refilling = false;
        refill_attempted = true;
    });
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
bool* Pcon::GetSettingsByName(const wchar_t* name) {
    if (settings_by_charname.find(name) == settings_by_charname.end()) {
        bool* def = settings_by_charname[L"default"];
        settings_by_charname[name] = new bool(*def);
    }
    return settings_by_charname[name];
}
void Pcon::LoadSettings(CSimpleIni* inifile, const char* section) {
	char buf_active[256];
	char buf_threshold[256];
	char buf_visible[256];
	snprintf(buf_active, 256, "%s_active", ini);
	snprintf(buf_threshold, 256, "%s_threshold", ini);
	snprintf(buf_visible, 256, "%s_visible", ini);
    threshold = inifile->GetLongValue(section, buf_threshold, threshold);

    bool* def = GetSettingsByName(L"default");
    *def = inifile->GetBoolValue(section, buf_active, *def);
    visible = inifile->GetBoolValue(section, buf_visible, visible);

    CSimpleIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    std::string sectionsub(section);
    sectionsub += ':';
    size_t section_len = sectionsub.size();
    for (CSimpleIni::Entry& entry : entries) {
        if (strncmp(entry.pItem, sectionsub.c_str(), section_len) != 0)
            continue;
        std::string str(entry.pItem);
        size_t charname_pos = section_len;
        std::wstring charname = GuiUtils::StringToWString(entry.pItem + charname_pos);
        bool* char_enabled = GetSettingsByName(charname.c_str());
        *char_enabled = inifile->GetBoolValue(entry.pItem, buf_active, *char_enabled);
    }
}
void Pcon::SaveSettings(CSimpleIni* inifile, const char* section) {
	char buf_active[256];
	char buf_threshold[256];
	char buf_visible[256];
	snprintf(buf_active, 256, "%s_active", ini);
	snprintf(buf_threshold, 256, "%s_threshold", ini);
	snprintf(buf_visible, 256, "%s_visible", ini);
    inifile->SetLongValue(section, buf_threshold, threshold);
    inifile->SetBoolValue(section, buf_visible, visible);

    for (auto charname_pcons : settings_by_charname) {
        bool* enabled = charname_pcons.second;
        if (charname_pcons.first == L"default") {
            inifile->SetBoolValue(section, buf_active, *enabled);
            continue;
        }
        std::wstring charname = charname_pcons.first;
        if (charname.empty())
            continue;
        std::string char_section(section);
        char_section.append(":");
        char_section.append(GuiUtils::WStringToString(charname).c_str());
        inifile->SetBoolValue(char_section.c_str(), buf_active, *enabled);
    }
}

// ================================================
int PconGeneric::QuantityForEach(const GW::Item* item) const {
	if (item->model_id == (DWORD)itemID) return 1;
	return 0;
}
bool PconGeneric::CanUseByEffect() const {
	GW::Agent* player = GW::Agents::GetPlayer();
	if (!player) return false;  // player doesn't exist?

	GW::EffectArray* effects = GetEffects();
	if (!effects) return true;

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
	if (!player) return false;  // player doesn't exist?

	GW::EffectArray* effects = GetEffects();
	if (!effects) return true;

	if (player->move_x == 0.0f && player->move_y == 0.0f) return false;

	for (DWORD i = 0; i < effects->size(); i++) {
		if (effects->at(i).GetTimeRemaining() < 1000) continue;
		if (effects->at(i).skill_id == (DWORD)SkillID::Sugar_Rush_short
			|| effects->at(i).skill_id == (DWORD)SkillID::Sugar_Rush_medium
			|| effects->at(i).skill_id == (DWORD)SkillID::Sugar_Rush_long
			|| effects->at(i).skill_id == (DWORD)SkillID::Sugar_Jolt_short
			|| effects->at(i).skill_id == (DWORD)SkillID::Sugar_Jolt_long) {
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
			if (mod->identifier() == 0x2458) {
				return mod->arg2() * 5;
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
	GW::Agent* player = GW::Agents::GetPlayer();
	if (!player) return false;  // player doesn't exist?

	GW::EffectArray* effects = GetEffects();
	if (!effects) return true;

	for (DWORD i = 0; i < effects->size(); i++) {
		if (effects->at(i).skill_id == (DWORD)GW::Constants::SkillID::Lunar_Blessing)
			return false;
	}
	return true;
}
