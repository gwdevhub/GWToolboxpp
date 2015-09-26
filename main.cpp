#include "GWToolbox\GWToolbox.h"
#include "GWToolbox\logger.h"

// Do all your startup things here instead.
void init(HMODULE hModule){
	__try {
		Logger::Init();
		LOG("Creating toolbox thread\n");
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)GWToolbox::SafeThreadEntry, hModule, 0, 0);
	} __except (Logger::GenerateDump(GetExceptionInformation())) {
	}
}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		__try {
			DisableThreadLibraryCalls(_HDllHandle);
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
		} __except (Logger::GenerateDump(GetExceptionInformation())) {
		}
	}
	return TRUE;
}
