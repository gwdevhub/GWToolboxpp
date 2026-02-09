#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameEntities/Item.h>

#include <Utils/GuiUtils.h>

#include <ToolboxWidget.h>

namespace GW {
    namespace Constants {

        enum class Bag : uint8_t;
        enum class Rarity : uint8_t;
        enum class ItemType : uint8_t;
    }
    namespace UI {
        enum class UIMessage : uint32_t;
        namespace UIPacket {
            struct kMouseAction;
        }
    }
    namespace Merchant {
        enum class TransactionType : uint32_t;
    }
}

class InventoryOverlayWidget;

class InventoryManager : public ToolboxUIElement {
    InventoryManager()
    {
        is_movable = is_resizable = has_closebutton = has_titlebar = can_show_in_main_window = false;
    }
    ~InventoryManager() override = default;
public:

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

    static InventoryManager& Instance()
    {
        static InventoryManager instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Inventory Management"; }
    [[nodiscard]] const char* SettingsName() const override { return "Inventory Settings"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BOXES; }

    void Draw(IDirect3DDevice9* device) override;
    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static uint16_t CountItemsByName(const wchar_t* name_enc);

    bool DrawItemContextMenu(bool open = false);
    // Find an empty (or partially empty) inventory slot that this item can go into
    static std::pair<GW::Bag*, uint32_t> GetAvailableInventorySlot(GW::Item* like_item = nullptr);
    static uint16_t RefillUpToQuantity(uint16_t quantity, const std::vector<uint32_t>& model_ids);
    static uint16_t StoreItems(uint16_t quantity, const std::vector<uint32_t>& model_ids);
    // Find an empty (or partially empty) inventory slot that this item can go into. !entire_stack = Returns slots that are the same item, but won't hold all of them.
    static GW::Item* GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack = false);
    // Checks model info and struct info to make sure item is the same.
    static bool IsSameItem(const GW::Item* item1, const GW::Item* item2);

    static void ItemClickCallback(GW::HookStatus*, GW::UI::UIPacket::kMouseAction*, GW::Item*);



protected:
    void ShowVisibleRadio() override { }

public:
    struct Item : GW::Item {
        GW::ItemModifier* GetModifier(uint32_t identifier) const;
        GW::Constants::Rarity GetRarity() const;
        uint32_t GetUses() const;
        bool IsIdentificationKit() const;
        bool IsSalvageKit() const;
        bool IsTome() const;
        bool IsLesserKit() const;
        bool IsExpertSalvageKit() const;
        bool IsPerfectSalvageKit() const;
        bool IsWeapon() const;
        bool IsArmor() const;
        bool IsSalvagable(bool check_bag = true, bool check_blocked_from_being_salvaged = true) const;
        bool IsHiddenFromMerchants() const;

        bool IsInventoryItem() const;
        bool IsStorageItem() const;

        bool IsRareMaterial() const;
        bool IsOfferedInTrade() const;
        bool CanOfferToTrade() const;

        [[nodiscard]] bool IsSparkly() const
        {
            return (interaction & 0x2000) == 0;
        }

        [[nodiscard]] bool GetIsIdentified() const
        {
            return (interaction & 1) != 0;
        }
        [[nodiscard]] bool CanBeIdentified() const;
        [[nodiscard]] bool IsPrefixUpgradable() const
        {
            return ((interaction >> 14) & 1) == 0;
        }

        [[nodiscard]] bool IsSuffixUpgradable() const
        {
            return ((interaction >> 15) & 1) == 0;
        }

        [[nodiscard]] bool IsStackable() const
        {
            return (interaction & 0x80000) != 0;
        }

        [[nodiscard]] bool IsUsable() const
        {
            return (interaction & 0x1000000) != 0;
        }

        [[nodiscard]] bool IsTradable() const
        {
            return (interaction & 0x100) == 0;
        }

        [[nodiscard]] bool IsInscription() const
        {
            return (interaction & 0x25000000) == 0x25000000;
        }

        [[nodiscard]] bool IsOldSchool() const;

        [[nodiscard]] bool IsUpgradable() const { 
            return GetIsInscribable() || IsPrefixUpgradable() || IsSuffixUpgradable();
        }

        [[nodiscard]] bool IsBlue() const
        {
            return single_item_name && single_item_name[0] == 0xA3F;
        }

        [[nodiscard]] bool IsPurple() const
        {
            return (interaction & 0x400000) != 0;
        }

        [[nodiscard]] bool IsGreen() const
        {
            return (interaction & 0x10) != 0;
        }

        [[nodiscard]] bool IsGold() const
        {
            return (interaction & 0x20000) != 0;
        }
    };

    static Item* GetNextUnsalvagedItem(const Item* salvage_kit = nullptr, const Item* start_after_item = nullptr);
};
