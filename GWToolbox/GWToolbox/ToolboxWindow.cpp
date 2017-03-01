#include "ToolboxWindow.h"

#include "ImGuiAddons.h"
#include <imgui_internal.h>

void ToolboxWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	visible = ini->GetBoolValue(Name(), "visible", false);
}

void ToolboxWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), "visible", visible);
}

void ToolboxWindow::DrawSettings() {
	if (ImGui::CollapsingHeader(Name(), ImGuiTreeNodeFlags_AllowOverlapMode)) {
		ImGui::PushID(Name());
		ShowVisibleRadio();
		ImVec2 pos(0, 0);
		if (ImGuiWindow* window = ImGui::FindWindowByName(Name())) pos = window->Pos;
		if (ImGui::DragFloat2("Position", (float*)&pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowPos(Name(), pos);
		}
		ImGui::ShowHelp("You need to show the window for this control to work");
		DrawSettingInternal();
		ImGui::PopID();
	} else {
		ShowVisibleRadio();
	}
}

void ToolboxWindow::ShowVisibleRadio() {
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - ImGui::GetTextLineHeight());
	ImGui::PushID(Name());
	ImGui::Checkbox("##check", &visible);
	ImGui::PopID();

}
