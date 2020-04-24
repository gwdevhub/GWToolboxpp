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
	/*GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemGeneral>(&salvage_hook_entry, [this](GW::HookStatus* status,GW::Packet::StoC::ItemGeneral* packet) {
		if (packet->item_id == pending_salvage_item.item_id && !current_salvage_session.salvage_item_id)
			status->blocked = true;
		});*/
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
	//GW::StoC::RemoveCallback(GW::Packet::StoC::ItemGeneral::STATIC_HEADER, &salvage_hook_entry);
	salvage_listeners_attached = false;
}
void InventoryManager::IdentifyAll() {
	if (!is_identifying_all || is_identifying)
		return;
	// Get next item to identify
	Item* unid = GetNextUnidentifiedItem();
	if (!unid) {
		Log::Info("Identified %d items", identified_count);
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
		IdentifyAll();
}
void InventoryManager::ContinueSalvage() {
	is_salvaging = false;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
		CancelSalvage();
		return;
	}
	Item* current_item = static_cast<Item*>(GW::Items::GetItemBySlot(pending_salvage_item.bag, pending_salvage_item.slot + 1));
	if (current_item && current_item->item_id == pending_salvage_item.item_id && current_salvage_session.salvage_item_id != 0) {
		// Popup dialog for salvage; salvage materials and cycle.
		ClearSalvageSession(nullptr);
		GW::CtoS::SendPacket(0x4, GAME_CMSG_ITEM_SALVAGE_MATERIALS);
		pending_salvage_at = (clock() / CLOCKS_PER_SEC);
		is_salvaging = true;
		return;
	}
	if (pending_salvage_item.item_id)
		salvaged_count++;
	if (current_item && current_item->item_id == pending_salvage_item.item_id && current_item->quantity == pending_salvage_item.quantity) {
		CancelSalvage();
		Log::Error("Salvage flagged as complete, but item still exists in slot %d/%d", current_item->bag->index+1, current_item->slot+1);
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
		if (!potential_items_size) {
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
	Item* kit = GetSalvageKit(only_use_superior_salvage_kits);
	if (!kit) {
		CancelSalvage();
		Log::Warning("No more salvage kit uses");
		return;
	}
	Item* item = GetNextUnsalvagedItem(kit);
	if (!item) {
		Log::Info("Salvaged %d items", salvaged_count);
		CancelSalvage();
		return;
	}
	
	Salvage(item, kit);
}
void InventoryManager::Initialize() {
	ToolboxUIElement::Initialize();
	GW::Chat::CreateCommand(L"salvage", [this](const wchar_t* message, int argc, LPWSTR* argv) {
		CancelSalvage();
		if (argc != 2) {
			Log::Warning("Syntax: /salvage blue, /salvage purple or /salvage all");
			return;
		}
		if (wcscmp(argv[1], L"blue") == 0) {
			SalvageAll(SalvageAllType::BlueAndLower);
		}
		else if (wcscmp(argv[1], L"purple") == 0) {
			SalvageAll(SalvageAllType::PurpleAndLower);
		}
		else if (wcscmp(argv[1], L"all") == 0) {
			SalvageAll(SalvageAllType::GoldAndLower);
		}
		else {
			Log::Warning("Syntax: /salvage blue, /salvage purple or /salvage all");
		}
		});
	GW::Chat::CreateCommand(L"salvageblues", [this](const wchar_t* message, int argc, LPWSTR* argv) {
		CancelSalvage();
		SalvageAll(SalvageAllType::GoldAndLower);
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&on_map_change_entry, [this](...) {
		CancelAll();
		});
	/*GW::CtoS::RegisterPacketCallback(&redo_salvage_entry, GAME_CMSG_ITEM_IDENTIFY, [this](GW::HookStatus*,void* packet) {
		struct Packet {
			uint32_t header;
			uint32_t kit_id;
			uint32_t item_id;
		};
		Packet* pack = static_cast<Packet*>(packet);
		current_id_kit_selected = 
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SalvageSessionDone>(&redo_salvage_entry, [this](...) {
		CancelAll();
		});*/
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
		potential_items_size = 0;
		while ((found = GetNextUnsalvagedItem(context_item, found)) && potential_items_size < potential_items_max) {
			PotentialItem* item = &potential_salvage_all_items[potential_items_size];
			GW::UI::AsyncDecodeStr(found->complete_name_enc ? found->complete_name_enc : found->name_enc, &item->name);
			item->sanitised = false;
			item ->rarity = found->GetRarity();
			potential_items_size++;
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
			if (item->IsGreen())
				continue;
			switch (identify_all_type) {
			case IdentifyAllType::All:		return item;
			case IdentifyAllType::Blue:		if (item->IsBlue()) return item;
			case IdentifyAllType::Purple:	if (item->IsPurple()) return item;
			case IdentifyAllType::Gold:		if (item->IsGold()) return item;
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
		GW::Bag* bag = GW::Items::GetBag(bag_i);
		if (!bag) continue;
		GW::ItemArray items = bag->items;
		items_found = 0;
		for (size_t i = (bag_i == start_bag ? start_position : 0); i < items.size() && items_found < bag->items_count; i++) {
			item = static_cast<Item*>(items[i]);
			if (!item)
				continue;
			items_found++;
			if (!item->IsSalvagable())
				continue;
			if (item->IsMaterial())
				continue; // Don't salvage rare mats
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
	Item* item = nullptr;
	if (context_item && context_item->IsSalvageKit() && context_item->bag) {
		item = static_cast<Item*>(GW::Items::GetItemBySlot(context_item->bag->index + 1, context_item->slot + 1));
		if (item && item->IsSalvageKit())
			return item;
	}
	// NOTE: the following code would normally fetch another kit, but its not a good idea to presume the player wants this to happen for salvage kits.
	if (!context_item) {
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
	Item* item = nullptr;
	if (context_item && context_item->IsIdentificationKit() && context_item->bag) {
		item = static_cast<Item*>(GW::Items::GetItemBySlot(context_item->bag->index + 1, context_item->slot + 1));
		if (item && item->IsIdentificationKit())
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
	if(im_item->IsWeapon() || im_item->IsArmor())
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
	Item* current_kit = static_cast<Item*>(GW::Items::GetItemBySlot(pending_identify_kit.bag, pending_identify_kit.slot + 1));
	if (current_kit && current_kit->item_id == pending_identify_kit.item_id && current_kit->GetUses() == pending_identify_kit.uses)
		return true;
	Item* current_item = static_cast<Item*>(GW::Items::GetItemBySlot(pending_identify_item.bag, pending_identify_item.slot + 1));
	if (current_item && current_item->item_id == pending_identify_item.item_id && !current_item->GetIsIdentified())
		return true;
	return false;
}
bool InventoryManager::IsPendingSalvage() {
	if (!pending_salvage_kit.item_id || !pending_salvage_item.item_id)
		return false;
	if (current_salvage_session.salvage_item_id)
		return false;
	Item* current_kit = static_cast<Item*>(GW::Items::GetItemBySlot(pending_salvage_kit.bag, pending_salvage_kit.slot + 1));
	if (current_kit && current_kit->item_id == pending_salvage_kit.item_id && current_kit->GetUses() == pending_salvage_kit.uses)
		return true;
	Item* current_item = static_cast<Item*>(GW::Items::GetItemBySlot(pending_salvage_item.bag, pending_salvage_item.slot + 1));
	if (current_item && current_item->item_id == pending_salvage_item.item_id && current_item->quantity == pending_salvage_item.quantity)
		return true;
	return false;
}
void InventoryManager::DrawSettingInternal() {
	ImGui::TextDisabled("This module is responsible for salvaging and identifying functions either by ctrl + clicking on a salvage or identification kit, or by using the command /salvage <type>");
	ImGui::Text("Salvaging options");
	ImGui::Indent();
	ImGui::Checkbox("Only use Superior Salvage Kits",&only_use_superior_salvage_kits);
	ImGui::ShowHelp("Salvaging items with lesser salvage kits produce less materials.\nSalvaging items with superior salvage kits produce rare materials.");
	ImGui::Unindent();
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
		IdentifyAll();
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
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && ImGui::Button("Store Item", size)) {
			GW::HookStatus st = { 0 };
			ImGui::GetIO().KeysDown[VK_CONTROL] = true;
			is_manual_item_click = true;
			GameSettings::ItemClickCallback(&st, 7, context_item->slot, context_item->bag);
			is_manual_item_click = false;
			ImGui::CloseCurrentPopup();
		}
		if (context_item->IsIdentificationKit()) {
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
				identify_all_type = type;
				is_identifying_all = true;
				ImGui::CloseCurrentPopup();
			}
		}
		else if (context_item->IsSalvageKit()) {
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
			}
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		/*if (context_item->IsSalvagable()) {
			switch (context_item->GetRarity()) {
			case GW::Constants::Rarity::Blue:
				IdentifyAllType = IdentifyAllType::Blue;

			}
			switch (static_cast<GW::Constants::ItemType>(context_item->type)) {
				case GW::Constants::ItemType::
			}
		}*/
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
				CancelSalvage();
				ImGui::CloseCurrentPopup();
			}
		}
		else {
			// Are you sure prompt; at this point we've already got the list of items via FetchPotentialItems()
			ImGui::Text("You're about to salvage %d item%s:", potential_items_size, potential_items_size == 1 ? "" : "s");
			static std::wregex sanitiser(L"<[^>]+>");
			for(size_t i = 0; i< potential_items_size;i++) {
				PotentialItem* item = &potential_salvage_all_items[i];
				switch (item->rarity) {
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
				if (!item->sanitised && !item->name.empty()) {
					item->name = std::regex_replace(item->name, sanitiser, L"");
					item->sanitised = true;
				}
				ImGui::Text("  %ls", item->name.c_str());
				ImGui::PopStyleColor();
			}
			ImGui::Text("\n\nAre you sure?");
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				is_salvaging_all = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
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
	if(!im->v)
	if (im->is_manual_item_click) return;
	Item* item = static_cast<Item*>(GW::Items::GetItemBySlot(bag,slot + 1));
	if (!item || !(item->IsIdentificationKit() || item->IsSalvageKit()))
		return;
	if (!item->bag || !item->bag->IsInventoryBag())
		return;
	im->context_item = item;
	im->show_item_context_menu = true;
	im->context_item_name_ws.clear();
	im->context_item_name_s.clear();
	GW::UI::AsyncDecodeStr(item->name_enc, &im->context_item_name_ws);
	status->blocked = true;
	return;
}
