
#include "API\APIMain.h"
#include <stdio.h>

using namespace GWAPI;

GWAPIMgr* GW;

// Do all your startup things here instead.
void init(HMODULE hModule){
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);

	GW = GWAPIMgr::GetInstance();

	AgentMgr::AgentArray agents = GW->Agents->GetAgentArray();

	AgentMgr::Agent* player = agents.GetPlayer();


	printf("X: %f Y: %f", player->X, player->Y);

}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}