#include <stdint.h>
#include <Windows.h>

#include "Logger.h"
#include "CursorFix.h"

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/MemoryMgr.h>

typedef BOOL WINAPI GetClipCursor_t(
    _Out_ LPRECT lpRect);

GW::THook<GetClipCursor_t*> g_hkGetClipCursor;
BOOL WINAPI fnGetClipCursor(LPRECT lpRect)
{
    GW::HookBase::EnterHook();
    BOOL retval = GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), lpRect);
    GW::HookBase::LeaveHook();
    return retval;
}

void InstallCursorFix()
{
    GetClipCursor_t* fn = (GetClipCursor_t*)GetProcAddress(GetModuleHandleA("user32.dll"), "GetClipCursor");
    if (!fn) {
        Log::Warning("Cursor Fix not installed, message devs about this!");
        return;
    }
    g_hkGetClipCursor.Detour(fn, fnGetClipCursor);
}

void UninstallCursorFix()
{
    if (g_hkGetClipCursor.Valid())
        GW::HookBase::DisableHooks(&g_hkGetClipCursor);
}
