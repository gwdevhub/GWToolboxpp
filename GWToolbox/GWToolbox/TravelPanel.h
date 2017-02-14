#pragma once

#include <string>
#include <Windows.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Constants\Maps.h>

#include <OSHGui\OSHGui.hpp>

#include "ToolboxPanel.h"
#include "logger.h"

class TravelPanel : public ToolboxPanel {
public:
	const int n_outposts = 181;
	const char* Name() override { return "Travel Panel"; }

	TravelPanel();

	bool TravelFavorite(unsigned int idx);

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettings() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	void TravelButton(const char* text, int x_idx, GW::Constants::MapID mapid);
	GW::Constants::MapID IndexToOutpostID(int index);

	GW::Constants::District district;
	int district_number;

	int fav_count;
	std::vector<int> fav_index;
};
