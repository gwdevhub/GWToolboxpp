#pragma once

#include <ToolboxModule.h>

// Replaces the artwork on the Guild Wars ("Reforged") startup splash window.
// The splash background and the "GUILD WARS REFORGED" logo are both built by the
// DnCtl image-control constructor, which takes the compressed image bytes and a
// flag identifying which control it is (0 = background, 1 = foreground logo). We
// hook that constructor and, when the user has configured a replacement, hand it
// our own file bytes instead. The control scales the decoded image to its target
// rect, so any size/format the game's decoder accepts works - no re-encoding.
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
