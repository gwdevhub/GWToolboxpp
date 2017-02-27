#include "Pcons.h"

#include <d3dx9tex.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\PartyMgr.h>

#include "ChatLogger.h"
#include "GuiUtils.h"
#include <OtherModules\Resources.h>

using namespace GW::Constants;

float Pcon::size = 46.0f;
int Pcon::delay = 5000;
bool Pcon::disable_when_not_found = true;
DWORD Pcon::player_id = 0;

// ================================================
Pcon::Pcon(const char* file, 
	ImVec2 uv0_, ImVec2 uv1_, 
	const char* name, int threshold_) 
	: chatName(name), threshold(threshold_),
	uv0(uv0_), uv1(uv1_), texture(nullptr),
	enabled(false), quantity(-1) {

	Resources::Instance()->LoadTextureAsync(&texture, file, "img");
}
void Pcon::Draw(IDirect3DDevice9* device) {
	if (texture == nullptr) return;
	ImVec2 pos = ImGui::GetCursorPos();
	ImVec2 s(size, size);
	ImVec4 bg = enabled ? ImVec4(0.0f, 1.0f, 0.0f, 0.4f) : ImVec4(0, 0, 0, 0);
	ImVec4 tint(1, 1, 1, 1);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, -1, bg, tint)) {
		enabled = !enabled;
		ScanInventory();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s\nQuantity: %d", chatName, quantity);
	}
	ImGui::PopStyleColor();

	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f12]);
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
void Pcon::Update() {
	if (!enabled) return; // not enabled, do nothing

	GW::Agent* player = GW::Agents().GetPlayer();

	 // === Use item if possible ===
	if (player 
		&& !player->GetIsDead()
		&& (player_id == 0 || player->Id == player_id)
		&& TIMER_DIFF(timer) > delay
		&& CanUseByInstanceType()
		&& CanUseByEffect()) {

		bool used = false;
		int qty = CheckInventory(&used);

		if (qty >= 0) { // if not, bag was undefined and we just ignore everything
			quantity = qty;
			if (used) {
				timer = TIMER_INIT();
				if (quantity == 0) { // if we just used the last one
					mapid = GW::Map().GetMapID();
					maptype = GW::Map().GetInstanceType();
					ChatLogger::Err("Just used the last %s", chatName);
					if (disable_when_not_found) enabled = false;
				}
			} else {
				// we should have used but didn't find the item
				if (disable_when_not_found) enabled = false;
				if (mapid != GW::Map().GetMapID()
					|| maptype != GW::Map().GetInstanceType()) { // only yell at the user once
					mapid = GW::Map().GetMapID();
					maptype = GW::Map().GetInstanceType();
					ChatLogger::Err("Cannot find %s", chatName);
				}
			}
		}
	}

	// === Warn the user if zoning into outpost with quantity = 0 or low ===
	// todo: refill automatically ?
	if (GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost
		&& (mapid != GW::Map().GetMapID() || maptype != GW::Map().GetInstanceType())) {
		mapid = GW::Map().GetMapID();
		maptype = GW::Map().GetInstanceType();

		if (quantity == 0) {
			ChatLogger::Log("[Warning] Cannot find %s, please refill or disable", chatName);
		} else if (quantity < threshold) {
			ChatLogger::Log("[Warning] Low on %s, please refill or disable", chatName);
		}
	}
}
void Pcon::ScanInventory() {
	int qty = CheckInventory();
	if (qty >= 0) quantity = qty;
}
int Pcon::CheckInventory(bool* used) const {
	int count = 0;
	int used_qty = 0;
	GW::Bag** bags = GW::Items().GetBagArray();
	if (bags == nullptr) return -1;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		GW::Bag* bag = bags[bagIndex];
		if (bag != nullptr) {
			GW::ItemArray items = bag->Items;
			if (items.valid()) {
				for (size_t i = 0; i < items.size(); i++) {
					GW::Item* item = items[i];
					if (item != nullptr) {
						int qtyea = QuantityForEach(item);
						if (qtyea > 0) {
							if (used != nullptr && !*used) {
								GW::Items().UseItem(item);
								*used = true;
								used_qty = qtyea;
							}
							count += qtyea * item->Quantity;
						}
					}
				}
			}			
		}
	}
	return count - used_qty;
}
bool Pcon::CanUseByInstanceType() const {
	return GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable;
}

// ================================================
int PconGeneric::QuantityForEach(const GW::Item* item) const {
	if (item->ModelId == (DWORD)itemID) return 1;
	return 0;
}
bool PconGeneric::CanUseByEffect() const {
	GW::AgentEffectsArray AgEffects = GW::Effects().GetPartyEffectArray();
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

	if (!GW::Partymgr().GetIsPartyLoaded()) return false;

	GW::MapAgentArray mapAgents = GW::Agents().GetMapAgentArray();
	if (!mapAgents.valid()) return false;

	int n_players = GW::Agents().GetAmountOfPlayersInInstance();
	for (int i = 1; i <= n_players; ++i) {
		DWORD currentPlayerAgID = GW::Agents().GetAgentIdByLoginNumber(i);
		if (currentPlayerAgID <= 0) return false;
		if (currentPlayerAgID >= mapAgents.size()) return false;
		if (mapAgents[currentPlayerAgID].GetIsDead()) return false;
	}

	return true;
}

// ================================================
bool PconCity::CanUseByInstanceType() const {
	return GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost;
}
bool PconCity::CanUseByEffect() const {
	GW::Agent* player = GW::Agents().GetPlayer();
	if (player == nullptr) return false;
	if (player->MoveX == 0.0f && player->MoveY == 0.0f) return false;

	GW::AgentEffectsArray AgEffects = GW::Effects().GetPartyEffectArray();
	if (!AgEffects.valid()) return false; // don't know

	GW::EffectArray effects = AgEffects[0].Effects;
	if (!effects.valid()) return false; // don't know

	for (DWORD i = 0; i < effects.size(); i++) {
		if (effects[i].GetTimeRemaining() < 1000) continue;
		if (effects[i].SkillId == (DWORD)SkillID::Sugar_Rush_short
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
		return 1;
	default:
		return 0;
	}
}

// ================================================
bool PconAlcohol::CanUseByEffect() const {
	return GW::Effects().GetAlcoholLevel() <= 1;
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
	case ItemID::AgedHungersAle:
	case ItemID::Keg:
	case ItemID::FlaskOfFirewater:
	case ItemID::KrytanBrandy:
		return 5;
	default:
		return 0;
	}
}

// ================================================
void PconLunar::Update() {
	int cur_delay = Pcon::delay;
	Pcon::delay = 500;
	Pcon::Update();
	Pcon::delay = cur_delay;
}
int PconLunar::QuantityForEach(const GW::Item* item) const {
	switch (item->ModelId) {
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
		return 1;
	default:
		return 0;
	}
}
bool PconLunar::CanUseByEffect() const {
	GW::AgentEffectsArray AgEffects = GW::Effects().GetPartyEffectArray();
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
