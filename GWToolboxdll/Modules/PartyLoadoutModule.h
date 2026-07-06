#pragma once

#include <ToolboxModule.h>

// Renders extended party-loadout templates (header 15 / type 1) natively in-game and
// applies their hero builds and behaviours.
class PartyLoadoutModule : public ToolboxModule {
    PartyLoadoutModule() = default;
    ~PartyLoadoutModule() override = default;

public:
    static PartyLoadoutModule& Instance()
    {
        static PartyLoadoutModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Party Loadout Templates"; }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
};
