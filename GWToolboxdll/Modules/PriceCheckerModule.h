#pragma once

#include <ToolboxModule.h>

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
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void SaveSettings(ToolboxIni* ini) override;

    // Returns a list of prices by identifier. Materials have identifiers are "model_id", but runes and mods are "model_id-mod_struct"
    static const std::unordered_map<std::string,uint32_t>& FetchPrices();
};
