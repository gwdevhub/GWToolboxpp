#include <Windows.h>
#include "HackUtils.h"
#include "GameLoop.h"

typedef void(__fastcall *GameLoop_t)();
BYTE* gameloopfunc = NULL;
GameLoop_t gameloopret = NULL;

typedef void(__fastcall *SetOnlineStatus_t)(DWORD status);
SetOnlineStatus_t _setonlinestatus = NULL;

typedef void(__fastcall *SetSkillbar_t)(DWORD AgentId,DWORD SkillCount, DWORD* SkillArray, DWORD* SKillPvpMask);
SetSkillbar_t setSB = (SetSkillbar_t)0x07C1510;



DWORD* PlayerPtr = (DWORD*)0xD559CC;

DWORD skillcount = 8;
DWORD skills[8] = { 4, 4, 4, 4, 4, 4, 4, 4 };
DWORD mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

CallQueue GameLoop;

void __declspec(naked) gameloophook()
{
	_asm PUSHAD

	GameLoop.CallFunctions();

	_asm POPAD
	_asm JMP gameloopret
}

void init(HMODULE hModule){
	CScanner scan;
	DWORD status = 0;

	gameloopfunc = (BYTE*)scan.FindPattern("\x53\x56\xDF\xE0\xF6\xC4\x41\x0F\x85", "xxxxxxxxx", -0x12);
	if (gameloopfunc) gameloopret = (GameLoop_t)CHooker::Detour(gameloopfunc, (BYTE*)gameloophook, 6);

	while (1){
		Sleep(100);
		if (GetAsyncKeyState(VK_HOME) & 1)
			GameLoop.Enqueue(setSB, *PlayerPtr, skillcount, skills, mask);
	}
}


BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}