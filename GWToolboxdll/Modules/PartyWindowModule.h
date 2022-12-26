#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/GameEntities/Agent.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>
#include <Timer.h>

class PartyWindowModule : public ToolboxModule {
    PartyWindowModule() = default;
public:
    static PartyWindowModule& Instance() {
        static PartyWindowModule instance;
        return instance;
    }

    const char* Name() const override { return "Party Window"; }
    const char* SettingsName() const override { return "Party Settings"; }
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

private:
    void LoadDefaults();
private:
};
