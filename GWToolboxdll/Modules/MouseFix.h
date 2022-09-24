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
    const char8_t* Icon() const override { return ICON_FA_MOUSE_POINTER; }

    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;
};
