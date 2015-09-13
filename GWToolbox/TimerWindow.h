#pragma once

#include "EmptyForm.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "logger.h"

class TimerWindow : public EmptyForm {
public:
	class TimerLabel : public DragButton {
	public:
		TimerLabel() {}
		virtual void CalculateLabelLocation() override {
			label_->SetLocation(OSHGui::Drawing::PointI(DefaultBorderPadding, 
				GetSize().Height / 2 - label_->GetSize().Height / 2));
		}
	};

	const int WIDTH = 180;
	const int HEIGHT = 50;
	const int URGOZ_HEIGHT = 25;

public:
	TimerWindow();

	inline static const wchar_t* IniSection() { return L"timer"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const char* ThemeKey() { return "timer"; }

	void UpdateUI();

	void SetFreeze(bool b) {
		containerPanel_->SetEnabled(!b);
		timer_->SetEnabled(!b);
		urgoz_timer_->SetEnabled(!b);
	}

private:
	TimerLabel* timer_;
	TimerLabel* shadow_;
	TimerLabel* urgoz_timer_;
	TimerLabel* urgoz_shadow_;
	long current_time_;
	bool in_urgoz_;

	void SaveLocation();
};
