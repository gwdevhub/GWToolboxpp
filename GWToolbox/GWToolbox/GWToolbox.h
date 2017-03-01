#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

#include "ToolboxModule.h"

class GWToolbox {
	GWToolbox() {};
	~GWToolbox() {};
public:
	static GWToolbox& Instance() {
		static GWToolbox instance;
		return instance;
	}

	// will create a new toolbox object and run it, can be used as argument for createThread
	static void SafeThreadEntry(HMODULE mod);
private:
	static void ThreadEntry(HMODULE dllmodule);

	//// DirectX event handlers declaration
	//static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice);
	//static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice,
	//	D3DPRESENT_PARAMETERS* pPresentationParameters);

	// Input event handler
	static LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

	//------ Public methods ------//
public:

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
