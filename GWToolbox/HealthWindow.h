#pragma once

#include "EmptyForm.h"
#include "../include/OSHGui/OSHGui.hpp"

class HealthWindow : public EmptyForm {
public:
	const int WIDTH = 180;
	const int HEIGHT = 50;
	const int ABS_HEIGHT = 25;

	HealthWindow();

	inline static const wchar_t* IniSection() { return L"TargetHealth"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }

	void Show(bool show);

	void UpdateUI();

private:
	DragButton* percent;
	DragButton* percent_shadow;
	DragButton* absolute;
	DragButton* absolute_shadow;

	float current_hp;
	long current_max;

	bool show_;

	void SaveLocation();
};
