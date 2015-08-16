#include "GWToolbox.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"
#include "../include/OSHGui/Drawing/Theme.hpp"
#include "../include/OSHGui/Input/WindowsMessage.hpp"

#include <iostream>

using namespace OSHGui::Drawing;
using namespace OSHGui::Input;

namespace{
	LPD3DXFONT dbgFont = NULL;
	RECT Type1Rect = { 50, 50, 300, 14 };
	GWAPI::GWAPIMgr * mgr;
	GWAPI::DirectXMgr * dx;

	HHOOK oshinputhook;
	WindowsMessage input;
}

LRESULT CALLBACK MessageHook(int code, WPARAM wParam, LPARAM lParam) {

	// do nothing if application is not initialized or we got a message that we shouldn't handle
	if (!Application::InstancePtr()->HasBeenInitialized()
		|| (lParam & 0x80000000)
		|| (lParam & 0x40000000)
		|| (code != HC_ACTION)) {
		return CallNextHookEx(NULL, code, wParam, lParam);
	}

		LPMSG msg = (LPMSG)lParam;

		switch (msg->message) {
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			break;

		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			if (input.ProcessMessage(msg)) {
				//std::cout << "consumed mouse event " << msg->message << '\n';
				return TRUE;
			}
			break;

			// send keyboard messages to both gw and toolbox
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
		case WM_IME_CHAR:
			//std::cout << "processing keyboard event " << msg->message << '\n';
			input.ProcessMessage(msg);
			break;
		}
	return CallNextHookEx(NULL, code, wParam, lParam);
}


void create_gui(IDirect3DDevice9* pDevice) {
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(new Direct3D9Renderer(pDevice)));

	Application * app = Application::InstancePtr();

	Theme theme;
	try {
		theme.Load("DefaultTheme.txt"); // TODO: use a use local path or standard path instead
		app->SetTheme(theme);
	} catch (Misc::InvalidThemeException e) {
	}
	
	auto font = FontManager::LoadFont("Arial", 8.0f, false); //Arial, 8PT, no anti-aliasing
	app->SetDefaultFont(font);
	app->SetCursorEnabled(false);

	auto form = std::make_shared<Form>();
	form->SetText("GWToolbox++");
	form->SetSize(100, 300);

	app->Run(form);
	app->Enable();

	app->RegisterHotkey(Hotkey(Key::Insert, [] {
		Application::InstancePtr()->Toggle();
		//std::cout << "hotkey fired! \n";
	}));

	oshinputhook = SetWindowsHookEx(WH_GETMESSAGE, MessageHook, NULL, GetCurrentThreadId());
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
	GWAPI::GWAPIMgr::Initialize();

	mgr = GWAPI::GWAPIMgr::GetInstance();
	dx = mgr->DirectX;

	dx->CreateRenderHooks(endScene, resetScene);
	
	input.SetKeyboardInputEnabled(true);
	input.SetMouseInputEnabled(true);

	m_Active = true;

	Application * app = Application::InstancePtr();
	while (true) { // main loop
		if (app->HasBeenInitialized()) {
		}
		Sleep(10);

		if (GetAsyncKeyState(VK_END) & 1)
			destroy();
	}
}

void GWToolbox::destroy()
{
	//UnhookWindowsHookEx(oshinputhook);
	GWAPI::GWAPIMgr::Destruct();
	m_Active = false;
	ExitThread(EXIT_SUCCESS);
}

// what is this for?
bool GWToolbox::isActive() 
{
	return m_Active;
}


void GWToolbox::threadEntry(HMODULE mod) {
	GWToolbox * tb = new GWToolbox(mod);
	tb->exec();
}
