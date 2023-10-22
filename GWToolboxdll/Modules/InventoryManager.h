#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Packets/StoC.h>

#include <ToolboxWidget.h>

namespace GW::Constants {
    enum class Rarity : uint8_t {
        White,
        Blue,
        Purple,
        Gold,
        Green
    };
}

class InventoryManager : public ToolboxUIElement {
public:
    InventoryManager()
    {
        current_salvage_session.salvage_item_id = 0;
        is_movable = is_resizable = has_closebutton = can_show_in_main_window = false;
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

    static InventoryManager& Instance()
    {
        static InventoryManager instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Inventory Management"; }
    [[nodiscard]] const char* SettingsName() const override { return "Inventory Settings"; }

    void Draw(IDirect3DDevice9* device) override;
    bool DrawItemContextMenu(bool open = false);

    void IdentifyAll(IdentifyAllType type);
    void SalvageAll(SalvageAllType type);
    bool IsPendingIdentify() const;
    bool IsPendingSalvage() const;
    bool HasSettings() override { return true; }
    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    bool WndProc(UINT, WPARAM, LPARAM) override;

    // Find an empty (or partially empty) inventory slot that this item can go into
    static std::pair<GW::Bag*, uint32_t> GetAvailableInventorySlot(GW::Item* like_item = nullptr);
    static uint16_t RefillUpToQuantity(uint16_t quantity, const std::vector<uint32_t>& model_ids);
    // Find an empty (or partially empty) inventory slot that this item can go into. !entire_stack = Returns slots that are the same item, but won't hold all of them.
    static GW::Item* GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack = false);
    // Checks model info and struct info to make sure item is the same.
    static bool IsSameItem(const GW::Item* item1, const GW::Item* item2);

    static void ItemClickCallback(GW::HookStatus*, uint32_t type, uint32_t slot, const GW::Bag* bag);
    static void OnOfferTradeItem(GW::HookStatus* status, uint32_t item_id, uint32_t quantity);
    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*);


    IdentifyAllType identify_all_type = IdentifyAllType::None;
    SalvageAllType salvage_all_type = SalvageAllType::None;

protected:
    void ShowVisibleRadio() override { };

private:
    bool trade_whole_stacks = false;
    bool show_item_context_menu = false;
    bool is_identifying = false;
    bool is_identifying_all = false;
    bool is_salvaging = false;
    bool is_salvaging_all = false;
    bool has_prompted_salvage = false;
    bool show_salvage_all_popup = true;
    bool salvage_listeners_attached = false;
    bool only_use_superior_salvage_kits = false;
    bool hide_unsellable_items = false;
    bool hide_weapon_sets_and_customized_items = false;
    std::map<uint32_t, std::string> hide_from_merchant_items;
    bool salvage_rare_mats = false;
    bool show_transact_quantity_popup = false;
    bool transaction_listeners_attached = false;

    bool wiki_link_on_context_menu = false;
    bool right_click_context_menu_in_explorable = true;
    bool right_click_context_menu_in_outpost = true;

    std::map<GW::Constants::Bag, bool> bags_to_salvage_from = {
        {GW::Constants::Bag::Backpack, true},
        {GW::Constants::Bag::Belt_Pouch, true},
        {GW::Constants::Bag::Bag_1, true},
        {GW::Constants::Bag::Bag_2, true}
    };

    size_t identified_count = 0;
    size_t salvaged_count = 0;

    GW::Packet::StoC::SalvageSession current_salvage_session{};

    void ContinueIdentify();
    void ContinueSalvage();

    GW::HookEntry on_map_change_entry;
    GW::HookEntry salvage_hook_entry;
    GW::HookEntry transaction_hook_entry;
    GW::HookEntry ItemClick_Entry;


    void FetchPotentialItems();
    void AttachSalvageListeners();
    void DetachSalvageListeners();
    static void ClearSalvageSession(GW::HookStatus* status = nullptr, void* packet = nullptr);
    void CancelSalvage();
    void CancelIdentify();
    void CancelAll();
    void ContinueTransaction();
    void CancelTransaction();
    static void ClearTransactionSession(GW::HookStatus* status = nullptr, void* packet = nullptr);
    void AttachTransactionListeners();
    void DetachTransactionListeners();

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
        bool IsWeapon();
        bool IsArmor();
        bool IsSalvagable();
        bool IsHiddenFromMerchants();

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

    Item* GetNextUnsalvagedItem(const Item* salvage_kit = nullptr, const Item* start_after_item = nullptr);
    Item* GetNextUnidentifiedItem(const Item* start_after_item = nullptr) const;
    void Identify(const Item* item, const Item* kit);
    void Salvage(Item* item, const Item* kit);

    uint32_t stack_prompt_item_id = 0;

private:
    struct TransactItems {
        uint32_t type = 0;
        uint32_t gold_give = 0;
        uint32_t item_give_count = 0;
        uint32_t item_give_ids[16]{};
        uint32_t item_give_quantities[16]{};
        uint32_t gold_recv = 0;
        uint32_t item_recv_count = 0;
        uint32_t item_recv_ids[16]{};
        uint32_t item_recv_quantities[16]{};
    };

    struct CtoS_QuoteItem {
        uint32_t header = GAME_CMSG_REQUEST_QUOTE;
        uint32_t type = 0;
        uint32_t unk1 = 0;
        uint32_t gold_give = 0;
        uint32_t item_give_count = 0;
        uint32_t item_give_ids[16]{};
        uint32_t gold_recv = 0;
        uint32_t item_recv_count = 0;
        uint32_t item_recv_ids[16]{};
    };

    static_assert(sizeof(CtoS_QuoteItem) == 0x9C);

    struct PendingTransaction {
        enum State : uint8_t {
            None,
            Prompt,
            Pending,
            Quoting,
            Quoted,
            Transacting
        } state = None;

        uint32_t type = 0;
        uint32_t price = 0;
        uint32_t item_id = 0;
        clock_t state_timestamp = 0;
        uint8_t retries = 0;

        void setState(const State _state)
        {
            state = _state;
            state_timestamp = clock();
        }

        CtoS_QuoteItem quote();
        TransactItems transact();
        Item* item() const;
        bool in_progress() const { return state > Prompt; }
        bool selling();
    };


    struct PendingItem {
        uint32_t item_id = 0;
        uint32_t slot = 0;
        GW::Constants::Bag bag = GW::Constants::Bag::None;
        uint32_t uses = 0;
        uint32_t quantity = 0;
        bool set(const Item* item = nullptr);
        GuiUtils::EncString name;
        GuiUtils::EncString desc;
        GuiUtils::EncString wiki_name;

        class PluralEncString : public GuiUtils::EncString {
        protected:
            void sanitise() override;
        };

        PluralEncString plural_item_name;

        Item* item() const;
    };

    struct PotentialItem : PendingItem {
        bool proceed = true;
    };

    std::vector<PotentialItem*> potential_salvage_all_items{}; // List of items that would be processed if user confirms Salvage All
    void ClearPotentialItems();
    PendingItem pending_identify_item;
    PendingItem pending_identify_kit;
    PendingItem pending_salvage_item;
    PendingItem pending_salvage_kit;
    PendingTransaction pending_transaction;

    int pending_transaction_amount = 0;
    bool pending_cancel_transaction = false;
    bool is_transacting = false;
    bool has_prompted_transaction = false;

    clock_t pending_salvage_at = 0;
    clock_t pending_identify_at = 0;
    PendingItem context_item;
    bool pending_cancel_salvage = false;
};
