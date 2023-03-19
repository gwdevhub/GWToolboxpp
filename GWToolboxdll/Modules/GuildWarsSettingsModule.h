#pragma once

#include <ToolboxModule.h>

class GuildWarsSettingsModule : public ToolboxModule {
    GuildWarsSettingsModule() = default;
    ~GuildWarsSettingsModule() = default;

public:
    static GuildWarsSettingsModule& Instance() {
        static GuildWarsSettingsModule instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars Settings"; }
    const char* Description() const override { return "Ability to save or load Guild Wars settings to a file on disk"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
};
