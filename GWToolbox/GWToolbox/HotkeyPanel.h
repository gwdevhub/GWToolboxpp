#pragma once 

#include <vector>
#include <functional>
#include <algorithm>
#include <Windows.h>
#include <string>

#include <OSHGui\OSHGui.hpp>
#include <GWCA\GWCA.h>

#include "ToolboxPanel.h"
#include "logger.h"
#include "Timer.h"
#include "Hotkeys.h"

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeyPanel : public ToolboxPanel {
private:
	std::vector<TBHotkey*> hotkeys;				// list of hotkeys

	OSHGui::ComboBox* create_combo_;
	OSHGui::ComboBox* delete_combo_;
	OSHGui::ScrollPanel* scroll_panel_;

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

public:
	HotkeyPanel(OSHGui::Control* parent);
	
	inline bool ToggleClicker() { return clickerActive = !clickerActive; }
	inline bool ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }
	inline bool ToggleRupt() { return ruptActive = !ruptActive; }

	void BuildUI() override;

	// Update. Will always be called every frame.
	void Main() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override {}

	bool ProcessMessage(LPMSG msg);

	void UpdateDeleteCombo();

private:
	void AddHotkey(TBHotkey* hotkey);
	void DeleteHotkey(int index);
	void UpdateScrollBarMax();

	inline long NewID() { return ++max_id_; }
};
