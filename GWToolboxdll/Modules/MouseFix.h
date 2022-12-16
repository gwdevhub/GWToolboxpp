#pragma once

#include <ToolboxModule.h>

class MouseFix : public ToolboxModule {
    MouseFix() = default;
    ~MouseFix() = default;

public:
    static MouseFix& Instance()
    {
        static MouseFix instance;
        return instance;
    }
    const char* Name() const override { return "Mouse Settings"; }
    const char* Icon() const override { return ICON_FA_MOUSE_POINTER; }

    void LoadSettings(CSimpleIniA*) override;
    void SaveSettings(CSimpleIniA*) override;
    void Initialize() override;
    void Terminate() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;
    void DrawSettingInternal() override;
};
