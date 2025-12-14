#include "stdafx.h"

#include <Xinput.h>
#include "GamepadModule.h"
#pragma comment(lib, "Xinput.lib")

#include <GWCA/GWCA.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Timer.h>
#include <Defines.h>
#include <ImGuiAddons.h>

namespace {
    // Gamepad cursor position in GW screen coordinates and client coordinates
    GW::Vec2f last_gamepad_cursor_pos;
    GW::Vec2f gamepad_cursor_pos_client;
    bool was_in_cursor_mode = false;

    // XInput state tracking for gamepad A button
    XINPUT_STATE gamepad_states[XUSER_MAX_COUNT] = {0};

    clock_t XINPUT_GAMEPAD_A_HELD = 0;

    // Get the gamepad cursor position from GW's UI
    bool GetGamepadCursorPos(GW::Vec2f* screen_pos)
    {
        if (!GW::UI::IsInControllerCursorMode()) return false;
        const auto root = GW::UI::GetRootFrame();
        const auto frame = GW::UI::GetChildFrame(root, 0, 0);
        if (!frame) return false;
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);
        const auto height = bottom_right.y - top_left.y;
        *screen_pos = {top_left.x, top_left.y + height};
        return true;
    }

    bool noop_true(void*) {
        return true;
    }
} // namespace

void GamepadModule::Initialize()
{
    ToolboxModule::Initialize();
}

bool GamepadModule::WndProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    // We don't consume any messages, just inject synthetic ones
    // This needs to run early in the message processing pipeline
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    return false;
}

void GamepadModule::Update(float delta)
{
    UNREFERENCED_PARAMETER(delta);

    if (!GetGamepadCursorPos(&gamepad_cursor_pos_client)) {
        if (was_in_cursor_mode && ImGui::ShowingContextMenu()) {
            // Close current popup menu if we're in gamepad mode and didn't get cursor position
            ImGui::SetContextMenu(noop_true);
        }
        XINPUT_GAMEPAD_A_HELD = 0;
        was_in_cursor_mode = false;
        return;
    }
    was_in_cursor_mode = true;

    auto& io = ImGui::GetIO();
    io.MousePos = gamepad_cursor_pos_client;

    if (last_gamepad_cursor_pos != gamepad_cursor_pos_client) {
        // Cursor moved whilst pressing; cancel button hold.
        XINPUT_GAMEPAD_A_HELD = 0;
    }
    if (XINPUT_GAMEPAD_A_HELD && TIMER_DIFF(XINPUT_GAMEPAD_A_HELD) > 500) {
        // A button held for more than 500ms
        const LPARAM mouse_lparam = MAKELPARAM(static_cast<int>(gamepad_cursor_pos_client.x), static_cast<int>(gamepad_cursor_pos_client.y));
        auto hwnd = GW::MemoryMgr::GetGWWindowHandle();
        SendMessage(hwnd, WM_GW_RBUTTONCLICK, 0, mouse_lparam);
        XINPUT_GAMEPAD_A_HELD = 0;
    }

    // Poll XInput for gamepad A button state
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    // Check all 4 possible controllers
    for (DWORD i = 0; i < _countof(gamepad_states); i++) {
        if (XInputGetState(i, &state) != ERROR_SUCCESS) 
            continue;
        auto& prev_state = gamepad_states[i];
        const auto current_buttons_held = state.Gamepad.wButtons;
        const auto prev_buttons_held = prev_state.Gamepad.wButtons;
        if (current_buttons_held == prev_buttons_held) 
            continue;

        if ((current_buttons_held & XINPUT_GAMEPAD_A) != (prev_buttons_held & XINPUT_GAMEPAD_A)) {
            // A Button state changed
            if ((current_buttons_held & XINPUT_GAMEPAD_A)) {
                // A Button pressed
                if (!XINPUT_GAMEPAD_A_HELD) {
                    XINPUT_GAMEPAD_A_HELD = TIMER_INIT();
                }
            }
            else if(XINPUT_GAMEPAD_A_HELD != 0) {
                // A button released, and hold timer not cancelled.
                const LPARAM mouse_lparam = MAKELPARAM(static_cast<int>(gamepad_cursor_pos_client.x), static_cast<int>(gamepad_cursor_pos_client.y));
                auto hwnd = GW::MemoryMgr::GetGWWindowHandle();
                SendMessage(hwnd, WM_LBUTTONDOWN, 0, mouse_lparam);
                SendMessage(hwnd, WM_LBUTTONUP, 0, mouse_lparam);
                XINPUT_GAMEPAD_A_HELD = 0;
            }
        }
        prev_state = state;
    }
    last_gamepad_cursor_pos = gamepad_cursor_pos_client;
}
