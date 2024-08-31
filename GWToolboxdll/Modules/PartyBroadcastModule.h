#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>
#include <minwindef.h>
#include <ToolboxIni.h>

class PartyBroadcast : public ToolboxModule {
    PartyBroadcast() = default;
public:
    static PartyBroadcast& Instance()
    {
        static PartyBroadcast instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Party Broadcast"; }
    [[nodiscard]] const char* Description() const override { return "Broadcast party searches to https://party.gwtoolbox.com"; }
    [[nodiscard]] const char* SettingsName() const override { return "Game Settings"; }

    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Update(float) override;
    bool HasSettings() override { return false; }
};
