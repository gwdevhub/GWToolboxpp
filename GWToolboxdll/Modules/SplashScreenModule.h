#pragma once

#include <ToolboxModule.h>

class SplashScreenModule : public ToolboxModule {
    SplashScreenModule() = default;
    ~SplashScreenModule() override = default;

public:
    static SplashScreenModule& Instance()
    {
        static SplashScreenModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Splash Screen"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Replace the background and logo artwork on the Guild Wars startup splash window with your own images.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_IMAGE; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;
};
