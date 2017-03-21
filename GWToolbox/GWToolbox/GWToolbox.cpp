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

namespace {
	long OldWndProc = 0;
	bool tb_initialized = false;
	bool tb_destroyed = false;

	bool drawing_world = 0;
	int drawing_passes = 0;
	int last_drawing_passes = 0;

	struct gwdx {
		char pad_0000[24]; //0x0000
		void* unk; //0x0018 might not be a func pointer, seems like it tho lol
		char pad_001C[44]; //0x001C
		wchar_t gpuname[32]; //0x0048
		char pad_0088[8]; //0x0088
		IDirect3DDevice9* device; //0x0090 IDirect3DDevice9*
		char pad_0094[12]; //0x0094
		unsigned int framecount; //0x00A0
		char pad_00A4[4048]; //0x00A4
	}; //Size: 0x1074

	typedef bool(__fastcall *GwEndScene_t)(gwdx* ctx, void* unk);
	typedef bool(__fastcall *GwReset_t)(gwdx* ctx);

	GW::THook<GwEndScene_t> endscene_hook;
	GW::THook<GwReset_t> reset_hook;

	GwEndScene_t original_func = nullptr;
}

HRESULT WINAPI Present(IDirect3DDevice9* pDev, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
HRESULT WINAPI EndScene(IDirect3DDevice9* dev);
HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

bool __fastcall GwEndScene(gwdx* ctx, void* unk) {
	static GwEndScene_t original = endscene_hook.Original();
	GWToolbox::Draw(ctx->device);
	if (endscene_hook.Valid()) {
		return original(ctx, unk);
	} else {
		return original_func(ctx, unk);
	}
}

bool __fastcall GwReset(gwdx* ctx) {
	ImGui_ImplDX9_InvalidateDeviceObjects();
	return reset_hook.Original()(ctx);
}

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
	if (!GW::Initialize()){
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
		FreeLibraryAndExitThread((HMODULE)dllmodule, EXIT_SUCCESS);
		return EXIT_SUCCESS;
	}

	//printf("DxDevice = %X\n", (DWORD)(GW::DirectXHooker::Initialize()));

	Log::Log("Installing dx hooks\n");
	//GW::DirectXHooker::AddHook(GW::dx9::kPresent, Present);
	//GW::DirectXHooker::AddHook(GW::dx9::kEndScene, EndScene);
	//GW::DirectXHooker::AddHook(GW::dx9::kReset, ResetScene);
	//GW::Render::DetourEndScene(GwEndScene);
	//GW::Render::DetourReset(GwReset);
	original_func = (GwEndScene_t)GW::Scanner::Find("\x55\x8B\xEC\x83\xEC\x28\x56\x8B\xF1\x57\x89\x55\xF8", "xxxxxxxxxxxxx", 0);
	printf("GW EndScene address = 0x%X\n", (DWORD)original_func);
	endscene_hook.Detour(original_func, GwEndScene);

	GwReset_t original_reset = (GwReset_t)GW::Scanner::Find("\x55\x8B\xEC\x81\xEC\x98\x00\x00\x00\x53\x56\x57\x8B\xF1\x33\xD2", "xxxxxxxxxxxxxxxx", 0);
	printf("GW Reset address = 0x%X\n", (DWORD)original_reset);
	reset_hook.Detour(original_reset, GwReset);

	Log::Log("Installed dx hooks\n");

	Log::Log("oldwndproc %X\n", OldWndProc);
	Log::Log("Installed input event handler\n");

	Log::InitializeChat();

	Log::Log("Installed chat hooks\n");

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

	//GW::Render::EndSceneHook()->Cleanup();
	//GW::Render::ResetHook()->Cleanup();


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
	Resources::Instance().Initialize();
	GameSettings::Instance().Initialize();
	ToolboxSettings::Instance().Initialize();
	ChatFilter::Instance().Initialize();
	ChatCommands::Instance().Initialize();
	ToolboxTheme::Instance().Initialize();

	MainWindow::Instance().Initialize();
	PconPanel::Instance().Initialize();
	HotkeyPanel::Instance().Initialize();
	BuildPanel::Instance().Initialize();
	TravelPanel::Instance().Initialize();
	DialogPanel::Instance().Initialize();
	InfoPanel::Instance().Initialize();
	MaterialsPanel::Instance().Initialize();
	SettingsPanel::Instance().Initialize();

	TimerWindow::Instance().Initialize();
	HealthWindow::Instance().Initialize();
	DistanceWindow::Instance().Initialize();
	Minimap::Instance().Initialize();
	PartyDamage::Instance().Initialize();
	BondsWindow::Instance().Initialize();
	ClockWindow::Instance().Initialize();
	NotePadWindow::Instance().Initialize();

	LoadSettings();

	Resources::Instance().EndLoading();

	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents::GetAgentArray().valid()
		&& GW::Agents::GetPlayer() != nullptr) {

		DWORD playerNumber = GW::Agents::GetPlayer()->PlayerNumber;
		Log::Info("Hello %ls!", GW::Agents::GetPlayerNameByLoginNumber(playerNumber));
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

	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
		Log::Info("Bye!");
	}
}

HRESULT WINAPI EndScene(IDirect3DDevice9* device) {
	printf("[%d] EndScene\n", clock());

	return GW::DirectXHooker::Original<GW::dx9::EndScene_t>(GW::dx9::kEndScene)(device);
}

HRESULT WINAPI Present(IDirect3DDevice9* pDev, 
	CONST RECT* pSourceRect, 
	CONST RECT* pDestRect, 
	HWND hDestWindowOverride, 
	CONST RGNDATA* pDirtyRegion) {

	printf("[%d] Present\n", clock());

	return GW::DirectXHooker::Original<GW::dx9::Present_t>(GW::dx9::kPresent)(pDev,
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
		GW::Terminate();

		Log::Log("Restoring input hook\n");
		SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)OldWndProc);
		Log::Log("Destroying directX hook\n");
		endscene_hook.Retour();
		reset_hook.Retour();
	}
}

HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice,
	D3DPRESENT_PARAMETERS* pPresentationParameters) {

	drawing_world = true;

	ImGui_ImplDX9_InvalidateDeviceObjects();

	return GW::DirectXHooker::Original<GW::dx9::Reset_t>(
		GW::dx9::kReset)(pDevice, pPresentationParameters);
}
