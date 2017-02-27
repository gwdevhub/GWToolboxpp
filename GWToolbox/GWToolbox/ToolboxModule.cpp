#include "ToolboxModule.h"

void ToolboxModule::LoadSettings(CSimpleIni* ini) {
	LoadSettingInternal(ini);
}

void ToolboxModule::SaveSettings(CSimpleIni* ini) {
	SaveSettingInternal(ini);
}

void ToolboxModule::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		DrawSettingInternal();
	}
}
