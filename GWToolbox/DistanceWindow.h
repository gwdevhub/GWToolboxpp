#pragma once

#include "EmptyForm.h"
#include "../include/OSHGui/OSHGui.hpp"

class DistanceWindow : public EmptyForm {
public:
	const int WIDTH = 150;
	const int HEIGHT = 50;
	const int ABS_HEIGHT = 25;

	DistanceWindow();

	inline static const wchar_t* IniSection() { return L"Distance"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "distance"; }

	void Show(bool show);

	void UpdateUI();

private:
	DragButton* percent;
	DragButton* percent_shadow;
	DragButton* absolute;
	DragButton* absolute_shadow;

	float current_distance;

	void SaveLocation();
};
