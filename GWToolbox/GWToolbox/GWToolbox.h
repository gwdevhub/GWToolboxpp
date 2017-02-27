#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

#include "ToolboxModule.h"

// forward declarations
class ChatCommands;
class ChatFilter;
class GameSettings;
class ToolboxSettings;
class Resources;

class MainWindow;
class TimerWindow;
class BondsWindow;
class HealthWindow;
class DistanceWindow;
class PartyDamage;
class Minimap;
class ClockWindow;
class NotePadWindow;

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
	~GWToolbox();

	//------ Public methods ------//
public:
	static GWToolbox& Instance() { return *instance_; }
	static GWToolbox* InstancePtr() { return instance_; }

	ChatFilter* chat_filter;
	ChatCommands* chat_commands;
	ToolboxSettings* toolbox_settings;
	GameSettings* game_settings;
	Resources* resources;

	MainWindow* main_window;
	TimerWindow* timer_window;
	BondsWindow* bonds_window;
	HealthWindow* health_window;
	DistanceWindow* distance_window;
	PartyDamage* party_damage;
	Minimap* minimap;
	ClockWindow* clock_window;
	NotePadWindow* notepad_window;

	std::vector<ToolboxModule*> modules;

	void StartSelfDestruct() { must_self_destruct = true; }

	//------ Private Fields ------//
private:
	bool must_self_destruct;	// is true when toolbox should quit
	
	CSimpleIni* inifile = nullptr;
};
