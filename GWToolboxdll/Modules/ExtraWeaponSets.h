#pragma once

#include <ToolboxModule.h>

class ExtraWeaponSets : public ToolboxModule {
    ExtraWeaponSets() = default;
    ~ExtraWeaponSets() override = default;

public:
    static ExtraWeaponSets& Instance()
    {
        static ExtraWeaponSets instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Extra Weapon Sets"; }

    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
};
