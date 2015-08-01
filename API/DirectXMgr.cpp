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
	HMODULE hD3D9 = NULL;
	while (!hD3D9)
	{
		hD3D9 = GetModuleHandleA("d3d9.dll");
		Sleep(5);
	}

	DWORD pA1 = dwFindPattern((DWORD)hD3D9, 0x128000, (PBYTE)"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx");
	memcpy(&VTableStart, (void*)(pA1 + 2), 4);

	memcpy(endsceneRestore, (void*)(VTableStart[42]), 20);
	memcpy(resetRestore, (void*)(VTableStart[16]), 20);

	oEndScene = (EndScene_t)DetourFunction((BYTE*)(VTableStart[42]), (BYTE*)_endscene);
	oReset = (Reset_t)DetourFunction((BYTE*)(VTableStart[16]), (BYTE*)_reset);
}

void GWAPI::DirectXMgr::RestoreRenderHooks()
{
	DWORD dwOldProt;
	VirtualProtect((void*)(VTableStart[42]), 20, PAGE_READWRITE, &dwOldProt);
	memcpy((void*)(VTableStart[42]), endsceneRestore, 20);
	VirtualProtect((void*)(VTableStart[42]), 20, dwOldProt, 0);
	VirtualProtect((void*)(VTableStart[16]), 20, PAGE_READWRITE, &dwOldProt);
	memcpy((void*)(VTableStart[16]), resetRestore, 20);
	VirtualProtect((void*)(VTableStart[16]), 20, dwOldProt, 0);
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