#pragma once

#include <ToolboxModule.h>

class Obfuscator : public ToolboxModule {
public:
    static Obfuscator& Instance() {
        static Obfuscator instance;
        return instance;
    }

    const char* Name() const override { return "Obfuscator"; }
    const char* SettingsName() const override { return "Game Settings"; }

    void Initialize() override;
    void Update(float) override;
    void Terminate() override;
    void DrawSettingInternal() override;
    void SaveSettings(ToolboxIni* ini) override;
    void LoadSettings(ToolboxIni* ini) override;

    static void Obfuscate(bool obfuscate);
};
