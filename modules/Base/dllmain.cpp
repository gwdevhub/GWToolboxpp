#include <Windows.h>
#include <iostream>

#include <module_base.h>



DLLAPI BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH: break;
	case DLL_PROCESS_DETACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	default:
		break;
	}

	std::cout << "dllmain executed. Reason: " << reason << std::endl;

	return TRUE;
}
