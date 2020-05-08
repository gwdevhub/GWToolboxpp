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

	const char* Name() const { return "Toolbox"; }

	const char* SettingsName() const { return "Toolbox Settings"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;
	void RegisterSettingsContent() override;

	void RefreshButtons();

	bool visible = true;
	bool pending_refresh_buttons = true;

private:
	bool one_panel_at_time_only = false;

	float GetModuleWeighting(ToolboxUIElement* m) {
		auto found = module_weigtings.find(m->Name());
		return found == module_weigtings.end() ? 1.0f : found->second;
	}
	std::vector<std::pair<float, ToolboxUIElement*>> modules_to_draw{};
	std::unordered_map<std::string,float> module_weigtings {
		{"Pcons",0.51f},
		{"Hotkeys",0.52f},
		{"Builds",0.53f},
		{"Hero Builds",0.54f},
		{"Travel",0.55f},
		{"Dialogs",0.56f},
		{"Info",0.57f},
		{"Materials",0.58f},
		{"Trade",0.59f},
		{"Notepad",0.6f},
		{"Objective Timer",0.61f},
		{"Faction Leaderboard",0.71f},
		{"Daily Quests",0.72f},
		{"Friend List",0.73f},
		{"Settings",0.74f}
	};
};
