#pragma once

#include "ToolboxWindow.h"

class TimerWindow : public ToolboxWindow {
public:
	const char* Name() const override { return "Timer"; }

	TimerWindow() {};
	~TimerWindow() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
