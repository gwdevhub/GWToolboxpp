#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxModule.h"

class ToolboxPanel : public OSHGui::Panel, public ToolboxModule {
public:
	ToolboxPanel(OSHGui::Control* parent) : OSHGui::Panel(parent) {}
	ToolboxPanel() : OSHGui::Panel(nullptr) {}

	// Create user interface
	virtual void BuildUI() {};
};
