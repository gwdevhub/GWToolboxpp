#pragma once

#include "../include/OSHGui/OSHGui.hpp"

class TravelPanel : public OSHGui::Panel {
private:
	const int WIDTH = 200;
	const int HEIGHT = 300;
	const int SPACE = 6;
	const int BUTTON_WIDTH = (WIDTH - 3 * SPACE) / 2;
	const int BUTTON_HEIGHT = 25;
	

	int region_;
	int district_;
	int language_;

	void UpdateDistrict(int gui_index);

public:
	TravelPanel();

	int region() { return region_; }
	int district() { return district_; }
	int language() { return language_; }

	void BuildUI();				// create user interface
};

