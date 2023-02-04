#pragma once

#include <ToolboxModule.h>

class Teamspeak5Module : public ToolboxModule {
public:
    static Teamspeak5Module& Instance() {
        static Teamspeak5Module instance;
        return instance;
    }

    const char* Name() const override { return "Teamspeak 5"; }
    const char* Description() const override { return "Enables /teamspeak command to send current teamspeak 5 server info to chat"; }
    const char* Icon() const override { return ICON_FA_HEADSET; }

    const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingInternal() override;

    void LoadSettings(ToolboxIni* ini);
    void SaveSettings(ToolboxIni* ini);

};
