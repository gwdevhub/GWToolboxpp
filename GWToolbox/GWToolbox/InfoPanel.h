#pragma once

#include <GWCA\Constants\Constants.h>

#include "ToolboxPanel.h"

class InfoPanel : public ToolboxPanel {
private:
	OSHGui::Label* player_x;
	OSHGui::Label* player_y;
	OSHGui::Label* target_id;
	OSHGui::Label* map_id;
	OSHGui::Label* item_id;
	OSHGui::Label* dialog_id;

	float current_player_x;
	float current_player_y;
	long current_target_id;
	GW::Constants::MapID current_map_id;
	GW::Constants::InstanceType current_map_type;
	long current_item_id;
	long current_dialog_id;

public:
	InfoPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}

	void BuildUI() override;
	void UpdateUI() override;
	void MainRoutine() override {};
};

