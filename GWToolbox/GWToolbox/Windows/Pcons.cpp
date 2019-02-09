#include "Pcons.h"

#include <d3dx9tex.h>

#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\CtoSMgr.h>

#include <logger.h>
#include "GuiUtils.h"
#include <Modules\Resources.h>
#include <Windows\PconsWindow.h>
#include <mutex>

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
	ImVec2 uv0_, ImVec2 uv1_, int threshold_)
	: chat(chatname), ini(ininame), threshold(threshold_),
	uv0(uv0_), uv1(uv1_), texture(nullptr),
	enabled(false), quantity(0), timer(TIMER_INIT()) {

	Resources::Instance().LoadTextureAsync(&texture, 
		Resources::GetPath(L"img/pcons", filename), res_id);
}
void Pcon::Draw(IDirect3DDevice9* device) {
	if (texture == nullptr) return;
	ImVec2 pos = ImGui::GetCursorPos();
	ImVec2 s(size, size);
	ImVec4 bg = enabled ? ImColor(enabled_bg_color) : ImVec4(0, 0, 0, 0);
	ImVec4 tint(1, 1, 1, 1);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, -1, bg, tint)) {
		enabled = !enabled;
		pcon_quantity_checked = false;
		ScanInventory();
	}
	ImGui::PopStyleColor();

	ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
	ImVec4 color;
	if (quantity == 0) color = ImVec4(1, 0, 0, 1);
	else if (quantity < threshold) color = ImVec4(1, 1, 0, 1);
	else color = ImVec4(0, 1, 0, 1);

	ImGui::SetCursorPos(ImVec2(pos.x + 3, pos.y + 3));
	ImGui::TextColored(ImVec4(0, 0, 0, 1), "%d", quantity);
	ImGui::SetCursorPos(ImVec2(pos.x + 2, pos.y + 2));
	ImGui::TextColored(color, "%d", quantity);
	ImGui::PopFont();

	ImGui::SetCursorPos(pos);
	ImGui::Dummy(ImVec2(size, size));
}
void Pcon::Update(int delay) {
	if (!enabled) return; // not enabled, do nothing
	if (delay < 0) delay = Pcon::pcons_delay;

	GW::Agent* player = GW::Agents::GetPlayer();

	 // === Use item if possible ===
	if (player != nullptr
		&& !player->GetIsDead()
		&& (player_id == 0 || player->Id == player_id)
		&& TIMER_DIFF(timer) > delay
		&& CanUseByInstanceType()
		&& CanUseByEffect()) {

		bool used = false;
		int qty = CheckInventory(&used);

		AfterUsed(used, qty);
	}

	// === Warn the user if zoning into outpost with quantity = 0 or low ===
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
		&& (mapid != GW::Map::GetMapID() || maptype != GW::Map::GetInstanceType())) {
		mapid = GW::Map::GetMapID();
		maptype = GW::Map::GetInstanceType();
		pcon_quantity_checked = false;
		std::vector<std::vector<clock_t>>(22, std::vector<clock_t>(25)).swap(reserved_bag_slots); // Clear reserved slots.
	}	
}


bool Pcon::ReserveSlotForMove(int bagIndex, int slot) { // Should NOT be used directly, supposed to be used with a mutex.
	if (IsSlotReservedForMove(bagIndex, slot)) return false;
	reserved_bag_slots.at(bagIndex).at(slot) = TIMER_INIT();
	return true;
}
bool Pcon::IsSlotReservedForMove(int bagIndex, int slot) {
	clock_t slot_reserved_at = reserved_bag_slots.at(bagIndex).at(slot);
	bool is_reserved = slot_reserved_at && TIMER_DIFF(slot_reserved_at) < 1000; // 1000ms is reasonable for CtoS then StoC
	return is_reserved;
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
		} else {
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
void Pcon::ScanInventory() {
	int qty = CheckInventory();
	if (qty >= 0) quantity = qty;
	if (!enabled || !PconsWindow::Instance().GetEnabled()) {
		pcon_quantity_checked = false; // Not enabled; enforce a re-check when it is enabled again.
	}
	if (enabled && PconsWindow::Instance().GetEnabled() && !pcon_quantity_checked && maptype == GW::Constants::InstanceType::Outpost) {
		pcon_quantity_checked = true;
		if (quantity < threshold && refill_if_below_threshold) {
			quantity += Refill();
		}
		if (quantity == 0) {
			Log::Error("No more %s items found", chat);
		}
		else if (quantity < threshold) {
			Log::Warning("Low on %s", chat);
		}
	}
}
GW::Item* Pcon::FindVacantStackOrSlotInInventory() { // Scan bags, find an incomplete stack, or otherwise an empty slot.
	GW::Bag** bags = GW::Items::GetBagArray();
	if (bags == nullptr) return nullptr;
	int emptySlotIdx = -1;
	int emptyBagIdx = -1;
	std::lock_guard<std::mutex> lk(reserve_slot_mutex); // Lock to reserve the slot once found - pcons check on different threads!
	for (int bagIndex = 4; bagIndex > 0; --bagIndex) { // Work from last bag to first; pcons at bottom of inventory
		GW::Bag* bag = bags[bagIndex];
		if (bag == nullptr) continue;	// No bag, skip
		GW::ItemArray items = bag->Items;
		if (!items.valid()) continue;	// No item array, skip
		for (size_t i = items.size(); i > 0; i--) { // Work from last slot to first; pcons at bottom of inventory
			int slotIndex = i-1;
			GW::Item* item = items[slotIndex];
			if (!item || item == nullptr) {
				if (emptySlotIdx < 0 && !IsSlotReservedForMove(bagIndex, slotIndex)) {
					emptySlotIdx = slotIndex;
					emptyBagIdx = bagIndex;
				}
				continue;
			}
			int qtyea = QuantityForEach(item); 
			if (qtyea < 1 || item->Quantity >= 250) continue; // This is not the pcon you're looking for...
			if (IsSlotReservedForMove(bagIndex, item->Slot)) continue; // This slot already reserved.
			ReserveSlotForMove(bagIndex, item->Slot);
			return item;	// Found a stack with space.
		}
	}
	if (emptyBagIdx < 0 || emptySlotIdx < 0) return nullptr;
	GW::Item* item = new GW::Item(); // Create a "fake" item...
	item->Bag = bags[emptyBagIdx]; // ...that belongs in the empty bag/slot we found...
	item->Slot = emptySlotIdx;
	item->Quantity = 0; // ...with 250 available slots.
	ReserveSlotForMove(emptyBagIdx, emptySlotIdx); // Reserved this slot for moving for next 1000ms
	return item;
}
// Similar to GW::Items::MoveItem, except this returns amount moved and uses the split stack header when needed.
// Most of this logic should be integrated back into GWCA repo, but I've written it here for GWToolbox
int Pcon::MoveItem(GW::Item *item, GW::Bag *bag, int slot, int quantity = 0) {
	if (slot < 0) return 0;
	if (!item || !bag) return 0;
	if (bag->Items.size() < (unsigned)slot) return 0;
	if (quantity < 1) quantity = item->Quantity;
	if (quantity > item->Quantity) quantity = item->Quantity;
	GW::Item* destItem = bag->Items.valid() ? bag->Items[slot] : nullptr;
	int originalQuantity = destItem ? destItem->Quantity : 0;
	int vacantQuantity = 250 - originalQuantity;
	if (quantity > 1 && vacantQuantity < quantity) quantity = vacantQuantity;
	if (quantity == item->Quantity) { // Move the whole thing.
		GW::CtoS::SendPacket(0x10, CtoGS_MSGMoveItem, item->ItemId, bag->BagId, slot);
	} else { // Split stack
		GW::CtoS::SendPacket(0x14, CtoGS_MSGSplitStack, item->ItemId, quantity, bag->BagId, slot);
	}
	return quantity;
}
int Pcon::Refill() {
	// Log::Info("Refilling %s", chat);
	int amount_needed = threshold - quantity;
	int amount_moved = 0;
	if (amount_needed < 1) return amount_moved;
	GW::Bag** bags = GW::Items::GetBagArray();
	if (bags == nullptr) return amount_moved;
	for (int bagIndex = 8; bagIndex <= 21; ++bagIndex) {
		if (amount_needed < 1) return true;
		GW::Bag* bag = bags[bagIndex];
		if (bag == nullptr) continue;	// No bag, skip
		GW::ItemArray items = bag->Items;
		if (!items.valid()) continue;	// No item array, skip
		for (size_t i = 0; i < items.size(); i++) {
			if (amount_needed < 1) return amount_moved;
			GW::Item* storageItem = items[i];
			if (storageItem == nullptr) continue; // No item, skip
			int qtyea = QuantityForEach(storageItem);
			if (qtyea < 1) continue; // This is not the pcon you're looking for...
			// Found some pcons in storage to pull from - now find a slot in inventory to move them to.
			qtyea = storageItem->Quantity; // Reuse variable.
			GW::Item* inventoryItem = FindVacantStackOrSlotInInventory();
			if (inventoryItem == nullptr) return amount_moved; // No space for more pcons in inventory.
			int amount_to_move = storageItem->Quantity;
			if (amount_to_move > amount_needed)		amount_to_move = amount_needed;
			amount_moved += MoveItem(storageItem, inventoryItem->Bag, inventoryItem->Slot,amount_to_move);
			amount_needed -= amount_moved;
		}
	}
	return amount_moved;
}
int Pcon::CheckInventory(bool* used, int* used_qty_ptr) const {
	int count = 0;
	int used_qty = 0;
	GW::Bag** bags = GW::Items::GetBagArray();
	if (bags == nullptr) return -1;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		GW::Bag* bag = bags[bagIndex];
		if (bag == nullptr) continue;	// No bag, skip
		GW::ItemArray items = bag->Items;
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
			count += qtyea * item->Quantity;
		}
	}
	if (used_qty_ptr) *used_qty_ptr = used_qty;
	return count - used_qty;
}
bool Pcon::CanUseByInstanceType() const {
	return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
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
	if (item->ModelId == (DWORD)itemID) return 1;
	return 0;
}
bool PconGeneric::CanUseByEffect() const {
	GW::AgentEffectsArray AgEffects = GW::Effects::GetPartyEffectArray();
	if (!AgEffects.valid()) return false; // don't know

	GW::EffectArray effects = AgEffects[0].Effects;
	if (!effects.valid()) return false; // don't know

	for (DWORD i = 0; i < effects.size(); i++) {
		if (effects[i].SkillId == (DWORD)effectID
			&& effects[i].GetTimeRemaining() > 1000) {
			return false; // already on
		}
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

// ================================================
bool PconCity::CanUseByInstanceType() const {
	return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost;
}
bool PconCity::CanUseByEffect() const {
	GW::Agent* player = GW::Agents::GetPlayer();
	if (player == nullptr) return false;
	if (player->MoveX == 0.0f && player->MoveY == 0.0f) return false;

	GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
	if (!effects.valid()) return true; // When the player doesn't have any effect, the array is not created.

	for (DWORD i = 0; i < effects.size(); i++) {
		if (effects[i].GetTimeRemaining() < 1000) continue;
		if (effects[i].SkillId == (DWORD)SkillID::Sugar_Rush_short
			|| effects[i].SkillId == (DWORD)SkillID::Sugar_Rush_medium
			|| effects[i].SkillId == (DWORD)SkillID::Sugar_Rush_long
			|| effects[i].SkillId == (DWORD)SkillID::Sugar_Jolt_short
			|| effects[i].SkillId == (DWORD)SkillID::Sugar_Jolt_long) {
			return false; // already on
		}
	}
	return true;
}
int PconCity::QuantityForEach(const GW::Item* item) const {
	switch (item->ModelId) {
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
	return Pcon::alcohol_level <= 1;
}
int PconAlcohol::QuantityForEach(const GW::Item* item) const {
	switch (item->ModelId) {
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
		GW::ItemModifier *mod = item->ModStruct;
		if (mod == nullptr) return 5; // we don't think this will ever happen

		for (DWORD i = 0; i < item->ModStructSize; i++) {
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
		&& (player_id == 0 || player->Id == player_id)) {
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
	switch (item->ModelId) {
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
	GW::AgentEffectsArray AgEffects = GW::Effects::GetPartyEffectArray();
	if (!AgEffects.valid()) return false; // don't know

	GW::EffectArray effects = AgEffects[0].Effects;
	if (!effects.valid()) return false; // don't know

	for (DWORD i = 0; i < effects.size(); i++) {
		if (effects[i].SkillId == (DWORD)GW::Constants::SkillID::Lunar_Blessing) {
			return false; // already on
		}
	}
	return true;
}
