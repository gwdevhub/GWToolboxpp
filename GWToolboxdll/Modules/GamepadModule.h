#pragma once

#include <ToolboxModule.h>

class GamepadModule : public ToolboxModule {
    GamepadModule() = default;
    ~GamepadModule() override = default;

public:
    static GamepadModule& Instance()
    {
        static GamepadModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Gamepad Module"; }
    [[nodiscard]] const char* Description() const override { return "Enables gamepad interaction with toolbox"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GAMEPAD; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;
};
