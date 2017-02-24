#include "GWToolbox.h"

#include "Defines.h"

#include <string>
#include <Windows.h>
#include <windowsx.h>

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

#include <imgui.h>
#include <imgui\examples\directx9_example\imgui_impl_dx9.h>

#include "Timer.h"
#include "MainWindow.h"
#include "TimerWindow.h"
#include "Settings.h"
#include "ChatLogger.h"
#include "logger.h"
#include "SettingManager.h"

GWToolbox* GWToolbox::instance_ = nullptr;
GW::DirectXHooker* GWToolbox::dx_hooker = nullptr;
long GWToolbox::OldWndProc = 0;


void GWToolbox::SafeThreadEntry(HMODULE dllmodule) {
	__try {
		GWToolbox::ThreadEntry(dllmodule);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
		LOG("SafeThreadEntry __except body\n");
	}
}

void GWToolbox::ThreadEntry(HMODULE dllmodule) {
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

	LOG("Installing dx hooks\n");
	dx_hooker->AddHook(GW::dx9::kEndScene, (void*)endScene);
	dx_hooker->AddHook(GW::dx9::kReset, (void*)resetScene);
	LOG("Installed dx hooks\n");

	LOG("Installing input event handler\n");
	HWND gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);
	LOG("oldwndproc %X\n", OldWndProc);
	LOG("Installed input event handler\n");

	Config::IniWrite("launcher", "dllversion", GWTOOLBOX_VERSION);

	ChatLogger::Init();

	while (instanceptr() == nullptr || !instance().must_self_destruct_) { // wait until destruction
		Sleep(10);

#ifdef _DEBUG
		if (GetAsyncKeyState(VK_END) & 1) {
			instance().must_self_destruct_ = true;
			break;
		}
#endif
	}

	LOG("Destroying GWToolbox++\n");

	Sleep(100);

	for (ToolboxModule* module : GWToolbox::instance().modules) {
		module->SaveSettings(Config::inifile_);
	}
	LOG("Closing settings\n");
	instance().main_window->settings_panel->Close();
	LOG("saving health log\n");
	
	Sleep(100);
	LOG("Deleting config\n");
	delete instance().chat_commands;
	Sleep(100);
	LOG("Restoring input hook\n");
	SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
	ImGui_ImplDX9_Shutdown();
	Sleep(100);
	LOG("Destroying API\n");
	GW::Api::Destruct();
	LOG("Destroying directX hook\n");
	delete dx_hooker;
	SettingManager::SaveAll();
	LOG("Destroying Config\n");
	Config::Destroy();
	LOG("Closing log/console, bye!\n");
	Logger::Close();
	Sleep(100);
	FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
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

	MSG msg;
	msg.hwnd = hWnd;
	msg.message = Message;
	msg.wParam = wParam;
	msg.lParam = lParam;

	GWToolbox& tb = GWToolbox::instance();

	ImGuiIO& io = ImGui::GetIO();
	switch (Message) {
	case WM_LBUTTONDOWN: io.MouseDown[0] = true; break;
	case WM_LBUTTONUP: io.MouseDown[0] = false; break;

	case WM_MBUTTONDOWN: 
		io.KeysDown[VK_MBUTTON] = true;
		io.MouseDown[2] = true; 
		break;
	case WM_MBUTTONUP: 
		io.KeysDown[VK_MBUTTON] = false;
		io.MouseDown[2] = false; 
		break;
	case WM_MOUSEWHEEL: io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f; break;
	case WM_MOUSEMOVE:
		io.MousePos.x = (float)GET_X_LPARAM(lParam);
		io.MousePos.y = (float)GET_Y_LPARAM(lParam);
		break;
	case WM_KEYDOWN:
		if (wParam < 256)
			io.KeysDown[wParam] = true;
		break;
	case WM_KEYUP:
		if (wParam < 256)
			io.KeysDown[wParam] = false;
		break;
	case WM_CHAR: // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		break;
	default:
		break;
	}

	switch (Message) {
	// Send right mouse button events to gw (move view around) and don't mess with them
	case WM_RBUTTONDOWN: tb.right_mouse_pressed_ = true; break;
	case WM_RBUTTONUP: tb.right_mouse_pressed_ = false; break;

	// Send button up mouse events to both gw and imgui, to avoid gw being stuck on mouse-down
	case WM_LBUTTONUP:
		tb.minimap->WndProc(Message, wParam, lParam); 
		//tb.minimap->OnMouseUp(msg);
		break;
		
	// Send other mouse events to imgui first and consume them if used
	case WM_LBUTTONDOWN:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
		if (tb.right_mouse_pressed_) break;
		if (tb.minimap->WndProc(Message, wParam, lParam)) return true;
		break;

	// send keyboard messages to gw, imgui and toolbox
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
		// send input to chat commands for camera movement
		if (tb.chat_commands->ProcessMessage(&msg)) {
			return true;
		}

		// send input to toolbox to trigger hotkeys
		tb.main_window->hotkey_panel->ProcessMessage(&msg);

		// block alt-enter if in borderless to avoid graphic glitches (no reason to go fullscreen anyway)
		if (tb.other_settings->borderless_window
			&& (GetAsyncKeyState(VK_MENU) < 0)
			&& (GetAsyncKeyState(VK_RETURN) < 0)) {
			return true;
		}

		break;

	case WM_SIZE:
		//tb.new_screen_size_ = SizeI(LOWORD(lParam), HIWORD(lParam));
		//if (tb.new_screen_size_ != tb.old_screen_size_) {
		//	tb.must_resize_ = true;
		//}
		break;

	default:
		break;
	}

	if (!tb.right_mouse_pressed_ && Message != WM_RBUTTONUP) {
		if (ImGui::GetIO().WantCaptureMouse) return true;
		if (ImGui::GetIO().WantCaptureKeyboard) return true;
		if (ImGui::GetIO().WantTextInput) return true;
	}
	
	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}

GWToolbox::GWToolbox(IDirect3DDevice9* device) : 
	must_self_destruct_(false),
	right_mouse_pressed_(false),
	must_resize_(false) {

	LOG("Creating GWToolbox\n");

	modules.push_back(main_window = new MainWindow(device));
	modules.push_back(chat_filter = new ChatFilter());
	modules.push_back(chat_commands = new ChatCommands());
	modules.push_back(other_settings = new OtherSettings());

	modules.push_back(timer_window = new TimerWindow());
	modules.push_back(health_window = new HealthWindow());
	modules.push_back(distance_window = new DistanceWindow());
	modules.push_back(minimap = new Minimap());
	modules.push_back(party_damage = new PartyDamage());
	modules.push_back(bonds_window = new BondsWindow(device));

	for (ToolboxModule* module : modules) {
		module->LoadSettings(Config::inifile_);
	}

	SettingManager::LoadAll();
	SettingManager::ApplyAll();

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents().GetAgentArray().valid()
		&& GW::Agents().GetPlayer() != nullptr) {

		DWORD playerNumber = GW::Agents().GetPlayer()->PlayerNumber;
		ChatLogger::Log("Hello %ls!", GW::Agents().GetPlayerNameByLoginNumber(playerNumber));
	}
}

// All rendering done here.
HRESULT WINAPI GWToolbox::endScene(IDirect3DDevice9* pDevice) {

	if (instanceptr() == nullptr) {

		instance_ = new GWToolbox(pDevice);

		ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), pDevice);
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		static std::string imgui_inifile = GuiUtils::getPath("interface.ini");
		io.IniFilename = imgui_inifile.c_str();
		GuiUtils::LoadFonts();
	}

	GWToolbox& tb = GWToolbox::instance();

	if (!tb.must_self_destruct_) {
		
		if (tb.must_resize_) {
			tb.must_resize_ = false;
			tb.ResizeUI();
			//tb.old_screen_size_ = tb.new_screen_size_;
		}

		//D3DVIEWPORT9 viewport;
		//pDevice->GetViewport(&viewport);

		//if (location.X < 0) {
		//	location.X = 0;
		//}
		//if (location.Y < 0) {
		//	location.Y = 0;
		//}
		//if (location.X > static_cast<int>(viewport.Width) - size.GetWidth()) {
		//	location.X = static_cast<int>(viewport.Width) - size.GetWidth();
		//}
		//if (location.Y > static_cast<int>(viewport.Height) - size.GetHeight()) {
		//	location.Y = static_cast<int>(viewport.Height) - size.GetHeight();
		//}
		//if (location != tb.main_window->GetLocation()) {
		//	tb.main_window->SetLocation(location);
		//}
		ImGui_ImplDX9_NewFrame();

		for (ToolboxModule* module : tb.modules) {
			module->Update();
		}

		for (ToolboxModule* module : tb.modules) {
			module->Draw(pDevice);
		}

		ImGui::ShowTestWindow();
		ImGui::Render();

		if (tb.must_self_destruct_) {
			if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading) {
				ChatLogger::Log("Bye!");
			}
		}
	}

	GW::dx9::EndScene_t endscene_orig = dx_hooker->original<GW::dx9::EndScene_t>(GW::dx9::kEndScene);
	return endscene_orig(pDevice);
}

HRESULT WINAPI GWToolbox::resetScene(IDirect3DDevice9* pDevice, 
	D3DPRESENT_PARAMETERS* pPresentationParameters) {

	ImGui_ImplDX9_InvalidateDeviceObjects();

	GW::dx9::Reset_t reset_orig = dx_hooker->original<GW::dx9::Reset_t>(GW::dx9::kReset);
	return reset_orig(pDevice, pPresentationParameters);
}

void GWToolbox::ResizeUI() {
	if (GWToolbox::instanceptr()) {
		//main_window->ResizeUI(old_screen_size_, new_screen_size_);
		//timer_window_->ResizeUI(old_screen_size_, new_screen_size_);
		//health_window_->ResizeUI(old_screen_size_, new_screen_size_);
		//distance_window_->ResizeUI(old_screen_size_, new_screen_size_);
		//party_damage->ResizeUI(old_screen_size_, new_screen_size_);
		//bonds_window->ResizeUI(old_screen_size_, new_screen_size_);
	}
}
