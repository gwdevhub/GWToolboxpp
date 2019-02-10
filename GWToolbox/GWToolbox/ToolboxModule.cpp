#include "stdafx.h"

#include "Utf8.h"
#include "GWToolbox.h"
#include "ToolboxModule.h"

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
