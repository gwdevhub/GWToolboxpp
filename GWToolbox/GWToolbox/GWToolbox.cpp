#include "GWToolbox.h"

#include "Defines.h"

#include <string>

#include <OSHGui\OSHGui.hpp>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\CameraMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\FriendListMgr.h>
#include <GWCA\Managers\GuildMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\MerchantMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\PlayerMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA_DX\DirectXHooker.h>

#include "Timer.h"
#include "MainWindow.h"
#include "TimerWindow.h"
#include "Settings.h"
#include "ChatLogger.h"
#include "logger.h"

GWToolbox* GWToolbox::instance_ = NULL;
OSHGui::Drawing::Direct3D9Renderer* GWToolbox::renderer = NULL;
long GWToolbox::OldWndProc = 0;
OSHGui::Input::WindowsMessage GWToolbox::input;
GW::DirectXHooker* GWToolbox::dx_hooker = nullptr;

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
	if (!GW::Api::Initialize()){
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
		FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
	}

	GW::Gamethread();
	GW::CtoS();
	GW::StoC();
	GW::Agents();
	GW::Partymgr();
	GW::Items();
	GW::Skillbarmgr();
	GW::Effects();
	GW::Chat();
	GW::Merchant();
	GW::Guildmgr();
	GW::Map();
	GW::FriendListmgr();
	GW::Cameramgr();

	dx_hooker = new GW::DirectXHooker();
	printf("DxDevice = %X\n", dx_hooker->device());

	LOG("Creating Config\n");
	Config::Initialize();

	LOG("Creating GWToolbox++\n");
	instance_ = new GWToolbox(dllmodule);

	//*(byte*)0 = 0; // uncomment for guaranteed fun

	instance_->Exec();
}

void GWToolbox::Exec() {
	LOG("Installing dx hooks\n");
	dx_hooker->AddHook(GW::dx9::kEndScene, (void*)endScene);
	dx_hooker->AddHook(GW::dx9::kReset, (void*)resetScene);
	LOG("Installed dx hooks\n");

	LOG("Installing input event handler\n");
	HWND gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);
	LOG("oldwndproc %X\n", OldWndProc);
	LOG("Installed input event handler\n");

	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

	Config::IniWrite(L"launcher", L"dllversion", GWTOOLBOX_VERSION);

	ChatLogger::Init();

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents().GetAgentArray().valid()
		&& GW::Agents().GetPlayer() != nullptr) {

		DWORD playerNumber = GW::Agents().GetPlayer()->PlayerNumber;
		ChatLogger::LogF(L"Hello %ls!", GW::Agents().GetPlayerNameByLoginNumber(playerNumber));
	}

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

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading) {
		ChatLogger::Log(L"Bye!");
	}

	LOG("Destroying GWToolbox++\n");

	Sleep(100);

	if (app->HasBeenInitialized()) {
		LOG("Disabling GUI\n");
		Application::InstancePtr()->Disable();
		LOG("Closing settings\n");
		main_window().settings_panel().Close();
		LOG("saving health log\n");
		party_damage().SaveIni();
	}
	Sleep(100);
	LOG("Deleting config\n");
	delete chat_commands_;
	Sleep(100);
	LOG("Restoring input hook\n");
	SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
	Sleep(100);
	LOG("Destroying API\n");
	GW::Api::Destruct();
	LOG("Destroying directX hook\n");
	delete dx_hooker;
	LOG("Destroying Config\n");
	Config::Destroy();
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
		Config::Save();
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	if (Application::Instance().HasBeenInitialized()) {
		MSG msg;
		msg.hwnd = hWnd;
		msg.message = Message;
		msg.wParam = wParam;
		msg.lParam = lParam;

		GWToolbox& tb = GWToolbox::instance();

		switch (Message) {
		// Send right mouse button events to gw (move view around) and don't mess with them
		case WM_RBUTTONDOWN: tb.right_mouse_pressed_ = true; break;
		case WM_RBUTTONUP: tb.right_mouse_pressed_ = false; break;

		// Send button up mouse events to both gw and osh, to avoid gw being stuck on mouse-down
		case WM_LBUTTONUP:
			input.ProcessMouseMessage(&msg);
			tb.minimap_->OnMouseUp(msg);
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
			switch (Message) {
			case WM_MOUSEMOVE: if (tb.minimap_->OnMouseMove(msg)) return true; break;
			case WM_LBUTTONDOWN: if (tb.minimap_->OnMouseDown(msg)) return true; break;
			case WM_MOUSEWHEEL: if (tb.minimap_->OnMouseWheel(msg)) return true; break;
			case WM_LBUTTONDBLCLK: if (tb.minimap_->OnMouseDblClick(msg)) return true; break;
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
		GWToolbox& tb = GWToolbox::instance();
		tb.main_window_ = new MainWindow();
		tb.main_window_->SetFont(app.GetDefaultFont());
		std::shared_ptr<MainWindow> shared_ptr = std::shared_ptr<MainWindow>(tb.main_window_);
		app.Run(shared_ptr);

		LOG("Creating timer\n");
		tb.timer_window_ = new TimerWindow();
		LOG("Creating bonds window\n");
		tb.bonds_window_ = new BondsWindow();
		LOG("Creating health window\n");
		tb.health_window_ = new HealthWindow();
		LOG("Creating distance window\n");
		tb.distance_window_ = new DistanceWindow();
		LOG("Creating party damage window\n");
		tb.party_damage_ = new PartyDamage();
		LOG("Creating Minimap\n");
		tb.minimap_ = new Minimap();
		LOG("Applying settings\n");
		tb.main_window().settings_panel().ApplySettings();
		LOG("Enabling app\n");
		app.Enable();
		tb.initialized_ = true;
		LOG("Gui Created\n");
		LOG("Saving theme\n");
		tb.SaveTheme();

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
	if (!tb.must_self_destruct_ && Application::Instance().HasBeenInitialized()) {
		
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

		tb.minimap_->Render(pDevice);

		renderer->BeginRendering();
		Application::InstancePtr()->Render();
		renderer->EndRendering();
	}

	GW::dx9::EndScene_t endscene_orig = dx_hooker->original<GW::dx9::EndScene_t>(GW::dx9::kEndScene);
	return endscene_orig(pDevice);
}

HRESULT WINAPI GWToolbox::resetScene(IDirect3DDevice9* pDevice, 
	D3DPRESENT_PARAMETERS* pPresentationParameters) {

	if (Application::Instance().HasBeenInitialized()) {
		// pre-reset here.
		renderer->PreD3DReset();

		HRESULT result = dx_hooker->original<GW::dx9::Reset_t>(GW::dx9::kReset)(pDevice, pPresentationParameters);
		if (result == D3D_OK) {
			// post-reset here.
			renderer->PostD3DReset();
		}

		return result;
	}

	GW::dx9::Reset_t reset_orig = dx_hooker->original<GW::dx9::Reset_t>(GW::dx9::kReset);
	return reset_orig(pDevice, pPresentationParameters);
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
