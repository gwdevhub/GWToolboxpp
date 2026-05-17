#include "stdafx.h"
#include <Xinput.h>
#include "GamepadModule.h"

#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <Timer.h>
#include <Defines.h>
#include <ImGuiAddons.h>
#include <GWCA/Utilities/Hooker.h>

namespace {
    // Gamepad cursor position in GW screen coordinates and client coordinates
    GW::Vec2f last_gamepad_cursor_pos;
    GW::Vec2f gamepad_cursor_pos_client;
    bool was_in_cursor_mode = false;

    using XInputGetState_pt = DWORD(WINAPI*)(DWORD dwUserIndex, XINPUT_STATE* pState);
    XInputGetState_pt XInputGetState_Func = nullptr, XInputGetState_Ret = nullptr;

    clock_t XINPUT_GAMEPAD_A_HELD = 0;

    bool noop_true(void*)
    {
        return true;
    }
    
    // Qol Fix: Disable gamepad state when gw isn't in focus
    DWORD WINAPI OnXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
        GW::Hook::EnterHook();
        DWORD ret = ERROR_DEVICE_NOT_CONNECTED;
        if (GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow()) {
            ret = XInputGetState_Ret(dwUserIndex, pState);
        }
        GW::Hook::LeaveHook();
        return ret;
    }
    
    void HookXInput() {
        if (XInputGetState_Func) return;
        HMODULE hXInput = GetModuleHandleA("xinput1_4.dll");
        XInputGetState_Func = hXInput ? (XInputGetState_pt)GetProcAddress(hXInput, "XInputGetState") : nullptr;
        if (XInputGetState_Func) {
            GW::Hook::CreateHook((void**)&XInputGetState_Func, OnXInputGetState, (void**)&XInputGetState_Ret);
            GW::Hook::EnableHooks(XInputGetState_Func);
        }
    }
} // namespace

bool GamepadModule::GetGamepadCursorPos(GW::Vec2f* screen_pos)
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

bool GamepadModule::GetGamepadState(_XINPUT_GAMEPAD* gamepad)
{
    static XINPUT_STATE state;
    memset(&state, 0, sizeof(XINPUT_STATE));
    bool success = false;

    // Check all 4 possible controllers
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        if (!(XInputGetState_Func && XInputGetState_Func(i, &state) == ERROR_SUCCESS)) continue;
        if (!success) {
            memset(gamepad, 0, sizeof(*gamepad));
            success = true;
        }
        gamepad->bLeftTrigger |= state.Gamepad.bLeftTrigger;
        gamepad->bRightTrigger |= state.Gamepad.bRightTrigger;
        gamepad->wButtons |= state.Gamepad.wButtons;
        gamepad->sThumbLX |= state.Gamepad.sThumbLX;
        gamepad->sThumbLY |= state.Gamepad.sThumbLY;
        gamepad->sThumbRX |= state.Gamepad.sThumbRX;
        gamepad->sThumbRY |= state.Gamepad.sThumbRY;
    }
    return success;
}

void GamepadModule::Update(float delta)
{
    UNREFERENCED_PARAMETER(delta);
    HookXInput();
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

    bool cursor_moved = last_gamepad_cursor_pos != gamepad_cursor_pos_client;
    last_gamepad_cursor_pos = gamepad_cursor_pos_client;

    if (cursor_moved) {
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

    static XINPUT_GAMEPAD state;
    static XINPUT_GAMEPAD prev_state;
    if (GetGamepadState(&state)) {
        const auto current_buttons_held = state.wButtons;
        const auto prev_buttons_held = prev_state.wButtons;
        if (current_buttons_held != prev_buttons_held) {
            if ((current_buttons_held & XINPUT_GAMEPAD_A) != (prev_buttons_held & XINPUT_GAMEPAD_A)) {
                // A Button state changed
                if ((current_buttons_held & XINPUT_GAMEPAD_A)) {
                    // A Button pressed
                    if (!XINPUT_GAMEPAD_A_HELD) {
                        XINPUT_GAMEPAD_A_HELD = TIMER_INIT();
                    }
                }
                else if (XINPUT_GAMEPAD_A_HELD != 0) {
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

    }
}
