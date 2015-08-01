#include "GWToolbox.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

using namespace OSHGui::Drawing;

namespace{
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
	static GWAPI::DirectXMgr::EndScene_t retfunc = dx->GetEndsceneReturn();
	static bool init = false;
	if (!init) {
		init = true;
		create_gui(pDevice);
	}

	auto &renderer = Application::Instance().GetRenderer();

	renderer.BeginRendering();

	Application::Instance().Render();

	renderer.EndRendering();


	return retfunc(pDevice);
}

static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	static GWAPI::DirectXMgr::Reset_t retfunc = dx->GetResetReturn();


	// pre-reset here.


	HRESULT result = retfunc(pDevice, pPresentationParameters);
	if (result == D3D_OK){
		// post-reset here.
	}

	return result;
}



void GWToolbox::init() {
	mgr = GWAPI::GWAPIMgr::GetInstance();
	dx = mgr->DirectX;

	dx->CreateRenderHooks(endScene, resetScene);
}
