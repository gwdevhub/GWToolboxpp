#include "GameThreadMgr.h"

void __stdcall GWAPI::GameThreadMgr::CallFunctions()
{

	if (m_Calls.empty()) return;

	std::unique_lock<std::mutex> VecLock(m_CallVecMutex);
	for (const auto& Call : m_Calls)
	{
		Call();
	}

	m_Calls.clear();
}

void __declspec(naked) GWAPI::GameThreadMgr::gameLoopHook()
{
	_asm PUSHAD

	GWAPIMgr::GetInstance()->GameThread->CallFunctions();

	_asm POPAD
	_asm JMP MemoryMgr::GameLoopReturn
}

void __declspec(naked) GWAPI::GameThreadMgr::renderHook()
{
	Sleep(1);
	_asm {
		POP ESI
		POP EBX
		FSTP DWORD PTR DS : [0xA3F998]
		MOV ESP, EBP
		POP EBP
		RETN
	}
}

void GWAPI::GameThreadMgr::ToggleRenderHook()
{
	static bool enabled = false;
	static BYTE restorebuf[5];
	static DWORD dwProt;

	enabled = !enabled;

	if (enabled)
	{
		memcpy(restorebuf, MemoryMgr::RenderLoopLocation, 5);

		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		MemoryMgr::RenderLoopLocation[0] = 0xE9;
		*(DWORD*)(MemoryMgr::RenderLoopLocation) = (DWORD)((BYTE*)renderHook - (MemoryMgr::RenderLoopLocation + 5));
		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, dwProt, NULL);
	}
	else{
		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		memcpy(MemoryMgr::RenderLoopLocation, restorebuf, 5);
		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, dwProt, NULL);
	}
}

GWAPI::GameThreadMgr::GameThreadMgr(GWAPI::GWAPIMgr* obj) : parent(obj)
{
	memcpy(GameLoopRestore, MemoryMgr::GameLoopLocation, 5);
	MemoryMgr::GameLoopReturn = (BYTE*)MemoryMgr::Detour(MemoryMgr::GameLoopLocation, (BYTE*)gameLoopHook, 5);
}

GWAPI::GameThreadMgr::~GameThreadMgr()
{
	DWORD dwProt;
	VirtualProtect(MemoryMgr::GameLoopLocation, 5, PAGE_READWRITE, &dwProt);
	memcpy(MemoryMgr::GameLoopLocation, GameLoopRestore, 5);
	VirtualProtect(MemoryMgr::GameLoopLocation, 5, dwProt, NULL);
}
