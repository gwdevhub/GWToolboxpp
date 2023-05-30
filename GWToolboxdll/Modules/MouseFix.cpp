#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include "Defines.h"
#include "ImGuiAddons.h"
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
namespace {
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
    bool* HasRegisteredTrackMouseEvent = nullptr;
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
            GW::Hook::CreateHook(SetCursorPosCenter_Func, OnSetCursorPosCenter, reinterpret_cast<void**>(&SetCursorPosCenter_Ret));
        }

        GWCA_INFO("[SCAN] ProcessInput_Func = %p", ProcessInput_Func);
        GWCA_INFO("[SCAN] HasRegisteredTrackMouseEvent = %p", HasRegisteredTrackMouseEvent);
        GWCA_INFO("[SCAN] gw_mouse_move = %p", gw_mouse_move);
        GWCA_INFO("[SCAN] SetCursorPosCenter_Func = %p", SetCursorPosCenter_Func);

        ASSERT(ProcessInput_Func && HasRegisteredTrackMouseEvent && gw_mouse_move && SetCursorPosCenter_Func);

        return true;
    }

    void CursorFixEnable(const bool enable)
    {
        ASSERT(CursorFixInitialise());
        if (enable) {
            if (ProcessInput_Func)
                GW::Hook::EnableHooks(ProcessInput_Func);
            if (SetCursorPosCenter_Func)
                GW::Hook::EnableHooks(SetCursorPosCenter_Func);
        }
        else {
            if (SetCursorPosCenter_Func)
                GW::Hook::DisableHooks(SetCursorPosCenter_Func);
            if (ProcessInput_Func)
                GW::Hook::DisableHooks(ProcessInput_Func);
        }
    }

    bool initialized = false;
    bool enable_cursor_fix = true;

    /*
     *  Logic for scaling gw cursor up or down
     */
    HBITMAP ScaleBitmap(HBITMAP inBitmap, int inWidth, int inHeight, int outWidth, int outHeight)
    {
        // NB: We could use GDIPlus for this logic which has better image res handling etc, but no need
        HDC destDC = nullptr, srcDC = nullptr;
        BYTE* ppvBits = nullptr;
        BOOL bResult = 0;
        HBITMAP outBitmap = nullptr;

        // create a destination bitmap and DC with size w/h
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biWidth = outWidth;
        bmi.bmiHeader.biHeight = outWidth;
        bmi.bmiHeader.biPlanes = 1;

        // Do not use CreateCompatibleBitmap otherwise api will not allocate memory for bitmap
        destDC = CreateCompatibleDC(nullptr);
        if (!destDC)
            goto cleanup;
        outBitmap = CreateDIBSection(destDC, &bmi, DIB_RGB_COLORS, (void**)&ppvBits, nullptr, 0);
        if (outBitmap == nullptr)
            goto cleanup;
        if (SelectObject(destDC, outBitmap) == nullptr)
            goto cleanup;

        srcDC = CreateCompatibleDC(nullptr);
        if (!srcDC)
            goto cleanup;
        if (SelectObject(srcDC, inBitmap) == nullptr)
            goto cleanup;

        // copy and scaling to new width/height (w,h)
        if (SetStretchBltMode(destDC, WHITEONBLACK) == 0)
            goto cleanup;
        bResult = StretchBlt(destDC, 0, 0, outWidth, outHeight, srcDC, 0, 0, inWidth, inHeight, SRCCOPY);
    cleanup:
        if (!bResult) {
            if (outBitmap) {
                DeleteObject(outBitmap);
                outBitmap = nullptr;
            }
        }
        if (destDC)
            DeleteDC(destDC);
        if (srcDC)
            DeleteDC(srcDC);

        return outBitmap;
    }

    int cursor_size = 32;
    HCURSOR current_cursor = nullptr;
    bool cursor_size_hooked = false;
    HCURSOR ScaleCursor(HCURSOR cursor, const int targetSize)
    {
        ICONINFO icon_info;
        HCURSOR new_cursor = nullptr;
        BITMAP tmpBitmap;
        HBITMAP scaledMask = nullptr, scaledColor = nullptr;
        if (!GetIconInfo(cursor, &icon_info))
            goto cleanup;
        if (GetObject(icon_info.hbmMask, sizeof(BITMAP), &tmpBitmap) == 0)
            goto cleanup;
        if (tmpBitmap.bmWidth == targetSize)
            goto cleanup;
        scaledMask = ScaleBitmap(icon_info.hbmMask, tmpBitmap.bmWidth, tmpBitmap.bmHeight, targetSize, targetSize);
        if (!scaledMask)
            goto cleanup;
        if (GetObject(icon_info.hbmColor, sizeof(BITMAP), &tmpBitmap) == 0)
            goto cleanup;
        scaledColor = ScaleBitmap(icon_info.hbmColor, tmpBitmap.bmWidth, tmpBitmap.bmHeight, targetSize, targetSize);
        if (!scaledColor)
            goto cleanup;
        icon_info.hbmColor = scaledColor;
        icon_info.hbmMask = scaledMask;
        new_cursor = CreateIconIndirect(&icon_info);
    cleanup:
        if (scaledColor)
            DeleteObject(scaledColor);
        if (scaledMask)
            DeleteObject(scaledMask);
        return new_cursor;
    }
    struct GWWindowUserData {
        uint8_t unk[0xc43];
        HCURSOR cursor; // h0c44
        uint8_t unk1[0xb0];
        HWND window_handle; // h0cf8
    };
    typedef void(__cdecl* ChangeCursorIcon_pt)(GWWindowUserData*);
    ChangeCursorIcon_pt ChangeCursorIcon_Func = nullptr;
    ChangeCursorIcon_pt ChangeCursorIcon_Ret = nullptr;
    void OnChangeCursorIcon(GWWindowUserData* user_data)
    {
        GW::Hook::EnterHook();
        ChangeCursorIcon_Ret(user_data);
        // Cursor has been changed by the game; pull it back out, scale it to target size..
        if (cursor_size < 0 || cursor_size > 64 || cursor_size == 32)
            return GW::Hook::LeaveHook();
        if (!(user_data && user_data->cursor && user_data->cursor != current_cursor))
            return GW::Hook::LeaveHook();
        const HCURSOR new_cursor = ScaleCursor(user_data->cursor, cursor_size);
        if (!new_cursor)
            return GW::Hook::LeaveHook();
        if (user_data->cursor == new_cursor)
            return GW::Hook::LeaveHook();
        if (user_data->cursor) {
            // Don't forget to free the original cursor before overwriting the handle
            DestroyCursor(user_data->cursor);
            SetClassLongA(user_data->window_handle, GCL_HCURSOR, 0);
            SetCursor(nullptr);
            user_data->cursor = nullptr;
        }
        user_data->cursor = new_cursor;
        SetCursor(new_cursor);
        // Also override the window class for the cursor
        SetClassLongA(user_data->window_handle, GCL_HCURSOR, reinterpret_cast<LONG>(new_cursor));
        current_cursor = new_cursor;
        GW::Hook::LeaveHook();
    }
    void RedrawCursorIcon()
    {
        GW::GameThread::Enqueue([] {
            // Force redraw
            const auto user_data = (GWWindowUserData*)GetWindowLongA(GW::MemoryMgr::GetGWWindowHandle(), -0x15);
            current_cursor = nullptr;
            if (user_data)
                OnChangeCursorIcon(user_data);
        });
    }
    void SetCursorSize(int new_size)
    {
        cursor_size = new_size;
        if (cursor_size != 32) {
            if (!cursor_size_hooked)
                GW::HookBase::EnableHooks(ChangeCursorIcon_Func);
            cursor_size_hooked = true;
        }
        else {
            if (cursor_size_hooked)
                GW::HookBase::DisableHooks(ChangeCursorIcon_Func);
            cursor_size_hooked = false;
        }
    }
} // namespace

void MouseFix::Initialize()
{
    ToolboxModule::Initialize();

    const uintptr_t address = GW::Scanner::Find("\x8b\x41\x08\x89\x82\x50\x0c\x00\x00", "xxxxxxxxx", 0x9);
    ChangeCursorIcon_Func = (ChangeCursorIcon_pt)GW::Scanner::FunctionFromNearCall(address);
    if (ChangeCursorIcon_Func) {
        GW::HookBase::CreateHook(ChangeCursorIcon_Func, OnChangeCursorIcon, (void**)&ChangeCursorIcon_Ret);
    }
}
void MouseFix::LoadSettings(ToolboxIni* ini)
{
    enable_cursor_fix = ini->GetBoolValue(Name(), VAR_NAME(enable_cursor_fix), enable_cursor_fix);
    SetCursorSize(ini->GetLongValue(Name(), VAR_NAME(cursor_size), cursor_size));
    RedrawCursorIcon();
}

void MouseFix::SaveSettings(ToolboxIni* ini)
{
    ini->SetBoolValue(Name(), VAR_NAME(enable_cursor_fix), enable_cursor_fix);
    ini->SetLongValue(Name(), VAR_NAME(cursor_size), cursor_size);
}

void MouseFix::Terminate()
{
    ToolboxModule::Terminate();
    if (initialized) {
        CursorFixEnable(false);
        OldCursorFix::UninstallCursorFix();
    }
    if (ChangeCursorIcon_Func)
        GW::HookBase::RemoveHook(ChangeCursorIcon_Func);
}
void MouseFix::DrawSettingInternal()
{
    if (ImGui::Checkbox("Enable cursor fix", &enable_cursor_fix)) {
        CursorFixEnable(enable_cursor_fix);
    }
    ImGui::SliderInt("Guild Wars cursor size", &cursor_size, 16, 64);
    ImGui::ShowHelp("Sizes other than 32 might lead the the cursor disappearing at random.\n"
                    "Right click to make the cursor dis- and reappear for this to take effect.");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SetCursorSize(cursor_size);
        RedrawCursorIcon();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        SetCursorSize(32);
        RedrawCursorIcon();
    }
}

bool MouseFix::WndProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    if (!enable_cursor_fix)
        return false;
    if (!initialized) {
        OldCursorFix::InstallCursorFix();
        CursorFixEnable(enable_cursor_fix);
        initialized = true;
    }
    CursorFixWndProc(Message, wParam, lParam);
    return false;
}
