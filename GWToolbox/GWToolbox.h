#include "../API/APIMain.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

#include "Config.h"
#include "MainWindow.h"

using namespace OSHGui;

class GWToolbox {

private:
	static GWToolbox* instance_;

	Config* const config_;
	MainWindow* main_window_;

private:
	GWToolbox(HMODULE mod) :
		m_dllmodule(mod),
		config_(new Config()),
		main_window_(NULL)
	{ }

	// Executes setup and main loop of toolbox. 
	void exec();

	// Self destructs
	void destroy();

	HMODULE m_dllmodule;	// Handle to the dll module we are running, used to clear the module from GW on eject.

public:
	static bool capture_input;

	// will create a new toolbox object and run it, can be used as argument for createThread
	static void threadEntry(HMODULE mod);

	// returns toolbox instance
	static GWToolbox* instance() { return instance_; }
	inline Config* config() { return config_; }
	inline void set_main_window(MainWindow* main_window) {
		main_window_ = main_window;
	}
	inline MainWindow* main_window() { return main_window_; }
	
};
