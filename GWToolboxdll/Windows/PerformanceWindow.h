#pragma once

#include "ToolboxWindow.h"

class PerformanceWindow : public ToolboxWindow {
    PerformanceWindow() = default;
    ~PerformanceWindow() override = default;

public:
    static PerformanceWindow& Instance()
    {
        static PerformanceWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Performance"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TACHOMETER_ALT; }

    struct Settings {
        int slow_threshold_us = 1000;
    };

    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;
    void DrawSettingsInternal() override;
};
