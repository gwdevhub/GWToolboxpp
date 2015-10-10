#include <Windows.h>
#include <stdio.h>
#include <d3d9.h>
#include <cstdlib>

#include <GWCA/APIMain.h>

#include "Minimap.h"

using namespace GWAPI;

unsigned long OldWndProc = 0;
Minimap* minimap = nullptr;

HRESULT WINAPI EndScene(IDirect3DDevice9* dev) {
	static GWAPI::DirectXMgr::EndScene_t origfunc = GWAPIMgr::instance()->DirectX()->EndsceneReturn();
	
	minimap->Render(dev);
	
	return origfunc(dev);
}

HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice,
	D3DPRESENT_PARAMETERS* pPresentationParameters) {
	static GWAPI::DirectXMgr::Reset_t origfunc = GWAPIMgr::instance()->DirectX()->ResetReturn();

	return origfunc(pDevice, pPresentationParameters);;
}

void init(HMODULE hModule) {
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	freopen_s(&fh, "CONOUT$", "w", stderr);
	SetConsoleTitleA("GWTB++ Debug Console");
	printf("Hello world!\n");

	printf("Initializing API\n");
	if (GWAPI::GWAPIMgr::Initialize()) {
		printf("Initialized successful\n");

		minimap = new Minimap();
		minimap->Scale(0.0002f);
		minimap->Translate(0, -3000.0f);
		minimap->SetLocation(100, 100);
		minimap->SetSize(600, 600);

		GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::instance();
		api->DirectX()->CreateRenderHooks(EndScene, ResetScene);

		//HWND gw_window_handle = GWAPI::MemoryMgr::GetGWWindowHandle();
		//OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)WndProc);

		while (true) {

			if (GetAsyncKeyState(VK_END) & 1) break;

			Sleep(1);
		}

		//SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
		GWAPI::GWAPIMgr::Destruct();

		Sleep(100);

	} else {
		printf("Initialize Failed\n");
	}

	printf("Bye!\n");
	FreeConsole();
	FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved) {
	if (_Reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}
