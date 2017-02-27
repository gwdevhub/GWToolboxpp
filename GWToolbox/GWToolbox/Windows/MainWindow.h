#pragma once

#include <vector>

#include <ToolboxWindow.h>
#include <ToolboxPanel.h>

class PconPanel;
class HotkeyPanel;
class TravelPanel;
class BuildPanel;
class DialogPanel;
class InfoPanel;
class MaterialsPanel;
class SettingsPanel;

class MainWindow : public ToolboxWindow {
public:
	static MainWindow* Instance();

	const char* Name() const override { return "Main Window"; }
	const char* GuiName() const { return "Toolbox++"; }

	MainWindow();
	~MainWindow();
	
	// Update. Will always be called every frame.
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

public:
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
