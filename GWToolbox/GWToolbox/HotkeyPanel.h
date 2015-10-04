#pragma once 

#include <vector>
#include <functional>
#include <algorithm>
#include <Windows.h>
#include <string>

#include "OSHGui\OSHGui.hpp"
#include "GWCA\APIMain.h"

#include "ToolboxPanel.h"
#include "logger.h"
#include "Timer.h"
#include "Hotkeys.h"

using namespace GWAPI;
using namespace std;


// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeyPanel : public ToolboxPanel {
private:
	static const int MAX_SHOWN = 4;			// number of hotkeys shown in interface
	int first_shown_;						// index of first one shown

	vector<TBHotkey*> hotkeys;				// list of hotkeys

	OSHGui::ScrollBar* scrollbar_;
	OSHGui::ComboBox* delete_combo_;

	long max_id_;

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

protected:
	bool OnMouseScroll(const OSHGui::MouseMessage &mouse) override;

public:
	HotkeyPanel();
	
	inline bool ToggleClicker() { return clickerActive = !clickerActive; }
	inline bool ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }
	inline bool ToggleRupt() { return ruptActive = !ruptActive; }

	void LoadIni();				// load settings from ini file
	void BuildUI() override;	// create user interface
	inline void UpdateUI() override {};
	void MainRoutine() override;

	bool ProcessMessage(LPMSG msg);
	void set_first_shown(int first);

	void DrawSelf(OSHGui::Drawing::RenderContext &context) override;

	void AddHotkey(TBHotkey* hotkey);
	void DeleteHotkey(int index);
	void UpdateDeleteCombo();
	void UpdateScrollBarMax();

	inline long NewID() { return ++max_id_; }
};
