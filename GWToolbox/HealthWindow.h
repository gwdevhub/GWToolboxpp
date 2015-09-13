#pragma once

#include "ToolboxWindow.h"
#include "../include/OSHGui/OSHGui.hpp"

class HealthWindow : public ToolboxWindow {
public:
	const int WIDTH = 150;
	const int HEIGHT = 50;
	const int ABS_HEIGHT = 25;

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
