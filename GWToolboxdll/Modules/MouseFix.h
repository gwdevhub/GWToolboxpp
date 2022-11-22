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

#ifndef _DEBUG
    bool HasSettings() override { return false; }
#endif

    void Terminate() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    void DrawSettingInternal() override;
};
