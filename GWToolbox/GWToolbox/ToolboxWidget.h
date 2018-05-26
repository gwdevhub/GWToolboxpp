#pragma once

#include "ToolboxUIElement.h"

class ToolboxWidget : public ToolboxUIElement {
public:
	bool IsWidget() const override { return true; }

	virtual void LoadSettings(CSimpleIni* ini) override {
		ToolboxUIElement::LoadSettings(ini);
		lock_move = true;
		lock_size = true;
	}

	virtual void DrawSettings() override;

	ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, 
		bool noinput_if_frozen = true) const;
};
