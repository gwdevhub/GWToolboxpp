#pragma once

#include "ToolboxWindow.h"

class HealthWindow : public ToolboxWindow {
	HealthWindow() {};
	~HealthWindow() {};
public:
	static HealthWindow& Instance() {
		static HealthWindow instance;
		return instance;
	}

	const char* Name() const override { return "Target Health"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
