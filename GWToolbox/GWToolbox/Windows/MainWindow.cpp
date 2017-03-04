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

void MainWindow::Initialize() {
	ToolboxWindow::Initialize();

	panels.push_back(&PconPanel::Instance());
	panels.push_back(&HotkeyPanel::Instance());
	panels.push_back(&BuildPanel::Instance());
	panels.push_back(&TravelPanel::Instance());
	panels.push_back(&DialogPanel::Instance());
	panels.push_back(&InfoPanel::Instance());
	panels.push_back(&MaterialsPanel::Instance());
	panels.push_back(&SettingsPanel::Instance());
}

void MainWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini); // don't load visible
	visible = true;
	one_panel_at_time_only = ini->GetBoolValue(Name(), "one_panel_at_time_only", true);
}

void MainWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini); // don't save visible
	ini->SetBoolValue(Name(), "one_panel_at_time_only", one_panel_at_time_only);
}
void MainWindow::DrawSettingInternal() {
	ImGui::Checkbox("One panel at a time", &one_panel_at_time_only);
}

void MainWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;

	static bool open = true;
	ImGui::SetNextWindowSize(ImVec2(100.0f, 300.0f), ImGuiSetCond_Always);
	if (ImGui::Begin(Name(), &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {

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
}
