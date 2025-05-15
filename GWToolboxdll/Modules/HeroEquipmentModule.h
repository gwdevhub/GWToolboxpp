#pragma once

#include <ToolboxModule.h>

class HeroEquipmentModule : public ToolboxModule {
    HeroEquipmentModule() = default;
    ~HeroEquipmentModule() override = default;

public:
    static HeroEquipmentModule& Instance()
    {
        static HeroEquipmentModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Hero Equipment"; }
    [[nodiscard]] const char* Description() const override { return "Allows ability for hero equipment to be viewed separately from the in-game Inventory window"; }

    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
};
