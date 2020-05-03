#include "stdafx.h"

#include "CursorFix.h"

#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include "logger.h"

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
    GetClipCursor_Func = (GetClipCursor_pt)GetProcAddress(GetModuleHandleA("user32.dll"), "GetClipCursor");
    if (!GetClipCursor_Func) {
        Log::Warning("Cursor Fix not installed, message devs about this!");
        return;
    }
    GW::HookBase::CreateHook(GetClipCursor_Func, fnGetClipCursor, (void **)&RetGetClipCursor);
    GW::HookBase::EnableHooks(GetClipCursor_Func);
}

void UninstallCursorFix()
{
    if (GetClipCursor_Func) {
        GW::HookBase::DisableHooks(GetClipCursor_Func);
        GW::HookBase::RemoveHook(GetClipCursor_Func);
    }
}
