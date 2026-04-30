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

    void Draw(IDirect3DDevice9* pDevice) override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
