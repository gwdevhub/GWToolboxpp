#pragma once

#include <ToolboxModule.h>

class TeamspeakModule : public ToolboxModule {
public:
    static TeamspeakModule& Instance()
    {
        static TeamspeakModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Teamspeak 3"; }
    [[nodiscard]] const char* Description() const override { return "Enables /teamspeak command to send current teamspeak 3 server info to chat"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }

    [[nodiscard]] const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingsInternal() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
