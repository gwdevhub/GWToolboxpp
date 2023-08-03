#pragma once

#include <ToolboxModule.h>

class Teamspeak5Module : public ToolboxModule {
public:
    static Teamspeak5Module& Instance()
    {
        static Teamspeak5Module instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Teamspeak 5"; }
    [[nodiscard]] const char* Description() const override { return "Enables /teamspeak command to send current teamspeak 5 server info to chat"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }

    [[nodiscard]] const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingsInternal() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
