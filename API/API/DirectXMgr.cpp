#include "DirectXMgr.h"
#include <Windows.h>
#include <stdio.h>
#include <Psapi.h>
#include "PatternScanner.h"

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

	printf("DX Start\n");

	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded;
	if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		printf("EnumProcessModules failed, error %d\n", GetLastError());
		return;
	}

	DWORD pA1 = NULL;
	for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i) {
		TCHAR mod_name[MAX_PATH];
		if (!GetModuleBaseName(hProcess, hMods[i], mod_name, MAX_PATH)) {
			printf("GetModuleBaseName failed, error %X\n", GetLastError());
			continue;
		}

		if (wcscmp(mod_name, L"d3d9.dll") != 0) continue;

		printf("Found directx! name = %ls, handle = %X\n", mod_name, hMods[i]);

		MODULEINFO info;
		if (!GetModuleInformation(hProcess, hMods[i], &info, sizeof(MODULEINFO))) {
			printf("GetModuleInformation failed, error %X\n", GetLastError());
			continue;
		}
		DWORD start = (DWORD)info.lpBaseOfDll;
		DWORD size = info.SizeOfImage;

		printf("Module info: start %X, size %X\n", start, size);

		DWORD scan_size = 0x128000;
		if (size < scan_size) continue;

		PatternScanner dx_scan(start, scan_size);

		pA1 = dx_scan.FindPattern(
			"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx", 2);
		if (pA1 != NULL) {
			printf("Vtable scan found! Addr: %X\n", pA1);
			break;
		} else {
			printf("Scanned a module but could not find pattern!\n");
		}
	}

	memcpy(&vtable_start_, (void*)pA1, 4);
	printf("memcpy vtable done\n");
	DWORD dwEndsceneLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[42]));
	endscene_ = (EndScene_t)hk_endscene_.Detour((BYTE*)(vtable_start_[42]), (BYTE*)_endscene, dwEndsceneLen);
	printf("DX endscene! Len: %d\n", dwEndsceneLen);

	DWORD dwResetLen = Hook::CalculateDetourLength((BYTE*)(vtable_start_[16]));
	reset_ = (Reset_t)hk_reset_.Detour((BYTE*)(vtable_start_[16]), (BYTE*)_reset, dwResetLen);
	printf("DX reset! Len: %d\n", dwResetLen);

	hooked_ = true;
}

void GWAPI::DirectXMgr::RestoreHooks()
{
	if (!hooked_) return;

	hk_endscene_.Retour();
	hk_reset_.Retour();

	hooked_ = false;
}

GWAPI::DirectXMgr::~DirectXMgr()
{
}
