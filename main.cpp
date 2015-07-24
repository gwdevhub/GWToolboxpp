
#include "API\APIMain.h"

using namespace GWAPI;

// Do all your startup things here instead.
void init(HMODULE hModule){
	if (Memory.Scan())
	{
		// Found all addresses, can do stuff now.
		char buf[128];
		AgentArray Agents = Memory.GetAgentArray();

		Agent* player = Agents.GetPlayer();

		if (player == NULL) return;

		sprintf_s(buf, "Player X: %.2f\nPlayer Y: %.2f", player->X, player->Y);

		MessageBoxA(0, buf, 0, 0);

	}else
	{
		MessageBoxA(0, "Did not find all addresses.", 0, 0);
	}
}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}