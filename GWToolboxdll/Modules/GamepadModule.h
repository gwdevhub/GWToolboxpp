#pragma once

#include <ToolboxModule.h>

struct _XINPUT_GAMEPAD;
namespace GW {
    struct Vec2f;
}

class GamepadModule : public ToolboxModule {
    GamepadModule() = default;
    ~GamepadModule() override = default;

public:
    static GamepadModule& Instance()
    {
        static GamepadModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "手柄模块"; }
    [[nodiscard]] const char* Description() const override { return "允许使用游戏手柄操作工具箱。"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GAMEPAD; }
    bool HasSettings() override { return false; }

    void Update(float delta) override;
    // Get (and combine) the current gamepad state from all available controllers.
    bool GetGamepadState(_XINPUT_GAMEPAD* gamepad);
    // Get the gamepad cursor position from GW's UI. Returns true is screen_pos was successfully fetched.
    bool GetGamepadCursorPos(GW::Vec2f* screen_pos);
};
