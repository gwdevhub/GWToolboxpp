#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"

class DistanceWindow : public ToolboxWindow {
public:
	const int WIDTH = 150;
	const int LABEL_HEIGHT = 20;
	const int PERCENT_HEIGHT = 50;
	const int ABSOLUTE_HEIGHT = 25;
	const int HEIGHT = LABEL_HEIGHT + PERCENT_HEIGHT + ABSOLUTE_HEIGHT;

	DistanceWindow();

	inline static const wchar_t* IniSection() { return L"Distance"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "distance"; }

	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;

	void SetFreeze(bool b) {
		Control::SetEnabled(!b);
		containerPanel_->SetEnabled(!b);
		percent->SetEnabled(!b);
		absolute->SetEnabled(!b);
	}

	inline void SetHideTarget(bool b) { hide_target = b; }

private:
	DragButton* percent;
	DragButton* percent_shadow;
	DragButton* absolute;
	DragButton* absolute_shadow;

	long current_distance;

	bool hide_target;

	void SaveLocation();
};
