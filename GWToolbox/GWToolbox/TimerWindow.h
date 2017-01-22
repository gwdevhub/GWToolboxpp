#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"
#include "logger.h"

class TimerWindow : public ToolboxWindow {
public:
	class TimerLabel : public DragButton {
	public:
		TimerLabel(OSHGui::Control* parent) : DragButton(parent) {}
		virtual void CalculateLabelLocation() override {
			label_->SetLocation(OSHGui::Drawing::PointI(Padding,
				GetSize().Height / 2 - label_->GetSize().Height / 2));
		}
	};

	const int WIDTH = 250;
	const int HEIGHT = 50;
	const int URGOZ_HEIGHT = 25;

public:
	TimerWindow();

	inline static const wchar_t* IniSection() { return L"timer"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const char* ThemeKey() { return "timer"; }

	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;

	void SetFreeze(bool b) {
		Control::SetEnabled(!b);
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
