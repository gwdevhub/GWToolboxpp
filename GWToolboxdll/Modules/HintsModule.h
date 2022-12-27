#pragma once

#include <ToolboxModule.h>

class HintsModule : public ToolboxModule {
public:
    static HintsModule& Instance() {
        static HintsModule instance;
        return instance;
    }
    const char* Icon() const override { return ICON_FA_LIGHTBULB; }
    const char* Name() const override { return "Hints"; }
    const char* SettingsName() const override { return "In-Game Hints"; }

    void Initialize() override;
    void Update(float) override;
    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;


};
