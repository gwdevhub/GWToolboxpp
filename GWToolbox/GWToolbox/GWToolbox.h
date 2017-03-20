#pragma once

#include <Windows.h>

#include <vector>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

#include "ToolboxModule.h"

DWORD __stdcall SafeThreadEntry(LPVOID mod);
DWORD __stdcall ThreadEntry(LPVOID dllmodule);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

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

	void StartSelfDestruct() { must_self_destruct = true; }
	bool must_self_destruct = false;	// is true when toolbox should quit

	void RegisterModule(ToolboxModule* m) { modules.push_back(m); }

	std::vector<ToolboxModule*> modules;

private:
	CSimpleIni* inifile = nullptr;
};
