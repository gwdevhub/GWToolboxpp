#pragma once

#include "ToolboxPanel.h"

class InfoPanel : public ToolboxPanel {
public:
	InfoPanel() {}

	const char* Name() override { return "Info Panel"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
