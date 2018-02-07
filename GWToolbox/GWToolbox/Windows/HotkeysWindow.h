#pragma once 

#include <vector>
#include <functional>
#include <algorithm>
#include <Windows.h>
#include <string>

#include "Key.h"
#include "ToolboxWindow.h"
#include "logger.h"
#include "Timer.h"
#include "Hotkeys.h"

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class HotkeysWindow : public ToolboxWindow {
	HotkeysWindow() {};
	~HotkeysWindow() {};
public:
	static HotkeysWindow& Instance() {
		static HotkeysWindow instance;
		return instance;
	}

	const char* Name() const override { return "Hotkeys"; }

	void Initialize() override;
	void Terminate() override;
	
	inline bool ToggleClicker() { return clickerActive = !clickerActive; }
	inline bool ToggleCoinDrop() { return dropCoinsActive = !dropCoinsActive; }
	inline bool ToggleRupt() { return ruptActive = !ruptActive; }

	// Update. Will always be called every frame.
	void Update(float delta) override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	std::vector<TBHotkey*> hotkeys;				// list of hotkeys

	long max_id_;
	bool block_hotkeys = false;

	bool clickerActive = false;				// clicker is active or not
	bool dropCoinsActive = false;			// coin dropper is active or not

	clock_t clickerTimer;					// timer for clicker
	clock_t dropCoinsTimer;					// timer for coin dropper

	unsigned int ruptSkillID = 0;			// skill id of the skill to rupt
	unsigned int ruptSkillSlot = 0;			// skill slot of the skill to rupt with
	bool ruptActive = false;				// rupter active or not

	float movementX = 0;					// X coordinate of the destination of movement macro
	float movementY = 0;					// Y coordinate of the destination of movement macro
};
