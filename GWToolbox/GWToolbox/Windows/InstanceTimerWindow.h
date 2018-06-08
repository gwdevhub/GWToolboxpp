#pragma once

#include <Windows.h>
#include <vector>
#include <Defines.h>

#include "ToolboxWindow.h"

class InstanceTimerWindow : public ToolboxWindow {
	InstanceTimerWindow() {};
	~InstanceTimerWindow() {};
public:
	static InstanceTimerWindow& Instance() {
		static InstanceTimerWindow instance;
		return instance;
	}

	const char* Name() const override { return "Instance Timer"; }

	void Initialize() override;

	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	//DoA
	float doaChestX[4] = { 227,10943,3435,-14436 };
	float doaChestY[4] = { -10264,-10151,15590,-523  };
	DWORD doaQ[4] = { 0,0,0,0 };
	char doatime[4][32];
	char DoAArea[4][10] = { "City","Veil","Gloom","Foundry" };
	int DOAIDs[4] = { 0x2742,0x2740,0x2741,0x273F };
	//	Gate to  	  Veil, Gloom, Foundry, City    is open!
	//					0       1     2       3
	DWORD DOAD[4] = { 0,0,0,0 };

	//Underworld
	int UWIDs[11] = { 146,147,148,149,150,151,152,153,154,155,157 };
	//                Chamber, Restore, Escort, UWG, Vale, Wastes, Pits, Planes, Mnts Pools, dhuum
	//                   0        1        2     3     4      5      6     7      8      9      10
	DWORD UWT[10] = { 0,0,0,0,0,0,0,0,0,0 };
	DWORD UWD[10] = { 0,0,0,0,0,0,0,0,0,0 };
	char UWTtime[10][32];
	char UWDtime[10][32];
	char UWArea[10][10] = { "Chamber","Restore","Escort","UWG","Vale","Wastes","Pits","Planes","Mnts","Pools" };
	//Dhuum
	DWORD DhuumS = 0;
	DWORD DhuumD = 0;
	char DhuumStarttime[32];
	char DhuumDonetime[32];

	//Fissure of Woe
	int FOWIDs[11] = { 309,310,311,312,313,314,315,316,317,318,319 };
	//                 ToC, Wailing Lord, Griffons, Defend Forge, Camp, Menzies, Cave, Khobay, ToS, Burning Fores, Hunt
	//                   0        1           2           3         4      5       6     7      8         9         10
	long FOWT[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
	long FOWD[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
	char FOWTtime[11][32];
	char FOWDtime[11][32];
	char FOWArea[11][16] = { "ToC","Wailing Lord","Griffons","Defend","Camp","Menzies","Restore","Khobay","ToS","Burning Forest", "The Hunt" };

	bool doareset = false;
	bool uwreset = false;
	bool fowreset = false;
	bool show_uwtimer = true;
	bool show_doatimer = true;
	bool show_fowtimer = true;
	char buf[256];	
};