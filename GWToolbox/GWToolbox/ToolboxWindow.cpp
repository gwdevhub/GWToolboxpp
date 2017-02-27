#include "ToolboxWindow.h"

void ToolboxWindow::LoadSettings(CSimpleIni* ini) {
	visible = ini->GetBoolValue(Name(), "visible", false);

	LoadSettingInternal(ini);
}

void ToolboxWindow::SaveSettings(CSimpleIni* ini) {
	ini->SetBoolValue(Name(), "visible", visible);

	SaveSettingInternal(ini);
}

void ToolboxWindow::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		if (ImGui::Button("Reset Position")) {
			ImGui::SetWindowPos(Name(), ImVec2(50.0f, 50.0f));
		}
		ImGui::SameLine();
		ImGui::Checkbox("Visible", &visible);

		DrawSettingInternal();
	}
}
