#include "GameThreadMgr.h"

#include "MemoryMgr.h"

void __stdcall GWAPI::GameThreadMgr::CallFunctions()
{
	std::unique_lock<std::mutex> VecLock(call_vector_mutex_);
	if (calls_.empty()) return;

	for (const auto& Call : calls_)
	{
		Call();
	}

	calls_.clear();

	if (calls_permanent_.empty()) return;

	for (const auto& Call : calls_permanent_)
	{
		Call();
	}
}

void __declspec(naked) GWAPI::GameThreadMgr::gameLoopHook()
{
	static GWAPIMgr* inst;
	_asm PUSHAD

	if (inst == NULL)
		inst = GWAPIMgr::instance();
	if (inst != NULL)
		inst->Gamethread()->CallFunctions();

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

	render_state_ = !render_state_;

	if (render_state_)
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

GWAPI::GameThreadMgr::GameThreadMgr(GWAPI::GWAPIMgr* obj) : parent_(obj), render_state_(false)
{
	MemoryMgr::GameLoopReturn = (BYTE*)hk_game_thread_.Detour(MemoryMgr::GameLoopLocation, (BYTE*)gameLoopHook, 5);
}

GWAPI::GameThreadMgr::~GameThreadMgr()
{
}

void GWAPI::GameThreadMgr::RestoreHooks()
{
	if (render_state_) ToggleRenderHook();
	hk_game_thread_.Retour();
}
