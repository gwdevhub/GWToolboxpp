#include "GWToolbox.h"

#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"
#include "../include/OSHGui/Drawing/Theme.hpp"
#include "../include/OSHGui/Input/WindowsMessage.hpp"

#include <string>

#include "TBMainWindow.h"

using namespace OSHGui::Drawing;
using namespace OSHGui::Input;

GWToolbox* GWToolbox::instance = NULL;

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
		GWToolbox::getInstance()->config->save();
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
		case WM_MOUSEWHEEL:
		case WM_MBUTTONDOWN:
			if (input.ProcessMessage(&msg)) {
				//LOG("consumed mouse event %d\n", Message);
				return TRUE;
			}
			break;

		// send keyboard messages to gw, osh and toolbox (does osh need them?)
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
		case WM_IME_CHAR:
			//LOG("processing keyboard event %d, key %u\n", Message, wParam);
			input.ProcessMessage(&msg);
			GWToolbox::getInstance()->hotkeyMgr->processMessage(&msg);
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

	string path = GWToolbox::getInstance()->config->getPathA("DefaultTheme.txt");
	try {
		Theme theme;
		theme.Load(path);
		app->SetTheme(theme);
		LOG("Loaded theme file %s\n", path.c_str());
	} catch (Misc::InvalidThemeException e) {
		ERR("WARNING Could not load theme file %s\n", path.c_str());
	}
	
	FontPtr font = FontManager::LoadFont("Arial", 8.0f, false); //Arial, 8PT, no anti-aliasing
	app->SetDefaultFont(font);
	app->SetCursorEnabled(false);

	std::shared_ptr<TBMainWindow> mainWindow = std::make_shared<TBMainWindow>();
	app->Run(mainWindow);
	
	GWToolbox * tb = GWToolbox::getInstance();
	tb->pcons->buildUI();
	tb->builds->buildUI();
	tb->hotkeys->buildUI();

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

	pcons->loadIni();
	builds->loadIni();
	hotkeys->loadIni();

	LOG("Installing dx hooks\n");
	dx->CreateRenderHooks(endScene, resetScene);
	
	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

	m_Active = true;

	Application * app = Application::InstancePtr();
	while (true) { // main loop
		if (app->HasBeenInitialized()) {
			pcons->mainRoutine();
			builds->mainRoutine();
			hotkeys->mainRoutine();
		}
		Sleep(10);

		if (GetAsyncKeyState(VK_END) & 1)
			destroy();
	}
}

void GWToolbox::destroy()
{
	LOG("Destroying GWToolbox++\n");
	delete pcons;
	delete builds;
	delete hotkeys;

	config->save();
	delete config;
	delete hotkeyMgr;

	HWND hWnd = GWAPI::MemoryMgr::GetGWWindowHandle();
	SetWindowLongPtr(hWnd, GWL_WNDPROC, (long)OldWndProc);
	
	GWAPI::GWAPIMgr::Destruct();
#if DEBUG_BUILD
	FreeConsole();
#endif
	m_Active = false;
	FreeLibraryAndExitThread(m_dllmodule, EXIT_SUCCESS);
}

// what is this for?
bool GWToolbox::isActive() {
	return m_Active;
}


void GWToolbox::threadEntry(HMODULE mod) {
	if (instance) return;

	LOG("Initializing GWAPI\n");
	GWAPI::GWAPIMgr::Initialize();

	LOG("Creating GWToolbox++\n");
	instance = new GWToolbox(mod);

	LOG("Running GWToolbox++\n");
	instance->exec();
}
