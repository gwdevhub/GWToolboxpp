#include <Windows.h>
#include <stdio.h>

#include <GWCA/APIMain.h>
#include <OSHGui/OSHGui.hpp>

#include "Viewer.h"

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

		GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::instance();

		api->DirectX()->CreateRenderHooks(Viewer::EndScene, Viewer::ResetScene);

		HWND gw_window_handle = GWAPI::MemoryMgr::GetGWWindowHandle();
		Viewer::OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)Viewer::WndProc);

		Viewer::input.SetKeyboardInputEnabled(true);
		Viewer::input.SetMouseInputEnabled(true);

		while (true) {
			if (GetAsyncKeyState(VK_END) & 1) break;
		}

		// 214315 gate of anguish

		OSHGui::Application::InstancePtr()->Disable();
		SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)Viewer::OldWndProc);
		GWAPI::GWAPIMgr::Destruct();

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
