#pragma once

#include <ToolboxModule.h>

// Disables the camera-glitch mouse fix entirely; none of its code is compiled when 0.
// The cursor-size scaling feature is unaffected by this define.
// Disabled Aug 2026 after a game update affected lookaround speed; code kept for reference.
#define MOUSEFIX_ENABLE_CAMERA_FIX 0

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
#if MOUSEFIX_ENABLE_CAMERA_FIX
    [[nodiscard]] const char* Description() const override { return " - Fixes occasional camera glitch when looking around in-game\n - Adds option to scale cursor size"; }
#else
    [[nodiscard]] const char* Description() const override { return " - Adds option to scale cursor size"; }
#endif // MOUSEFIX_ENABLE_CAMERA_FIX
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
