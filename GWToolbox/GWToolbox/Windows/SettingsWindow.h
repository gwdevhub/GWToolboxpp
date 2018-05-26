#pragma once

#include "ToolboxWindow.h"

class SettingsWindow : public ToolboxWindow {
	SettingsWindow() {};
	~SettingsWindow() {};
public:
	static SettingsWindow& Instance() {
		static SettingsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Settings"; }

	void Initialize() override;

	void LoadSettings(CSimpleIni* ini) override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	int sep_windows = 0;
	int sep_widgets = 0;
};
