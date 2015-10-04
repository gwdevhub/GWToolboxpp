#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"

class HealthWindow : public ToolboxWindow {
public:
	const int WIDTH = 150;
	const int LABEL_HEIGHT = 20;
	const int PERCENT_HEIGHT = 50;
	const int ABSOLUTE_HEIGHT = 25;
	const int HEIGHT = LABEL_HEIGHT + PERCENT_HEIGHT + ABSOLUTE_HEIGHT;

	HealthWindow();

	inline static const wchar_t* IniSection() { return L"TargetHealth"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "health"; }

	void Show(bool show);

	void UpdateUI();
	inline void MainRoutine() {};

	void SetFreeze(bool b) {
		containerPanel_->SetEnabled(!b);
		percent->SetEnabled(!b);
		absolute->SetEnabled(!b);
	}

	inline void SetHideTarget(bool b) { hide_target = b; }

private:
	bool enabled;

	DragButton* percent;
	DragButton* percent_shadow;
	DragButton* absolute;
	DragButton* absolute_shadow;

	float current_hp;
	long current_max;
	bool hide_target;

	void SaveLocation();

	void _Show(bool show);
};
