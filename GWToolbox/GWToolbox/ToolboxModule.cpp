#include "ToolboxModule.h"

#include "GWToolbox.h"

void ToolboxModule::Initialize() {
	GWToolbox::Instance().RegisterModule(this);
}

void ToolboxModule::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		ImGui::PushID(Name());
		DrawSettingInternal();
		ImGui::PopID();
	}
}
