#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxModule.h"

/*
The difference between a ToolboxPanel and a ToolboxModule
is that the Panel also has a button in the main interface
from which it can be opened. 

This class provides the abstraction to group all such panels,
and a function to draw their main window button.
*/

class ToolboxPanel : public OSHGui::Panel, public ToolboxModule {
public:
	ToolboxPanel(OSHGui::Control* parent) : OSHGui::Panel(parent) {}
	ToolboxPanel() : OSHGui::Panel(nullptr) {}

	// Create user interface
	virtual void BuildUI() {};
};
