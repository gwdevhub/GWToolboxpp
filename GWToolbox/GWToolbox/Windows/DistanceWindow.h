#pragma once

#include "ToolboxWindow.h"

class DistanceWindow : public ToolboxWindow {
	DistanceWindow() {};
	~DistanceWindow() {};
public:
	static DistanceWindow& Instance() {
		static DistanceWindow instance;
		return instance;
	}

	const char* Name() const override { return "Distance"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
