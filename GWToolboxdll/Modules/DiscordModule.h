#pragma once

#include <ToolboxModule.h>


class DiscordModule : public ToolboxModule {
    DiscordModule() = default;

    ~DiscordModule() override = default;

public:
    static DiscordModule& Instance()
    {
        static DiscordModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Discord频道"; }
    [[nodiscard]] const char* Description() const override { return "在《激战》的Discord频道中展示更清晰的展示当前信息。"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }

    [[nodiscard]] const char* SettingsName() const override { return "第三方客户端"; }

    struct Settings {
        bool discord_enabled = true;
        bool hide_activity_when_offline = true;
        bool show_location_info = true;
        bool show_character_info = true;
        bool show_party_info = true;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawSettingsInternal() override;
};
