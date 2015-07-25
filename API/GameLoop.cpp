#include "GameLoop.h"

template<typename F, typename... ArgTypes>
void GWAPI::GameThreadMgr::Enqueue(F&& Func, ArgTypes&&... Args)
{
	std::unique_lock<std::mutex> VecLock(m_CallVecMutex);
	m_Calls.emplace_back(std::bind(Func, Args...));
}

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

GWAPI::GameThreadMgr* GWAPI::GameThreadMgr::GetInstance()
{
	static GameThreadMgr* inst = new GameThreadMgr();
	return inst;
}

void __declspec(naked) GWAPI::gameLoopHook()
{
	static BYTE* GameLoopRet = MemoryMgr::GetInstance()->GameLoopReturn;

	_asm PUSHAD

	GameThreadMgr::GetInstance()->CallFunctions();

	_asm POPAD
	_asm JMP GameLoopRet
}

void __declspec(naked) GWAPI::renderHook()
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

void GWAPI::ToggleRenderHook()
{
	static bool enabled = false;
	static BYTE restorebuf[5];
	static DWORD dwProt;

	enabled = !enabled;

	if (enabled)
	{
		memcpy(restorebuf, MemoryMgr::GetInstance()->RenderLoopLocation, 5);

		VirtualProtect(MemoryMgr::GetInstance()->RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		MemoryMgr::GetInstance()->RenderLoopLocation[0] = 0xE9;
		*(DWORD*)(MemoryMgr::GetInstance()->RenderLoopLocation) = (DWORD)((BYTE*)renderHook - (MemoryMgr::GetInstance()->RenderLoopLocation + 5));
		VirtualProtect(MemoryMgr::GetInstance()->RenderLoopLocation, 5, dwProt, NULL);
	}
	else{
		VirtualProtect(MemoryMgr::GetInstance()->RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		memcpy(MemoryMgr::GetInstance()->RenderLoopLocation, restorebuf, 5);
		VirtualProtect(MemoryMgr::GetInstance()->RenderLoopLocation, 5, dwProt, NULL);
	}
}