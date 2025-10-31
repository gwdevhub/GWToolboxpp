#pragma once

#include <ToolboxModule.h>

class SteamModule : public ToolboxModule {
    SteamModule() = default;
    ~SteamModule() override = default;

public:
    static SteamModule& Instance()
    {
        static SteamModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Steam Integration"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GAMEPAD; }
    [[nodiscard]] const char* Description() const override { return "Allows Guild Wars to integrate with Steam for overlay and other steam features"; }

    void Initialize() override;
    void DrawSettingsInternal() override;
};
