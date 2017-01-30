#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"

class TimerWindow : public ToolboxWindow {
public:
	TimerWindow() {}

	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;
};
