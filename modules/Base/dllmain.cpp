#include <Windows.h>
#include <iostream>

#include <module_base.h>



DLLAPI BOOL WINAPI DllMain(HINSTANCE , DWORD reason, LPVOID) {
	switch (reason) {
	case DLL_PROCESS_ATTACH: break;
	case DLL_PROCESS_DETACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	default:
		break;
	}

	return TRUE;
}
