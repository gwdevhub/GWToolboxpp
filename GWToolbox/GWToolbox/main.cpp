#include "Defines.h"
#include "GWToolbox.h"
#include "logger.h"

// Do all your startup things here instead.
DWORD WINAPI init(HMODULE hModule){
	__try {
		if (*(DWORD*)0x00DE0000 != NULL){
			MessageBox(0,"Please restart guild wars and try again.", "GWToolbox++ - Clientside Error Detected", 0);
			FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
		}

		Log::InitializeLog();
		Log::Log("Waiting for logged character\n");
		while (*(void**)0xA2B294 == nullptr) {
			Sleep(100);
		}
		
		Log::Log("Creating toolbox thread\n");
		SafeThreadEntry(hModule);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
	}
	return 0;
}

// DLL entry point, dont do things in this thread unless you know what you are doing.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	DisableThreadLibraryCalls(_HDllHandle);
	if (_Reason == DLL_PROCESS_ATTACH){
		__try {
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
		} __except ( EXCEPT_EXPRESSION_ENTRY ) {
		}
	}
	return TRUE;
}
