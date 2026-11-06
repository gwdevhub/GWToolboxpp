#include <Windows.h>

#include "ToolboxPlugin.h"

DLLAPI BOOL WINAPI DllMain(const HMODULE hModule, const DWORD reason, LPVOID)
{
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            if (GW::Runtime::BindGWCA(GetModuleHandleW(L"gwca.dll"), &GW::Runtime::api) != GW::Runtime::Status::Success) {
                return FALSE;
            }
            plugin_handle = hModule;
            break;
        case DLL_PROCESS_DETACH:
            GW::Runtime::api = {};
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        default:
            break;
    }

    return TRUE;
}
