#pragma once

#include <string>
#include <vector>
#include <Windows.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Constants\Maps.h>

#include "ToolboxWindow.h"
#include "logger.h"

class TravelWindow : public ToolboxWindow {
	TravelWindow() {};
	~TravelWindow() {};
public:
	static TravelWindow& Instance() {
		static TravelWindow instance;
		return instance;
	}

	const char* Name() const override { return "Travel"; }

	void Initialize() override;

	bool TravelFavorite(unsigned int idx);

	void Travel(GW::Constants::MapID MapID, GW::Constants::District district, int district_number = 0);
	void ScrollToOutpost(GW::Constants::MapID outpost_id, GW::Constants::District district = GW::Constants::District::Current, int district_number = 0);
	bool IsWaitingForMapTravel();

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void Update(float delta) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	// ==== Helpers ====
	void TravelButton(const char* text, int x_idx, GW::Constants::MapID mapid);
	GW::Constants::MapID IndexToOutpostID(int index);

	// ==== Travel variables ====
	GW::Constants::District district;
	int district_number;

	// ==== Favorites ====
	int fav_count;
	std::vector<int> fav_index;

	// ==== options ====
	bool close_on_travel = false;

	// ==== scroll to outpost ====
	GW::Constants::MapID scroll_to_outpost_id = GW::Constants::MapID::None;		// Which outpost do we want to end up in?
	GW::Constants::MapID scroll_from_outpost_id = GW::Constants::MapID::None;	// Which outpost did we start from?

	bool map_travel_countdown_started = false;
	bool pending_map_travel = false;
};
