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
	MainWindow() {};
	~MainWindow() {};
public:
	static MainWindow& Instance() {
		static MainWindow instance;
		return instance;
	}

	const char* Name() const { return "Toolbox"; }

	void Initialize() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void RegisterPanel(ToolboxPanel* panel) {
		panels.push_back(panel);
	}

private:
	std::vector<ToolboxPanel*> panels;

	bool one_panel_at_time_only;
};
