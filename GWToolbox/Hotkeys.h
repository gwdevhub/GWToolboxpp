#pragma once

#include <vector>
#include <functional>
#include <string>
#include "../include/OSHGui/OSHGui.hpp"
#include "Timer.h"
#include "../API/APIMain.h"
#include "HotkeyMgr.h"

using namespace std;

class Hotkeys {
	class UIHotkey {
	public:
		wstring const name;
		wstring const type;
		wstring const id;
		TBHotkey* const hotkey;

		UIHotkey(wstring _n, wstring _t, wstring _id, TBHotkey* hk) :
			name(_n), type(_t), id(_id), hotkey(hk) {}
	};

private:
	vector<UIHotkey> hotkeys;				// list of hotkeys

	bool clickerActive = false;				// clicker is active or not
	bool dropCoinsActive = false;			// coin dropper is active or not

	clock_t clickerTimer;					// timer for clicker
	clock_t dropCoinsTimer;					// timer for coin dropper
	
	unsigned int ruptSkillID = 0;			// skill id of the skill to rupt
	unsigned int ruptSkillSlot = 0;			// skill slot of the skill to rupt with
	bool ruptActive = false;				// rupter active or not

	float movementX = 0;					// X coordinate of the destination of movement macro
	float movementY = 0;					// Y coordinate of the destination of movement macro


public:
	Hotkeys();
	~Hotkeys();

	inline bool toggleClicker() { return clickerActive = !clickerActive; }
	inline bool toggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }
	inline bool toggleRupt() { return ruptActive = !ruptActive; }

	void loadIni();							// load settings from ini file
	OSHGui::Panel* buildUI();				// create user interface
	void mainRoutine();						// do... nothing atm

};