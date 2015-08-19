#include "GWToolbox.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"
#include "../include/OSHGui/Drawing/Theme.hpp"
#include "../include/OSHGui/Input/WindowsMessage.hpp"

#include <string>

using namespace OSHGui::Drawing;
using namespace OSHGui::Input;

GWToolbox* GWToolbox::instance = NULL;

namespace{
	LPD3DXFONT dbgFont = NULL;
	RECT Type1Rect = { 50, 50, 300, 14 };
	GWAPI::GWAPIMgr * mgr;
	GWAPI::DirectXMgr * dx;

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
				LOG("consumed mouse event %d\n", Message);
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
			LOG("processing keyboard event %d, key %u\n", Message, wParam);
			input.ProcessMessage(&msg);
			GWToolbox::getInstance()->hotkeyMgr->processMessage(&msg);
			break;
		}
	}

	return CallWindowProc((WNDPROC)OldWndProc, hWnd, Message, wParam, lParam);
}


void create_gui(IDirect3DDevice9* pDevice) {
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(new Direct3D9Renderer(pDevice)));

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
	
	auto font = FontManager::LoadFont("Arial", 8.0f, false); //Arial, 8PT, no anti-aliasing
	app->SetDefaultFont(font);
	app->SetCursorEnabled(false);

	auto form = std::make_shared<Form>();
	form->SetText("GWToolbox++");
	form->SetSize(100, 300);
	
	Button * pcons = new Button();
	pcons->SetText("Pcons");
	pcons->SetBounds(0, 0, 100, 30);
	pcons->GetClickEvent() += ClickEventHandler([pcons](Control*) {
		LOG("Clicked on pcons!\n");
	});
	
	form->AddControl(pcons);

	app->Run(form);
	app->Enable();

	GWToolbox * tb = GWToolbox::getInstance();
	tb->pcons->buildUI();
	tb->builds->buildUI();
	tb->hotkeys->buildUI();

	HWND hWnd = GWAPI::MemoryMgr::GetGWWindowHandle();
	OldWndProc = SetWindowLongPtr(hWnd, GWL_WNDPROC, (long)NewWndProc);
}

// All rendering done here.
static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice) {
	static bool init = false;
	if (!init) {
		init = true;
		create_gui(pDevice);
	}

	auto &renderer = Application::Instance().GetRenderer();

	renderer.BeginRendering();

	Application::Instance().Render();

	renderer.EndRendering();

	if (!dbgFont)
		D3DXCreateFontA(pDevice, 14, 0, FW_BOLD, 0, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &dbgFont);


	//dbgFont->DrawTextA(NULL, "AHJHJHJHJHJHJHJHJHJ", strlen("AHJHJHJHJHJHJHJHJHJ") + 1, &Type1Rect, DT_NOCLIP, 0xFF00FF00);

	return  dx->GetEndsceneReturn()(pDevice);
}

static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {

	// pre-reset here.

	if (dbgFont) dbgFont->OnLostDevice();
	HRESULT result = dx->GetResetReturn()(pDevice, pPresentationParameters);
	if (result == D3D_OK){
		if (dbgFont) dbgFont->OnResetDevice();// post-reset here.
	}

	return result;
}


void GWToolbox::exec() {
	mgr = GWAPI::GWAPIMgr::GetInstance();
	dx = mgr->DirectX;
	

	pcons->loadIni();
	builds->loadIni();
	hotkeys->loadIni();

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
	
	delete pcons;
	delete builds;
	delete hotkeys;

	config->save();
	delete config;
	delete hotkeyMgr;

	HWND hWnd = GWAPI::MemoryMgr::GetGWWindowHandle();
	SetWindowLongPtr(hWnd, GWL_WNDPROC, (long)OldWndProc);
	
	GWAPI::GWAPIMgr::Destruct();
	m_Active = false;
	ExitThread(EXIT_SUCCESS);
}

// what is this for?
bool GWToolbox::isActive() {
	return m_Active;
}


void GWToolbox::threadEntry(HMODULE mod) {
	if (instance) return;

	GWAPI::GWAPIMgr::Initialize();

	instance = new GWToolbox(mod);
	instance->exec();
}
