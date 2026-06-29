#pragma once

#include <ToolboxModule.h>

// Remembers each hero command panel's position (keyed by party slot or hero id) and restores it when the panel reappears, since the game's own per-slot memory is unreliable.
class HeroPanelPositionModule : public ToolboxModule {
    HeroPanelPositionModule() = default;
    ~HeroPanelPositionModule() override = default;

public:
    static HeroPanelPositionModule& Instance()
    {
        static HeroPanelPositionModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Hero Panel Positions"; }
    [[nodiscard]] const char* Description() const override { return "Remembers the on-screen position of each hero's command panel and restores it when the panel reappears."; }
    [[nodiscard]] bool HasSettings() override { return false; }

    // Registers the keying-mode radio buttons into the existing Party Settings section.
    void RegisterSettingsContent() override;

    void Initialize() override;
    void SignalTerminate() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
