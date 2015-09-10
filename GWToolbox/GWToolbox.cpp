#include "GWToolbox.h"

#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"
#include "../include/OSHGui/Drawing/Theme.hpp"
#include "../include/OSHGui/Input/WindowsMessage.hpp"

#include <string>

#include "Timer.h"
#include "MainWindow.h"
#include "TimerWindow.h"

using namespace OSHGui::Drawing;
using namespace OSHGui::Input;

GWToolbox* GWToolbox::instance_ = NULL;
bool GWToolbox::capture_input = false;

namespace{
	GWAPI::GWAPIMgr * mgr;
	GWAPI::DirectXMgr * dx;

	Direct3D9Renderer* renderer;

	HHOOK oshinputhook;
	long OldWndProc = 0;
	WindowsMessage input;
}

static LRESULT CALLBACK NewWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	
	if (Message == WM_QUIT || Message == WM_CLOSE) {
		GWToolbox::instance()->config()->save();
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
		case WM_MBUTTONUP:
			input.ProcessMessage(&msg);
			break;
		
		// Send other mouse events to osh first and consume them if used
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_MOUSEWHEEL:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
			if (input.ProcessMessage(&msg)) {
				return TRUE;
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
			GWToolbox::instance()->main_window()->hotkey_panel()->ProcessMessage(&msg);
			if (GWToolbox::capture_input) {
				input.ProcessMessage(&msg);
				return TRUE;
			}
			break;
		}
	}

	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}


void create_gui(IDirect3DDevice9* pDevice) {

	LOG("Creating GUI\n");
	renderer = new Direct3D9Renderer(pDevice);
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(renderer));

	Application * app = Application::InstancePtr();

	string path = GuiUtils::getPathA("Theme.txt");
	try {
		Theme theme;
		theme.Load(path);
		app->SetTheme(theme);
		LOG("Loaded theme file %s\n", path.c_str());
	} catch (Misc::InvalidThemeException e) {
		ERR("WARNING Could not load theme file %s\n", path.c_str());
	}
	
	app->SetDefaultFont(GuiUtils::getTBFont(10.0f, false));

	app->SetCursorEnabled(false);

	MainWindow* main_window = new MainWindow();
	std::shared_ptr<MainWindow> shared_ptr = std::shared_ptr<MainWindow>(main_window);
	app->Run(shared_ptr);
	GWToolbox::instance()->set_main_window(main_window);

	TimerWindow* timer_window = new TimerWindow();
	std::shared_ptr<TimerWindow> timer_shared = std::shared_ptr<TimerWindow>(timer_window);
	timer_shared->Show(timer_shared);
	GWToolbox::instance()->set_timer_window(timer_window);
	
	app->Enable();

	HWND hWnd = GWAPI::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(hWnd, GWL_WNDPROC, (long)NewWndProc);
}

// All rendering done here.
static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice) {
	static GWAPI::DirectXMgr::EndScene_t origfunc = dx->GetEndsceneReturn();
	static bool init = false;
	if (!init) {
		init = true;
		create_gui(pDevice);
	}

	GWToolbox* tb = GWToolbox::instance();
	if (tb && tb->timer_window()) {
		tb->timer_window()->UpdateLabel();
	}

	renderer->BeginRendering();

	Application::Instance().Render();

	renderer->EndRendering();


	return origfunc(pDevice);
}

static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	static GWAPI::DirectXMgr::Reset_t origfunc = dx->GetResetReturn();


	// pre-reset here.

	renderer->PreD3DReset();

	HRESULT result = origfunc(pDevice, pPresentationParameters);
	if (result == D3D_OK){
		// post-reset here.
		renderer->PostD3DReset();
	}

	return result;
}

void GWToolbox::exec() {
	mgr = GWAPI::GWAPIMgr::GetInstance();
	dx = mgr->DirectX;

	LOG("Installing dx hooks\n");
	dx->CreateRenderHooks(endScene, resetScene);
	
	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

	Application * app = Application::InstancePtr();

	while (true) { // main loop
		if (app->HasBeenInitialized()) {

			if (main_window_) main_window_->MainRoutine();

			
		}

		Sleep(10);

		if (GetAsyncKeyState(VK_END) & 1)
			Destroy();
		if (must_self_destruct_)
			Destroy();
	}
}

void GWToolbox::Destroy()
{
	LOG("Destroying GWToolbox++\n");


	config_->save();
	delete config_;

	HWND hWnd = GWAPI::MemoryMgr::GetGWWindowHandle();
	SetWindowLongPtr(hWnd, GWL_WNDPROC, (long)OldWndProc);
	
	GWAPI::GWAPIMgr::Destruct();
#if DEBUG_BUILD
	FreeConsole();
#endif
	FreeLibraryAndExitThread(m_dllmodule, EXIT_SUCCESS);
}


void GWToolbox::threadEntry(HMODULE mod) {
	if (GWToolbox::instance()) return;

	LOG("Initializing GWAPI\n");
	GWAPI::GWAPIMgr::Initialize();

	LOG("Creating GWToolbox++\n");
	instance_ = new GWToolbox(mod);

	LOG("Running GWToolbox++\n");
	instance_->exec();
}
