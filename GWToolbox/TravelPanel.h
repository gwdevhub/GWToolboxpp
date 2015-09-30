#pragma once
#include <string>
#include <Windows.h>
#include "../include/OSHGui/OSHGui.hpp"
#include "ToolboxPanel.h"
#include "../API/GwConstants.h"
#include "logger.h"

class TravelPanel : public ToolboxPanel {
private:
	static const int BUTTON_HEIGHT = 25;
	const int n_outposts = 185;

	class TravelCombo : public OSHGui::ComboBox {
	public:
		TravelCombo();
	};

	bool current_district_;
	DWORD region_;
	DWORD district_;
	DWORD language_;

	void AddTravelButton(std::string text, int grid_x, int grid_y, 
		GwConstants::MapID map_id);
	void UpdateDistrict(int gui_index);
	std::string IndexToOutpostName(int index);
	GwConstants::MapID IndexToOutpostID(int index);

public:
	TravelPanel();

	DWORD region();
	DWORD district() { return district_; }
	DWORD language();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override {};
};

