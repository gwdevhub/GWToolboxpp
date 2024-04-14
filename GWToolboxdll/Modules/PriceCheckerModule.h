#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameEntities/Item.h>

class PriceChecker : public ToolboxModule {
    PriceChecker() = default;
    ~PriceChecker() override = default;

public:
    static PriceChecker& Instance()
    {
        static PriceChecker instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Price Checker"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    static int GetPrice(GW::ItemModifier itemModifier);
};
