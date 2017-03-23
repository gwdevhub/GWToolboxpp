#pragma once

#include "ToolboxModule.h"

class ToolboxUIElement : public ToolboxModule {
public:
	// save 'visible' field
	virtual void LoadSettings(CSimpleIni* ini) override {
		ToolboxModule::LoadSettings(ini);
		visible = ini->GetBoolValue(Name(), "visible", false);
	}

	// load 'visible' field
	virtual void SaveSettings(CSimpleIni* ini) override {
		ToolboxModule::SaveSettings(ini);
		ini->SetBoolValue(Name(), "visible", visible);
	}

	virtual bool ToggleVisible() { return visible = !visible; }

	bool visible;
	bool lock_move;
	bool lock_size;

protected:
	void ShowVisibleRadio() {
		ImGui::SameLine(ImGui::GetContentRegionAvailWidth() 
			- ImGui::GetTextLineHeight() 
			- ImGui::GetStyle().FramePadding.y * 2);
		ImGui::PushID(Name());
		ImGui::Checkbox("##check", &visible);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
		ImGui::PopID();
	}
};
