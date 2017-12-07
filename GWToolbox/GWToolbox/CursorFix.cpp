#include "CursorFix.h"
#include "logger.h"
#include <GWCA\Managers\MemoryMgr.h>
#include <GWCA\Utilities\Hooker.h>

typedef BOOL WINAPI GetClipCursor_t(
    _Out_ LPRECT lpRect);

GW::THook<GetClipCursor_t*> g_hkGetClipCursor;
BOOL WINAPI fnGetClipCursor(LPRECT lpRect)
{
    return GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), lpRect);
}

void InstallCursorFix()
{
    GetClipCursor_t* fn = (GetClipCursor_t*)GetProcAddress(GetModuleHandleA("user32.dll"), "GetClipCursor");
    if (!fn) {
        Log::Warning("Cursor Fix not installed, message devs about this!");
        return;
    }
    g_hkGetClipCursor.Detour(fn, fnGetClipCursor, GW::HookInternal::CalculateDetourLength((BYTE*)fn));
}

void UninstallCursorFix()
{
    if (g_hkGetClipCursor.Valid())
        g_hkGetClipCursor.Retour();
}
