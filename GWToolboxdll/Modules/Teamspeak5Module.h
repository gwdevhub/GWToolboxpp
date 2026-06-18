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

    struct Settings {
        bool enabled = true;
        int ws_port = 5899;
        std::string gwtoolbox_teamspeak5_api_key;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingsInternal() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
