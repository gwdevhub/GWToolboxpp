#include "../API/APIMain.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

#include "Config.h"
#include "MainWindow.h"
#include "TimerWindow.h"

using namespace OSHGui;

class GWToolbox {

private:
	static GWToolbox* instance_;

	Config* const config_;
	MainWindow* main_window_;
	TimerWindow* timer_window_;

	bool must_self_destruct_;

private:
	GWToolbox(HMODULE mod) :
		m_dllmodule(mod),
		config_(new Config()) { 
		main_window_ = NULL;
		timer_window_ = NULL;
		must_self_destruct_ = false;
	}

	// Executes setup and main loop of toolbox. 
	void exec();

	// Self destructs
	void Destroy();

	HMODULE m_dllmodule;	// Handle to the dll module we are running, used to clear the module from GW on eject.

public:
	static bool capture_input;

	// will create a new toolbox object and run it, can be used as argument for createThread
	static void threadEntry(HMODULE mod);

	// returns toolbox instance
	static GWToolbox* instance() { return instance_; }
	inline Config* config() { return config_; }
	inline MainWindow* main_window() { return main_window_; }
	inline TimerWindow* timer_window() { return timer_window_; }
	
	inline void set_main_window(MainWindow* w) { main_window_ = w; }
	inline void set_timer_window(TimerWindow* w) { timer_window_ = w; }

	void StartSelfDestruct() { must_self_destruct_ = true; }
};
