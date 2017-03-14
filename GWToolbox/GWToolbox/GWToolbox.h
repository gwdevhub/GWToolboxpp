#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

#include "ToolboxModule.h"

DWORD __stdcall SafeThreadEntry(LPVOID mod);
DWORD __stdcall ThreadEntry(LPVOID dllmodule);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

HRESULT WINAPI Present(IDirect3DDevice9* pDev, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

class GWToolbox {
	GWToolbox() {};
	~GWToolbox() {};
public:
	static GWToolbox& Instance() {
		static GWToolbox instance;
		return instance;
	}

	static void Draw(IDirect3DDevice9* device);

	void Initialize();
	void Terminate();

	void LoadSettings();
	void SaveSettings();

	std::vector<ToolboxModule*> modules;

	void StartSelfDestruct() { must_self_destruct = true; }
	bool must_self_destruct = false;	// is true when toolbox should quit

private:
	CSimpleIni* inifile = nullptr;
};
