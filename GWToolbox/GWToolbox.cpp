#include "GWToolbox.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

using namespace OSHGui::Drawing;

namespace{
	LPD3DXFONT dbgFont = NULL;
	RECT Type1Rect = { 50, 50, 300, 14 };
	GWAPI::GWAPIMgr * mgr;
	GWAPI::DirectXMgr * dx;
}

void create_gui(IDirect3DDevice9* pDevice) {
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(new Direct3D9Renderer(pDevice)));

	Application * app = Application::InstancePtr();

	auto font = FontManager::LoadFont("Arial", 8.0f, false); //Arial, 8PT, no anti-aliasing
	app->SetDefaultFont(font);

	auto form = std::make_shared<Form>();

	app->Run(form);

	app->Enable();

	app->RegisterHotkey(Hotkey(Key::Insert, [] {
		Application::Instance().Toggle();
	}));
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



void GWToolbox::init() {
	mgr = GWAPI::GWAPIMgr::GetInstance();
	dx = mgr->DirectX;

	dx->CreateRenderHooks(endScene, resetScene);
}
