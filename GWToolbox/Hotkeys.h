#pragma once

#include <vector>
#include "Timer.h"
#include "../API/APIMain.h"

using namespace std;

typedef unsigned char BYTE;

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
	const BYTE Looter;
	const BYTE Identifier;
	const BYTE Rupt;
	const BYTE Movement;
	const BYTE Drop1Coin;
	const BYTE DropCoins;
	const BYTE count;

	bool clickerToggle = false;			// clicker is active or not
	bool dropCoinsToggle = false;		// coin dropper is active or not

	timer_t clickerTimer;				// timer for clicker
	timer_t dropCoinsTimer;				// timer for coin dropper
	
	unsigned int ruptSkillID = 0;		// skill id of the skill to rupt
	unsigned int ruptSkillSlot = 0;		// skill slot of the skill to rupt with
	bool ruptToggle = false;			// rupter active or not

	int movementX = 0;					// X coordinate of the destination of movement macro
	int movementY = 0;					// Y coordinate of the destination of movement macro
	
	vector<string> hotkeyName;			// name of hotkeys in Ini file

	vector<void(Hotkeys::*)()> callbacks;		// functions to call when hotkey is used

	void callbackStuck();
	void callbackRecall();
	void callbackUA();
	void callbackResign();
	void callbackTeamResign();
	void callbackClicker();
	void callbackRes();
	void callbackAge();
	void callbackPstone();
	void callbackGhostTarget();
	void callbackGhostPop();
	void callbackGstonePop();
	void callbackLegioPop();
	void callbackRainbowUse();
	void callbackLooter();
	void callbackIdentifier();
	void callbackRupt();
	void callbackMovement();
	void callbackDrop1Coin();
	void callbackDropCoins();

	inline bool isLoading()		{ return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Loading; }
	inline bool isExplorable()	{ return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Explorable; }
	inline bool isOutpost()		{ return GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost; }

public:
	Hotkeys();
	~Hotkeys();

	void loadIni();							// load settings from ini file
	void buildUI();							// create user interface
	void mainRoutine();						// do... nothing atm

};