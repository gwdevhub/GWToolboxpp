#include "MainWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\PartyMgr.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

#include "ToolboxModule.h"
#include "Panels\PconPanel.h"
#include "Panels\HotkeyPanel.h"
#include "Panels\TravelPanel.h"
#include "Panels\BuildPanel.h"
#include "Panels\DialogPanel.h"
#include "Panels\InfoPanel.h"
#include "Panels\MaterialsPanel.h"
#include "Panels\SettingsPanel.h"

MainWindow* MainWindow::Instance() {
	return GWToolbox::Instance().main_window;
}

MainWindow::MainWindow() {	
	panels.push_back(pcon_panel = new PconPanel());
	panels.push_back(hotkey_panel = new HotkeyPanel());
	panels.push_back(build_panel = new BuildPanel());
	panels.push_back(travel_panel = new TravelPanel());
	panels.push_back(dialog_panel = new DialogPanel());
	panels.push_back(info_panel = new InfoPanel());
	panels.push_back(materials_panel = new MaterialsPanel());
	panels.push_back(settings_panel = new SettingsPanel());
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
	visible = true;
	LoadSettingInternal(ini);
}

void MainWindow::SaveSettings(CSimpleIni* ini) {
	// don't save visible
	SaveSettingInternal(ini);
}
void MainWindow::LoadSettingInternal(CSimpleIni* ini) {
	for (ToolboxPanel* panel : panels) {
		panel->LoadSettings(ini);
	}
	one_panel_at_time_only = ini->GetBoolValue(Name(), "one_panel_at_time_only", true);
}

void MainWindow::SaveSettingInternal(CSimpleIni* ini) {
	for (ToolboxPanel* panel : panels) {
		panel->SaveSettings(ini);
	}
	ini->SetBoolValue(Name(), "one_panel_at_time_only", one_panel_at_time_only);
}

void MainWindow::DrawSettingInternal() {
	//use_minimized_alt_pos.Draw();
	//tabs_left.Draw();
	
	ImGui::Checkbox("One panel at a time", &one_panel_at_time_only);
	for (ToolboxPanel* panel : panels) {
		panel->DrawSettings();
	}
}

void MainWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;

	static bool open = true;
	ImGui::SetNextWindowSize(ImVec2(100.0f, 300.0f), ImGuiSetCond_Always);
	if (ImGui::Begin(GuiName(), &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::f11]);
		for (unsigned int i = 0; i < panels.size(); ++i) {
			if (i > 0) ImGui::Separator();
			ImGui::PushID(i);
			if (panels[i]->DrawTabButton(device)) {
				if (panels[i]->visible && one_panel_at_time_only) {
					for (unsigned int j = 0; j < panels.size(); ++j) {
						if (j != i) panels[j]->visible = false;
					}
				}
			}
			ImGui::PopID();
		}
		ImGui::PopFont();
	}
	ImGui::End();

	if (!open) GWToolbox::Instance().StartSelfDestruct();

	for (ToolboxPanel* panel : panels) {
		if (panel->visible) panel->Draw(device);
	}
}
