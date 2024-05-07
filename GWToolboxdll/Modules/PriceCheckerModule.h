#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameEntities/Item.h>
#include <string.h>

class PriceCheckerModule : public ToolboxModule {
    PriceCheckerModule() = default;
    ~PriceCheckerModule() override = default;

public:
    static PriceCheckerModule& Instance()
    {
        static PriceCheckerModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Price Checker"; }
    [[nodiscard]] const char* Description() const override { return "Adds extra information to the hover description of an item"; }
    bool HasSettings() override { return true; }
    [[nodiscard]] const char* SettingsName() const override { return "Inventory Settings"; }

    void Initialize() override;
    void LoadSettings(ToolboxIni* ini) override;
    void RegisterSettingsContent() override;
    void SaveSettings(ToolboxIni* ini) override;

    // Returns a list of prices by identifier. Materials have identifiers are "model_id", but runes and mods are "model_id-mod_struct"
    static const std::unordered_map<std::string,uint32_t>& FetchPrices();
};
