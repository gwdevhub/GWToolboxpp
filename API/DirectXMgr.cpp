#include "DirectXMgr.h"
#include "..\GWToolbox\logger.h"

GWAPI::DirectXMgr::DirectXMgr(GWAPIMgr* obj) : parent_(obj)
{

}

GWAPI::DirectXMgr::EndScene_t GWAPI::DirectXMgr::EndsceneReturn()
{
	return endscene_;
}

GWAPI::DirectXMgr::Reset_t GWAPI::DirectXMgr::ResetReturn()
{
	return reset_;
}

void GWAPI::DirectXMgr::CreateRenderHooks(EndScene_t _endscene, Reset_t _reset)
{
	if (hooked_) return;

	LOG("DX Start\n");

	HMODULE hD3D9 = NULL;
	while (!hD3D9)
	{
		hD3D9 = GetModuleHandleA("d3d9.dll");
		Sleep(5);
	}

	LOG("DX Module found! Addr: %X\n", hD3D9);

	DWORD pA1 = FindPattern((DWORD)hD3D9, 0x128000, "\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx");
	LOG("Vtable scan found! Addr: %X\n", pA1);
	memcpy(&vtable_start_, (void*)(pA1 + 2), 4);
	LOG("memcpy vtable done\n");
	DWORD dwEndsceneLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[42]));
	endscene_ = (EndScene_t)hk_endscene_.Detour((BYTE*)(vtable_start_[42]), (BYTE*)_endscene, dwEndsceneLen);
	LOG("DX endscene! Len: %d Addr: %X\n", dwEndsceneLen, hD3D9);

	DWORD dwResetLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[16]));
	reset_ = (Reset_t)hk_reset_.Detour((BYTE*)(vtable_start_[16]), (BYTE*)_reset, dwResetLen);
	LOG("DX reset! Len: %d Addr: %X\n", hD3D9);

	hooked_ = true;
}

void GWAPI::DirectXMgr::RestoreHooks()
{
	if (!hooked_) return;

	hk_endscene_.Retour();
	hk_reset_.Retour();

	hooked_ = false;
}

DWORD GWAPI::DirectXMgr::FindPattern(DWORD _base, DWORD _size, char* _pattern, char* _mask)
{

	LOG("Finding pattern Base: %X Size: %X\n", _base, _size);
	int pattern_length = strlen(_mask);

	LOG("Pattern: \n");

	for (int i = 0; i < pattern_length; i++) {
		printf("%02X \n", _pattern[i]);
		Logger::FlushFile();
	}
	
	bool found = false;

	//For each byte from start to end
	for (DWORD i = _base; i < _base + _size - pattern_length; i++)
	{
		found = true;
		//For each byte in the pattern
		for (int idx = 0; idx < pattern_length; idx++)
		{
			if (_mask[idx] == 'x' && _pattern[idx] != *(char*)(i + idx))
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			return i;
		}
	}
	return NULL;
}

GWAPI::DirectXMgr::~DirectXMgr()
{
}
