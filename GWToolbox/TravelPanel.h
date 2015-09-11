#pragma once
#include <Windows.h>
#include "../include/OSHGui/OSHGui.hpp"
#include "../API/GwConstants.h"

class TravelPanel : public OSHGui::Panel {
private:
	const int WIDTH = 200;
	const int HEIGHT = 300;
	const int SPACE = 6;
	const int BUTTON_WIDTH = (WIDTH - 3 * SPACE) / 2;
	const int BUTTON_HEIGHT = 25;
	

	DWORD region_;
	DWORD district_;
	DWORD language_;

	void AddTravelButton(std::string text, int grid_x, int grid_y, GwConstants::MapID map_id);
	void UpdateDistrict(int gui_index);

public:
	TravelPanel();

	DWORD region() { return region_; }
	DWORD district() { return district_; }
	DWORD language() { return language_; }

	void BuildUI();				// create user interface
};

