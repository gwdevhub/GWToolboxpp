#pragma once

#include <ToolboxModule.h>

class PartyWindowModule : public ToolboxModule {
    PartyWindowModule() = default;

public:
    static PartyWindowModule& Instance()
    {
        static PartyWindowModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Party Window"; }
    [[nodiscard]] const char* SettingsName() const override { return "Party Settings"; }
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

private:
    static void LoadDefaults();

private:
};
