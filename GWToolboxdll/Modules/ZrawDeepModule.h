#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>

class ZrawDeepModule : public ToolboxModule {
public:
    static ZrawDeepModule& Instance() {
        static ZrawDeepModule instance;
        return instance;
    }

    const char* Name() const override { return "24h Deep Mode"; }
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    bool HasSettings() override;
    void SetEnabled(bool enabled);
    void Update(float delta) override;
    void DrawSettingInternal() override;
    void DisplayDialogue(GW::Packet::StoC::DisplayDialogue*);
    void PlayKanaxaiDialog(uint8_t idx);
    void SaveSettings(ToolboxIni* ini) override;
    void LoadSettings(ToolboxIni* ini) override;

    void SetTransmogs();
    bool IsEnabled();

    GW::HookEntry ZrawDeepModule_StoCs;
};
