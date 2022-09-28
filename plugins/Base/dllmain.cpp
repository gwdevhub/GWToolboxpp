#include <Windows.h>
#include <iostream>

#include "ToolboxPlugin.h"

DLLAPI BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID) {
	switch (reason) {
        case DLL_PROCESS_ATTACH: plugin_handle = hModule; break;
	case DLL_PROCESS_DETACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	default:
		break;
	}

	return TRUE;
}
