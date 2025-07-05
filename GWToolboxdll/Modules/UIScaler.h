#pragma once

#include <ToolboxModule.h>

class UIScaler : public ToolboxModule {
    UIScaler() = default;
    ~UIScaler() override = default;

public:
    static UIScaler& Instance()
    {
        static UIScaler instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "UI Scaler"; }
    [[nodiscard]] const char* Description() const override { 
        return "Allows you to apply a scale multiplier on top of the Guild Wars interface size setting.\nUse /setuiscale <0.1 - 10.0> to set the scale factor.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TEXT_HEIGHT; }

    static float GetUIScaleFactor();
    void Initialize() override;
    void SignalTerminate() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void DrawSettingsInternal() override;
};
