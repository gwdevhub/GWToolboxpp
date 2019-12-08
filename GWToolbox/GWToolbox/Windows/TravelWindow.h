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

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	static void Travel(GW::Constants::MapID map_id,
		GW::Constants::District district, int district_number = 0);

private:
	// ==== Helpers ====
	void TravelButton(const char* text, int x_idx, GW::Constants::MapID mapid);
	GW::Constants::MapID IndexToOutpostID(int index);

	// ==== Travel variables ====
	GW::Constants::District district = GW::Constants::District::Current;
	int district_number = 0;

	// ==== Favorites ====
	int fav_count = 0;
	std::vector<int> fav_index;

	// ==== options ====
	bool close_on_travel = false;
};
