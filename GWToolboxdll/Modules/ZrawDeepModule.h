#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>

class ZrawDeepModule : public ToolboxModule {
    ~ZrawDeepModule() override
    {
        delete mp3;
    };
    Mp3* mp3 = nullptr;
    
    bool enabled = false;
    bool transmo_team = true;
    bool rewrite_npc_dialogs = true;
    bool kanaxais_true_form = true;

    clock_t pending_transmog = 0;
    bool can_terminate = true;
    bool terminating = false;
public:
    static ZrawDeepModule& Instance() {
        static ZrawDeepModule instance;
        return instance;
    }

    const char* Name() const override { return "24h Deep Mode"; }
    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override { return can_terminate; }
    bool HasSettings() override { return enabled; }
    void SetEnabled(bool enabled);
    void Update(float delta) override;
    void DrawSettingInternal() override;
    void DisplayDialogue(GW::Packet::StoC::DisplayDialogue*);
    void PlayKanaxaiDialog(uint8_t idx);
    void SaveSettings(ToolboxIni* ini) override;
    void LoadSettings(ToolboxIni* ini) override;

    void SetTransmogs();
    bool IsEnabled() const
    {
        return enabled;
    }

    GW::HookEntry ZrawDeepModule_StoCs;
};
