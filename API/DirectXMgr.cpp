#include "DirectXMgr.h"

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


	HMODULE hD3D9 = NULL;
	while (!hD3D9)
	{
		hD3D9 = GetModuleHandleA("d3d9.dll");
		Sleep(5);
	}

	DWORD pA1 = dwFindPattern((DWORD)hD3D9, 0x128000, (PBYTE)"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx");
	memcpy(&vtable_start_, (void*)(pA1 + 2), 4);

	DWORD dwEndsceneLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[42]));
	endscene_ = (EndScene_t)hk_endscene_.Detour((BYTE*)(vtable_start_[42]), (BYTE*)_endscene, dwEndsceneLen);

	DWORD dwResetLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[16]));
	reset_ = (Reset_t)hk_reset_.Detour((BYTE*)(vtable_start_[16]), (BYTE*)_reset, dwResetLen);

	hooked_ = true;
}

void GWAPI::DirectXMgr::RestoreHooks()
{
	if (!hooked_) return;

	hk_endscene_.Retour();
	hk_reset_.Retour();

	hooked_ = false;
}

DWORD GWAPI::DirectXMgr::dwFindPattern(DWORD dwAddress, DWORD dwLen, BYTE* bMask, char* szMask)
{
	for (DWORD i = 0; i < dwLen; i++)
		if (bDataCompare((BYTE*)(dwAddress + i), bMask, szMask))
			return (DWORD)(dwAddress + i);
	return 0;
}

bool GWAPI::DirectXMgr::bDataCompare(const BYTE* pData, const BYTE* bMask, const char* szMask)
{
	for (; *szMask; ++szMask, ++pData, ++bMask)
		if (*szMask == 'x' && *pData != *bMask)
			return false;
	return ((*szMask) == NULL);
}

GWAPI::DirectXMgr::~DirectXMgr()
{
}
