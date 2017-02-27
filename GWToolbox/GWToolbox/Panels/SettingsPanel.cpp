#include "SettingsPanel.h"

#include <Windows.h>

#include "Defines.h"
#include "GuiUtils.h" 
#include "GWToolbox.h"
#include <OtherModules\ToolboxSettings.h>
#include <OtherModules\GameSettings.h>
#include <OtherModules\Resources.h>

SettingsPanel::SettingsPanel() {
	Resources::Instance()->LoadTextureAsync(&texture, "settings.png", "img");
}

void SettingsPanel::Draw(IDirect3DDevice9* pDevice) {
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiSetCond_FirstUseEver);
	ImGui::Begin(Name(), &visible);
	ImGui::Text("GWToolbox++ version %s by Has and KAOS", GWTOOLBOX_VERSION);
	float w = (ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (ImGui::Button("Open Settings Folder", ImVec2(w, 0))) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecute(NULL, "open", GuiUtils::getSettingsFolder().c_str(), NULL, NULL, SW_SHOWNORMAL);
	}
	ImGui::SameLine();
	if (ImGui::Button("Open GWToolbox++ Website", ImVec2(w, 0))) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecute(NULL, "open", GWTOOLBOX_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
	}

	ImGui::Checkbox("Freeze Widgets", &ToolboxSettings::Instance()->freeze_widgets);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2 + ImGui::GetStyle().ItemSpacing.x / 2);
	if (ImGui::Checkbox("Borderless Window", &GameSettings::Instance()->borderless_window)) {
		GameSettings::Instance()->ApplyBorderless(GameSettings::Instance()->borderless_window);
	}

	for (ToolboxModule* module : GWToolbox::Instance().modules) {
		module->DrawSettings();
	}

	//if (ImGui::CollapsingHeader("Theme")) {
	//}
		
	ImGui::End();
}
