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
