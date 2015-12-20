#pragma once

#include <queue>
#include <string>

#include <OSHGui\OSHGui.hpp>

#include "ToolboxWindow.h"
#include "Timer.h"

class PartyDamage : public ToolboxWindow {
	static const int MAX_PLAYERS = 12;
	static const int ABS_WIDTH = 70;
	static const int PERC_WIDTH = 70;
	static const int WIDTH = ABS_WIDTH + PERC_WIDTH;

public:
	PartyDamage();

	inline static const wchar_t* IniSection() { return L"damage"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* InikeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "damage"; }

	void UpdateUI();
	void MainRoutine();

	void ShowWindow(bool show) override {
		ToolboxWindow::ShowWindow(show);
		party_size_ = 0;
	}

	void SetFreeze(bool b);

	void WriteChat();
	void Reset();

private:
	void SaveLocation();

	int party_size_;
	int line_height_;

	long total;
	long damage[MAX_PLAYERS];
	DragButton* absolute[MAX_PLAYERS];
	DragButton* percent[MAX_PLAYERS];

	clock_t send_timer;
	std::queue<std::wstring> send_queue;
};
