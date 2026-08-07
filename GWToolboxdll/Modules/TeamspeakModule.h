#pragma once

#include <ToolboxModule.h>

class TeamspeakModule : public ToolboxModule {
public:
    static TeamspeakModule& Instance()
    {
        static TeamspeakModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "TeamSpeak 3"; }
    [[nodiscard]] const char* Description() const override { return "启用 /teamspeak 命令以将当前 TeamSpeak 3 服务器信息发送到聊天中"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }

    [[nodiscard]] const char* SettingsName() const override { return "Third Party Integration"; }

    struct Settings {
        bool enabled = false;
        std::string teamspeak3_api_key;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void DrawSettingsInternal() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
