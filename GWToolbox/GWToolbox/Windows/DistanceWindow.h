#pragma once

#include "ToolboxWindow.h"

class DistanceWindow : public ToolboxWindow {
public:
	const char* Name() const override { return "Distance"; }

	DistanceWindow() {};
	~DistanceWindow() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
