#pragma once

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Packets/StoC.h>

#include <ToolboxModule.h>

class ZrawDeepModule : public ToolboxModule {
public:
    static ZrawDeepModule& Instance()
    {
        static ZrawDeepModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "24h Deep Mode"; }
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    bool HasSettings() override;
    void SetEnabled(bool enabled);
    void Update(float delta) override;
    void DrawSettingsInternal() override;
    static void DisplayDialogue(GW::Packet::StoC::DisplayDialogue*);
    static void PlayKanaxaiDialog(uint8_t idx);
    void SaveSettings(ToolboxIni* ini) override;
    void LoadSettings(ToolboxIni* ini) override;

    void SetTransmogs() const;
    static bool IsEnabled();

    GW::HookEntry ZrawDeepModule_StoCs;
};
