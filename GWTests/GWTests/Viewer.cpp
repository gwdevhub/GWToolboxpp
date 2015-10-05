#include "Viewer.h"
#include "GWCA/APIMain.h"

bool Viewer::capture_input = false;
unsigned long Viewer::OldWndProc = 0;
OSHGui::Input::WindowsMessage Viewer::input;
OSHGui::Drawing::Direct3D9Renderer* Viewer::renderer = nullptr;

using namespace OSHGui;
using namespace std;
using namespace GWAPI;

HRESULT WINAPI Viewer::EndScene(IDirect3DDevice9* pDevice) {
	static GWAPI::DirectXMgr::EndScene_t origfunc = GWAPIMgr::instance()->DirectX()->EndsceneReturn();
	static bool init = false;
	static Viewer* viewer;
	static Application* app;
	if (!init) {
		init = true;
		renderer = new Drawing::Direct3D9Renderer(pDevice);
		Application::Initialize(unique_ptr<Drawing::Direct3D9Renderer>(renderer));
		app = Application::InstancePtr();
		app->SetCursorEnabled(false);
		app->SetDefaultFont(Drawing::FontManager::LoadFont("Arial", 12.0, true));

		viewer = new Viewer();
		shared_ptr<Viewer> shared_ptr = std::shared_ptr<Viewer>(viewer);
		app->Run(shared_ptr);
		app->Enable();
	}

	viewer->UpdateUI();

	renderer->BeginRendering();
	app->Render();
	renderer->EndRendering();
	return origfunc(pDevice);
}

HRESULT WINAPI Viewer::ResetScene(IDirect3DDevice9* pDevice, 
	D3DPRESENT_PARAMETERS* pPresentationParameters) {
	static GWAPI::DirectXMgr::Reset_t origfunc = GWAPIMgr::instance()->DirectX()->ResetReturn();

	renderer->PreD3DReset();
	HRESULT result = origfunc(pDevice, pPresentationParameters);
	if (result == D3D_OK) {
		renderer->PostD3DReset();
	}
	return result;
}

LRESULT CALLBACK Viewer::WndProc(HWND hWnd, UINT Message, 
	WPARAM wParam, LPARAM lParam) {

	if (Message == WM_QUIT || Message == WM_CLOSE) {
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	if (Application::InstancePtr()->HasBeenInitialized()) {
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
			input.ProcessMessage(&msg);
			break;

			// Send other mouse events to osh first and consume them if used
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_MOUSEWHEEL:
			if (input.ProcessMessage(&msg)) {
				return true;
			} else {
				Application::InstancePtr()->clearFocus();
			}
			break;

			// send keyboard messages to gw, osh and toolbox
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
			if (Viewer::capture_input) {
				input.ProcessMessage(&msg);
				return true;
			}
			break;
		}
	}

	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}


Viewer::Viewer() {
	SetSize(400, 300);
	SetLocation(0, 0);
	SetText(L"GW Tests");
}

void Viewer::UpdateUI() {

}