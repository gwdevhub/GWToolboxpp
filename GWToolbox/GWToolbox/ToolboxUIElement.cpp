#include "ToolboxUIElement.h"

#include <GWToolbox.h>

void ToolboxUIElement::Initialize() {
	ToolboxModule::Initialize();
	GWToolbox::Instance().RegisterUIElement(this);
}

void ToolboxUIElement::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	visible = ini->GetBoolValue(Name(), "visible", false);
}

// load 'visible' field
void ToolboxUIElement::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), "visible", visible);
}

void ToolboxUIElement::ShowVisibleRadio() {
	ImGui::SameLine(ImGui::GetContentRegionAvailWidth()
		- ImGui::GetTextLineHeight()
		- ImGui::GetStyle().FramePadding.y * 2);
	ImGui::PushID(Name());
	ImGui::Checkbox("##check", &visible);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
	ImGui::PopID();
}
