#pragma once

#include <vector>

#include <ToolboxWindow.h>

class MainWindow : public ToolboxWindow {
	MainWindow() {};
	~MainWindow() {};
public:
	static MainWindow& Instance() {
		static MainWindow instance;
		return instance;
	}

	const char* Name() const override { return "Toolbox"; }

	const char* SettingsName() const override  { return "Toolbox Settings"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;
	void RegisterSettingsContent() override;

	void RefreshButtons();

	bool pending_refresh_buttons = true;

private:
	bool one_panel_at_time_only = false;

	float GetModuleWeighting(ToolboxUIElement* m) {
		auto found = module_weigtings.find(m->Name());
		return found == module_weigtings.end() ? 1.0f : found->second;
	}
	std::vector<std::pair<float, ToolboxUIElement*>> modules_to_draw{};
	std::unordered_map<std::string,float> module_weigtings {
		{"Pcons",0.5f},
		{"Hotkeys",0.52f},
		{"Builds",0.54f},
		{"Hero Builds",0.56f},
		{"Travel",0.58f},
		{"Dialogs",0.6f},
		{"Info",0.62f},
		{"Materials",0.64f},
		{"Trade",0.66f},
		{"Notepad",0.68f},
		{"Objective Timer",0.7f},
		{"Faction Leaderboard",0.72f},
		{"Daily Quests",0.74f},
		{"Friend List",0.76f},
		{"Damage",0.78f},
		{"Minimap",0.8f},
		{"Settings",0.82f}
	};
protected:
	const float SettingsWeighting() override { return 1.1f; };
};
