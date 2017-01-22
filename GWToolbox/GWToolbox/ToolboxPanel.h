#pragma once

#include "OSHGui\OSHGui.hpp"

class ToolboxPanel : public OSHGui::Panel {
public:
	ToolboxPanel(OSHGui::Control* parent) : OSHGui::Panel(parent) {}

	// Create user interface
	virtual void BuildUI() = 0;

	// Update. Will always be called every frame.
	virtual void Main() = 0;

	// Draw user interface. Will be called every frame if the element is visible
	virtual void Draw() = 0;
};
