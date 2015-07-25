#pragma once

#include "APIncludes.h"

#include <d3d9.h>
#include <d3dx9.h>
#include "detours.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

class D3DXHook {
	bool bDataCompare(const BYTE* pData, const BYTE* bMask, const char* szMask);
	DWORD dwFindPattern(DWORD dwAddress, DWORD dwLen, BYTE* bMask, char* szMask);
public:
	typedef HRESULT(__stdcall *EndScene_t)(IDirect3DDevice9* pDevice);
	typedef HRESULT(__stdcall *Reset_t)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

	D3DXHook(EndScene_t _endscene, Reset_t _reset);
	~D3DXHook();
private:
	EndScene_t oEndScene = NULL;
	Reset_t oReset = NULL;

	BYTE endsceneRestore[20];
	BYTE resetRestore[20];

	DWORD* VTableStart = 0;
};