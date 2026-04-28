#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Item.h>

namespace GW {

    struct Bag;
    struct Item;
    struct Inventory;

    typedef Array<Item*> ItemArray;

    struct PvPItemUpgradeInfo;
    struct PvPItemInfo;
    struct CompositeModelInfo;

    namespace Constants {
        enum class Bag : uint8_t;
        enum class StoragePane : uint8_t;
        enum class MaterialSlot : uint32_t;
        enum HeroID : uint32_t;
    }

    namespace UI {
        namespace UIPacket {
            struct kMouseAction;
        }
    }

    enum class EquipmentType : uint32_t {
        Cape = 0x0, Helm = 0x2, CostumeBody = 0x4, CostumeHeadpiece = 0x6, Unknown = 0xff
    };
    enum class EquipmentStatus : uint32_t {
        AlwaysHide, HideInTownsAndOutposts, HideInCombatAreas, AlwaysShow
    };

    struct Module;
    extern Module ItemModule;

    namespace Items {
        GWCA_API SalvageSessionInfo* GetSalvageSessionInfo();
        GWCA_API ItemArray* GetItemArray();
        GWCA_API Inventory* GetInventory();
        GWCA_API Inventory* GetHeroInventory(GW::Constants::HeroID hero_id);
        GWCA_API Bag** GetBagArray();
        GWCA_API Bag* GetBag(Constants::Bag bag_id);
        GWCA_API Bag* GetBagByIndex(uint32_t bag_index);
        GWCA_API uint32_t GetMaterialStorageStackSize();
        GWCA_API Item* GetItemBySlot(const Bag* bag, uint32_t slot);
        GWCA_API Item* GetHoveredItem();
        GWCA_API Item* GetItemById(uint32_t item_id);
        GWCA_API bool UseItem(const Item* item);
        GWCA_API bool EquipItem(const Item* item, uint32_t agent_id = 0);
        GWCA_API bool DropItem(const Item* item, uint32_t quantity);
        GWCA_API bool PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id);
        GWCA_API bool CanInteractWithItem(const Item* item);
        GWCA_API bool PickUpItem(const Item* item, uint32_t call_target = 0);
        GWCA_API bool OpenXunlaiWindow(bool anniversary_pane_unlocked = true, bool storage_pane_unlocked = true);
        GWCA_API bool CanAccessXunlaiChest();
        GWCA_API bool DropGold(uint32_t amount = 1);
        GWCA_API bool SalvageStart(uint32_t salvage_kit_id, uint32_t item_id);
        GWCA_API bool IdentifyItem(uint32_t identification_kit_id, uint32_t item_id);
        GWCA_API bool SalvageSessionCancel();
        GWCA_API bool DestroyItem(uint32_t item_id);
        GWCA_API bool SalvageMaterials();
        GWCA_API uint32_t GetGoldAmountOnCharacter();
        GWCA_API uint32_t GetGoldAmountInStorage();
        GWCA_API uint32_t DepositGold(uint32_t amount = 0);
        GWCA_API uint32_t WithdrawGold(uint32_t amount = 0);
        GWCA_API bool MoveItem(const Item* from, Constants::Bag bag_id, uint32_t slot, uint32_t quantity = 0);
        GWCA_API bool MoveItem(const Item* item, const Bag* bag, uint32_t slot, uint32_t quantity = 0);
        GWCA_API bool MoveItem(const Item* from, const Item* to, uint32_t quantity = 0);
        GWCA_API bool UseItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
        GWCA_API uint32_t CountItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
        GWCA_API Item* GetItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
        GWCA_API Item* GetItemByModelIdAndModifiers(uint32_t modelid, const ItemModifier* modifiers, uint32_t modifiers_len, int bagStart = 1, int bagEnd = 4);
        GWCA_API GW::Constants::StoragePane GetStoragePage(void);
        GWCA_API bool GetIsStorageOpen(void);
        typedef HookCallback<GW::UI::UIPacket::kMouseAction*, GW::Item*> ItemClickCallback;
        GWCA_API void RegisterItemClickCallback(HookEntry* entry, const ItemClickCallback& callback);
        GWCA_API void RemoveItemClickCallback(HookEntry* entry);
        GWCA_API Constants::MaterialSlot GetMaterialSlot(const Item* item);
        GWCA_API EquipmentStatus GetEquipmentVisibility(EquipmentType type);
        GWCA_API bool SetEquipmentVisibility(EquipmentType type, EquipmentStatus state);
        GWCA_API const BaseArray<PvPItemUpgradeInfo>& GetPvPItemUpgradesArray();
        GWCA_API const PvPItemUpgradeInfo* GetPvPItemUpgrade(uint32_t pvp_item_upgrade_idx);
        GWCA_API const PvPItemInfo* GetPvPItemInfo(uint32_t pvp_item_idx);
        GWCA_API const BaseArray<PvPItemInfo>& GetPvPItemInfoArray();
        GWCA_API const CompositeModelInfo* GetCompositeModelInfo(uint32_t model_file_id);
        GWCA_API const BaseArray<CompositeModelInfo>& GetCompositeModelInfoArray();
        GWCA_API bool GetPvPItemUpgradeEncodedName(uint32_t pvp_item_upgrade_idx, wchar_t** out);
        GWCA_API bool GetPvPItemUpgradeEncodedDescription(uint32_t pvp_item_upgrade_idx, wchar_t** out);
        GWCA_API const ItemFormula* GetItemFormula(const GW::Item* item);
    };
}

// ============================================================
// C Interop API
// ============================================================
extern "C" {
    // Lookups
    GWCA_API void* GetSalvageSessionInfo();
    GWCA_API void* GetInventory();
    GWCA_API void* GetBag(uint8_t bag_id);
    GWCA_API void* GetBagByIndex(uint32_t bag_index);
    GWCA_API void* GetItemBySlot(const void* bag, uint32_t slot);
    GWCA_API void* GetHoveredItem();
    GWCA_API void* GetItemById(uint32_t item_id);
    GWCA_API void* GetItemByModelId(uint32_t model_id, int bag_start, int bag_end);
    GWCA_API void* GetItemFormula(const void* item);
    GWCA_API uint32_t GetMaterialStorageStackSize();
    GWCA_API uint32_t GetMaterialSlot(const void* item);
    GWCA_API uint32_t GetStoragePage();
    GWCA_API bool     GetIsStorageOpen();

    // Actions
    GWCA_API bool     UseItem(const void* item);
    GWCA_API bool     EquipItem(const void* item, uint32_t agent_id);
    GWCA_API bool     DropItem(const void* item, uint32_t quantity);
    GWCA_API bool     PickUpItem(const void* item, uint32_t call_target);
    GWCA_API bool     CanInteractWithItem(const void* item);
    GWCA_API bool     PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id);
    GWCA_API bool     InteractAgent(const void* agent, bool call_target);

    // Item management
    GWCA_API bool     MoveItemToBag(const void* item, uint8_t bag_id, uint32_t slot, uint32_t quantity);
    GWCA_API bool     MoveItemToItem(const void* from, const void* to, uint32_t quantity);
    GWCA_API bool     UseItemByModelId(uint32_t model_id, int bag_start, int bag_end);
    GWCA_API uint32_t CountItemByModelId(uint32_t model_id, int bag_start, int bag_end);
    GWCA_API bool     DestroyItem(uint32_t item_id);

    // Salvage / Identify
    GWCA_API bool     SalvageStart(uint32_t salvage_kit_id, uint32_t item_id);
    GWCA_API bool     SalvageSessionCancel();
    GWCA_API bool     SalvageMaterials();
    GWCA_API bool     IdentifyItem(uint32_t identification_kit_id, uint32_t item_id);

    // Gold
    GWCA_API uint32_t GetGoldAmountOnCharacter();
    GWCA_API uint32_t GetGoldAmountInStorage();
    GWCA_API uint32_t DepositGold(uint32_t amount);
    GWCA_API uint32_t WithdrawGold(uint32_t amount);
    GWCA_API bool     DropGold(uint32_t amount);

    // Storage
    GWCA_API bool     OpenXunlaiWindow(bool anniversary_pane_unlocked, bool storage_pane_unlocked);
    GWCA_API bool     CanAccessXunlaiChest();

    // Equipment visibility
    GWCA_API uint32_t GetEquipmentVisibility(uint32_t type);
    GWCA_API bool     SetEquipmentVisibility(uint32_t type, uint32_t state);

    // PvP items
    GWCA_API const void* GetPvPItemUpgrade(uint32_t idx);
    GWCA_API const void* GetPvPItemInfo(uint32_t idx);
    GWCA_API const void* GetCompositeModelInfo(uint32_t model_file_id);
    GWCA_API bool        GetPvPItemUpgradeEncodedName(uint32_t idx, wchar_t** out);
    GWCA_API bool        GetPvPItemUpgradeEncodedDescription(uint32_t idx, wchar_t** out);
}