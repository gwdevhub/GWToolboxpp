#pragma once

#include <queue>
#include <string>
#include <map>

#include <OSHGui\OSHGui.hpp>
#include <GWCA\GwConstants.h>
#include <GWCA\GWCA.h>

#include "ToolboxWindow.h"
#include "Timer.h"

class PartyDamage : public ToolboxWindow {
	static const int MAX_PLAYERS = 12;
	static const int ABS_WIDTH = 50;
	static const int PERC_WIDTH = 50;
	static const int WIDTH = ABS_WIDTH + PERC_WIDTH;
	static const int RECENT_HEIGHT = 6;
	static const int RECENT_MAX_TIME = 7000;

	struct PlayerDamage {
		long damage = 0;
		clock_t last_damage;
		long recent_damage;
		std::wstring name;
		GwConstants::Profession primary = GwConstants::Profession::None;
		GwConstants::Profession secondary = GwConstants::Profession::None;

		void Reset() {
			damage = 0;
			recent_damage = 0;
			name = L"";
			primary = GwConstants::Profession::None;
			secondary = GwConstants::Profession::None;
		}
	};

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
	void DamagePacketCallback(GWAPI::StoC::P151* packet);
	void MapLoadedCallback(GWAPI::StoC::P230* packet);

	void SaveLocation();
	float GetPartOfTotal(long dmg);
	inline float GetPercentageOfTotal(long dmg) { return GetPartOfTotal(dmg) * 100.0f; };
	float GetPartOfMax(long dmg);
	inline float GetPercentageOfMax(long dmg) { return GetPartOfMax(dmg) * 100.0f; };
	long GetMax();

	// damage values
	long total;
	PlayerDamage damage[MAX_PLAYERS];
	std::map<DWORD, long> hp_map;

	// UI elements
	int line_height_;
	int party_size_;
	DragButton* absolute[MAX_PLAYERS];
	DragButton* percent[MAX_PLAYERS];
	OSHGui::Panel* bar[MAX_PLAYERS];
	OSHGui::Panel* recent[MAX_PLAYERS];

	// main routine variables
	bool in_explorable = false;
	clock_t send_timer;
	std::queue<std::wstring> send_queue;
};
