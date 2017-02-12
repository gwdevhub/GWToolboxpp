#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

#include <OSHGui\OSHGui.hpp>
#include <OSHGui\Input\WindowsMessage.hpp>

#include "ChatFilter.h"
#include "ChatCommands.h"
#include "OtherSettings.h"

#include "MainWindow.h"
#include "TimerWindow.h"
#include "BondsWindow.h"
#include "HealthWindow.h"
#include "DistanceWindow.h"
#include "PartyDamage.h"
#include "Minimap.h"

class GWToolbox {
	//------ Static Fields ------//
private:
	static GWToolbox* instance_;
	static GW::DirectXHooker* dx_hooker;
	static OSHGui::Drawing::Direct3D9Renderer* renderer;
	static OSHGui::Input::WindowsMessage input;
	static long OldWndProc;
	static HMODULE dllmodule;

	//------ Static Methods ------//
public:
	// will create a new toolbox object and run it, can be used as argument for createThread
	static void SafeThreadEntry(HMODULE mod);
private:
	static void ThreadEntry(HMODULE dllmodule);

	static void CreateGui(IDirect3DDevice9* pDevice);

	// DirectX event handlers declaration
	static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice);
	static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice,
		D3DPRESENT_PARAMETERS* pPresentationParameters);

	// Input event handler
	static LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

	//------ Constructor ------//
private:
	GWToolbox() :
		chat_commands(nullptr),
		main_window(nullptr),
		timer_window(nullptr),
		bonds_window(nullptr),
		health_window(nullptr),
		distance_window(nullptr),
		party_damage(nullptr),
		initialized_(false),
		must_self_destruct_(false),
		must_resize_(false),
		right_mouse_pressed_(false),
		adjust_on_resize_(false),
		capture_input_(false) {
	}

	//------ Public methods ------//
public:
	static GWToolbox& instance() { return *instance_; }

	inline bool initialized() { return initialized_; }

	inline bool capture_input() { return capture_input_; }
	inline void set_capture_input(bool capture) { capture_input_ = capture; }
	inline void set_adjust_on_resize(bool active) { adjust_on_resize_ = active; }

	ChatFilter* chat_filter;
	ChatCommands* chat_commands;
	OtherSettings* other_settings;

	MainWindow* main_window;
	TimerWindow* timer_window;
	BondsWindow* bonds_window;
	HealthWindow* health_window;
	DistanceWindow* distance_window;
	PartyDamage* party_damage;

	Minimap* minimap;

	std::vector<ToolboxModule*> modules;

	void StartSelfDestruct() { must_self_destruct_ = true; }

	//------ Private Methods ------//
private:
	void ResizeUI();

	void LoadTheme();
	void SaveTheme();

	//------ Private Fields ------//
private:
	bool initialized_;		// true after initialization
	bool capture_input_;
	bool right_mouse_pressed_;	// if true right mouse has been pressed, ignore move events
	bool must_self_destruct_;	// is true when toolbox should quit
	
	bool adjust_on_resize_; // if true toolbox windows will try to adjust position on gw window resize
	bool must_resize_;		// true when a resize event is sent, is checked by render thread
	OSHGui::Drawing::SizeI old_screen_size_;
	OSHGui::Drawing::SizeI new_screen_size_;
};
