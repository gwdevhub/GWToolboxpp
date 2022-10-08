#include "stdafx.h"

#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include "MouseFix.h"
#include <hidusage.h>

namespace OldCursorFix {

    typedef BOOL(WINAPI* GetClipCursor_pt)(_Out_ LPRECT lpRect);

    GetClipCursor_pt GetClipCursor_Func;
    GetClipCursor_pt RetGetClipCursor;

    BOOL WINAPI fnGetClipCursor(LPRECT lpRect) { return GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), lpRect); }

    void InstallCursorFix()
    {
        const HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (!hUser32) {
            Log::Warning("Cursor Fix not installed, message devs about this!");
            return;
        }
        GetClipCursor_Func = reinterpret_cast<GetClipCursor_pt>(GetProcAddress(hUser32, "GetClipCursor"));
        if (!GetClipCursor_Func) {
            Log::Warning("Cursor Fix not installed, message devs about this!");
            return;
        }
        GW::HookBase::CreateHook(GetClipCursor_Func, fnGetClipCursor, reinterpret_cast<void**>(&RetGetClipCursor));
        GW::HookBase::EnableHooks(GetClipCursor_Func);
    }

    void UninstallCursorFix()
    {
        if (GetClipCursor_Func) {
            GW::HookBase::DisableHooks(GetClipCursor_Func);
            GW::HookBase::RemoveHook(GetClipCursor_Func);
        }
    } // Collect the relative mouse position from raw input, instead of using the position passed into GW.
} // namespace OldCursorFix

typedef bool(__cdecl* OnProcessInput_pt)(uint32_t* wParam, uint32_t* lParam);
OnProcessInput_pt ProcessInput_Func = nullptr;
OnProcessInput_pt ProcessInput_Ret = nullptr;

struct GwMouseMove {
    int center_x;
    int center_y;
    uint32_t unk;
    uint32_t mouse_button_state; // 0x1 - LMB, 0x2 - MMB, 0x4 - RMB
    uint32_t move_camera;        // 1 == control camera while right mouse button pressed
    int captured_x;
    int captured_y;
};
GwMouseMove* gw_mouse_move = nullptr;
LONG rawInputRelativePosX = 0;
LONG rawInputRelativePosY = 0;
bool* HasRegisteredTrackMouseEvent = 0;
typedef void(__cdecl* SetCursorPosCenter_pt)(GwMouseMove* wParam);
SetCursorPosCenter_pt SetCursorPosCenter_Func = nullptr;
SetCursorPosCenter_pt SetCursorPosCenter_Ret = nullptr;

// Override (and rewrite) GW's handling of setting the mouse cursor to the center of the screen (bypass GameMutex, may be the cause of camera glitch)
// This could be a patch really, but rewriting the function out is a bit more readable.
void OnSetCursorPosCenter(GwMouseMove* gwmm)
{
    GW::Hook::EnterHook();
    // @Enhancement: Maybe assert that gwmm == gw_mouse_move?
    const HWND gw_window_handle = GetFocus();
    // @Enhancement: Maybe check that the focussed window handle is the GW window handle?
    RECT rect;
    if (!(gw_window_handle && GetClientRect(gw_window_handle, &rect)))
        goto leave;
    gwmm->center_x = (rect.left + rect.right) / 2;
    gwmm->center_y = (rect.bottom + rect.top) / 2;
    rawInputRelativePosX = rawInputRelativePosY = 0;
    SetPhysicalCursorPos(gwmm->captured_x, gwmm->captured_y);
leave:
    GW::Hook::LeaveHook();
}

// Override (and rewrite) GW's handling of mouse event 0x200 to stop camera glitching.
bool OnProcessInput(uint32_t* wParam, uint32_t* lParam)
{
    GW::Hook::EnterHook();
    if (!(HasRegisteredTrackMouseEvent && gw_mouse_move))
        goto forward_call; // Failed to find addresses for variables
    if (!(wParam && wParam[1] == 0x200))
        goto forward_call; // Not mouse movement
    if (!(*HasRegisteredTrackMouseEvent && gw_mouse_move->move_camera))
        goto forward_call; // Not moving the camera, or GW hasn't yet called TrackMouseEvent

    lParam[0] = 0x12;
    // Set the output parameters to be the relative position of the mouse to the center of the screen
    // NB: Original function uses ClientToScreen here; we've already grabbed the correct value via CursorFixWndProc
    lParam[1] = rawInputRelativePosX;
    lParam[2] = rawInputRelativePosY;

    // Reset the cursor position to the middle of the viewport
    OnSetCursorPosCenter(gw_mouse_move);
    GW::Hook::LeaveHook();
    return true;
forward_call:
    GW::Hook::LeaveHook();
    return ProcessInput_Ret(wParam, lParam);
}

void CursorFixWndProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    if (!(Message == WM_INPUT && GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT && lParam))
        return; // Not raw input
    if (!gw_mouse_move)
        return; // No gw mouse move ptr; this shouldn't happen

    UINT dwSize = sizeof(RAWINPUT);
    BYTE lpb[sizeof(RAWINPUT)];
    ASSERT(GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize);

    const RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb);
    if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
        // If its a relative mouse move, process the action
        if (gw_mouse_move->move_camera) {
            rawInputRelativePosX += raw->data.mouse.lLastX;
            rawInputRelativePosY += raw->data.mouse.lLastY;
        }
        else {
            rawInputRelativePosX = rawInputRelativePosY = 0;
        }
    }
}
bool CursorFixInitialise()
{
    if (gw_mouse_move)
        return true;
    const auto hwnd = GW::MemoryMgr::GetGWWindowHandle();
    if (!hwnd)
        return false;
    uintptr_t address = GW::Scanner::FindAssertion(R"(p:\code\base\os\win32\osinput.cpp)", "osMsg", 0x32);
    address = GW::Scanner::FunctionFromNearCall(address);
    if (address) {
        ProcessInput_Func = reinterpret_cast<OnProcessInput_pt>(address);
        address += 0x2b3;
        HasRegisteredTrackMouseEvent = *reinterpret_cast<bool**>(address);
        address += 0x85;
        gw_mouse_move = *reinterpret_cast<GwMouseMove**>(address);
        address += 0x7;
        SetCursorPosCenter_Func = reinterpret_cast<SetCursorPosCenter_pt>(GW::Scanner::FunctionFromNearCall(address));

        GW::Hook::CreateHook(ProcessInput_Func, OnProcessInput, reinterpret_cast<void**>(&ProcessInput_Ret));
        GW::Hook::EnableHooks(ProcessInput_Func);

        GW::Hook::CreateHook(SetCursorPosCenter_Func, OnSetCursorPosCenter, reinterpret_cast<void**>(&SetCursorPosCenter_Ret));
        GW::Hook::EnableHooks(SetCursorPosCenter_Func);
    }

    GWCA_INFO("[SCAN] ProcessInput_Func = %p", ProcessInput_Func);
    GWCA_INFO("[SCAN] HasRegisteredTrackMouseEvent = %p", HasRegisteredTrackMouseEvent);
    GWCA_INFO("[SCAN] gw_mouse_move = %p", gw_mouse_move);
    GWCA_INFO("[SCAN] SetCursorPosCenter_Func = %p", SetCursorPosCenter_Func);

    ASSERT(ProcessInput_Func && HasRegisteredTrackMouseEvent && gw_mouse_move && SetCursorPosCenter_Func);

    // RegisterRawInputDevices to be able to receive WM_INPUT via WndProc
    static RAWINPUTDEVICE rid;
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage = HID_USAGE_GENERIC_MOUSE;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    ASSERT(RegisterRawInputDevices(&rid, 1, sizeof(rid)));
    return true;
}

void CursorFixTerminate()
{
    GW::Hook::DisableHooks(SetCursorPosCenter_Func);
    GW::Hook::DisableHooks(ProcessInput_Func);
}

bool initialized = false;
void MouseFix::Initialize()
{
    ToolboxModule::Initialize();
}

void MouseFix::Terminate()
{
    if (initialized) {
        CursorFixTerminate();
        OldCursorFix::UninstallCursorFix();
    }
}

bool MouseFix::WndProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    if (!initialized) [[unlikely]] {
        OldCursorFix::InstallCursorFix();
        ASSERT(CursorFixInitialise());
        initialized = true;
    }
    CursorFixWndProc(Message, wParam, lParam);
    return false;
}
