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

#include "ChatLogger.h"
#include "logger.h"

#include <OtherModules\ChatCommands.h>
#include <OtherModules\ChatFilter.h>
#include <OtherModules\GameSettings.h>
#include <OtherModules\ToolboxSettings.h>
#include <OtherModules\Resources.h>

#include <Windows\MainWindow.h>
#include <Windows\TimerWindow.h>
#include <Windows\BondsWindow.h>
#include <Windows\HealthWindow.h>
#include <Windows\DistanceWindow.h>
#include <Windows\PartyDamage.h>
#include <Windows\Minimap\Minimap.h>
#include <Windows\ClockWindow.h>
#include <Windows\NotePadWindow.h>

#include <Panels\HotkeyPanel.h>

GWToolbox* GWToolbox::instance_ = nullptr;
GW::DirectXHooker* GWToolbox::dx_hooker = nullptr;
long GWToolbox::OldWndProc = 0;

bool tb_initialized = false;
bool tb_destroyed = false;

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
		return;
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

	LOG("Installing dx hooks\n");
	dx_hooker->AddHook(GW::dx9::kEndScene, (void*)endScene);
	dx_hooker->AddHook(GW::dx9::kReset, (void*)resetScene);
	LOG("Installed dx hooks\n");

	LOG("Installing input event handler\n");
	HWND gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);
	LOG("oldwndproc %X\n", OldWndProc);
	LOG("Installed input event handler\n");

	ChatLogger::Init();

	while (!tb_destroyed) { // wait until destruction
		Sleep(100);

#ifdef _DEBUG
		if (GetAsyncKeyState(VK_END) & 1) {
			Instance().StartSelfDestruct();
			break;
		}
#endif
	}
	
	Sleep(100);
	LOG("Restoring input hook\n");
	SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
	Sleep(100);
	LOG("Destroying API\n");
	GW::Api::Destruct();
	LOG("Destroying directX hook\n");
	delete dx_hooker;
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
	static bool right_mouse_down = false;

	if (instance_ == nullptr) {
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	if (Message == WM_QUIT || Message == WM_CLOSE) {
		// todo: save
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	if (Message == WM_RBUTTONDOWN) right_mouse_down = true;
	if (Message == WM_RBUTTONDBLCLK) right_mouse_down = true;
	if (Message == WM_RBUTTONUP) right_mouse_down = false;

	// === Send events to ImGui ===
	ImGuiIO& io = ImGui::GetIO();
	if (!right_mouse_down) {
		switch (Message) {
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			io.MouseDown[0] = true; 
			break;
		case WM_LBUTTONUP: 
			io.MouseDown[0] = false; 
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
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
	}

	// === Send events to toolbox ===
	GWToolbox& tb = GWToolbox::Instance();
	switch (Message) {
	// Send button up mouse events to both gw and imgui, to avoid gw being stuck on mouse-down
	case WM_LBUTTONUP:
		tb.minimap->WndProc(Message, wParam, lParam); 
		break;
		
	// Send other mouse events to imgui first and consume them if used
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		if (!right_mouse_down) {
			if (tb.minimap->WndProc(Message, wParam, lParam)) return true;
			if (io.WantCaptureMouse) return true;
		}
		break;

	// send keyboard messages to gw, imgui and toolbox
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (io.WantTextInput) break; // send keyup events to gw
		// else fallthrough
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_IME_CHAR:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONDBLCLK:
	case WM_XBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
	case WM_MBUTTONUP:
		if (io.WantTextInput) return true;

		// send input to chat commands for camera movement
		if (tb.chat_commands->WndProc(Message, wParam, lParam)) {
			return true;
		}

		// send input to toolbox to trigger hotkeys
		tb.main_window->hotkey_panel->WndProc(Message, wParam, lParam);

		// block alt-enter if in borderless to avoid graphic glitches (no reason to go fullscreen anyway)
		if (tb.game_settings->borderless_window
			&& (GetAsyncKeyState(VK_MENU) < 0)
			&& (GetAsyncKeyState(VK_RETURN) < 0)) {
			return true;
		}
		break;

	case WM_SIZE:
		break;

	default:
		break;
	}
	
	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}

GWToolbox::GWToolbox(IDirect3DDevice9* device) : must_self_destruct(false) {
	LOG("Creating Toolbox\n");
	instance_ = this;

	LOG("Opening ini file\n");
	inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(GuiUtils::getPath("GWToolbox.ini").c_str());
	inifile->SetValue("launcher", "dllversion", GWTOOLBOX_VERSION);

	LOG("Creating Modules\n");
	modules.push_back(resources = new Resources());
	modules.push_back(game_settings = new GameSettings());
	modules.push_back(toolbox_settings = new ToolboxSettings());
	modules.push_back(main_window = new MainWindow());
	modules.push_back(chat_filter = new ChatFilter());
	modules.push_back(chat_commands = new ChatCommands());

	modules.push_back(timer_window = new TimerWindow());
	modules.push_back(health_window = new HealthWindow());
	modules.push_back(distance_window = new DistanceWindow());
	modules.push_back(minimap = new Minimap());
	modules.push_back(party_damage = new PartyDamage());
	modules.push_back(bonds_window = new BondsWindow());
	modules.push_back(clock_window = new ClockWindow());
	modules.push_back(notepad_window = new NotePadWindow());

	resources->EndLoading();

	for (ToolboxModule* module : modules) {
		module->LoadSettings(inifile);
	}

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents().GetAgentArray().valid()
		&& GW::Agents().GetPlayer() != nullptr) {

		DWORD playerNumber = GW::Agents().GetPlayer()->PlayerNumber;
		ChatLogger::Log("Hello %ls!", GW::Agents().GetPlayerNameByLoginNumber(playerNumber));
	}
}

GWToolbox::~GWToolbox() {
	for (ToolboxModule* module : modules) {
		module->SaveSettings(inifile);
	}

	for (ToolboxModule* module : modules) {
		delete module;
	}

	inifile->SaveFile(GuiUtils::getPath("GWToolbox.ini").c_str());
	inifile->Reset();
	delete inifile;

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading) {
		ChatLogger::Log("Bye!");
	}
}

// All rendering done here.
HRESULT WINAPI GWToolbox::endScene(IDirect3DDevice9* pDevice) {

	// === initialization ===
	if (!tb_initialized) {
		instance_ = new GWToolbox(pDevice);

		ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), pDevice);
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		static std::string imgui_inifile = GuiUtils::getPath("interface.ini");
		io.IniFilename = imgui_inifile.c_str();
		GuiUtils::LoadFonts();

		tb_initialized = true;
	}

	// === runtime ===
	if (instance_) {
		GWToolbox& tb = *instance_;

		ImGui_ImplDX9_NewFrame();

		for (ToolboxModule* module : tb.modules) {
			module->Update();
		}

		for (ToolboxModule* module : tb.modules) {
			module->Draw(pDevice);
		}

		//ImGui::ShowTestWindow();

		ImGui::Render();
	}

	// === destruction ===
	if (instance_ && instance_->must_self_destruct) {
		delete instance_;
		instance_ = nullptr;

		ImGui_ImplDX9_Shutdown();

		tb_destroyed = true;
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
