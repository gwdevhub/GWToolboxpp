#include "GWToolbox.h"

#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"
#include "../include/OSHGui/Drawing/Theme.hpp"
#include "../include/OSHGui/Input/WindowsMessage.hpp"

#include <string>
#include <time.h>
#include <fstream>

#include "Timer.h"
#include "MainWindow.h"
#include "TimerWindow.h"

using namespace std;

GWToolbox* GWToolbox::instance_ = NULL;
GWAPI::DirectXMgr* GWToolbox::dx = NULL;
OSHGui::Drawing::Direct3D9Renderer* GWToolbox::renderer = NULL;
long GWToolbox::OldWndProc = 0;
OSHGui::Input::WindowsMessage GWToolbox::input;

void GWToolbox::SafeThreadEntry(HMODULE dllmodule) {
	__try {
		GWToolbox::ThreadEntry(dllmodule);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		GWToolbox::ExceptionHappened();
	}
}

void GWToolbox::ExceptionHappened() {
	// TODO write log file

	MessageBoxA(0,
		"GWToolbox crashed, oops\n\n"
		"A log file has been created in the GWToolbox data folder.\n"
		"Open it by typing running %LOCALAPPDATA% and looking for GWToolboxpp folder\n"
		"Please send the file to the GWToolbox++ developers.\n"
		"Thank you and sorry for the inconvenience.",
		"GWToolbox++ Crash!", 0);

	// TODO kill process ?
}

void GWToolbox::ThreadEntry(HMODULE dllmodule) {
	if (GWToolbox::instance()) return;

	LOG("Initializing API... ");
	GWAPI::GWAPIMgr::Initialize();
	LOG("ok\n");

	LOG("Creating GWToolbox++... ");
	instance_ = new GWToolbox(dllmodule);
	LOG("ok\n");

	//*(byte*)0 = 0; // uncomment for guaranteed fun

	instance_->Exec();
}

void GWToolbox::Exec() {
	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::GetInstance();
	dx = api->DirectX;

	LOG("Installing dx hooks... ");
	dx->CreateRenderHooks(endScene, resetScene);
	LOG("ok\n");

	LOG("Installing input event handler... ");
	HWND gw_window_handle = GWAPI::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)NewWndProc);
	LOG("ok\n");

	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

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

		if (DEBUG_BUILD && GetAsyncKeyState(VK_END) & 1)
			break;
		if (must_self_destruct_)
			break;
	}

	LOG("Destroying GWToolbox++\n");

	Sleep(100);

	config_->save();
	Sleep(100);
	delete config_;
	Sleep(100);
	SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
	Sleep(100);
	GWAPI::GWAPIMgr::Destruct();
#if DEBUG_BUILD
	FreeConsole();
#endif
	Sleep(100);
	FreeLibraryAndExitThread(dll_module_, EXIT_SUCCESS);
}

// TODO delete
LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo) {
	PEXCEPTION_RECORD records = ExceptionInfo->ExceptionRecord;
	PCONTEXT cpudbg = ExceptionInfo->ContextRecord;
	if (records->ExceptionFlags != 0) {

		time_t rawtime;
		time(&rawtime);

		struct tm timeinfo;
		localtime_s(&timeinfo, &rawtime);

		char buffer[64];
		asctime_s(buffer, sizeof(buffer), &timeinfo);

		string filename = string("Crash-") + string(buffer) + string(".txt");
		string path = GuiUtils::getPathA(filename.c_str());
		ofstream logfile(path.c_str());


		logfile << hex;
		logfile << "Code: " << records->ExceptionCode << endl;
		logfile << "Addr: " << records->ExceptionAddress << endl;
		logfile << "Flag: " << records->ExceptionFlags << endl << endl;

		logfile << "Registers:" << endl;
		logfile << "\teax: " << cpudbg->Eax << endl;
		logfile << "\tebx: " << cpudbg->Ebx << endl;
		logfile << "\tecx: " << cpudbg->Ecx << endl;
		logfile << "\tedx: " << cpudbg->Edx << endl;
		logfile << "\tesi: " << cpudbg->Esi << endl;
		logfile << "\tedi: " << cpudbg->Edi << endl;
		logfile << "\tesp: " << cpudbg->Esp << endl;
		logfile << "\tebp: " << cpudbg->Ebp << endl;
		logfile << "\teip: " << cpudbg->Eip << endl << endl;

		logfile << "Stack:" << endl;

		for (DWORD i = 0; i <= 0x40; i += 4)
			logfile << "\t[esp+" << i << "]: " << ((DWORD*)(cpudbg->Esp))[i / 4];


		logfile.close();

		MessageBoxA(0, "Guild Wars - Crash!", "GWToolbox has detected Guild Wars has crashed, oops.\nThis may be a crash toolbox has made, or something completely irrelevant to it.\nFor help on this matter, please access the log file placed in the \"crash\" folder of the GWToolbox data folder with the correct date and time, and send the file along with any information or actions surrounding the crash to the GWToolbox++ developers.\nThank you and sorry for the inconvenience.", 0);
		return EXCEPTION_EXECUTE_HANDLER;
	} else {
		return EXCEPTION_CONTINUE_EXECUTION;
	}
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
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		GWToolbox::ExceptionHappened();
	}
}

void GWToolbox::CreateGui(IDirect3DDevice9* pDevice) {

	LOG("Creating GUI...");
	renderer = new Direct3D9Renderer(pDevice);
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(renderer));

	Application * app = Application::InstancePtr();

	string path = GuiUtils::getPathA("Theme.txt");
	try {
		Theme theme;
		theme.Load(path);
		app->SetTheme(theme);
	} catch (Misc::InvalidThemeException e) {
		ERR("WARNING Could not load theme file %s\n", path.c_str());
	}
	
	app->SetDefaultFont(GuiUtils::getTBFont(10.0f, true));

	app->SetCursorEnabled(false);
	try {
		MainWindow* main_window = new MainWindow();
		main_window->SetFont(app->GetDefaultFont());
		std::shared_ptr<MainWindow> shared_ptr = std::shared_ptr<MainWindow>(main_window);
		app->Run(shared_ptr);

		GWToolbox::instance()->set_main_window(main_window);
		GWToolbox::instance()->set_timer_window(new TimerWindow());
		GWToolbox::instance()->set_bonds_window(new BondsWindow());
		GWToolbox::instance()->set_health_window(new HealthWindow());
		GWToolbox::instance()->set_distance_window(new DistanceWindow());

		app->Enable();
		GWToolbox::instance()->set_initialized();

		LOG("ok\n");
	} catch (Misc::FileNotFoundException e) {
		LOG("Error: file not found %s\n", e.what());
		GWToolbox::instance()->StartSelfDestruct();
	}
}

// All rendering done here.
HRESULT WINAPI GWToolbox::endScene(IDirect3DDevice9* pDevice) {
	static GWAPI::DirectXMgr::EndScene_t origfunc = dx->GetEndsceneReturn();
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

