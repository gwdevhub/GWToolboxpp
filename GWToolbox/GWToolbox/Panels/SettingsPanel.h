#pragma once

#include "ToolboxPanel.h"

class SettingsPanel : public ToolboxPanel {
	SettingsPanel() {};
	~SettingsPanel() {};
public:
	static SettingsPanel& Instance() {
		static SettingsPanel instance;
		return instance;
	}

	const char* Name() const override { return "Settings"; }

	void Initialize() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	int sep_windows = 0;
	int sep_widgets = 0;
};
