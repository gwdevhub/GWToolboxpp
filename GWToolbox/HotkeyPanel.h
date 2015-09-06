#pragma once 

#include <vector>
#include <functional>
#include <algorithm>
#include <Windows.h>
#include <string>

#include "../include/OSHGui/OSHGui.hpp"

#include "../API/APIMain.h"
#include "logger.h"
#include "Timer.h"
#include "Hotkeys.h"

using namespace GWAPI;
using namespace std;


// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeyPanel : public OSHGui::Panel {
private:
	static const int MAX_SHOWN = 4;
	int first_shown_;

	vector<TBHotkey*> hotkeys;				// list of hotkeys

	OSHGui::ScrollBar* scrollbar_;

	bool clickerActive = false;				// clicker is active or not
	bool dropCoinsActive = false;			// coin dropper is active or not

	clock_t clickerTimer;					// timer for clicker
	clock_t dropCoinsTimer;					// timer for coin dropper

	unsigned int ruptSkillID = 0;			// skill id of the skill to rupt
	unsigned int ruptSkillSlot = 0;			// skill slot of the skill to rupt with
	bool ruptActive = false;				// rupter active or not

	float movementX = 0;					// X coordinate of the destination of movement macro
	float movementY = 0;					// Y coordinate of the destination of movement macro

	void CalculateHotkeyPositions();
	void ResetHotkeyPositions();

public:
	HotkeyPanel();
	
	inline bool toggleClicker() { return clickerActive = !clickerActive; }
	inline bool toggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }
	inline bool toggleRupt() { return ruptActive = !ruptActive; }

	void loadIni();							// load settings from ini file
	void buildUI();				// create user interface
	void mainRoutine();						// do... nothing atm

	bool ProcessMessage(LPMSG msg);
	void set_first_shown(int first);

	void DrawSelf(OSHGui::Drawing::RenderContext &context) override;
};
