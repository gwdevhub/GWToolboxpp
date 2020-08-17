#pragma once

#include <imgui.h>
#include <d3d9.h>
#include <Windows.h>
#include <SimpleIni.h>

#ifdef BUILD_DLL
#define DLLAPI extern "C" __declspec(dllexport)
#else
#define DLLAPI
#endif

class TBModule; // Full declaration below. 

//
// Dll interface. You must implement those functions
// 
DLLAPI TBModule* getObj();

DLLAPI const char* getName();


class TBModule {
public:
	TBModule() = default;
	virtual ~TBModule() = default;
	TBModule(const TBModule&) = delete;

	// name of the window and the ini section
	virtual const char* Name() const = 0;

	// Initialize module
	virtual void Initialize(ImGuiContext* ctx) { ImGui::SetCurrentContext(ctx); }

	// Send termination signal to module. 
	virtual void SignalTerminate() {};

	// Can we terminate this module?
	virtual bool CanTerminate() { return true; }

	// Terminate module. Release any resources used. 
	virtual void Terminate() {};

	// Update. Will always be called once every frame. Delta is in seconds. 
	virtual void Update(float) {};

	// Draw. Will always be called once every frame. 
	virtual void Draw(IDirect3DDevice9*) {};

	// Optional. Prefer using ImGui::GetIO() during update or render, if possible. 
	virtual bool WndProc(UINT /*message*/, WPARAM, LPARAM) { return false; }

	// Load settings from ini
	virtual void LoadSettings(CSimpleIni*) {};

	// Save settings to ini
	virtual void SaveSettings(CSimpleIni*) {};

	// Draw settings. 
	virtual void DrawSettings() {};
};
