#include "MainWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\PartyMgr.h>

#include "logger.h"
#include "GuiUtils.h"
#include "Config.h"
#include "GWToolbox.h"

#include <imgui.h>

MainWindow::MainWindow(IDirect3DDevice9* device) {	
	panels.push_back(pcon_panel = new PconPanel(device));
	panels.push_back(hotkey_panel = new HotkeyPanel(device));
	panels.push_back(build_panel = new BuildPanel(device));
	panels.push_back(travel_panel = new TravelPanel(device));
	panels.push_back(dialog_panel = new DialogPanel(device));
	panels.push_back(info_panel = new InfoPanel(device));
	panels.push_back(materials_panel = new MaterialsPanel(device));
	panels.push_back(settings_panel = new SettingsPanel(device));
	visible = true;
}

MainWindow::~MainWindow() {
	for (ToolboxPanel* panel : panels) {
		delete panel;
	}
}

void MainWindow::Update() {
	for (ToolboxPanel* panel : panels) {
		panel->Update();
	}
}

void MainWindow::LoadSettings(CSimpleIni* ini) {
	for (ToolboxPanel* panel : panels) {
		panel->LoadSettings(ini);
	}
	one_panel_at_time_only = ini->GetBoolValue(Name(), "one_panel_at_time_only", true);
}

void MainWindow::SaveSettings(CSimpleIni* ini) const {
	for (ToolboxPanel* panel : panels) {
		panel->SaveSettings(ini);
	}
	ini->SetBoolValue(Name(), "one_panel_at_time_only", one_panel_at_time_only);
}

void MainWindow::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		//use_minimized_alt_pos.Draw();
		//tabs_left.Draw();
		ImGui::Checkbox("One panel at a time", &one_panel_at_time_only);
		for (ToolboxPanel* panel : panels) {
			panel->DrawSettings();
		}
	}
}


void MainWindow::Draw(IDirect3DDevice9* device) {
	static bool open = true;
	ImGui::SetNextWindowSize(ImVec2(100.0f, 300.0f), ImGuiSetCond_Always);
	if (ImGui::Begin("Toolbox++", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
		if (!open) GWToolbox::instance().StartSelfDestruct();

		for (unsigned int i = 0; i < panels.size(); ++i) {
			if (i > 0) ImGui::Separator();
			if (panels[i]->DrawTabButton(device)) {
				if (panels[i]->visible && one_panel_at_time_only) {
					for (unsigned int j = 0; j < panels.size(); ++j) {
						if (j != i) panels[j]->visible = false;
					}
				}
			}
		}
	}
	ImGui::End();

	for (ToolboxPanel* panel : panels) {
		if (panel->visible) panel->Draw(device);
	}
}

