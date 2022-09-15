#pragma once

#include <vector>

#include <GWCA/GameEntities/Item.h>
#include <GWCA/Packets/StoC.h>
#include <ToolboxModule.h>

using ItemModelID = decltype(GW::Item::model_id);
enum class Rarity : uint8_t { White, Blue, Purple, Gold, Green, Unknown };

class ItemFilter : public ToolboxModule {
public:
    static ItemFilter& Instance() {
        static ItemFilter instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Filter"; }
    [[nodiscard]] const char8_t* Icon() const override { return ICON_FA_COINS; }
    [[nodiscard]] const char* SettingsName() const override { return "Item Settings"; }

    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override { return suppressed_packets.empty(); }
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    void Update([[maybe_unused]] float delta) override {}

    [[nodiscard]] GW::AgentID GetItemOwner(GW::ItemID item_id) const;
    void SpawnSuppressedItems();

private:
    struct ItemOwner {
        GW::ItemID item;
        GW::AgentID owner;
    };

    std::vector<GW::Packet::StoC::AgentAdd> suppressed_packets{};
    std::vector<ItemOwner> item_owners{};

    [[nodiscard]] bool WantToHide(const GW::Item& item, bool can_pick_up) const;
    bool hide_player_white = false;
    bool hide_player_blue = false;
    bool hide_player_purple = false;
    bool hide_player_gold = false;
    bool hide_player_green = false;
    bool hide_party_white = false;
    bool hide_party_blue = false;
    bool hide_party_purple = false;
    bool hide_party_gold = false;
    bool hide_party_green = false;
    std::map<ItemModelID, std::string> dont_hide_for_player{};
    std::map<ItemModelID, std::string> dont_hide_for_party{};

    static void OnAgentAdd(GW::HookStatus*, GW::Packet::StoC::AgentAdd*);
    static void OnAgentRemove(GW::HookStatus*, GW::Packet::StoC::AgentRemove*);
    static void OnMapLoad(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);
    static void OnItemReuseId(GW::HookStatus*, GW::Packet::StoC::ItemGeneral_ReuseID*);
    static void OnItemUpdateOwner(GW::HookStatus*, GW::Packet::StoC::ItemUpdateOwner*);

    GW::HookEntry OnAgentAdd_Entry;
    GW::HookEntry OnAgentRemove_Entry;
    GW::HookEntry OnMapLoad_Entry;
    GW::HookEntry OnItemReuseId_Entry;
    GW::HookEntry OnItemUpdateOwner_Entry;
};
