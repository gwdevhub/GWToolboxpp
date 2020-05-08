#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>

#include <GWCA/GameContainers/Array.h>

#include "ToolboxWidget.h"


namespace GW {
	namespace Constants {
		enum class Rarity : uint8_t {
			White, Blue, Purple, Gold, Green
		};
	}
}

class InventoryManager : public ToolboxUIElement {
public:
	InventoryManager() {
		current_salvage_session.salvage_item_id = 0;
	}
	~InventoryManager() {
		ClearPotentialItems();
	}
	enum class SalvageAllType : uint8_t {
		None,
		White,
		BlueAndLower,
		PurpleAndLower,
		GoldAndLower
	};
	enum class IdentifyAllType : uint8_t {
		None,
		All,
		Blue,
		Purple,
		Gold
	};

	static InventoryManager& Instance() {
		static InventoryManager instance;
		return instance;
	}
	const char* Name() const override { return "Inventory Management"; }
	const char* SettingsName() const override { return "Inventory Settings"; }

	void Draw(IDirect3DDevice9* device) override;

	void IdentifyAll(IdentifyAllType type);
	void SalvageAll(SalvageAllType type);
	bool IsPendingIdentify();
	bool IsPendingSalvage();
	bool HasSettings() { return true; };
	void Initialize() override;
	void Update(float delta) override;
	void DrawSettingInternal() override;
	void RegisterSettingsContent() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	static void CmdIdentify(const wchar_t* message, int argc, LPWSTR* argv);
	static void CmdSalvage(const wchar_t* message, int argc, LPWSTR* argv);

	// Find an empty (or partially empty) inventory slot that this item can go into
	std::pair<GW::Bag*, uint32_t> InventoryManager::GetAvailableInventorySlot(GW::Item* like_item = nullptr);
	// Find an empty (or partially empty) inventory slot that this item can go into. !entire_stack = Returns slots that are the same item, but won't hold all of them.
	GW::Item* InventoryManager::GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack = false);
	// Checks model info and struct info to make sure item is the same.
	GW::Item* GetSameItem(GW::Item* like_item, GW::Bag* bag);
	// Checks model info and struct info to make sure item is the same.
	bool IsSameItem(GW::Item* item1, GW::Item* item2);

	static void ItemClickCallback(GW::HookStatus*, uint32_t type, uint32_t slot, GW::Bag* bag);

	IdentifyAllType identify_all_type = IdentifyAllType::None;
	SalvageAllType salvage_all_type = SalvageAllType::None;

private:
	bool show_item_context_menu = false;
	bool is_identifying = false;
	bool is_identifying_all = false;
	bool is_salvaging = false;
	bool is_salvaging_all = false;
	bool has_prompted_salvage = false;
	bool is_manual_item_click = false;
	bool show_salvage_all_popup = true;
	bool salvage_listeners_attached = false;

	bool only_use_superior_salvage_kits = false;
	bool salvage_rare_mats = false;
	std::map<GW::Constants::Bag, bool> bags_to_salvage_from = {
		{ GW::Constants::Bag::Backpack,true },
		{ GW::Constants::Bag::Belt_Pouch,true },
		{ GW::Constants::Bag::Bag_1,true },
		{ GW::Constants::Bag::Bag_2,true }
	};

	size_t identified_count = 0;
	size_t salvaged_count = 0;
	

	GW::Packet::StoC::SalvageSession current_salvage_session;

	void ContinueIdentify();
	void ContinueSalvage();

	GW::HookEntry on_map_change_entry;
	GW::HookEntry salvage_hook_entry;
	GW::HookEntry redo_salvage_entry;
	GW::HookEntry redo_identify_entry;

	void FetchPotentialItems();

	void AttachSalvageListeners();
	void DetachSalvageListeners();
	static void ClearSalvageSession(GW::HookStatus* status, ...) {
		if(status) status->blocked = true;
		Instance().current_salvage_session.salvage_item_id = 0;
	};
	void CancelSalvage() {
		DetachSalvageListeners();
		ClearSalvageSession(nullptr);
		potential_salvage_all_items.clear();
		is_salvaging = has_prompted_salvage = is_salvaging_all = false;
		pending_salvage_item.item_id = 0;
		pending_salvage_kit.item_id = 0;
		salvage_all_type = SalvageAllType::None;
		salvaged_count = 0;
		context_item.item_id = 0;
		pending_cancel_salvage = false;
	}
	void CancelIdentify() {
		is_identifying = is_identifying_all = false;
		pending_identify_item.item_id = 0;
		pending_identify_kit.item_id = 0;
		identify_all_type = IdentifyAllType::None;
		identified_count = 0;
		context_item.item_id = 0;
	}
	inline void CancelAll() {
		ClearPotentialItems();
		CancelSalvage();
		CancelIdentify();
	}
public:
	struct Item : GW::Item {
		inline GW::ItemModifier* GetModifier(uint32_t identifier) {
			for (size_t i = 0; i < mod_struct_size; i++) {
				GW::ItemModifier* mod = &mod_struct[i];
				if (mod->identifier() == identifier)
					return mod;
			}
			return nullptr;
		}
		inline GW::ItemModifier* GetInscription() {
			GW::ItemModifier* m = nullptr;
			m = GetModifier(0x2532);
			if (!m) m = GetModifier(0xA530);
			return m;
		}
		inline GW::ItemModifier* GetSuffix() {
			GW::ItemModifier* m = nullptr;
			if (!IsWeapon()) {
				m = GetModifier(0x2530);
			}
			else {
				// TODO: Different rules for weapons
			}
			return m;
		}
		inline GW::ItemModifier* GetPrefix() {
			GW::ItemModifier* m = nullptr;
			if (!IsWeapon()) {
				m = GetModifier(0xA530);
			}
			else {
				// TODO: Different rules for weapons
			}
			return m;
		}
		inline GW::Constants::Rarity GetRarity() {
			if (IsGreen()) return GW::Constants::Rarity::Green;
			if (IsGold()) return GW::Constants::Rarity::Gold;
			if (IsPurple()) return GW::Constants::Rarity::Purple;
			if (IsBlue()) return GW::Constants::Rarity::Blue;
			return GW::Constants::Rarity::White;
		}
		inline uint32_t GetRequirement() {
			auto mod = GetModifier(0x2798);
			return mod ? mod->arg2() : 0;
		}
		inline GW::Constants::Attribute GetAttribute() {
			auto mod = GetModifier(0x2798);
			return mod ? (GW::Constants::Attribute)mod->arg1() : GW::Constants::Attribute::None;
		}
		inline bool GetIsIdentified() {
			return interaction & 1;
		}
		inline bool HasInscription() {
			return type != 11 && (GetModifier(0x2532) || GetModifier(0xA532));
		}
		inline uint32_t GetUses() {
			auto mod = GetModifier(0x2458);
			return mod ? mod->arg2() : quantity;
		}
		inline bool HasSalvagePopup(GW::Item* salvage_kit = nullptr) {
			if (!IsSalvagable())
				return false;
			switch (GetRarity()) {
			case GW::Constants::Rarity::White:
				return false;
			case GW::Constants::Rarity::Blue: // Lesser kits don't have a popup when used on blue items
				return salvage_kit && ((Item*)salvage_kit)->IsLesserKit() == false;
			}
			return true; // All other rarities (gold, purple) show a popup
		}
		inline bool IsKit() {
			return type == 29;
		}
		inline bool IsTome() {
			return model_id >= 21786 && model_id <= 21805;
		}
		inline bool IsIdentificationKit() {
			auto mod = GetModifier(0x25E8);
			return mod && mod->arg1() == 1;
		}
		inline bool IsSalvageKit() {
			return IsLesserKit() || IsExpertSalvageKit();
		}
		inline bool IsLesserKit() {
			auto mod = GetModifier(0x25E8);
			return mod && mod->arg1() == 3;
		}
		inline bool IsExpertSalvageKit() {
			auto mod = GetModifier(0x25E8);
			switch (mod ? mod->arg1() : 0) {
			case 2: case 6:
				return true;
			}
			return false;
		}
		inline uint32_t GetUpgradeWeaponType() {
			auto mod = GetModifier(0x25B8);
			return mod ? mod->arg1() : 0;
		}
		inline uint32_t GetProfession() {
			auto mod = GetModifier(0xA4B8);
			return mod ? mod->arg1() : 0;
		}
		inline uint32_t GetMinDamage() {
			auto mod = GetModifier(0xA7A8);
			return mod ? mod->arg2() : 0;
		}
		inline uint32_t GetMaxDamage() {
			auto mod = GetModifier(0xA7A8);
			return mod ? mod->arg1() : 0;
		}
		inline uint32_t GetArmorRating() {
			auto mod = GetModifier(type == 0x18 ? 0xA7B8 : 0xA3C8);
			return mod ? mod->arg1() : 0;
		}
		inline bool IsRune() {
			return type == 8 && GetModifier(0x2530) && !GetUpgradeWeaponType();
		}
		inline bool IsInsignia() {
			return type == 8 && GetModifier(0xA530) && !GetUpgradeWeaponType();
		}
		inline uint8_t IsRuneOrInsignia() {
			if (IsRune()) return 1;
			if (IsInsignia()) return 2;
			return 0;
		}
		inline bool IsWeapon() {
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
			}
			return false;
		}
		inline bool IsArmor() {
			switch ((GW::Constants::ItemType)type) {
			case GW::Constants::ItemType::Headpiece:
			case GW::Constants::ItemType::Chestpiece:
			case GW::Constants::ItemType::Leggings:
			case GW::Constants::ItemType::Boots:
			case GW::Constants::ItemType::Gloves:
				return true;
			}
			return false;
		}
		inline bool IsWeaponModOrInscription() {
			return type == static_cast<uint8_t>(GW::Constants::ItemType::Rune_Mod) && !IsRuneOrInsignia();
		}
		inline uint32_t GetMaterialSlot() {
			auto mod = GetModifier(0x2508);
			return mod ? mod->arg1() : 0;
		}
		inline bool IsMaterial() {
			return GetModifier(0x2508);
		}
		inline bool IsStackable() {
			return interaction & 0x80000;
		}
		inline bool IsEquippable() {
			return interaction & 0x200000;
		}
		inline bool IsSalvagable() {
			if (IsUsable() || IsGreen())
				return false; // Non-salvagable flag set
			if (!bag) return false;
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
				switch (model_id) {
				case GW::Constants::ItemID::BoltofDamask:
				case GW::Constants::ItemID::BoltofLinen:
				case GW::Constants::ItemID::BoltofSilk:
				case GW::Constants::ItemID::DeldrimorSteelIngot:
				case GW::Constants::ItemID::ElonianLeatherSquare:
				case GW::Constants::ItemID::LeatherSquare:
				case GW::Constants::ItemID::LumpofCharcoal:
				case GW::Constants::ItemID::RollofParchment:
				case GW::Constants::ItemID::RollofVellum:
				case GW::Constants::ItemID::SpiritwoodPlank:
				case GW::Constants::ItemID::SteelIngot:
				case GW::Constants::ItemID::TemperedGlassVial:
				case GW::Constants::ItemID::VialofInk:
					return true;
				}
			}
			if (IsWeapon() || IsArmor())
				return true;
			return false;
		}
		inline bool IsCommonMaterial() {
			return interaction & 0x20;
		}
		inline bool IsRareMaterial() {
			auto mod = GetModifier(0x2508);
			return mod && mod->arg1() > 11;
		}
		inline bool IsUsable() {
			return interaction & 0x1000000;
		}
		inline bool IsBlue() {
			return single_item_name && single_item_name[0] == 0xA3F;
		}
		inline bool IsPurple() {
			return interaction & 0x400000;
		}
		inline bool IsGreen() {
			return interaction & 0x10;
		}
		inline bool IsGold() {
			return interaction & 0x20000;
		}
	};
public:
	Item* GetNextUnsalvagedItem(Item* salvage_kit = nullptr, Item* start_after_item = nullptr);
	Item* GetSalvageKit(bool only_superior_kits = false);
	Item* GetNextUnidentifiedItem(Item* start_after_item = nullptr);
	Item* GetIdentificationKit();
	void Identify(Item* item, Item* kit);
	void Salvage(Item* item, Item* kit);
private:
	struct PendingItem {
		uint32_t item_id = 0;
		uint32_t slot = 0;
		GW::Constants::Bag bag = GW::Constants::Bag::None;
		uint32_t uses = 0;
		uint32_t quantity = 0;
		Item* item() {
			if (!item_id) return nullptr;
			Item* item = static_cast<Item*>(GW::Items::GetItemBySlot(bag, slot + 1));
			return item && item->item_id == item_id ? item : nullptr;
		}
	};
	struct PotentialItem : PendingItem {
		std::wstring name;
		std::string name_s;
		std::wstring desc;
		std::string desc_s;
		bool proceed = true;
		bool sanitised = false;
	};
	std::vector<PotentialItem*> potential_salvage_all_items; // List of items that would be processed if user confirms Salvage All
	inline void ClearPotentialItems() {
		for (auto item : potential_salvage_all_items) {
			delete item;
		}
		potential_salvage_all_items.clear();
	}
	PendingItem pending_identify_item;
	PendingItem pending_identify_kit;
	PendingItem pending_salvage_item;
	PendingItem pending_salvage_kit;
	clock_t pending_salvage_at = 0;
	clock_t pending_identify_at = 0;
	PendingItem context_item;
	std::wstring context_item_name_ws;
	std::string context_item_name_s;
	bool pending_cancel_salvage = false;
};