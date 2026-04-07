#pragma once

#include <ToolboxModule.h>
#include <GWCA/Constants/Constants.h>

namespace GW {
    struct Item;
}

// Responsible only for fetching and exposing trader price data.
// All tooltip rendering is handled by ItemTooltipModule.
class PriceCheckerModule : public ToolboxModule {
    PriceCheckerModule() = default;
    ~PriceCheckerModule() = default;

public:
    static PriceCheckerModule& Instance()
    {
        static PriceCheckerModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Price Checker"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COINS; }

    void Initialize() override;
    void Terminate() override;

    void SaveSettings(ToolboxIni* ini) override;
    void LoadSettings(ToolboxIni* ini) override;

    void DrawSettingsInternal() override;

    // Fetch (and cache) prices from kamadan.gwtoolbox.com.
    // Returns the current price map; triggers a background refresh if stale.
    static const std::unordered_map<std::string, uint32_t>& FetchPrices();

    // Returns the trader sell price for the given item or material slot.
    // May return the fixed merchant price instead when in Melandru's Accord.
    static uint32_t GetTraderSellPrice(const GW::Item* item);
    static uint32_t GetTraderSellPrice(GW::Constants::MaterialSlot material);

    // Returns the fixed NPC merchant sell price for a material.
    static uint32_t GetMerchantSellPrice(GW::Constants::MaterialSlot material);

    // Look up the price for an item by inspecting its mod struct.
    // mod_start_index allows retrieving a second distinct price on the same item.
    // Populates item_name_out when non-null.
    static uint32_t GetPriceByItem(const GW::Item* item,
        std::string* item_name_out = nullptr,
        unsigned int mod_start_index = 0);
};
