#include "GWToolbox.h"

#include <string>

#include "OSHGui\OSHGui.hpp"

#include "Timer.h"
#include "MainWindow.h"
#include "TimerWindow.h"

const wchar_t * GWToolbox::Host = L"http://fbgmguild.com/GWToolboxpp/";
const wchar_t* GWToolbox::Version = L"1.3";


GWToolbox* GWToolbox::instance_ = NULL;
GWAPI::DirectXMgr* GWToolbox::dx = NULL;
OSHGui::Drawing::Direct3D9Renderer* GWToolbox::renderer = NULL;
long GWToolbox::OldWndProc = 0;
OSHGui::Input::WindowsMessage GWToolbox::input;

void GWToolbox::SafeThreadEntry(HMODULE dllmodule) {
	__try {
		GWToolbox::ThreadEntry(dllmodule);
	} __except ( EXCEPT_EXPRESSION ) {
		LOG("SafeThreadEntry __except body\n");
	}
}

void GWToolbox::ThreadEntry(HMODULE dllmodule) {
	if (GWToolbox::instance()) return;

	LOG("Initializing API\n");
	if (!GWAPI::GWAPIMgr::Initialize()){
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
		FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
	}

	LOG("Creating GWToolbox++\n");
	instance_ = new GWToolbox(dllmodule);

	//*(byte*)0 = 0; // uncomment for guaranteed fun

	instance_->Exec();
}

void GWToolbox::Exec() {
	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::instance();
	dx = api->DirectX();

	LOG("Installing dx hooks\n");
	dx->CreateRenderHooks(endScene, resetScene);
	LOG("Installed dx hooks\n");

	LOG("Installing input event handler\n");
	HWND gw_window_handle = GWAPI::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)NewWndProc);
	LOG("Installed input event handler\n");

	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

	config_->iniWrite(L"launcher", L"dllversion", Version);

	Application * app = Application::InstancePtr();

	while (true) { // main loop
		if (app->HasBeenInitialized() && initialized_) {
			__try {
				main_window_->MainRoutine();
				timer_window_->MainRoutine();
				bonds_window_->MainRoutine();
				health_window_->MainRoutine();
				distance_window_->MainRoutine();
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				LOG("Badness happened! (in main thread)\n");
			}
		}

		Sleep(10);
#ifdef _DEBUG
		if (GetAsyncKeyState(VK_END) & 1)
			break;
#endif
		if (must_self_destruct_)
			break;
	}

	LOG("Destroying GWToolbox++\n");

	Sleep(100);

	LOG("Disabling GUI\n");
	Application::InstancePtr()->Disable();
	LOG("Closing settings\n");
	main_window()->settings_panel()->Close();
	LOG("Saving config file\n");
	config_->save();
	Sleep(100);
	LOG("Deleting config\n");
	delete config_;
	Sleep(100);
	LOG("Restoring input hook\n");
	SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
	Sleep(100);
	LOG("Destroying API\n");
	GWAPI::GWAPIMgr::Destruct();
	LOG("Closing log/console, bye!\n");
	Logger::Close();
	Sleep(100);
	FreeLibraryAndExitThread(dll_module_, EXIT_SUCCESS);
}

LRESULT CALLBACK GWToolbox::NewWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	
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
			GWToolbox::instance()->main_window()->hotkey_panel()->ProcessMessage(&msg);
			if (GWToolbox::instance()->capture_input()) {
				input.ProcessMessage(&msg);
				return true;
			}
			break;
		}
	}

	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}


void GWToolbox::SafeCreateGui(IDirect3DDevice9* pDevice) {
	__try {
		GWToolbox::CreateGui(pDevice);
	} __except ( EXCEPT_EXPRESSION ) {
		LOG("SafeCreateGui __except body\n");
	}
}

void GWToolbox::CreateGui(IDirect3DDevice9* pDevice) {

	LOG("Creating GUI\n");
	LOG("Creating Renderer\n");
	renderer = new Direct3D9Renderer(pDevice);

	LOG("Creating OSH Application\n");
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(renderer));

	Application * app = Application::InstancePtr();

	LOG("Loading Theme\n");
	std::wstring path = GuiUtils::getPath(L"Theme.txt");
	try {
		Theme theme;
		theme.Load(path);
		app->SetTheme(theme);
		LOG("Loaded Theme\n");
	} catch (Misc::InvalidThemeException e) {
		LOG("WARNING Could not load theme file %s\n", path.c_str());
	}
	
	LOG("Loading font\n");
	app->SetDefaultFont(GuiUtils::getTBFont(10.0f, true));

	app->SetCursorEnabled(false);
	try {

		LOG("Creating main window\n");
		MainWindow* main_window = new MainWindow();
		main_window->SetFont(app->GetDefaultFont());
		std::shared_ptr<MainWindow> shared_ptr = std::shared_ptr<MainWindow>(main_window);
		app->Run(shared_ptr);

		GWToolbox::instance()->set_main_window(main_window);
		LOG("Creating timer\n");
		GWToolbox::instance()->set_timer_window(new TimerWindow());
		LOG("Creating bonds window\n");
		GWToolbox::instance()->set_bonds_window(new BondsWindow());
		LOG("Creating health window\n");
		GWToolbox::instance()->set_health_window(new HealthWindow());
		LOG("Creating distance window\n");
		GWToolbox::instance()->set_distance_window(new DistanceWindow());
		LOG("Enabling app\n");
		app->Enable();
		GWToolbox::instance()->set_initialized();
		LOG("Gui Created\n");
	} catch (Misc::FileNotFoundException e) {
		LOG("Error: file not found %s\n", e.what());
		GWToolbox::instance()->StartSelfDestruct();
	}
}

// All rendering done here.
HRESULT WINAPI GWToolbox::endScene(IDirect3DDevice9* pDevice) {
	static GWAPI::DirectXMgr::EndScene_t origfunc = dx->EndsceneReturn();
	static bool init = false;
	if (!init) {
		init = true;
		GWToolbox::SafeCreateGui(pDevice);
	}

	GWToolbox::instance()->UpdateUI();

	renderer->BeginRendering();

	Application::InstancePtr()->Render();

	renderer->EndRendering();

	return origfunc(pDevice);
}

HRESULT WINAPI GWToolbox::resetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	static GWAPI::DirectXMgr::Reset_t origfunc = dx->ResetReturn();

	// pre-reset here.

	renderer->PreD3DReset();

	HRESULT result = origfunc(pDevice, pPresentationParameters);
	if (result == D3D_OK){
		// post-reset here.
		renderer->PostD3DReset();
	}

	return result;
}

void GWToolbox::UpdateUI() {
	if (initialized_) {
		__try {
			main_window_->UpdateUI();
			timer_window_->UpdateUI();
			bonds_window_->UpdateUI();
			health_window_->UpdateUI();
			distance_window_->UpdateUI();
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			LOG("Badness happened! (in render thread)\n");
		}
	}
}

