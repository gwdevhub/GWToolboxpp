#include "GWToolbox.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

using namespace OSHGui::Drawing;

void create_gui(IDirect3DDevice9* pDevice) {
	Application::Initialize(std::unique_ptr<Direct3D9Renderer>(new Direct3D9Renderer(pDevice)));

	Application * app = Application::InstancePtr();

	auto form = std::make_shared<Form>();

	app->Run(form);

	app->Enable();

	app->RegisterHotkey(Hotkey(Key::Insert, [] {
		Application::Instance().Toggle();
	}));
}

static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice) {
	static bool init = false;
	if (!init) {
		init = true;
		create_gui(pDevice);
	}

	Application::Instance().Render();

	return 0;
}

static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	// nothing for now
	return 0;
}



void GWToolbox::init() {
	GWAPI::GWAPIMgr * mgr = GWAPI::GWAPIMgr::GetInstance();
	GWAPI::DirectXMgr * dx = mgr->DirectX;

	dx->CreateRenderHooks(endScene, resetScene);
}
