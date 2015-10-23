#include <Windows.h>
#include <stdio.h>
#include <d3d9.h>
#include <cstdlib>

#include <GWCA/APIMain.h>

#include "Minimap.h"

using namespace GWAPI;

unsigned long OldWndProc = 0;
Minimap* minimap = nullptr;
GWAPI::DirectXMgr* dx;

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {

	if (Message == WM_QUIT || Message == WM_CLOSE) {
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	MSG msg;
	msg.hwnd = hWnd;
	msg.message = Message;
	msg.wParam = wParam;
	msg.lParam = lParam;

	switch (Message) {
		// Send right mouse button events to gw (move view around) and don't mess with them
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		break;

		// Send button up mouse events to both gw and osh, to avoid gw being stuck on mouse-down
	case WM_LBUTTONUP:
		if (minimap->OnMouseUp(msg)) return true;
		break;

	case WM_MOUSEMOVE:
		if (minimap->OnMouseMove(msg)) return true;
		break;

	case WM_LBUTTONDOWN:
		if (minimap->OnMouseDown(msg)) return true;
		break;

	case WM_MOUSEWHEEL:
		if (minimap->OnMouseWheel(msg)) return true;
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_IME_CHAR:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		break;
	}

	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}

HRESULT WINAPI EndScene(IDirect3DDevice9* dev) {	
	minimap->Render(dev);
	
	return dx->EndsceneReturn()(dev);
}

HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice,
	D3DPRESENT_PARAMETERS* pPresentationParameters) {

	return dx->ResetReturn()(pDevice, pPresentationParameters);
}

void init(HMODULE hModule) {
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);
	freopen_s(&fh, "CONOUT$", "w", stderr);
	SetConsoleTitleA("GWTB++ Debug Console");
	printf("Hello world!\n");

	printf("Initializing API\n");
	if (GWAPI::GWCA::Initialize()) {
		printf("Initialized successful\n");

		minimap = new Minimap();
		minimap->Scale(0.0002f);
		minimap->Translate(0, -3000.0f);
		minimap->SetLocation(100, 100);
		minimap->SetSize(400, 400);

		
		GWAPI::GWCA::Api().DirectX().CreateRenderHooks(EndScene, ResetScene);

		HWND gw_window_handle = GWAPI::MemoryMgr::GetGWWindowHandle();
		OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)WndProc);

		while (true) {

			if (GetAsyncKeyState(VK_END) & 1) break;

			Sleep(1);
		}

		SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
		GWAPI::GWCA::Destruct();

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
