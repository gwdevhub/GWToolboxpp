#pragma once 

#include "ToolboxWindow.h"
#include "Missions.h"

// class used to keep a list of hotkeys, capture keyboard event and fire hotkeys as needed
class MissionsWindow : public ToolboxWindow {
	MissionsWindow() {};
	~MissionsWindow() {};
public:
	static MissionsWindow& Instance() {
		static MissionsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Missions"; }

	void Initialize() override;
	void Initialize_Prophecies();
	void Initialize_Factions();
	void Initialize_Nightfall();
	void Initialize_EotN();
	void Initialize_Dungeons();
	void Terminate() override;
	void Draw(IDirect3DDevice9* pDevice) override;


	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	std::map<Missions::Campaign, std::vector<Missions::Mission*>> missions{
		{ Missions::Campaign::Prophecies, {} },
		{ Missions::Campaign::Factions, {} },
		{ Missions::Campaign::Nightfall, {} },
		{ Missions::Campaign::EyeOfTheNorth, {} },
		{ Missions::Campaign::Dungeon, {} },
	};

	int missions_per_row = 5;
};
