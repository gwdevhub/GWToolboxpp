#pragma once

#include <ToolboxWidget.h>

#include <Modules/InventoryItem.h>

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
    // Withdraw `amount` items matching the encoded name from storage to inventory.
    // If check_already_withdrawn is true, items already in inventory count toward the amount.
    static uint16_t WithdrawItemsByName(const wchar_t* name_enc, uint32_t amount, bool check_already_withdrawn = true);
    // Withdraw `amount` items matching the model id from storage to inventory.
    // If check_already_withdrawn is true, items already in inventory count toward the amount.
    static uint16_t WithdrawItemsByModelID(uint32_t model_id, uint32_t amount, bool check_already_withdrawn = true);
    // Find an empty (or partially empty) inventory slot that this item can go into. !entire_stack = Returns slots that are the same item, but won't hold all of them.
    static GW::Item* GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack = false);
    // Checks model info and struct info to make sure item is the same.
    static bool IsSameItem(const GW::Item* item1, const GW::Item* item2);

    static void ItemClickCallback(GW::HookStatus*, GW::UI::UIPacket::kMouseAction*, GW::Item*);



protected:
    void ShowVisibleRadio() override { }

public:
    using Item = InventoryItem;

    static uint16_t MoveItem(const Item* item, const uint16_t quantity = 1000u);
    static Item* GetNextUnsalvagedItem(const Item* salvage_kit = nullptr, const Item* start_after_item = nullptr);
};
