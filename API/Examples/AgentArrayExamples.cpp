#include <Windows.h>
#include <cstdio>

#include "AgentArray.h"
#include "CallQueue.h"

GWCA::AgentArray AgArray;

typedef void(__fastcall *CtoGSPacketSend_t)(DWORD, DWORD, DWORD*);
CtoGSPacketSend_t ctosfunc;
DWORD* packetLoc;

void init(HMODULE hModule){
	
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	try{
		AgArray = GWCA::AgentArray();
	}
	catch (int){
		printf("[FAILURE]");
		return;
	}
	printf("PlayerID: %d\n", AgArray.GetPlayerID());
	printf("MaxAgents: %d\n", AgArray.GetMaxAgents());

	GWCA::AgentArray::Agent* Player = AgArray.GetPlayer();

	while (Player != NULL){
		printf("X: %f Y: %f Enchanted: %u\n", Player->X, Player->Y,Player->GetIsEnchanted());
		Sleep(500);
	}
}


BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}