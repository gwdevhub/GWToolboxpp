#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

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
	static long OldWndProc;

	//------ Static Methods ------//
public:
	// will create a new toolbox object and run it, can be used as argument for createThread
	static void SafeThreadEntry(HMODULE mod);
private:
	static void ThreadEntry(HMODULE dllmodule);

	// DirectX event handlers declaration
	static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice);
	static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice,
		D3DPRESENT_PARAMETERS* pPresentationParameters);

	// Input event handler
	static LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

	//------ Constructor ------//
private:
	GWToolbox(IDirect3DDevice9* pDevice);

	//------ Public methods ------//
public:
	static GWToolbox& instance() { return *instance_; }
	static GWToolbox* instanceptr() { return instance_; }

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

	//------ Private Fields ------//
private:
	bool right_mouse_pressed_;	// if true right mouse has been pressed, ignore move events
	bool must_self_destruct_;	// is true when toolbox should quit
	
	bool must_resize_;		// true when a resize event is sent, is checked by render thread
};
