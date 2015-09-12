#pragma once

#include "../include/OSHGui/OSHGui.hpp"

class InfoPanel : public OSHGui::Panel {
private:
	OSHGui::Label* player_x;
	OSHGui::Label* player_y;
	OSHGui::Label* target_id;
	OSHGui::Label* map_id;
	OSHGui::Label* item_id;
	OSHGui::Label* dialog_id;

public:
	InfoPanel();

	void BuildUI();				// create user interface

	void UpdateUI();
};

