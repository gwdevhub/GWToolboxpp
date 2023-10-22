#pragma once

#include <ToolboxModule.h>

class GuildWarsSettingsModule : public ToolboxModule {
    GuildWarsSettingsModule() = default;
    ~GuildWarsSettingsModule() override = default;

public:
    static GuildWarsSettingsModule& Instance()
    {
        static GuildWarsSettingsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_CHECK_SQUARE; }
    [[nodiscard]] const char* Name() const override { return "Guild Wars Settings"; }
    [[nodiscard]] const char* Description() const override { return "Ability to save or load Guild Wars settings to a file on disk"; }

    void Initialize() override;
    void Update(float) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void Terminate() override;
    void DrawSettingsInternal() override;
};
