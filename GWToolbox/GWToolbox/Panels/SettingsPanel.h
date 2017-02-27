#pragma once

#include "ToolboxPanel.h"

class SettingsPanel : public ToolboxPanel {
public:
	const char* Name() const override { return "Settings Panel"; }
	const char* TabButtonText() const override { return "Settings"; }

	SettingsPanel();
	~SettingsPanel() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettings() override {};
};
