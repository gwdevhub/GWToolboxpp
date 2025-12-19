#pragma once

#include <ToolboxModule.h>

class GuildWarsUI_ToolboxSettings : public ToolboxModule {
    GuildWarsUI_ToolboxSettings() = default;
    ~GuildWarsUI_ToolboxSettings() override = default;

public:
    static GuildWarsUI_ToolboxSettings& Instance()
    {
        static GuildWarsUI_ToolboxSettings instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Guild Wars F11 Toolbox Tab"; }
    [[nodiscard]] const char* Description() const override { return "Shows an extra tab in the F11 settings window for toolbox"; }

    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;

    static void AddCheckboxPref(const char* label, bool* variable);
};
