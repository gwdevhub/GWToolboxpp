#include "ToolboxModule.h"

void ToolboxModule::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		ImGui::PushID(Name());
		DrawSettingInternal();
		ImGui::PopID();
	}
}
