#pragma once

#include <ToolboxModule.h>

// Replaces the artwork shown on the Guild Wars startup ("DnSplash") window.
// The game embeds that art as Cmp-compressed DXT1 texture data described by a
// small struct; we re-encode a user image to DXT1 and repoint the struct at it.
// See the reverse-engineering notes for the struct layout and the stored-mode
// CmpDecompress fast path that lets us skip ArenaNet's Cmp compression.
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
        return "Replace the background artwork on the Guild Wars startup splash window with your own image.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_IMAGE; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;
};
