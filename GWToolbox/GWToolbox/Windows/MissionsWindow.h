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
	void Terminate() override;

	// Update. Will always be called every frame.
	// void Update(float delta) override;

	// Draw user interface. Will be called every frame if the element is visible
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
	float icon_size = 48.0f;
};
