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

    [[nodiscard]] const char* Name() const override { return "Discord"; }
    [[nodiscard]] const char* Description() const override { return "Show better 'Now Playing' info in Discord from Guild Wars"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }

    [[nodiscard]] const char* SettingsName() const override { return "Third Party Integration"; }

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
