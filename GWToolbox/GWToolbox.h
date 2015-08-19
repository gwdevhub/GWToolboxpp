#include "../API/APIMain.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

#include "Pcons.h"
#include "Builds.h"
#include "Hotkeys.h"
#include "Config.h"
#include "HotkeyMgr.h"

using namespace OSHGui;

class GWToolbox {

private:
	static GWToolbox* instance;

public:
	Config* const config;
	HotkeyMgr* const hotkeyMgr;

	Pcons* const pcons;
	Builds* const builds;
	Hotkeys* const hotkeys;

private:
	GWToolbox(HMODULE mod) :
		m_dllmodule(mod),
		config(new Config()),
		hotkeyMgr(new HotkeyMgr()),

		pcons(new Pcons()),
		builds(new Builds()),
		hotkeys(new Hotkeys())
	{ }

	// Executes setup and main loop of toolbox. 
	void exec();

	// Self destructs
	void destroy();

	bool isActive();		// ??? (also why a private accessor to a private field? xD)
	bool m_Active;			// ???
	HMODULE m_dllmodule;	// ???

public:
	// will create a new toolbox object and run it, can be used as argument for createThread
	static void threadEntry(HMODULE mod);

	// returns toolbox instance
	static GWToolbox* getInstance() { return instance; }
	
};
