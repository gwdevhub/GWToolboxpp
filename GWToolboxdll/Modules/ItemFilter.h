#pragma once

#include <vector>

#include <GWCA/GameEntities/Item.h>
#include <GWCA/Packets/StoC.h>
#include <ToolboxModule.h>

using ItemModelID = decltype(GW::Item::model_id);
enum class Rarity : uint8_t { White, Blue, Purple, Gold, Green, Unknown };

class ItemFilter : public ToolboxModule {
    ItemFilter() = default;
    ~ItemFilter() override {
        SpawnSuppressedItems();
    }

public:
    static ItemFilter& Instance() {
        static ItemFilter instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Filter"; }
    [[nodiscard]] const char* SettingsName() const override { return "Item Settings"; }

    void Initialize() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    void Update([[maybe_unused]] float delta) override {}

    void SpawnSuppressedItems();

private:
    std::vector<GW::Packet::StoC::AgentAdd> suppressed_packets{};

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

    static void OnAgentAdd(GW::HookStatus*, GW::Packet::StoC::PacketBase*);
    static void OnAgentRemove(GW::HookStatus*, GW::Packet::StoC::PacketBase*);
    static void OnMapLoad(GW::HookStatus*, GW::Packet::StoC::PacketBase*);

    GW::HookEntry OnAgentAdd_Entry;
    GW::HookEntry OnAgentRemove_Entry;
    GW::HookEntry OnMapLoad_Entry;
};
