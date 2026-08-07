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

    [[nodiscard]] const char* Name() const override { return "鼠标设置"; }
    [[nodiscard]] const char* Description() const override { return " - 修复在游戏内环视时偶尔出现的视角故障\n - 添加了缩放光标大小的选项"; }
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
