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

    [[nodiscard]] const char* Name() const override { return "英雄面板位置"; }
    [[nodiscard]] const char* Description() const override { return "记住每个英雄命令面板在屏幕上的位置，并在面板显示时恢复其位置。"; }
    [[nodiscard]] bool HasSettings() override { return false; }

    // Registers the keying-mode radio buttons into the existing Party Settings section.
    void RegisterSettingsContent() override;

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float delta) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
