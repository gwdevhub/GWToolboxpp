#pragma once

#include <vector>

#include "ToolboxModule.h"
#include "PconPanel.h"
#include "HotkeyPanel.h"
#include "TravelPanel.h"
#include "BuildPanel.h"
#include "DialogPanel.h"
#include "InfoPanel.h"
#include "MaterialsPanel.h"
#include "SettingsPanel.h"

class MainWindow : public ToolboxModule {
public:
	static const int WIDTH = 100;
	static const int HEIGHT = 285;
	static const int TAB_HEIGHT = 29;
	static const int TOGGLE_HEIGHT = 26;
	static const int TITLE_HEIGHT = 22;
	static const int SIDE_PANEL_WIDTH = 280;

public:
	const char* Name() const override { return "Main Window"; }

	MainWindow(IDirect3DDevice9* device);
	~MainWindow();
	
	// Update. Will always be called every frame.
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettings() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

public:
	bool minimized;

	PconPanel* pcon_panel;
	HotkeyPanel* hotkey_panel;
	BuildPanel* build_panel;
	TravelPanel* travel_panel;
	DialogPanel* dialog_panel;
	InfoPanel* info_panel;
	MaterialsPanel* materials_panel;
	SettingsPanel* settings_panel;

private:
	std::vector<ToolboxPanel*> panels;

	//SettingBool use_minimized_alt_pos;
	//SettingBool tabs_left;
	bool one_panel_at_time_only;
};
