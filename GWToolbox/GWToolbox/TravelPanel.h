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
	TravelPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}

	void BuildUI() override;

	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override {}

	void TravelFavorite(int fav_idx);

private:
	const int n_outposts = 180;

	class TravelCombo : public OSHGui::ComboBox {
	public:
		TravelCombo(OSHGui::Control* parent);
	};

	GW::Constants::District district_;
	int district_number_;

	void AddTravelButton(std::wstring text, int grid_x, int grid_y, 
		GW::Constants::MapID map_id);
	void UpdateDistrict(int gui_index);
	std::wstring IndexToOutpostName(int index);
	GW::Constants::MapID IndexToOutpostID(int index);

	OSHGui::ComboBox* combo_boxes_[3];
};
