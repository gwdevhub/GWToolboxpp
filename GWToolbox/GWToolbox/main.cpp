#include "Defines.h"
#include "GWToolbox.h"
#include "logger.h"

// Do all your startup things here instead.
void init(HMODULE hModule){
	__try {
		if (*(DWORD*)0x00DE0000 != NULL){
			MessageBox(0,"Please restart guild wars and try again.", "GWToolbox++ - Clientside Error Detected", 0);
			FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
		}

		Logger::Init();
		LOG("Waiting for logged character\n");
		while (*(void**)0xA2B294 == nullptr) {
			Sleep(100);
		}
		
		LOG("Creating toolbox thread\n");
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)GWToolbox::SafeThreadEntry, hModule, 0, 0);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
	}
}

// DLL entry point, not safe to stay in this thread for long.
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
