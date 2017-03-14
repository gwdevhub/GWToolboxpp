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

#include "logger.h"

#include <OtherModules\Resources.h>
#include <OtherModules\ChatCommands.h>
#include <OtherModules\ChatFilter.h>
#include <OtherModules\GameSettings.h>
#include <OtherModules\ToolboxSettings.h>
#include <OtherModules\ToolboxTheme.h>

#include <Windows\MainWindow.h>
#include <Windows\TimerWindow.h>
#include <Windows\BondsWindow.h>
#include <Windows\HealthWindow.h>
#include <Windows\DistanceWindow.h>
#include <Windows\PartyDamage.h>
#include <Windows\Minimap\Minimap.h>
#include <Windows\ClockWindow.h>
#include <Windows\NotePadWindow.h>

#include "Panels\PconPanel.h"
#include "Panels\HotkeyPanel.h"
#include "Panels\TravelPanel.h"
#include "Panels\BuildPanel.h"
#include "Panels\DialogPanel.h"
#include "Panels\InfoPanel.h"
#include "Panels\MaterialsPanel.h"
#include "Panels\SettingsPanel.h"

long OldWndProc = 0;
bool tb_initialized = false;
bool tb_destroyed = false;
GW::dx9::EndScene_t endscene_orig = nullptr;

DWORD __stdcall SafeThreadEntry(LPVOID dllmodule) {
	__try {
		return ThreadEntry(dllmodule);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
		Log::Log("SafeThreadEntry __except body\n");
		return EXIT_SUCCESS;
	}
}

DWORD __stdcall ThreadEntry(LPVOID dllmodule) {
	Log::Log("Initializing API\n");
	if (!GW::Api::Initialize()){
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
		FreeLibraryAndExitThread((HMODULE)dllmodule, EXIT_SUCCESS);
		return EXIT_SUCCESS;
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

	printf("DxDevice = %X\n", (unsigned int)(GW::DirectXHooker::Initialize()));

	Log::Log("Installing dx hooks\n");
	GW::DirectXHooker::Instance().AddHook(GW::dx9::kPresent, (void*)Present);
	GW::DirectXHooker::Instance().AddHook(GW::dx9::kReset, (void*)ResetScene);
	Log::Log("Installed dx hooks\n");

	Log::Log("oldwndproc %X\n", OldWndProc);
	Log::Log("Installed input event handler\n");

	Log::InitializeChat();

	while (!tb_destroyed) { // wait until destruction
		Sleep(100);

#ifdef _DEBUG
		if (GetAsyncKeyState(VK_END) & 1) {
			GWToolbox::Instance().StartSelfDestruct();
			break;
		}
#endif
	}
	
	Sleep(100);

	Sleep(100);
	Log::Log("Closing log/console, bye!\n");
	Log::Terminate();
	Sleep(100);
	FreeLibraryAndExitThread((HMODULE)dllmodule, EXIT_SUCCESS);
}

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	__try {
		return WndProc(hWnd, Message, wParam, lParam);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	static bool right_mouse_down = false;

	if (!tb_initialized || tb_destroyed) {
		return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
	}

	if (Message == WM_QUIT || Message == WM_CLOSE) {
		GWToolbox::Instance().SaveSettings();
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
		Minimap::Instance().WndProc(Message, wParam, lParam); 
		break;
		
	// Send other mouse events to imgui first and consume them if used
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		if (!right_mouse_down) {
			if (io.WantCaptureMouse) return true;
			if (Minimap::Instance().WndProc(Message, wParam, lParam)) return true;
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
		if (ChatCommands::Instance().WndProc(Message, wParam, lParam)) {
			return true;
		}

		// send input to toolbox to trigger hotkeys
		HotkeyPanel::Instance().WndProc(Message, wParam, lParam);

		// block alt-enter if in borderless to avoid graphic glitches (no reason to go fullscreen anyway)
		if (GameSettings::Instance().borderless_window
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

void GWToolbox::Initialize() {
	Log::Log("Creating Toolbox\n");

	Log::Log("Creating Modules\n");
	modules.push_back(&Resources::Instance());
	modules.push_back(&GameSettings::Instance());
	modules.push_back(&ToolboxSettings::Instance());
	modules.push_back(&ChatFilter::Instance());
	modules.push_back(&ChatCommands::Instance());
	modules.push_back(&ToolboxTheme::Instance());

	modules.push_back(&MainWindow::Instance());
	modules.push_back(&PconPanel::Instance());
	modules.push_back(&HotkeyPanel::Instance());
	modules.push_back(&BuildPanel::Instance());
	modules.push_back(&TravelPanel::Instance());
	modules.push_back(&DialogPanel::Instance());
	modules.push_back(&InfoPanel::Instance());
	modules.push_back(&MaterialsPanel::Instance());
	modules.push_back(&SettingsPanel::Instance());

	modules.push_back(&TimerWindow::Instance());
	modules.push_back(&HealthWindow::Instance());
	modules.push_back(&DistanceWindow::Instance());
	modules.push_back(&Minimap::Instance());
	modules.push_back(&PartyDamage::Instance());
	modules.push_back(&BondsWindow::Instance());
	modules.push_back(&ClockWindow::Instance());
	modules.push_back(&NotePadWindow::Instance());

	for (ToolboxModule* module : modules) {
		module->Initialize();
	}

	LoadSettings();

	Resources::Instance().EndLoading();

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents().GetAgentArray().valid()
		&& GW::Agents().GetPlayer() != nullptr) {

		DWORD playerNumber = GW::Agents().GetPlayer()->PlayerNumber;
		Log::Info("Hello %ls!", GW::Agents().GetPlayerNameByLoginNumber(playerNumber));
	}
}

void GWToolbox::LoadSettings() {
	Log::Log("Opening ini file\n");
	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(GuiUtils::getPath("GWToolbox.ini").c_str());
	inifile->SetValue("launcher", "dllversion", GWTOOLBOX_VERSION);

	for (ToolboxModule* module : modules) {
		module->LoadSettings(inifile);
	}
}

void GWToolbox::SaveSettings() {
	for (ToolboxModule* module : modules) {
		module->SaveSettings(inifile);
	}
	if (inifile) inifile->SaveFile(GuiUtils::getPath("GWToolbox.ini").c_str());
}

void GWToolbox::Terminate() {
	SaveSettings();
	inifile->Reset();
	delete inifile;

	for (ToolboxModule* module : modules) {
		module->Terminate();
	}

	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading) {
		Log::Info("Bye!");
	}
}

HRESULT WINAPI Present(IDirect3DDevice9* pDev, 
	CONST RECT* pSourceRect, 
	CONST RECT* pDestRect, 
	HWND hDestWindowOverride, 
	CONST RGNDATA* pDirtyRegion) {

	if (!tb_destroyed) {
		__try {
			GWToolbox::Draw(pDev);
		} __except (EXCEPT_EXPRESSION_LOOP) {
			Log::Log("Badness happened in EndScene!\n");
		}
	}

	return GW::DirectXHooker::Instance().original<GW::dx9::Present_t>(GW::dx9::kPresent)(pDev,
		pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

void GWToolbox::Draw(IDirect3DDevice9* device) {

	static HWND gw_window_handle = 0;

	// === initialization ===
	if (!tb_initialized && !GWToolbox::Instance().must_self_destruct) {

		gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
		OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);

		ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), device);
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		static std::string imgui_inifile = GuiUtils::getPath("interface.ini");
		io.IniFilename = imgui_inifile.c_str();
		GuiUtils::LoadFonts();

		GWToolbox::Instance().Initialize();

		tb_initialized = true;
	}

	// === runtime ===
	if (tb_initialized && !GWToolbox::Instance().must_self_destruct) {
		ImGui_ImplDX9_NewFrame();

		for (ToolboxModule* module : GWToolbox::Instance().modules) {
			module->Update();
		}

		for (ToolboxModule* module : GWToolbox::Instance().modules) {
			module->Draw(device);
		}

		//ImGui::ShowTestWindow();

		ImGui::Render();
	}

	// === destruction ===
	if (tb_initialized && GWToolbox::Instance().must_self_destruct) {
		tb_destroyed = true;

		GWToolbox::Instance().Terminate();

		ImGui_ImplDX9_Shutdown();

		Log::Log("Destroying API\n");
		GW::Api::Destruct();

		Log::Log("Restoring input hook\n");
		SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
		Log::Log("Destroying directX hook\n");
		GW::DirectXHooker::Instance().RemoveAllHooks();
	}
}

HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice,
	D3DPRESENT_PARAMETERS* pPresentationParameters) {

	ImGui_ImplDX9_InvalidateDeviceObjects();

	return GW::DirectXHooker::Instance().original<GW::dx9::Reset_t>(
		GW::dx9::kReset)(pDevice, pPresentationParameters);
}
