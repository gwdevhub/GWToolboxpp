#include "stdafx.h"
#include "InventoryManager.h"

#include <GWCA\Packets\Opcodes.h>

#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\CtoSMgr.h>
#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\ChatMgr.h>

#include "GameSettings.h"

#include <logger.h>
#include <GuiUtils.h>
#include <imgui.h>

namespace {
	static ImVec4 ItemBlue = ImColor(153, 238, 255).Value;
	static ImVec4 ItemPurple = ImColor(187, 137, 237).Value;
	static ImVec4 ItemGold = ImColor(255, 204, 86).Value;

	static const char* bag_names[5] = {
		"None",
		"Backpack",
		"Belt Pouch",
		"Bag 1",
		"Bag 2"
	};
}
void InventoryManager::SaveSettings(CSimpleIni* ini) {
	ToolboxUIElement::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(only_use_superior_salvage_kits), only_use_superior_salvage_kits);
	ini->SetBoolValue(Name(), VAR_NAME(salvage_rare_mats), salvage_rare_mats);
	ini->SetBoolValue(Name(), VAR_NAME(salvage_from_backpack), bags_to_salvage_from[GW::Constants::Bag::Backpack]);
	ini->SetBoolValue(Name(), VAR_NAME(salvage_from_belt_pouch), bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
	ini->SetBoolValue(Name(), VAR_NAME(salvage_from_bag_1), bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
	ini->SetBoolValue(Name(), VAR_NAME(salvage_from_bag_2), bags_to_salvage_from[GW::Constants::Bag::Bag_2]);
}
void InventoryManager::LoadSettings(CSimpleIni* ini) {
	ToolboxUIElement::LoadSettings(ini);
	only_use_superior_salvage_kits = ini->GetBoolValue(Name(), VAR_NAME(only_use_superior_salvage_kits), only_use_superior_salvage_kits);
	salvage_rare_mats = ini->GetBoolValue(Name(), VAR_NAME(salvage_rare_mats), salvage_rare_mats);
	bags_to_salvage_from[GW::Constants::Bag::Backpack] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_backpack), bags_to_salvage_from[GW::Constants::Bag::Backpack]);
	bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_belt_pouch), bags_to_salvage_from[GW::Constants::Bag::Belt_Pouch]);
	bags_to_salvage_from[GW::Constants::Bag::Bag_1] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_bag_1), bags_to_salvage_from[GW::Constants::Bag::Bag_1]);
	bags_to_salvage_from[GW::Constants::Bag::Bag_2] = ini->GetBoolValue(Name(), VAR_NAME(salvage_from_bag_2), bags_to_salvage_from[GW::Constants::Bag::Bag_2]);
}

void InventoryManager::RegisterSettingsContent() {
	ToolboxModule::RegisterSettingsContent();
}

void InventoryManager::CmdSalvage(const wchar_t* message, int argc, LPWSTR* argv) {
	auto im = &Instance();
	if (im->is_salvaging_all || im->is_salvaging)
		return;
	im->CancelSalvage();
	const wchar_t* arg2 = argc > 1 ? argv[1] : L"";
	if (wcscmp(arg2, L"blue") == 0) {
		im->SalvageAll(SalvageAllType::BlueAndLower);
	}
	else if (wcscmp(arg2, L"purple") == 0) {
		im->SalvageAll(SalvageAllType::PurpleAndLower);
	}
	else if (wcscmp(arg2, L"all") == 0) {
		im->SalvageAll(SalvageAllType::GoldAndLower);
	}
	else {
		Log::Warning("Syntax: /%ls blue, /%ls purple or /%ls all", argv[0], argv[0], argv[0]);
	}
}
void InventoryManager::CmdIdentify(const wchar_t* message, int argc, LPWSTR* argv) {
	auto im = &Instance();
	im->CancelIdentify();
	const wchar_t* arg2 = argc > 1 ? argv[1] : L"";
	if (wcscmp(arg2, L"blue") == 0) {
		im->IdentifyAll(IdentifyAllType::Blue);
	}
	else if (wcscmp(arg2, L"purple") == 0) {
		im->IdentifyAll(IdentifyAllType::Purple);
	}
	else if (wcscmp(arg2, L"gold") == 0) {
		im->IdentifyAll(IdentifyAllType::Gold);
	}
	else if (wcscmp(arg2, L"all") == 0) {
		im->IdentifyAll(IdentifyAllType::All);
	}
	else {
		Log::Warning("Syntax: /%ls blue, /%ls purple, /%ls gold or /%ls all", argv[0], argv[0], argv[0], argv[0]);
	}
}

void InventoryManager::AttachSalvageListeners() {
	if (salvage_listeners_attached)
		return;
	GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GW::Packet::StoC::SalvageSessionCancel::STATIC_HEADER, &ClearSalvageSession);
	GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GW::Packet::StoC::SalvageSessionDone::STATIC_HEADER, &ClearSalvageSession);
	GW::StoC::RegisterPacketCallback(&salvage_hook_entry, GW::Packet::StoC::SalvageSessionItemKept::STATIC_HEADER, &ClearSalvageSession);
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SalvageSessionSuccess>(&salvage_hook_entry, [this](GW::HookStatus* status, GW::Packet::StoC::SalvageSessionSuccess* packet) {
		ClearSalvageSession(status);
		GW::CtoS::SendPacket(0x4, GAME_CMSG_ITEM_SALVAGE_SESSION_DONE);
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SalvageSession>(&salvage_hook_entry, [this](GW::HookStatus* status, GW::Packet::StoC::SalvageSession* packet) {
		current_salvage_session = *packet;
		status->blocked = true;
		});
	salvage_listeners_attached = true;
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
	Item* kit = GetIdentificationKit();
	if (!kit) {
		CancelIdentify();
		Log::Warning("No more identification kit uses");
		return;
	}
	Identify(unid, kit);
}
void InventoryManager::ContinueIdentify() {
	is_identifying = false;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
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
	Item* current_item = pending_salvage_item.item();
	if (current_item && current_salvage_session.salvage_item_id != 0) {
		// Popup dialog for salvage; salvage materials and cycle.
		ClearSalvageSession(nullptr);
		GW::CtoS::SendPacket(0x4, GAME_CMSG_ITEM_SALVAGE_MATERIALS);
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
	Item* kit = GetSalvageKit(only_use_superior_salvage_kits);
	if (!kit) {
		CancelSalvage();
		Log::Warning("No more salvage kit uses");
		return;
	}
	if (!potential_salvage_all_items.size()) {
		Log::Info("Salvaged %d items", salvaged_count);
		CancelSalvage();
		return;
	}
	std::pair<GW::Bag*, uint32_t> available_slot = GetAvailableInventorySlot();
	if (!available_slot.first) {
		CancelSalvage();
		Log::Warning("No more space in inventory");
		return;
	}
	auto ref = *potential_salvage_all_items.begin();
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
void InventoryManager::Initialize() {
	ToolboxUIElement::Initialize();
	GW::Chat::CreateCommand(L"salvage", CmdSalvage);
	GW::Chat::CreateCommand(L"salv", CmdSalvage);
	GW::Chat::CreateCommand(L"identify", CmdIdentify);
	GW::Chat::CreateCommand(L"id", CmdIdentify);
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&on_map_change_entry, [this](...) {
		CancelAll();
		});
}
void InventoryManager::Salvage(Item* item, Item* kit) {
	if (!item || !kit)
		return;
	if (!item->IsSalvagable() || !kit->IsSalvageKit())
		return;
	pending_salvage_item.item_id = item->item_id;
	pending_salvage_item.bag = static_cast<GW::Constants::Bag>(item->bag->index + 1);
	pending_salvage_item.slot = item->slot;
	pending_salvage_item.quantity = item->quantity;
	pending_salvage_kit.item_id = kit->item_id;
	pending_salvage_kit.bag = static_cast<GW::Constants::Bag>(kit->bag->index + 1);
	pending_salvage_kit.slot = kit->slot;
	pending_salvage_kit.uses = kit->GetUses();
	AttachSalvageListeners();
	GW::CtoS::SendPacket(0x10, GAME_CMSG_ITEM_SALVAGE_SESSION_OPEN, GW::GameContext::instance()->world->salvage_session_id, pending_salvage_kit.item_id, pending_salvage_item.item_id);
	pending_salvage_at = (clock() / CLOCKS_PER_SEC);
	is_salvaging = true;
}
void InventoryManager::Identify(Item* item, Item* kit) {
	if (!item || !kit)
		return;
	if (item->GetIsIdentified() || !kit->IsIdentificationKit())
		return;
	pending_identify_item.item_id = item->item_id;
	pending_identify_item.bag = static_cast<GW::Constants::Bag>(item->bag->index + 1);
	pending_identify_item.slot = item->slot;
	pending_identify_kit.item_id = kit->item_id;
	pending_identify_kit.bag = static_cast<GW::Constants::Bag>(kit->bag->index + 1);
	pending_identify_kit.slot = kit->slot;
	pending_identify_kit.uses = kit->GetUses();
	GW::CtoS::SendPacket(0xC, GAME_CMSG_ITEM_IDENTIFY, pending_identify_kit.item_id, pending_identify_item.item_id);
	pending_identify_at = (clock() / CLOCKS_PER_SEC);
	is_identifying = true;
}
void InventoryManager::FetchPotentialItems() {
	Item* found = nullptr;
	if (salvage_all_type != SalvageAllType::None) {
		ClearPotentialItems();
		while (found = GetNextUnsalvagedItem(context_item.item(), found)) {
			PotentialItem* item = new PotentialItem();
			GW::UI::AsyncDecodeStr(found->complete_name_enc ? found->complete_name_enc : found->name_enc, &item->name);
			if(found->info_string)
				GW::UI::AsyncDecodeStr(found->info_string, &item->desc);
			item->item_id = found->item_id;
			item->slot = found->slot;
			item->bag = static_cast<GW::Constants::Bag>(found->bag->index + 1);
			potential_salvage_all_items.push_back(item);
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
	size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack);
	size_t items_found = 0;
	Item* item = nullptr;
	for (size_t bag_i = start_bag; bag_i <= end_bag; bag_i++) {
		GW::Bag* bag = GW::Items::GetBag(bag_i);
		if (!bag) continue;
		GW::ItemArray items = bag->items;
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
	size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);
	size_t items_found = 0;
	Item* item = nullptr;
	for (size_t bag_i = start_bag; bag_i <= end_bag; bag_i++) {
		if (!bags_to_salvage_from[static_cast<GW::Constants::Bag>(bag_i)])
			continue;
		GW::Bag* bag = GW::Items::GetBag(bag_i);
		if (!bag) continue;
		GW::ItemArray items = bag->items;
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
			if (item->IsRareMaterial() && !salvage_rare_mats)
				continue; // Don't salvage rare mats
			if (item->IsArmor() || item->customized)
				continue; // Don't salvage armor, or customised weapons.
			if (item->IsBlue() && !item->GetIsIdentified() && (kit && kit->IsLesserKit()))
				continue; // Note: lesser kits cant salvage blue unids - Guild Wars bug/feature
			auto rarity = item->GetRarity();
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
			}
		}
	}
	return nullptr;
}
InventoryManager::Item* InventoryManager::GetSalvageKit(bool only_superior_kits) {
	size_t start_bag = static_cast<size_t>(GW::Constants::Bag::Backpack);
	size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
		end_bag = static_cast<size_t>(GW::Constants::Bag::Storage_14);
	size_t items_found = 0;
	Item* item = context_item.item();
	if (item && item->IsSalvageKit()) {
		return item;
	}
	// NOTE: the following code would normally fetch another kit, but its not a good idea to presume the player wants this to happen for salvage kits.
	if (!context_item.item_id) {
		// If no context item, this wasn't triggered via a right click - probably /salvage all command. In this case, its ok to find a kit.
		for (size_t bag_i = start_bag; bag_i <= end_bag; bag_i++) {
			GW::Bag* bag = GW::Items::GetBag(bag_i);
			if (!bag) continue;
			GW::ItemArray items = bag->items;
			items_found = 0;
			for (size_t i = 0; i < items.size() && items_found < bag->items_count; i++) {
				item = static_cast<Item*>(items[i]);
				if (!item)
					continue;
				items_found++;
				if (!item->IsSalvageKit())
					continue;
				if (only_superior_kits && !item->IsExpertSalvageKit())
					continue;
				return item;
			}
		}
	}
	return nullptr;
}
InventoryManager::Item* InventoryManager::GetIdentificationKit() {
	size_t start_bag = static_cast<size_t>(GW::Constants::Bag::Backpack);
	size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack);
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
		end_bag = static_cast<size_t>(GW::Constants::Bag::Storage_14);
	size_t items_found = 0;
	Item* item = context_item.item();
	if (item && item->IsIdentificationKit()) {
		return item;
	}
	
	for (size_t bag_i = start_bag; bag_i <= end_bag; bag_i++) {
		GW::Bag* bag = GW::Items::GetBag(bag_i);
		if (!bag) continue;
		GW::ItemArray items = bag->items;
		items_found = 0;
		for (size_t i = 0; i < items.size() && items_found < bag->items_count; i++) {
			item = static_cast<Item*>(items[i]);
			if (!item)
				continue;
			items_found++;
			if (item->IsIdentificationKit())
				return item;
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
GW::Item* InventoryManager::GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack) {
	if (!like_item || ((Item*)like_item)->IsStackable())
		return nullptr;
	GW::Item* best_item = nullptr;
	GW::Bag** bags = GW::Items::GetBagArray();
	if (!bags) return best_item;
	for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx < static_cast<size_t>(GW::Constants::Bag::Equipment_Pack); bag_idx++) {
		GW::Bag* bag = bags[bag_idx];
		if (!bag || !bag->items_count || !bag->items.valid()) continue;
		for (auto item : bag->items) {
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
GW::Item* InventoryManager::GetSameItem(GW::Item* like_item, GW::Bag* bag) {
	if (!bag || !bag->items_count || !bag->items.valid())
		return nullptr;
	for (auto item : bag->items) {
		if (item && item->item_id != like_item->item_id && IsSameItem(like_item, item))
			return item;
	}
	return nullptr;
}
bool InventoryManager::IsSameItem(GW::Item* item1, GW::Item* item2) {
	return item1 && item2
		&& (!item1->model_id || !item2->model_id || item1->model_id == item2->model_id)
		&& (!item1->model_file_id || !item2->model_file_id || item1->model_file_id == item2->model_file_id)
		&& (!item1->mod_struct_size || !item2->mod_struct_size || item1->mod_struct_size == item2->mod_struct_size)
		&& (!item1->interaction || !item2->interaction || item1->interaction == item2->interaction);
}
bool InventoryManager::IsPendingIdentify() {
	if (!pending_identify_kit.item_id || !pending_identify_item.item_id)
		return false;
	Item* current_kit = pending_identify_kit.item();
	if (current_kit && current_kit->GetUses() == pending_identify_kit.uses)
		return true;
	Item* current_item = pending_identify_item.item();
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
	Item* current_item = pending_salvage_item.item();
	if (current_item && current_item->quantity == pending_salvage_item.quantity)
		return true;
	return false;
}
void InventoryManager::DrawSettingInternal() {
	ImGui::TextDisabled("This module is responsible for salvaging and identifying functions either by ctrl + clicking on a salvage or identification kit, or by using the command /salvage <type>");
	ImGui::Text("Salvage All options:");
	ImGui::SameLine();
	ImGui::TextDisabled("Note: Salvage All will only salvage items that are identified.");
	ImGui::Checkbox("Only use Superior Salvage Kits with /salvage command",&only_use_superior_salvage_kits);
	ImGui::ShowHelp("Salvaging items with lesser salvage kits produce less materials.\nSalvaging items with superior salvage kits can produce rare materials.\n\nCtrl + clicking on a normal Salvage Kit will still allow you to use Salvage All.");
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
}
void InventoryManager::Update(float delta) {
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
};
void InventoryManager::Draw(IDirect3DDevice9* device) {
	if (show_item_context_menu)
		ImGui::OpenPopup("Item Context Menu");
	show_item_context_menu = false;
	if (ImGui::BeginPopup("Item Context Menu")) {
		if (context_item_name_s.empty() && !context_item_name_ws.empty()) {
			context_item_name_s = GuiUtils::WStringToString(context_item_name_ws);
		}
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
		ImVec2 size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale,0);
		ImGui::Text(context_item_name_s.c_str());
		ImGui::Separator();
		// Shouldn't really fetch item() every frame, but its only when the menu is open and better than risking a crash
		Item* context_item_actual = context_item.item(); 
		if (context_item_actual && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && ImGui::Button("Store Item", size)) {
			GW::HookStatus st = { 0 };
			ImGui::GetIO().KeysDown[VK_CONTROL] = true;
			is_manual_item_click = true;
			GameSettings::ItemClickCallback(&st, 7, context_item_actual->slot, context_item_actual->bag);
			is_manual_item_click = false;
			ImGui::CloseCurrentPopup();
		}
		if (context_item_actual && context_item_actual->IsIdentificationKit()) {
			IdentifyAllType type = IdentifyAllType::None;
			if(ImGui::Button("Identify All Items", size))
				type = IdentifyAllType::All;
			ImGui::PushStyleColor(ImGuiCol_Text, ItemBlue);
			if (ImGui::Button("Identify All Blue Items", size))
				type = IdentifyAllType::Blue;
			ImGui::PushStyleColor(ImGuiCol_Text, ItemPurple);
			if (ImGui::Button("Identify All Purple Items", size))
				type = IdentifyAllType::Purple;
			ImGui::PushStyleColor(ImGuiCol_Text, ItemGold);
			if(ImGui::Button("Identify All Gold Items", size))
				type = IdentifyAllType::Gold;
			ImGui::PopStyleColor(3);
			if (type != IdentifyAllType::None) {
				ImGui::CloseCurrentPopup();
				CancelIdentify();
				is_identifying_all = true;
				IdentifyAll(type);
			}
		}
		else if (context_item_actual && context_item_actual->IsSalvageKit()) {
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
				SalvageAll(type);
			}
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
	if (show_salvage_all_popup) {
		ImGui::OpenPopup("Salvage All?");
		show_salvage_all_popup = false;
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
			static std::regex sanitiser("<[^>]+>");
			PotentialItem* pi;
			Item* item;
			GW::Bag* bag = nullptr;
			bool has_items_to_salvage = false;
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
				if (!pi->name.empty() && pi->name_s.empty()) {
					pi->name_s = GuiUtils::WStringToString(pi->name);
					pi->name_s = std::regex_replace(pi->name_s, sanitiser, "");
				}
				ImGui::Checkbox(pi->name_s.c_str(),&pi->proceed);
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered()) {
					if (!pi->desc.empty() && pi->desc_s.empty()) {
						pi->desc_s = GuiUtils::WStringToString(pi->desc);
						pi->desc_s = std::regex_replace(pi->desc_s, sanitiser, "");
					}
					ImGui::SetTooltip("%s",pi->desc_s.c_str());
				}
				has_items_to_salvage |= pi->proceed;
			}
			ImGui::Text("\n\nAre you sure?");
			ImVec2 btn_width = ImVec2(240, 0);
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
void InventoryManager::ItemClickCallback(GW::HookStatus* status, uint32_t type, uint32_t slot, GW::Bag* bag) {
	if (!ImGui::IsKeyDown(VK_CONTROL)) return;
	if (type != 7) return;
	auto im = &InventoryManager::Instance();
	if (im->is_manual_item_click) return;
	Item* item = static_cast<Item*>(GW::Items::GetItemBySlot(bag,slot + 1));
	if (!item || !(item->IsIdentificationKit() || item->IsSalvageKit()))
		return;
	if (!item->bag || !item->bag->IsInventoryBag())
		return;
	if (im->context_item.item_id == item->item_id && im->show_item_context_menu)
		return; // Double looped.
	im->context_item.item_id = item->item_id;
	im->context_item.slot = item->slot;
	im->context_item.bag = static_cast<GW::Constants::Bag>(item->bag->index + 1);
	im->show_item_context_menu = true;
	im->context_item_name_ws.clear();
	im->context_item_name_s.clear();
	GW::UI::AsyncDecodeStr(item->name_enc, &im->context_item_name_ws);
	status->blocked = true;
	return;
}
