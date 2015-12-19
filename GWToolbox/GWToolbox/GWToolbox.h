#pragma once

#include <Windows.h>

#include "GWCA\APIMain.h"
#include "OSHGui\OSHGui.hpp"
#include "OSHGui\Input\WindowsMessage.hpp"

#include "Config.h"
#include "MainWindow.h"
#include "TimerWindow.h"
#include "BondsWindow.h"
#include "HealthWindow.h"
#include "DistanceWindow.h"
#include "ChatCommands.h"
#include "ChatLogger.h"

#ifdef _DEBUG
#define EXCEPT_EXPRESSION EXCEPTION_CONTINUE_SEARCH
#else
#define EXCEPT_EXPRESSION Logger::GenerateDump(GetExceptionInformation())
#endif

class GWToolbox {
public:
	static const wchar_t * Host;
	static const wchar_t* Version;

	//------ Static Fields ------//
private:
	static GWToolbox* instance_;
	static OSHGui::Drawing::Direct3D9Renderer* renderer;
	static long OldWndProc;
	static OSHGui::Input::WindowsMessage input;

	//------ Static Methods ------//
public:

	// will create a new toolbox object and run it, can be used as argument for createThread
	static void SafeThreadEntry(HMODULE mod);
private:
	static void ThreadEntry(HMODULE dllmodule);

	static void SafeCreateGui(IDirect3DDevice9* pDevice);
	static void CreateGui(IDirect3DDevice9* pDevice);

	// DirectX event handlers declaration
	static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice);
	static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice,
		D3DPRESENT_PARAMETERS* pPresentationParameters);

	// Input event handler
	static LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);


	//------ Private Fields ------//
private:
	HMODULE dll_module_;	// Handle to the dll module we are running, used to clear the module from GW on eject.

	bool initialized_;
	bool capture_input_;
	bool must_self_destruct_;

	Config& config_;
	MainWindow* main_window_;
	TimerWindow* timer_window_;
	BondsWindow* bonds_window_;
	HealthWindow* health_window_;
	DistanceWindow* distance_window_;
	ChatCommands& chat_commands_;

	//------ Constructor ------//
private:
	GWToolbox(HMODULE mod) :
		dll_module_(mod),
		config_(*new Config()),
		main_window_(nullptr),
		timer_window_(nullptr),
		bonds_window_(nullptr),
		health_window_(nullptr),
		distance_window_(nullptr),
		initialized_(false),
		must_self_destruct_(false),
		capture_input_(false),
		chat_commands_(*new ChatCommands()) {
	}

	//------ Private Methods ------//
private:
	// Does everything: setup, main loop, destruction 
	void Exec();
	void UpdateUI();

	//------ Setters ------//
private:
	inline void set_initialized() { initialized_ = true; }
	inline void set_main_window(MainWindow* w) { main_window_ = w; }
	inline void set_timer_window(TimerWindow* w) { timer_window_ = w; }
	inline void set_bonds_window(BondsWindow* w) { bonds_window_ = w; }
	inline void set_health_window(HealthWindow* w) { health_window_ = w; }
	inline void set_distance_window(DistanceWindow* w) { distance_window_ = w; }

	//------ Public methods ------//
public:
	static GWToolbox& instance() { return *instance_; }

	inline bool initialized() { return initialized_; }

	inline bool capture_input() { return capture_input_; }
	inline void set_capture_input(bool capture) { capture_input_ = capture; }
	
	inline Config& config() { return config_; }
	inline MainWindow& main_window() { return *main_window_; }
	inline TimerWindow& timer_window() { return *timer_window_; }
	inline BondsWindow& bonds_window() { return *bonds_window_; }
	inline HealthWindow& health_window() { return *health_window_; }
	inline DistanceWindow& distance_window() { return *distance_window_; }
	
	void StartSelfDestruct() { 
		ChatLogger::Log(L"Bye!");
		must_self_destruct_ = true; }
};
