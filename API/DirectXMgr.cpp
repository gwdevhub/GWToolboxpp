#include "DirectXMgr.h"
#include "..\GWToolbox\logger.h"
#include <Windows.h>
#include <Psapi.h>

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

	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded;
	if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		LOG("EnumProcessModules failed, error %d\n", GetLastError());
		return;
	}

	DWORD pA1 = NULL;
	for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i) {
		TCHAR mod_name[MAX_PATH];
		if (!GetModuleBaseName(hProcess, hMods[i], mod_name, MAX_PATH)) {
			LOG("GetModuleBaseName failed, error %X\n", GetLastError());
			continue;
		}

		if (wcscmp(mod_name, L"d3d9.dll") != 0) continue;

		LOG("Found directx! name = %ls, handle = %X\n", mod_name, hMods[i]);

		MODULEINFO info;
		if (!GetModuleInformation(hProcess, hMods[i], &info, sizeof(MODULEINFO))) {
			LOG("GetModuleInformation failed, error %X\n", GetLastError());
			continue;
		}
		DWORD start = (DWORD)info.lpBaseOfDll;
		DWORD size = info.SizeOfImage;

		LOG("Module info: start %X, size %X\n", start, size);

		DWORD scan_size = 0x128000;
		if (size < scan_size) continue;

		pA1 = FindPattern(start, scan_size, 
			"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx");
		if (pA1 != NULL) {
			LOG("Vtable scan found! Addr: %X\n", pA1);
			break;
		} else {
			LOG("Scanned a module but could not find pattern!\n");
		}
	}

	memcpy(&vtable_start_, (void*)(pA1 + 2), 4);
	LOG("memcpy vtable done\n");
	DWORD dwEndsceneLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[42]));
	endscene_ = (EndScene_t)hk_endscene_.Detour((BYTE*)(vtable_start_[42]), (BYTE*)_endscene, dwEndsceneLen);
	LOG("DX endscene! Len: %d\n", dwEndsceneLen);

	DWORD dwResetLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[16]));
	reset_ = (Reset_t)hk_reset_.Detour((BYTE*)(vtable_start_[16]), (BYTE*)_reset, dwResetLen);
	LOG("DX reset! Len: %d\n", dwResetLen);

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
	int pattern_length = strlen(_mask);
	
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
