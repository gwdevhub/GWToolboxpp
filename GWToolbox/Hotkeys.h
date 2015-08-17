#pragma once

#include <vector>
#include <functional>

#include "Timer.h"
#include "../API/APIMain.h"

using namespace std;

typedef unsigned char BYTE;
//typedef std::function<void(const Hotkeys&)> HotkeyCallback;

class Hotkeys {
private:
	BYTE initializer;
	const BYTE Stuck;
	const BYTE Recall;
	const BYTE UA;
	const BYTE Resign;
	const BYTE TeamResign;
	const BYTE Clicker;
	const BYTE Res;
	const BYTE Age;
	const BYTE Pstone;
	const BYTE GhostTarget;
	const BYTE GhostPop;
	const BYTE GstonePop;
	const BYTE LegioPop;
	const BYTE RainbowUse;
	const BYTE Identifier;
	const BYTE Rupt;
	const BYTE Movement;
	const BYTE Drop1Coin;
	const BYTE DropCoins;
	const BYTE count;

	bool clickerToggle = false;				// clicker is active or not
	bool dropCoinsToggle = false;			// coin dropper is active or not

	timer_t clickerTimer;					// timer for clicker
	timer_t dropCoinsTimer;					// timer for coin dropper
	
	unsigned int ruptSkillID = 0;			// skill id of the skill to rupt
	unsigned int ruptSkillSlot = 0;			// skill slot of the skill to rupt with
	bool ruptToggle = false;				// rupter active or not

	float movementX = 0;					// X coordinate of the destination of movement macro
	float movementY = 0;					// Y coordinate of the destination of movement macro
	
	vector<string> hotkeyName;				// name of hotkeys in Ini file

	vector<void(*)(Hotkeys*) > callbacks;	// functions to call when hotkey is used

	static void callbackStuck(Hotkeys* self);
	static void callbackRecall(Hotkeys* self);
	static void callbackUA(Hotkeys* self);
	static void callbackResign(Hotkeys* self);
	static void callbackTeamResign(Hotkeys* self);
	static void callbackClicker(Hotkeys* self);
	static void callbackRes(Hotkeys* self);
	static void callbackAge(Hotkeys* self);
	static void callbackPstone(Hotkeys* self);
	static void callbackGhostTarget(Hotkeys* self);
	static void callbackGhostPop(Hotkeys* self);
	static void callbackGstonePop(Hotkeys* self);
	static void callbackLegioPop(Hotkeys* self);
	static void callbackRainbowUse(Hotkeys* self);
	static void callbackLooter(Hotkeys* self);
	static void callbackIdentifier(Hotkeys* self);
	static void callbackRupt(Hotkeys* self);
	static void callbackMovement(Hotkeys* self);
	static void callbackDrop1Coin(Hotkeys* self);
	static void callbackDropCoins(Hotkeys* self);

	static inline bool isLoading()		{ return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	static inline bool isExplorable()	{ return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	static inline bool isOutpost()		{ return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

public:
	Hotkeys();
	~Hotkeys();

	void loadIni();							// load settings from ini file
	void buildUI();							// create user interface
	void mainRoutine();						// do... nothing atm

};