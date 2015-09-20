#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

namespace GWAPI {

	class DirectXMgr {
		bool bDataCompare(const BYTE* pData, const BYTE* bMask, const char* szMask);
		DWORD dwFindPattern(DWORD dwAddress, DWORD dwLen, BYTE* bMask, char* szMask);

		DirectXMgr(GWAPIMgr* obj);
		~DirectXMgr();
	public:
		typedef HRESULT(WINAPI *EndScene_t)(IDirect3DDevice9* pDevice);
		typedef HRESULT(WINAPI *Reset_t)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

		void CreateRenderHooks(EndScene_t _endscene, Reset_t _reset);
		void RestoreRenderHooks();

		EndScene_t GetEndsceneReturn();
		Reset_t GetResetReturn();

		

	private:
		friend class GWAPIMgr;
		GWAPIMgr* parent;
		EndScene_t oEndScene = NULL;
		Reset_t oReset = NULL;

		Hook hkDXEndscene;
		Hook hkDXReset;

		bool hooked = false;

		DWORD* VTableStart = 0;
	};

}