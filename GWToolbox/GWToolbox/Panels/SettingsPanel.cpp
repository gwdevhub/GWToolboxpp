#include "SettingsPanel.h"

#include <Windows.h>

#include "Defines.h"
#include "GuiUtils.h" 
#include "GWToolbox.h"
#include <OtherModules\ToolboxSettings.h>
#include <OtherModules\GameSettings.h>
#include <OtherModules\Resources.h>

void SettingsPanel::Initialize() {
	ToolboxPanel::Initialize();
	Resources::Instance().LoadTextureAsync(&texture, "settings.png", "img");
}

void SettingsPanel::Draw(IDirect3DDevice9* pDevice) {
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiSetCond_FirstUseEver);
	ImGui::Begin(Name(), &visible);
	ImGui::Text("GWToolbox++ version "
		GWTOOLBOX_VERSION
		" by Has and KAOS"
		BETA_VERSION);
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

	ToolboxSettings::Instance().DrawFreezeSetting();
	ImGui::SameLine(ImGui::GetWindowWidth() / 2 + ImGui::GetStyle().ItemSpacing.x / 2);
	GameSettings::Instance().DrawBorderlessSetting();

	if (ImGui::CollapsingHeader("Help")) {
		ImGui::BulletText("Double-click on title bar to collapse a window.");
		ImGui::BulletText("Click and drag on lower right corner to resize a window.");
		ImGui::BulletText("Click and drag on any empty space to move a window.");
		ImGui::BulletText("Mouse Wheel to scroll.");
		if (ImGui::GetIO().FontAllowUserScaling)
			ImGui::BulletText("CTRL+Mouse Wheel to zoom window contents.");
		ImGui::BulletText("TAB/SHIFT+TAB to cycle through keyboard editable fields.");
		ImGui::BulletText("CTRL+Click on a slider or drag box to input text.");
		ImGui::BulletText(
			"While editing text:\n"
			"- Hold SHIFT or use mouse to select text\n"
			"- CTRL+Left/Right to word jump\n"
			"- CTRL+A or double-click to select all\n"
			"- CTRL+X,CTRL+C,CTRL+V clipboard\n"
			"- CTRL+Z,CTRL+Y undo/redo\n"
			"- ESCAPE to revert\n"
			"- You can apply arithmetic operators +,*,/ on numerical values. Use +- to subtract.\n");
	}

	for (ToolboxModule* module : GWToolbox::Instance().modules) {
		module->DrawSettings();
	}

	if (ImGui::Button("Save Now", ImVec2(w, 0))) {
		GWToolbox::Instance().SaveSettings();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toolbox normally saves settings on exit.\nClick to save to disk now.");
	ImGui::SameLine();
	if (ImGui::Button("Load Now", ImVec2(w, 0))) {
		GWToolbox::Instance().LoadSettings();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toolbox normally loads settings on launch.\nClick to re-load from disk now.");
		
	ImGui::End();
}
