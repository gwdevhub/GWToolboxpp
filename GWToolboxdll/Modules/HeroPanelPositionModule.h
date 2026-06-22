#pragma once

#include <ToolboxModule.h>

// Remembers where each hero's command panel (skill bar) was last placed and restores it
// whenever that panel is recreated - e.g. after reordering the party or swapping characters,
// which otherwise reshuffles the panels around the screen. Positions are keyed either by party
// slot or by hero id (user setting); the game's own per-slot memory is unreliable, so we override
// it in both modes.
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
