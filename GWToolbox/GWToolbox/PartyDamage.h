#pragma once

#include <OSHGui\OSHGui.hpp>

#include "ToolboxWindow.h"

class PartyDamage : public ToolboxWindow {
	static const int MAX_PLAYERS = 12;
	static const int ABS_WIDTH = 50;
	static const int PERC_WIDTH = 50;
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

private:
	void SaveLocation();

	int party_size_;
	int line_height_;

	long total;
	long damage[MAX_PLAYERS];
	DragButton* absolute[MAX_PLAYERS];
	//DragButton* absolute_shadow[MAX_PLAYERS];
	DragButton* percent[MAX_PLAYERS];
	//DragButton* percent_shadow[MAX_PLAYERS];
};
