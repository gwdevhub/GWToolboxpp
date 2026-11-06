#pragma once
#include <GWCA/Runtime.h>

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
        SalvageSessionInfo* GetSalvageSessionInfo();
        ItemArray* GetItemArray();
        Inventory* GetInventory();
        Inventory* GetHeroInventory(GW::Constants::HeroID hero_id);
        Bag** GetBagArray();
        Bag* GetBag(Constants::Bag bag_id);
        Bag* GetBagByIndex(uint32_t bag_index);
        uint32_t GetMaterialStorageStackSize();
        Item* GetItemBySlot(const Bag* bag, uint32_t slot);
        Item* GetHoveredItem();
        Item* GetItemById(uint32_t item_id);
        bool UseItem(const Item* item);
        bool EquipItem(const Item* item, uint32_t agent_id = 0);
        bool DropItem(const Item* item, uint32_t quantity);
        bool PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id);
        bool CanInteractWithItem(const Item* item);
        bool PickUpItem(const Item* item, uint32_t call_target = 0);
        bool OpenXunlaiWindow(bool anniversary_pane_unlocked = true, bool storage_pane_unlocked = true);
        bool CanAccessXunlaiChest();
        bool DropGold(uint32_t amount = 1);
        bool SalvageStart(uint32_t salvage_kit_id, uint32_t item_id);
        bool IdentifyItem(uint32_t identification_kit_id, uint32_t item_id);
        bool SalvageSessionCancel();
        bool DestroyItem(uint32_t item_id);
        bool SalvageMaterials();
        uint32_t GetGoldAmountOnCharacter();
        uint32_t GetGoldAmountInStorage();
        uint32_t DepositGold(uint32_t amount = 0);
        uint32_t WithdrawGold(uint32_t amount = 0);
        bool MoveItem(const Item* item, const Bag* bag, uint32_t slot, uint32_t quantity = 0);
        inline bool MoveItem(const Item* item, Constants::Bag bag_id, uint32_t slot, uint32_t quantity = 0)
        {
            return MoveItem(item, GetBag(bag_id), slot, quantity);
        }
        inline bool MoveItem(const Item* from, const Item* to, uint32_t quantity = 0)
        {
            return MoveItem(from, to->bag, to->slot, quantity);
        }
        bool UseItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
        uint32_t CountItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
        Item* GetItemByModelId(uint32_t model_id, int bag_start = 1, int bag_end = 4);
        Item* GetItemByModelIdAndModifiers(uint32_t modelid, const ItemModifier* modifiers, uint32_t modifiers_len, int bagStart = 1, int bagEnd = 4);
        GW::Constants::StoragePane GetStoragePage(void);
        bool GetIsStorageOpen(void);
        using NativeItemClickCallback = void (__cdecl *)(HookStatus* status, UI::UIPacket::kMouseAction* action,
                                                   Item* item, void* context);
        bool RegisterItemClickCallback(HookEntry* entry, NativeItemClickCallback callback, void* context = nullptr);
        bool RemoveItemClickCallbackNative(HookEntry* entry);
#if !defined(GWCA_STDLIB)
        inline bool RemoveItemClickCallback(HookEntry* entry) { return RemoveItemClickCallbackNative(entry); }
#endif
        Constants::MaterialSlot GetMaterialSlot(const Item* item);
        EquipmentStatus GetEquipmentVisibility(EquipmentType type);
        bool SetEquipmentVisibility(EquipmentType type, EquipmentStatus state);
        const BaseArray<PvPItemUpgradeInfo>& GetPvPItemUpgradesArray();
        const PvPItemUpgradeInfo* GetPvPItemUpgrade(uint32_t pvp_item_upgrade_idx);
        const PvPItemInfo* GetPvPItemInfo(uint32_t pvp_item_idx);
        const BaseArray<PvPItemInfo>& GetPvPItemInfoArray();
        const CompositeModelInfo* GetCompositeModelInfo(uint32_t model_file_id);
        const BaseArray<CompositeModelInfo>& GetCompositeModelInfoArray();
        bool GetPvPItemUpgradeEncodedName(uint32_t pvp_item_upgrade_idx, wchar_t** out);
        bool GetPvPItemUpgradeEncodedDescription(uint32_t pvp_item_upgrade_idx, wchar_t** out);
        const ItemFormula* GetItemFormula(const GW::Item* item);
    };
}

// C Interop API

namespace GW::Items {
    extern "C" GWCA_API SalvageSessionInfo* __cdecl GW_Items_GetSalvageSessionInfo() noexcept;
    extern "C" GWCA_API ItemArray* __cdecl GW_Items_GetItemArray() noexcept;
    extern "C" GWCA_API Inventory* __cdecl GW_Items_GetInventory() noexcept;
    extern "C" GWCA_API Inventory* __cdecl GW_Items_GetHeroInventory(GW::Constants::HeroID hero_id) noexcept;
    extern "C" GWCA_API Bag** __cdecl GW_Items_GetBagArray() noexcept;
    extern "C" GWCA_API Bag* __cdecl GW_Items_GetBag(Constants::Bag bag_id) noexcept;
    extern "C" GWCA_API Bag* __cdecl GW_Items_GetBagByIndex(uint32_t bag_index) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Items_GetMaterialStorageStackSize() noexcept;
    extern "C" GWCA_API Item* __cdecl GW_Items_GetItemBySlot(const Bag* bag, uint32_t slot) noexcept;
    extern "C" GWCA_API Item* __cdecl GW_Items_GetHoveredItem() noexcept;
    extern "C" GWCA_API Item* __cdecl GW_Items_GetItemById(uint32_t item_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_UseItem(const Item* item) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_EquipItem(const Item* item, uint32_t agent_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_DropItem(const Item* item, uint32_t quantity) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_CanInteractWithItem(const Item* item) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_PickUpItem(const Item* item, uint32_t call_target) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_OpenXunlaiWindow(bool anniversary_pane_unlocked, bool storage_pane_unlocked) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_CanAccessXunlaiChest() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_DropGold(uint32_t amount) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_SalvageStart(uint32_t salvage_kit_id, uint32_t item_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_IdentifyItem(uint32_t identification_kit_id, uint32_t item_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_SalvageSessionCancel() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_DestroyItem(uint32_t item_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_SalvageMaterials() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Items_GetGoldAmountOnCharacter() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Items_GetGoldAmountInStorage() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Items_DepositGold(uint32_t amount) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Items_WithdrawGold(uint32_t amount) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_MoveItem(const Item* item, const Bag* bag, uint32_t slot, uint32_t quantity) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_UseItemByModelId(uint32_t model_id, int bag_start, int bag_end) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Items_CountItemByModelId(uint32_t model_id, int bag_start, int bag_end) noexcept;
    extern "C" GWCA_API Item* __cdecl GW_Items_GetItemByModelId(uint32_t model_id, int bag_start, int bag_end) noexcept;
    extern "C" GWCA_API Item* __cdecl GW_Items_GetItemByModelIdAndModifiers(uint32_t modelid, const ItemModifier* modifiers, uint32_t modifiers_len, int bagStart, int bagEnd) noexcept;
    extern "C" GWCA_API GW::Constants::StoragePane __cdecl GW_Items_GetStoragePage() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_GetIsStorageOpen() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_RegisterItemClickCallback(HookEntry* entry, NativeItemClickCallback callback, void* context) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_RemoveItemClickCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API Constants::MaterialSlot __cdecl GW_Items_GetMaterialSlot(const Item* item) noexcept;
    extern "C" GWCA_API EquipmentStatus __cdecl GW_Items_GetEquipmentVisibility(EquipmentType type) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_SetEquipmentVisibility(EquipmentType type, EquipmentStatus state) noexcept;
    extern "C" GWCA_API const BaseArray < PvPItemUpgradeInfo >& __cdecl GW_Items_GetPvPItemUpgradesArray() noexcept;
    extern "C" GWCA_API const PvPItemUpgradeInfo* __cdecl GW_Items_GetPvPItemUpgrade(uint32_t pvp_item_upgrade_idx) noexcept;
    extern "C" GWCA_API const PvPItemInfo* __cdecl GW_Items_GetPvPItemInfo(uint32_t pvp_item_idx) noexcept;
    extern "C" GWCA_API const BaseArray < PvPItemInfo >& __cdecl GW_Items_GetPvPItemInfoArray() noexcept;
    extern "C" GWCA_API const CompositeModelInfo* __cdecl GW_Items_GetCompositeModelInfo(uint32_t model_file_id) noexcept;
    extern "C" GWCA_API const BaseArray < CompositeModelInfo >& __cdecl GW_Items_GetCompositeModelInfoArray() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_GetPvPItemUpgradeEncodedName(uint32_t pvp_item_upgrade_idx, wchar_t** out) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Items_GetPvPItemUpgradeEncodedDescription(uint32_t pvp_item_upgrade_idx, wchar_t** out) noexcept;
    extern "C" GWCA_API const ItemFormula* __cdecl GW_Items_GetItemFormula(const GW::Item* item) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Items {
    inline SalvageSessionInfo* GetSalvageSessionInfo()
    {
        return GWCA_CALL_C(GW_Items_GetSalvageSessionInfo);
    }

    inline ItemArray* GetItemArray()
    {
        return GWCA_CALL_C(GW_Items_GetItemArray);
    }

    inline Inventory* GetInventory()
    {
        return GWCA_CALL_C(GW_Items_GetInventory);
    }

    inline Inventory* GetHeroInventory(GW::Constants::HeroID hero_id)
    {
        return GWCA_CALL_C(GW_Items_GetHeroInventory, hero_id);
    }

    inline Bag** GetBagArray()
    {
        return GWCA_CALL_C(GW_Items_GetBagArray);
    }

    inline Bag* GetBag(Constants::Bag bag_id)
    {
        return GWCA_CALL_C(GW_Items_GetBag, bag_id);
    }

    inline Bag* GetBagByIndex(uint32_t bag_index)
    {
        return GWCA_CALL_C(GW_Items_GetBagByIndex, bag_index);
    }

    inline uint32_t GetMaterialStorageStackSize()
    {
        return GWCA_CALL_C(GW_Items_GetMaterialStorageStackSize);
    }

    inline Item* GetItemBySlot(const Bag* bag, uint32_t slot)
    {
        return GWCA_CALL_C(GW_Items_GetItemBySlot, bag, slot);
    }

    inline Item* GetHoveredItem()
    {
        return GWCA_CALL_C(GW_Items_GetHoveredItem);
    }

    inline Item* GetItemById(uint32_t item_id)
    {
        return GWCA_CALL_C(GW_Items_GetItemById, item_id);
    }

    inline bool UseItem(const Item* item)
    {
        return GWCA_CALL_C(GW_Items_UseItem, item);
    }

    inline bool EquipItem(const Item* item, uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Items_EquipItem, item, agent_id);
    }

    inline bool DropItem(const Item* item, uint32_t quantity)
    {
        return GWCA_CALL_C(GW_Items_DropItem, item, quantity);
    }

    inline bool PingWeaponSet(uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id)
    {
        return GWCA_CALL_C(GW_Items_PingWeaponSet, agent_id, weapon_item_id, offhand_item_id);
    }

    inline bool CanInteractWithItem(const Item* item)
    {
        return GWCA_CALL_C(GW_Items_CanInteractWithItem, item);
    }

    inline bool PickUpItem(const Item* item, uint32_t call_target)
    {
        return GWCA_CALL_C(GW_Items_PickUpItem, item, call_target);
    }

    inline bool OpenXunlaiWindow(bool anniversary_pane_unlocked, bool storage_pane_unlocked)
    {
        return GWCA_CALL_C(GW_Items_OpenXunlaiWindow, anniversary_pane_unlocked, storage_pane_unlocked);
    }

    inline bool CanAccessXunlaiChest()
    {
        return GWCA_CALL_C(GW_Items_CanAccessXunlaiChest);
    }

    inline bool DropGold(uint32_t amount)
    {
        return GWCA_CALL_C(GW_Items_DropGold, amount);
    }

    inline bool SalvageStart(uint32_t salvage_kit_id, uint32_t item_id)
    {
        return GWCA_CALL_C(GW_Items_SalvageStart, salvage_kit_id, item_id);
    }

    inline bool IdentifyItem(uint32_t identification_kit_id, uint32_t item_id)
    {
        return GWCA_CALL_C(GW_Items_IdentifyItem, identification_kit_id, item_id);
    }

    inline bool SalvageSessionCancel()
    {
        return GWCA_CALL_C(GW_Items_SalvageSessionCancel);
    }

    inline bool DestroyItem(uint32_t item_id)
    {
        return GWCA_CALL_C(GW_Items_DestroyItem, item_id);
    }

    inline bool SalvageMaterials()
    {
        return GWCA_CALL_C(GW_Items_SalvageMaterials);
    }

    inline uint32_t GetGoldAmountOnCharacter()
    {
        return GWCA_CALL_C(GW_Items_GetGoldAmountOnCharacter);
    }

    inline uint32_t GetGoldAmountInStorage()
    {
        return GWCA_CALL_C(GW_Items_GetGoldAmountInStorage);
    }

    inline uint32_t DepositGold(uint32_t amount)
    {
        return GWCA_CALL_C(GW_Items_DepositGold, amount);
    }

    inline uint32_t WithdrawGold(uint32_t amount)
    {
        return GWCA_CALL_C(GW_Items_WithdrawGold, amount);
    }

    inline bool MoveItem(const Item* item, const Bag* bag, uint32_t slot, uint32_t quantity)
    {
        return GWCA_CALL_C(GW_Items_MoveItem, item, bag, slot, quantity);
    }

    inline bool UseItemByModelId(uint32_t model_id, int bag_start, int bag_end)
    {
        return GWCA_CALL_C(GW_Items_UseItemByModelId, model_id, bag_start, bag_end);
    }

    inline uint32_t CountItemByModelId(uint32_t model_id, int bag_start, int bag_end)
    {
        return GWCA_CALL_C(GW_Items_CountItemByModelId, model_id, bag_start, bag_end);
    }

    inline Item* GetItemByModelId(uint32_t model_id, int bag_start, int bag_end)
    {
        return GWCA_CALL_C(GW_Items_GetItemByModelId, model_id, bag_start, bag_end);
    }

    inline Item* GetItemByModelIdAndModifiers(uint32_t modelid, const ItemModifier* modifiers, uint32_t modifiers_len, int bagStart, int bagEnd)
    {
        return GWCA_CALL_C(GW_Items_GetItemByModelIdAndModifiers, modelid, modifiers, modifiers_len, bagStart, bagEnd);
    }

    inline GW::Constants::StoragePane GetStoragePage()
    {
        return GWCA_CALL_C(GW_Items_GetStoragePage);
    }

    inline bool GetIsStorageOpen()
    {
        return GWCA_CALL_C(GW_Items_GetIsStorageOpen);
    }

    inline bool RegisterItemClickCallback(HookEntry* entry, NativeItemClickCallback callback, void* context)
    {
        return GWCA_CALL_C(GW_Items_RegisterItemClickCallback, entry, callback, context);
    }

    inline bool RemoveItemClickCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_Items_RemoveItemClickCallbackNative, entry);
    }

    inline Constants::MaterialSlot GetMaterialSlot(const Item* item)
    {
        return GWCA_CALL_C(GW_Items_GetMaterialSlot, item);
    }

    inline EquipmentStatus GetEquipmentVisibility(EquipmentType type)
    {
        return GWCA_CALL_C(GW_Items_GetEquipmentVisibility, type);
    }

    inline bool SetEquipmentVisibility(EquipmentType type, EquipmentStatus state)
    {
        return GWCA_CALL_C(GW_Items_SetEquipmentVisibility, type, state);
    }

    inline const BaseArray < PvPItemUpgradeInfo >& GetPvPItemUpgradesArray()
    {
        return GWCA_CALL_C(GW_Items_GetPvPItemUpgradesArray);
    }

    inline const PvPItemUpgradeInfo* GetPvPItemUpgrade(uint32_t pvp_item_upgrade_idx)
    {
        return GWCA_CALL_C(GW_Items_GetPvPItemUpgrade, pvp_item_upgrade_idx);
    }

    inline const PvPItemInfo* GetPvPItemInfo(uint32_t pvp_item_idx)
    {
        return GWCA_CALL_C(GW_Items_GetPvPItemInfo, pvp_item_idx);
    }

    inline const BaseArray < PvPItemInfo >& GetPvPItemInfoArray()
    {
        return GWCA_CALL_C(GW_Items_GetPvPItemInfoArray);
    }

    inline const CompositeModelInfo* GetCompositeModelInfo(uint32_t model_file_id)
    {
        return GWCA_CALL_C(GW_Items_GetCompositeModelInfo, model_file_id);
    }

    inline const BaseArray < CompositeModelInfo >& GetCompositeModelInfoArray()
    {
        return GWCA_CALL_C(GW_Items_GetCompositeModelInfoArray);
    }

    inline bool GetPvPItemUpgradeEncodedName(uint32_t pvp_item_upgrade_idx, wchar_t** out)
    {
        return GWCA_CALL_C(GW_Items_GetPvPItemUpgradeEncodedName, pvp_item_upgrade_idx, out);
    }

    inline bool GetPvPItemUpgradeEncodedDescription(uint32_t pvp_item_upgrade_idx, wchar_t** out)
    {
        return GWCA_CALL_C(GW_Items_GetPvPItemUpgradeEncodedDescription, pvp_item_upgrade_idx, out);
    }

    inline const ItemFormula* GetItemFormula(const GW::Item* item)
    {
        return GWCA_CALL_C(GW_Items_GetItemFormula, item);
    }
}
#endif
