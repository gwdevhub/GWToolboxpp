#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA_DX\DirectXHooker.h>

#include <OSHGui\OSHGui.hpp>
#include <OSHGui\Input\WindowsMessage.hpp>

#include "ChatLogger.h"
#include "Config.h"
#include "ChatCommands.h"

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
	static GWCA::DirectXHooker* dx_hooker;
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

	//------ Constructor ------//
private:
	GWToolbox(HMODULE mod) :
		dll_module_(mod),
		chat_commands_(new ChatCommands()),
		main_window_(nullptr),
		timer_window_(nullptr),
		bonds_window_(nullptr),
		health_window_(nullptr),
		distance_window_(nullptr),
		party_damage_(nullptr),
		initialized_(false),
		must_self_destruct_(false),
		must_resize_(false),
		right_mouse_pressed_(false),
		adjust_on_resize_(false),
		capture_input_(false) {

		ChatLogger::Init();

		if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
			&& GW::Agents().GetAgentArray().valid()
			&& GW::Agents().GetPlayer() != nullptr) {

			DWORD playerNumber = GW::Agents().GetPlayer()->PlayerNumber;
			ChatLogger::LogF(L"Hello %ls!", GW::Agents().GetPlayerNameByLoginNumber(playerNumber));
		}
	}

	//------ Public methods ------//
public:
	static GWToolbox& instance() { return *instance_; }

	inline bool initialized() { return initialized_; }

	inline bool capture_input() { return capture_input_; }
	inline void set_capture_input(bool capture) { capture_input_ = capture; }
	inline void set_adjust_on_resize(bool active) { adjust_on_resize_ = active; }

	inline ChatCommands& chat_commands() { return *chat_commands_; }

	inline MainWindow& main_window() { return *main_window_; }
	inline TimerWindow& timer_window() { return *timer_window_; }
	inline BondsWindow& bonds_window() { return *bonds_window_; }
	inline HealthWindow& health_window() { return *health_window_; }
	inline DistanceWindow& distance_window() { return *distance_window_; }
	inline PartyDamage& party_damage() { return *party_damage_; }
	inline Minimap& minimap() { return *minimap_; }

	void StartSelfDestruct() {
		if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading) {
			ChatLogger::Log(L"Bye!");
		}
		must_self_destruct_ = true;
	}

	//------ Private Methods ------//
private:
	// Does everything: setup, main loop, destruction 
	void Exec();

	// updates UI. will be ran in render loop
	void UpdateUI();

	void ResizeUI();

	void LoadTheme();
	void SaveTheme();

	//------ Private Fields ------//
private:
	HMODULE dll_module_;	// Handle to the dll module we are running, used to clear the module from GW on eject.

	bool initialized_;		// true after initialization
	bool capture_input_;
	bool right_mouse_pressed_;	// if true right mouse has been pressed, ignore move events
	bool must_self_destruct_;	// is true when toolbox should quit
	
	bool adjust_on_resize_; // if true toolbox windows will try to adjust position on gw window resize
	bool must_resize_;		// true when a resize event is sent, is checked by render thread
	OSHGui::Drawing::SizeI old_screen_size_;
	OSHGui::Drawing::SizeI new_screen_size_;

	ChatCommands* chat_commands_;

	MainWindow* main_window_;
	TimerWindow* timer_window_;
	BondsWindow* bonds_window_;
	HealthWindow* health_window_;
	DistanceWindow* distance_window_;
	PartyDamage* party_damage_;

	Minimap* minimap_;
};
