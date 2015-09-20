#include "DirectXMgr.h"

GWAPI::DirectXMgr::DirectXMgr(GWAPIMgr* obj) : parent(obj)
{

}

GWAPI::DirectXMgr::EndScene_t GWAPI::DirectXMgr::GetEndsceneReturn()
{
	return oEndScene;
}

GWAPI::DirectXMgr::Reset_t GWAPI::DirectXMgr::GetResetReturn()
{
	return oReset;
}

void GWAPI::DirectXMgr::CreateRenderHooks(EndScene_t _endscene, Reset_t _reset)
{
	if (hooked) return;


	HMODULE hD3D9 = NULL;
	while (!hD3D9)
	{
		hD3D9 = GetModuleHandleA("d3d9.dll");
		Sleep(5);
	}

	DWORD pA1 = dwFindPattern((DWORD)hD3D9, 0x128000, (PBYTE)"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx");
	memcpy(&VTableStart, (void*)(pA1 + 2), 4);

	DWORD dwEndsceneLen = Hook::CalculateDetourLength((BYTE*)(VTableStart[42]));
	oEndScene = (EndScene_t)hkDXEndscene.Detour((BYTE*)(VTableStart[42]), (BYTE*)_endscene, dwEndsceneLen);

	DWORD dwResetLen = Hook::CalculateDetourLength((BYTE*)(VTableStart[16]));
	oReset = (Reset_t)hkDXReset.Detour((BYTE*)(VTableStart[16]), (BYTE*)_reset, dwResetLen);

	hooked = true;
}

void GWAPI::DirectXMgr::RestoreRenderHooks()
{
	if (!hooked) return;

	hkDXEndscene.Retour();
	hkDXReset.Retour();

	hooked = false;
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
	if (hooked)
		RestoreRenderHooks();
}
