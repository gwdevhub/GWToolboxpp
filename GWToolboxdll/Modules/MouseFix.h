#pragma once

#include <ToolboxModule.h>

class MouseFix : public ToolboxModule {
    MouseFix() = default;
    ~MouseFix() override = default;

public:
    static MouseFix& Instance()
    {
        static MouseFix instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Mouse Settings"; }
    [[nodiscard]] const char* Description() const override { return " - Fixes occasional camera glitch when looking around in-game\n - Adds option to scale cursor size"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MOUSE_POINTER; }

    struct Settings {
        bool enable_cursor_fix = false;
        int cursor_size = 32;
    };

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Initialize() override;
    void Terminate() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;
    void DrawSettingsInternal() override;
};
