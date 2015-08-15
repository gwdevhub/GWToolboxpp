#include "API\APIMain.h"
#include "GWToolbox\GWToolbox.h"
#include <stdio.h>
GWToolbox* tb;
// Do all your startup things here instead.
void init(HMODULE hModule){

	tb = new GWToolbox(hModule);
	tb->exec();
}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}