#include "GameThreadMgr.h"

#include "MemoryMgr.h"

void __stdcall GWAPI::GameThreadMgr::CallFunctions()
{
	std::unique_lock<std::mutex> VecLock(m_CallVecMutex);
	if (m_Calls.empty()) return;

	for (const auto& Call : m_Calls)
	{
		Call();
	}

	m_Calls.clear();

	if (m_CallsPermanent.empty()) return;

	for (const auto& Call : m_CallsPermanent)
	{
		Call();
	}
}

void __declspec(naked) GWAPI::GameThreadMgr::gameLoopHook()
{
	static GWAPIMgr* inst;
	_asm PUSHAD

	if (inst == NULL)
		inst = GWAPIMgr::GetInstance();
	if (inst != NULL)
		inst->GameThread->CallFunctions();

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
	static BYTE restorebuf[5];
	static DWORD dwProt;

	m_RenderingState = !m_RenderingState;

	if (m_RenderingState)
	{
		memcpy(restorebuf, MemoryMgr::RenderLoopLocation, 5);

		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		memset(MemoryMgr::RenderLoopLocation, 0xE9, 1);
		*(DWORD*)(MemoryMgr::RenderLoopLocation + 1) = (DWORD)((BYTE*)renderHook - MemoryMgr::RenderLoopLocation) - 5;
		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, dwProt, NULL);
	}
	else{
		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		memcpy(MemoryMgr::RenderLoopLocation, restorebuf, 5);
		VirtualProtect(MemoryMgr::RenderLoopLocation, 5, dwProt, NULL);
	}
}

GWAPI::GameThreadMgr::GameThreadMgr(GWAPI::GWAPIMgr* obj) : parent(obj), m_RenderingState(false)
{
	MemoryMgr::GameLoopReturn = (BYTE*)hkGameThread.Detour(MemoryMgr::GameLoopLocation, (BYTE*)gameLoopHook, 5);
}

GWAPI::GameThreadMgr::~GameThreadMgr()
{
	if (m_RenderingState) ToggleRenderHook();
	hkGameThread.Retour();
}
