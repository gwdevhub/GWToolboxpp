#pragma once

#include "ToolboxPanel.h"

class InfoPanel : public ToolboxPanel {
public:
	InfoPanel(IDirect3DDevice9* pDevice);
	~InfoPanel() {}

	const char* Name() const override { return "Info Panel"; }
	const char* TabButtonText() const override { return "Info"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
