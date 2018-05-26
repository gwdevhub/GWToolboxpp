#pragma once

#include <d3dx9tex.h>

#include "ToolboxUIElement.h"

/*
A ToolboxWindow is a module which also has an interface
*/
class ToolboxWindow : public ToolboxUIElement {
public:
	bool IsWindow() const override { return true; }

	virtual void LoadSettings(CSimpleIni* ini) override {
		ToolboxUIElement::LoadSettings(ini);
		lock_move = ini->GetBoolValue(Name(), VAR_NAME(lock_move), false);
		lock_size = ini->GetBoolValue(Name(), VAR_NAME(lock_size), false);
		show_closebutton = ini->GetBoolValue(Name(), VAR_NAME(show_closebutton), true);
	}

	virtual void SaveSettings(CSimpleIni* ini) override {
		ToolboxUIElement::SaveSettings(ini);
		ini->SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
		ini->SetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
		ini->SetBoolValue(Name(), VAR_NAME(show_closebutton), show_closebutton);
	}

	// Encapsulate settings into an ImGui::CollapsingHeader(Name()), 
	// show a reset position button and a 'visible' checkbox
	virtual void DrawSettings() override;

	ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const;

	bool* GetVisiblePtr(bool force_show = false) {
		if (show_closebutton || force_show) return &visible;
		return nullptr;
	}

	bool show_closebutton = true;
};
