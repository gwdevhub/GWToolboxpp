#include "GameLoop.h"

template<typename F, typename... ArgTypes>
void GWAPI::CallQueue::Enqueue(F&& Func, ArgTypes&&... Args)
{
	std::unique_lock<std::mutex> VecLock(m_CallVecMutex);
	m_Calls.emplace_back(std::bind(Func, Args...));
}

void __stdcall GWAPI::CallQueue::CallFunctions()
{

	if (m_Calls.empty()) return;

	std::unique_lock<std::mutex> VecLock(m_CallVecMutex);
	for (const auto& Call : m_Calls)
	{
		Call();
	}

	m_Calls.clear();
}

void __declspec(naked) GWAPI::gameLoopHook()
{
	_asm PUSHAD

	GameThread.CallFunctions();

	_asm POPAD
	_asm JMP Memory.GameLoopReturn
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
		memcpy(restorebuf, Memory.RenderLoopLocation, 5);

		VirtualProtect(Memory.RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		Memory.RenderLoopLocation[0] = 0xE9;
		*(DWORD*)(Memory.RenderLoopLocation) = (DWORD)((BYTE*)renderHook - (Memory.RenderLoopLocation + 5));
		VirtualProtect(Memory.RenderLoopLocation, 5, dwProt, NULL);
	}
	else{
		VirtualProtect(Memory.RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
		memcpy(Memory.RenderLoopLocation, restorebuf, 5);
		VirtualProtect(Memory.RenderLoopLocation, 5, dwProt, NULL);
	}
}

GWAPI::CallQueue GWAPI::GameThread = GWAPI::CallQueue();