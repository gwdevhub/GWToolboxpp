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

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
