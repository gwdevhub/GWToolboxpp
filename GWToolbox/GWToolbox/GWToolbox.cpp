#include "GWToolbox.h"

#include <string>

#include <OSHGui\OSHGui.hpp>

#include <GWCA\GWCA.h>
#include <GWCA\DirectXMgr.h>
#include <GWCA\AgentMgr.h>
#include <GWCA\CameraMgr.h>
#include <GWCA\ChatMgr.h>
#include <GWCA\EffectMgr.h>
#include <GWCA\FriendListMgr.h>
#include <GWCA\GuildMgr.h>
#include <GWCA\ItemMgr.h>
#include <GWCA\MapMgr.h>
#include <GWCA\MerchantMgr.h>
#include <GWCA\PartyMgr.h>
#include <GWCA\PlayerMgr.h>
#include <GWCA\SkillbarMgr.h>
#include <GWCA\StoCMgr.h>

#include "Timer.h"
#include "MainWindow.h"
#include "TimerWindow.h"
#include "Settings.h"

const wchar_t * GWToolbox::Host = L"http://fbgmguild.com/GWToolboxpp/";
const wchar_t* GWToolbox::Version = L"1.7";


GWToolbox* GWToolbox::instance_ = NULL;
OSHGui::Drawing::Direct3D9Renderer* GWToolbox::renderer = NULL;
long GWToolbox::OldWndProc = 0;
OSHGui::Input::WindowsMessage GWToolbox::input;


void GWToolbox::SafeThreadEntry(HMODULE dllmodule) {
	__try {
		GWToolbox::ThreadEntry(dllmodule);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
		LOG("SafeThreadEntry __except body\n");
	}
}

void GWToolbox::ThreadEntry(HMODULE dllmodule) {
	if (instance_) return;

	LOG("Initializing API\n");
	if (!GWCA::Api::Initialize()){
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
		FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
	}

	GWCA::Gamethread();
	GWCA::CtoS();
	GWCA::StoC();
	GWCA::Agents();
	GWCA::Party();
	GWCA::Items();
	GWCA::Skillbar();
	GWCA::Effects();
	GWCA::Chat();
	GWCA::Merchant();
	GWCA::Guild();
	GWCA::Map();
	GWCA::FriendList();
	GWCA::Camera();

	LOG("Creating GWToolbox++\n");
	instance_ = new GWToolbox(dllmodule);

	//*(byte*)0 = 0; // uncomment for guaranteed fun

	instance_->Exec();
}

void GWToolbox::Exec() {
	LOG("Installing dx hooks\n");
	GWCA::DirectX().CreateRenderHooks(endScene, resetScene);
	LOG("Installed dx hooks\n");

	LOG("Installing input event handler\n");
	HWND gw_window_handle = GWCA::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);
	LOG("oldwndproc %X\n", OldWndProc);
	LOG("Installed input event handler\n");

	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

	config_->IniWrite(L"launcher", L"dllversion", Version);

	Application* app = Application::InstancePtr();

	while (!must_self_destruct_) { // main loop
		if (app->HasBeenInitialized() && initialized_) {
			__try {
				chat_commands_->MainRoutine();
				main_window_->MainRoutine();
				timer_window_->MainRoutine();
				bonds_window_->MainRoutine();
				health_window_->MainRoutine();
				distance_window_->MainRoutine();
				party_damage_->MainRoutine();
			} __except ( EXCEPT_EXPRESSION_LOOP ) {
				LOG("Badness happened! (in main thread)\n");
			}
		}

		Sleep(10);
#ifdef _DEBUG
		if (GetAsyncKeyState(VK_END) & 1) {
			must_self_destruct_ = true;
			break;
		}
			
#endif
	}

	LOG("Destroying GWToolbox++\n");

	Sleep(100);

	LOG("Disabling GUI\n");
	Application::InstancePtr()->Disable();
	LOG("Closing settings\n");
	main_window().settings_panel().Close();
	LOG("Saving config file\n");
	config_->Save();
	LOG("saving health log\n");
	party_damage().SaveIni();
	Sleep(100);
	LOG("Deleting config\n");
	delete config_;
	delete chat_commands_;
	Sleep(100);
	LOG("Restoring input hook\n");
	SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
	Sleep(100);
	LOG("Destroying API\n");
	GWCA::Api::Destruct();
	LOG("Closing log/console, bye!\n");
	Logger::Close();
	Sleep(100);
	FreeLibraryAndExitThread(dll_module_, EXIT_SUCCESS);
}

LRESULT CALLBACK GWToolbox::SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	__try {
		return GWToolbox::WndProc(hWnd, Message, wParam, lParam);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}
}


LRESULT CALLBACK GWToolbox::WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	
	if (Message == WM_QUIT || Message == WM_CLOSE) {
		GWToolbox::instance().config().Save();
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	if (Application::Instance().HasBeenInitialized()) {
		MSG msg;
		msg.hwnd = hWnd;
		msg.message = Message;
		msg.wParam = wParam;
		msg.lParam = lParam;

		switch (Message) {
		// Send right mouse button events to gw (move view around) and don't mess with them
		case WM_RBUTTONDOWN: GWToolbox::instance().right_mouse_pressed_ = true; break;
		case WM_RBUTTONUP: GWToolbox::instance().right_mouse_pressed_ = false; break;

		// Send button up mouse events to both gw and osh, to avoid gw being stuck on mouse-down
		case WM_LBUTTONUP:
			input.ProcessMouseMessage(&msg);
			break;
		
		// Send other mouse events to osh first and consume them if used
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_MOUSEWHEEL:
			if (GWToolbox::instance().right_mouse_pressed_) break;
			if (input.ProcessMouseMessage(&msg)) {
				return true;
			} else {
				Application::Instance().clearFocus();
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
			// send input to OSHgui for creating hotkeys and general UI stuff
			if (GWToolbox::instance().capture_input()) {
				input.ProcessMessage(&msg);
				return true;
			}

			// send input to chat commands for camera movement
			if (GWToolbox::instance().chat_commands().ProcessMessage(&msg)) {
				return true;
			}

			// send input to toolbox to trigger hotkeys
			GWToolbox::instance().main_window().hotkey_panel().ProcessMessage(&msg);

			// block alt-enter if in borderless to avoid graphic glitches (no reason to go fullscreen anyway)
			if (GWToolbox::instance().main_window().settings_panel().GetSetting(
				SettingsPanel::Setting_enum::e_BorderlessWindow)->ReadSetting()
				&& (GetAsyncKeyState(VK_MENU) < 0)
				&& (GetAsyncKeyState(VK_RETURN) < 0)) {
				return true;
			}

			break;

		case WM_SIZE:
			GWToolbox::instance().new_screen_size_ = SizeI(LOWORD(lParam), HIWORD(lParam));
			if (GWToolbox::instance().new_screen_size_ != GWToolbox::instance().old_screen_size_) {
				GWToolbox::instance().must_resize_ = true;
			}
			break;

		default:
			//printf("0x%X, 0x%X, 0x%X\n", Message, wParam, lParam);
			break;
		}
	}

	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}


void GWToolbox::SafeCreateGui(IDirect3DDevice9* pDevice) {
	__try {
		GWToolbox::CreateGui(pDevice);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
		LOG("SafeCreateGui __except body\n");
	}
}

void GWToolbox::CreateGui(IDirect3DDevice9* pDevice) {

	LOG("Creating GUI\n");
	LOG("Creating Renderer\n");
	renderer = new Direct3D9Renderer(pDevice);

	GWToolbox::instance().old_screen_size_ = renderer->GetDisplaySize();
	GWToolbox::instance().new_screen_size_ = renderer->GetDisplaySize();

	LOG("Creating OSH Application\n");
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(renderer));

	Application& app = Application::Instance();

	GWToolbox::instance().LoadTheme();
	
	LOG("Loading font\n");
	app.SetDefaultFont(GuiUtils::getTBFont(10.0f, true));

	app.SetCursorEnabled(false);
	try {

		LOG("Creating main window\n");
		MainWindow* main_window = new MainWindow();
		main_window->SetFont(app.GetDefaultFont());
		std::shared_ptr<MainWindow> shared_ptr = std::shared_ptr<MainWindow>(main_window);
		app.Run(shared_ptr);

		GWToolbox::instance().set_main_window(main_window);
		LOG("Creating timer\n");
		GWToolbox::instance().set_timer_window(new TimerWindow());
		LOG("Creating bonds window\n");
		GWToolbox::instance().set_bonds_window(new BondsWindow());
		LOG("Creating health window\n");
		GWToolbox::instance().set_health_window(new HealthWindow());
		LOG("Creating distance window\n");
		GWToolbox::instance().set_distance_window(new DistanceWindow());
		LOG("Creating party damage window\n");
		GWToolbox::instance().set_party_damage(new PartyDamage());
		LOG("Applying settings\n");
		GWToolbox::instance().main_window().settings_panel().ApplySettings();
		LOG("Enabling app\n");
		app.Enable();
		GWToolbox::instance().set_initialized();
		LOG("Gui Created\n");
		LOG("Saving theme\n");
		GWToolbox::instance().SaveTheme();

	} catch (Misc::FileNotFoundException e) {
		LOG("Error: file not found %s\n", e.what());
		GWToolbox::instance().StartSelfDestruct();
	}
}

// All rendering done here.
HRESULT WINAPI GWToolbox::endScene(IDirect3DDevice9* pDevice) {
	static bool init = false;
	if (!init) {
		init = true;
		GWToolbox::SafeCreateGui(pDevice);
	}

	GWToolbox& tb = GWToolbox::instance();
	if (!tb.must_self_destruct_) {

		if (tb.must_resize_) {
			tb.must_resize_ = false;
			renderer->UpdateSize();
			tb.ResizeUI();
			tb.old_screen_size_ = tb.new_screen_size_;
		}

		tb.UpdateUI();

		D3DVIEWPORT9 viewport;
		pDevice->GetViewport(&viewport);
		Drawing::PointI location = tb.main_window().GetLocation();
		Drawing::RectangleI size = tb.main_window().GetSize();

		if (location.X < 0) {
			location.X = 0;
		}
		if (location.Y < 0) {
			location.Y = 0;
		}
		if (location.X > static_cast<int>(viewport.Width) - size.GetWidth()) {
			location.X = static_cast<int>(viewport.Width) - size.GetWidth();
		}
		if (location.Y > static_cast<int>(viewport.Height) - size.GetHeight()) {
			location.Y = static_cast<int>(viewport.Height) - size.GetHeight();
		}
		if (location != tb.main_window().GetLocation()) {
			tb.main_window().SetLocation(location);
		}

		renderer->BeginRendering();
		Application::InstancePtr()->Render();
		renderer->EndRendering();
	}

	return GWCA::DirectX().EndsceneReturn()(pDevice);
}

HRESULT WINAPI GWToolbox::resetScene(IDirect3DDevice9* pDevice, 
	D3DPRESENT_PARAMETERS* pPresentationParameters) {
	// pre-reset here.
	renderer->PreD3DReset();

	HRESULT result = GWCA::DirectX().ResetReturn()(pDevice, pPresentationParameters);
	if (result == D3D_OK){
		// post-reset here.
		renderer->PostD3DReset();
	}

	return result;
}

void GWToolbox::UpdateUI() {
	if (initialized_) {
		__try {
			chat_commands_->UpdateUI();
			main_window_->UpdateUI();
			timer_window_->UpdateUI();
			health_window_->UpdateUI();
			distance_window_->UpdateUI();
			party_damage_->UpdateUI();
			bonds_window_->UpdateUI();
		} __except ( EXCEPT_EXPRESSION_LOOP ) {
			LOG("Badness happened! (in render thread)\n");
		}
	}
}

void GWToolbox::ResizeUI() {
	if (initialized_ && adjust_on_resize_) {
		main_window_->ResizeUI(old_screen_size_, new_screen_size_);
		timer_window_->ResizeUI(old_screen_size_, new_screen_size_);
		health_window_->ResizeUI(old_screen_size_, new_screen_size_);
		distance_window_->ResizeUI(old_screen_size_, new_screen_size_);
		party_damage_->ResizeUI(old_screen_size_, new_screen_size_);
		bonds_window_->ResizeUI(old_screen_size_, new_screen_size_);
	}
}

void GWToolbox::LoadTheme() {
	LOG("Loading Theme\n");
	std::wstring path = GuiUtils::getPath(L"Theme.txt");
	try {
		OSHGui::Drawing::Theme theme;
		theme.Load(path);
		OSHGui::Application::Instance().SetTheme(theme);
		LOG("Loaded Theme\n");
	} catch (Misc::InvalidThemeException e) {
		LOG("[Warning] Could not load theme file %s\n", path.c_str());
	}
}

void GWToolbox::SaveTheme() {
	OSHGui::Drawing::Theme& t = Application::Instance().GetTheme();
	t.Save(GuiUtils::getPath(L"Theme.txt"),
		OSHGui::Drawing::Theme::ColorStyle::Array);
}
