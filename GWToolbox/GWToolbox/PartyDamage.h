#pragma once

#include <queue>
#include <string>
#include <map>

#include <OSHGui\OSHGui.hpp>
#include <GWCA\GWCA.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA\Packets\StoC.h>

#include <SimpleIni.h>

#include "ToolboxWindow.h"
#include "Timer.h"
#include "GuiUtils.h"

class PartyDamage : public ToolboxWindow {
	const wchar_t* inifilename = L"healthlog.ini";
	const wchar_t* inisection = L"health";

	static const int MAX_PLAYERS = 12;
	static const int ABS_WIDTH = 50;
	static const int PERC_WIDTH = 50;
	static const int WIDTH = ABS_WIDTH + PERC_WIDTH;
	static const int RECENT_HEIGHT = 6;
	static const int RECENT_MAX_TIME = 7000;

	const OSHGui::Drawing::Color default_forecolor = OSHGui::Drawing::Color(1, 1, 1, 1);
	const OSHGui::Drawing::Color default_backcolor = OSHGui::Drawing::Color(0.3f, 0, 0, 0);
	const OSHGui::Drawing::Color default_forebarcolor = OSHGui::Drawing::Color(0.8f, 0.4f, 0.6f, 0.9f);
	const OSHGui::Drawing::Color default_backbarcolor = OSHGui::Drawing::Color(0.4f, 0.8f, 0.4f, 0.2f);

	struct PlayerDamage {
		long damage = 0;
		long recent_damage;
		clock_t last_damage;
		long agent_id = 0;
		std::wstring name;
		GW::Constants::Profession primary = GW::Constants::Profession::None;
		GW::Constants::Profession secondary = GW::Constants::Profession::None;

		void Reset() {
			damage = 0;
			recent_damage = 0;
			agent_id = 0;
			name = L"";
			primary = GW::Constants::Profession::None;
			secondary = GW::Constants::Profession::None;
		}
	};

public:
	PartyDamage();
	~PartyDamage();

	inline static const wchar_t* IniSection() { return L"damage"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* InikeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "damage"; }
	inline static const char* ThemeBarsKey() { return "damagebars"; }

	// Update. Will always be called every frame.
	void Main() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;

	void SetVisible(bool visible) override {
		Control::SetVisible(visible);
		party_size_ = 0;
	}

	void SetFreeze(bool b);
	void SetTransparentBackColor(bool b);

	void WritePartyDamage();
	void WriteDamageOf(int index, int rank = 0); // party index from 0 to 12
	void WriteOwnDamage();
	void ResetDamage();

	void LoadIni();
	void SaveIni();

private:
	bool DamagePacketCallback(GW::Packet::StoC::P151* packet);
	bool MapLoadedCallback(GW::Packet::StoC::P230* packet);

	void CreatePartyIndexMap();

	void SaveLocation();
	float GetPartOfTotal(long dmg) const;
	inline float GetPercentageOfTotal(long dmg) const
	{ return GetPartOfTotal(dmg) * 100.0f; };

	// damage values
	long total;
	PlayerDamage damage[MAX_PLAYERS];
	std::map<DWORD, long> hp_map;
	std::map<DWORD, int> party_index;

	// UI elements
	int line_height_;
	int party_size_;
	DragButton* absolute[MAX_PLAYERS];
	DragButton* percent[MAX_PLAYERS];
	OSHGui::Panel* bar[MAX_PLAYERS];
	OSHGui::Panel* recent[MAX_PLAYERS];
	OSHGui::Drawing::Color labelcolor;

	// main routine variables
	bool in_explorable = false;
	clock_t send_timer;
	std::queue<std::wstring> send_queue;

	// ini
	CSimpleIni* inifile_;
};
