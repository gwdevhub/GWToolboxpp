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
#include <GWCA\Managers\Render.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>

#include "logger.h"
#include "CursorFix.h"

#include <Modules\Resources.h>
#include <Modules\ChatCommands.h>
#include <Modules\ChatFilter.h>
#include <Modules\GameSettings.h>
#include <Modules\ToolboxSettings.h>
#include <Modules\ToolboxTheme.h>
#ifdef ENABLE_LUA
#  include <Modules\LUAInterface.h>
#endif
#include <Modules\Updater.h>

#include <Widgets\Minimap\Minimap.h>

#include "GuiUtils.h"

namespace {
	HMODULE dllmodule = 0;

	long OldWndProc = 0;
	bool tb_initialized = false;
	bool tb_destroyed = false;

	bool drawing_world = 0;
	int drawing_passes = 0;
	int last_drawing_passes = 0;
}

HMODULE GWToolbox::GetDLLModule() {
	return dllmodule;
}

DWORD __stdcall SafeThreadEntry(LPVOID module) {
	dllmodule = (HMODULE)module;
	__try {
		return ThreadEntry(nullptr);
	} __except ( EXCEPT_EXPRESSION_ENTRY ) {
		Log::Log("SafeThreadEntry __except body\n");
		return EXIT_SUCCESS;
	}
}

DWORD __stdcall ThreadEntry(LPVOID) {
	Log::Log("Initializing API\n");
	if (!GW::Initialize()){
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
		FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
		return EXIT_SUCCESS;
	}


	Log::Log("Installing Cursor Fix\n");

	InstallCursorFix();

	Log::Log("Installing dx hooks\n");
	GW::Render::SetRenderCallback([](IDirect3DDevice9* device) {
		GWToolbox::Instance().Draw(device);
	});
	GW::Render::SetResetCallback([](IDirect3DDevice9* device) {
		ImGui_ImplDX9_InvalidateDeviceObjects();
	});

	Log::Log("Installed dx hooks\n");

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
	Log::Log("Removing Cursor Fix\n");
	UninstallCursorFix();
	Sleep(100);
	Log::Log("Closing log/console, bye!\n");
	Log::Terminate();
	Sleep(100);
	FreeLibraryAndExitThread(dllmodule, EXIT_SUCCESS);
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

	GWToolbox::Instance().right_mouse_down = right_mouse_down;

	// === Send events to ImGui ===
	ImGuiIO& io = ImGui::GetIO();

	switch (Message) {
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if (!right_mouse_down) io.MouseDown[0] = true;
		break;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false; 
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		if (!right_mouse_down) {
			io.KeysDown[VK_MBUTTON] = true;
			io.MouseDown[2] = true;
		}
		break;
	case WM_MBUTTONUP:
		io.KeysDown[VK_MBUTTON] = false;
		io.MouseDown[2] = false;
		break;
	case WM_MOUSEWHEEL: 
		if (!right_mouse_down) io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f; 
		break;
	case WM_MOUSEMOVE:
		if (!right_mouse_down) {
			io.MousePos.x = (float)GET_X_LPARAM(lParam);
			io.MousePos.y = (float)GET_Y_LPARAM(lParam);
		}
		break;
	case WM_XBUTTONDOWN:
		if (!right_mouse_down) {
			if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) io.KeysDown[VK_XBUTTON1] = true;
			if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) io.KeysDown[VK_XBUTTON2] = true;
		}
		break;
	case WM_XBUTTONUP:
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) io.KeysDown[VK_XBUTTON1] = false;
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) io.KeysDown[VK_XBUTTON2] = false;
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

	// === Send events to toolbox ===
	GWToolbox& tb = GWToolbox::Instance();
	switch (Message) {
	// Send button up mouse events to everything, to avoid being stuck on mouse-down
	case WM_LBUTTONUP:
		for (ToolboxModule* m : tb.GetModules()) {
			m->WndProc(Message, wParam, lParam);
		}
		break;
		
	// Other mouse events:
	// - If right mouse down, leave it to gw
	// - ImGui first (above), if WantCaptureMouse that's it
	// - Toolbox module second (e.g.: minimap), if captured, that's it
	// - otherwise pass to gw
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		if (!right_mouse_down) {
			if (io.WantCaptureMouse) return true;
			bool captured = false;
			for (ToolboxModule* m : tb.GetModules()) {
				if (m->WndProc(Message, wParam, lParam)) captured = true;
			}
			if (captured) return true;
		}
		break;

	// keyboard messages
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (io.WantTextInput) break; // if imgui wants them, send to imgui (above) and to gw
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
		if (io.WantTextInput) return true; // if imgui wants them, send just to imgui (above)

		// send input to chat commands for camera movement
		if (ChatCommands::Instance().WndProc(Message, wParam, lParam)) {
			return true;
		}

		// send to toolbox modules
		for (ToolboxModule* m : tb.GetModules()) {
			m->WndProc(Message, wParam, lParam);
		}
		// note: capturing those events would prevent typing if you have a hotkey assigned to normal letters. 
		// We may want to not send events to toolbox if the player is typing in-game
		// Otherwise, we may want to capture events. 
		// For that, we may want to only capture *successfull* hotkey activations.

		// block alt-enter if in borderless to avoid graphic glitches (no reason to go fullscreen anyway)
		if (GameSettings::Instance().borderlesswindow
			&& (GetAsyncKeyState(VK_MENU) < 0)
			&& (GetAsyncKeyState(VK_RETURN) < 0)) {
			return true;
		}
		break;

	case WM_SIZE:
		// ImGui doesn't need this, it reads the viewport size directly
		break;

	default:
		break;
	}
	
	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}

void GWToolbox::Initialize() {
	Log::Log("Creating Toolbox\n");
	Resources::Instance().EnsureFolderExists(Resources::GetPath("img"));
	Resources::Instance().EnsureFolderExists(Resources::GetPath("img\\bonds"));
	Resources::Instance().EnsureFolderExists(Resources::GetPath("img\\icons"));
	Resources::Instance().EnsureFolderExists(Resources::GetPath("img\\materials"));
	Resources::Instance().EnsureFolderExists(Resources::GetPath("img\\pcons"));
	Resources::Instance().EnsureFolderExists(Resources::GetPath("location logs"));
	Resources::Instance().EnsureFileExists(Resources::GetPath("GWToolbox.ini"), 
		"https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/GWToolbox.ini", 
		[](bool success) {
		if (success) {
			GWToolbox::Instance().OpenSettingsFile();
			GWToolbox::Instance().LoadModuleSettings();
		}
	});
	// if the file does not exist we'll load module settings once downloaded, but we need the file open
	// in order to read defaults
	OpenSettingsFile();
	Resources::Instance().EnsureFileExists(Resources::GetPath("Markers.ini"), 
		"https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/Markers.ini", 
		[](bool success) {
		Minimap::Instance().custom_renderer.LoadMarkers();
	});

	Log::Log("Creating Modules\n");
	std::vector<ToolboxModule*> core_modules;
	core_modules.push_back(&Resources::Instance());
	core_modules.push_back(&Updater::Instance());
#ifdef ENABLE_LUA
	core_modules.push_back(&LUAInterface::Instance());
#endif
	core_modules.push_back(&GameSettings::Instance());
	core_modules.push_back(&ToolboxSettings::Instance());
	core_modules.push_back(&ChatFilter::Instance());
	core_modules.push_back(&ChatCommands::Instance());
	core_modules.push_back(&ToolboxTheme::Instance());

	for (ToolboxModule* core : core_modules) {
		core->Initialize();
	}
	LoadModuleSettings(); // This will only read settings of the core modules (specified above)

	ToolboxSettings::Instance().InitializeModules(); // initialize all other modules as specified by the user

	// Only read settings of non-core modules
	for (ToolboxModule* module : modules) {
		bool is_core = false;
		for (ToolboxModule* core : core_modules) {
			if (module == core) is_core = true;
		}
		if (!is_core) module->LoadSettings(inifile);
	}

	Updater::Instance().CheckForUpdate();

	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents::GetAgentArray().valid()
		&& GW::Agents::GetPlayer() != nullptr) {

		DWORD playerNumber = GW::Agents::GetPlayer()->PlayerNumber;
		Log::Info("Hello %ls!", GW::Agents::GetPlayerNameByLoginNumber(playerNumber));
	}
}

void GWToolbox::OpenSettingsFile() {
	Log::Log("Opening ini file\n");
	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath("GWToolbox.ini").c_str());
	inifile->SetValue("launcher", "dllversion", GWTOOLBOX_VERSION);
}
void GWToolbox::LoadModuleSettings() {
	for (ToolboxModule* module : modules) {
		module->LoadSettings(inifile);
	}
}

void GWToolbox::SaveSettings() {
	for (ToolboxModule* module : modules) {
		module->SaveSettings(inifile);
	}
	if (inifile) inifile->SaveFile(Resources::GetPath("GWToolbox.ini").c_str());
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

void GWToolbox::Draw(IDirect3DDevice9* device) {

	static HWND gw_window_handle = 0;
	static DWORD last_tick_count;

	// === initialization ===
	if (!tb_initialized && !GWToolbox::Instance().must_self_destruct) {

		Log::Log("installing event handler\n");
		gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
		OldWndProc = SetWindowLongPtr(gw_window_handle, GWL_WNDPROC, (long)SafeWndProc);
		Log::Log("Installed input event handler, oldwndproc = 0x%X\n", OldWndProc);

		ImGui_ImplDX9_Init(GW::MemoryMgr().GetGWWindowHandle(), device);
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		static std::string imgui_inifile = Resources::GetPath("interface.ini");
		io.IniFilename = imgui_inifile.c_str();

		Resources::Instance().EnsureFileExists(Resources::GetPath("Font.ttf"),
			"https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/Font.ttf", 
			[](bool success) {
			if (success) {
				GuiUtils::LoadFonts();
			} else {
				Log::Error("Cannot load font!");
			}
		});

		GWToolbox::Instance().Initialize();

		last_tick_count = GetTickCount();
		tb_initialized = true;
	}

	// === runtime ===
	if (tb_initialized 
		&& !GWToolbox::Instance().must_self_destruct
		&& GW::Render::GetViewportWidth() > 0
		&& GW::Render::GetViewportHeight() > 0) {

		ImGui_ImplDX9_NewFrame(GW::Render::GetViewportWidth(), GW::Render::GetViewportHeight());

		// Improve precision with QueryPerformanceCounter
		DWORD tick = GetTickCount();
		DWORD delta = tick - last_tick_count;
		float delta_f = delta / 1000.f;

		for (ToolboxModule* module : GWToolbox::Instance().modules) {
			module->Update(delta_f);
		}
		last_tick_count = tick;

		Resources::Instance().DxUpdate(device);

		for (ToolboxUIElement* uielement : GWToolbox::Instance().uielements) {
			uielement->Draw(device);
		}

#ifdef _DEBUG
		// Feel free to uncomment to play with ImGui's features
		//ImGui::ShowDemoWindow();
		//ImGui::ShowStyleEditor(); // Warning, this WILL change your theme. Back up theme.ini first!
#endif

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
	}
}
