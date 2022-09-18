#include "stdafx.h"

#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <CursorFix.h>
#include <Logger.h>

typedef BOOL (WINAPI *GetClipCursor_pt)(
    _Out_ LPRECT lpRect);

GetClipCursor_pt GetClipCursor_Func;
GetClipCursor_pt RetGetClipCursor;

BOOL WINAPI fnGetClipCursor(LPRECT lpRect)
{
    return GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), lpRect);
}

void InstallCursorFix()
{
    const HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (!hUser32) {
        Log::Warning("Cursor Fix not installed, message devs about this!");
        return;
    }
    // @Cleanup:
    // We could have a GetProcAddress that does that:
    // > bool GetProcAddress(HMODULE hModule, LPCSTR lpProcName, LPVOID lpProc);
    GetClipCursor_Func = (GetClipCursor_pt)reinterpret_cast<uintptr_t>(GetProcAddress(hUser32, "GetClipCursor"));
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
}
