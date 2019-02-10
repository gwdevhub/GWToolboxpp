#include "stdafx.h"

#include "Logger.h"
#include "CursorFix.h"

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/MemoryMgr.h>

typedef BOOL WINAPI GetClipCursor_pt(
    _Out_ LPRECT lpRect);

GetClipCursor_pt *GetClipCursor_Func;

BOOL WINAPI OnGetClipCursor(LPRECT lpRect)
{
    GW::HookBase::EnterHook();
    BOOL retval = GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), lpRect);
    GW::HookBase::LeaveHook();
    return retval;
}

void InstallCursorFix()
{
    GetClipCursor_Func = (GetClipCursor_pt *)GetProcAddress(GetModuleHandleA("user32.dll"), "GetClipCursor");
    if (!GetClipCursor_Func) {
        Log::Warning("Cursor Fix not installed, message devs about this!");
        return;
    }

    GW::HookBase::CreateHook(GetClipCursor_Func, OnGetClipCursor, NULL);
    GW::HookBase::EnableHooks(GetClipCursor_Func);
}

void UninstallCursorFix()
{
    if (GetClipCursor_Func) {
        GW::HookBase::DisableHooks(GetClipCursor_Func);
        GW::HookBase::RemoveHook(GetClipCursor_Func);
    }
}
