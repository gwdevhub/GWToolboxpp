#include "MainWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\PartyMgr.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

void MainWindow::Initialize() {
	// skip the Window initialize to avoid registering with ourselves
	ToolboxWindow::Initialize();
}

void MainWindow::LoadSettings(CSimpleIni* ini) {
	bool v = ini->GetBoolValue(Name(), VAR_NAME(visible), true);
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = false;
	visible = v;
	one_panel_at_time_only = ini->GetBoolValue(Name(), VAR_NAME(one_panel_at_time_only), false);
}

void MainWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(one_panel_at_time_only), one_panel_at_time_only);
}
void MainWindow::DrawSettingInternal() {
	ImGui::Checkbox("Close other windows when opening a new one", &one_panel_at_time_only);
	ImGui::ShowHelp("Only affects windows (with a title bar), not widgets");
}

void MainWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;

	static bool open = true;
	ImGui::SetNextWindowSize(ImVec2(110.0f, 300.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), show_closebutton ? &open : nullptr, GetWinFlags())) {

		ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f18));
		const std::vector<ToolboxUIElement*>& ui = GWToolbox::Instance().GetUIElements();
		bool drawn = false;
		for (unsigned int i = 0; i < ui.size(); ++i) {
			ImGui::PushID(i);
			if (ui[i]->show_menubutton) {
				if (drawn) ImGui::Separator();
				drawn = true;
				if (ui[i]->DrawTabButton(device)) {
					if (one_panel_at_time_only && ui[i]->visible) {
						for (unsigned int j = 0; j < ui.size(); ++j) {
							if (j != i && ui[j]->IsWindow() && ui[j] != this) ui[j]->visible = false;
						}
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
