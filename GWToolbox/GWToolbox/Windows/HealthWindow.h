#pragma once

#include "ToolboxWindow.h"

class HealthWindow : public ToolboxWindow {
public:
	const char* Name() const override { return "Target Health"; }

	HealthWindow() {};
	~HealthWindow() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
