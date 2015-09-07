#pragma once

#include "EmptyForm.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "logger.h"

class TimerWindow : public EmptyForm {
public:
	const int WIDTH = 180;
	const int HEIGHT = 50;

public:
	TimerWindow();

	inline static const wchar_t* IniSection() { return L"timer"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyColor() { return L"color"; }

	void UpdateLabel();

private:
	DragButton* timer_;
	DragButton* shadow_;
	long current_time_;

	void SaveLocation();
};
