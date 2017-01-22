#pragma once

#include <list>
#include <vector>

#include <GWCA\Constants\Constants.h>

#include "Timer.h"
#include "Pcons.h"
#include "ToolboxPanel.h"

class PconPanel : public ToolboxPanel {
private:
	Pcon* essence;
	Pcon* grail;
	Pcon* armor;
	Pcon* alcohol;
	Pcon* redrock;
	Pcon* bluerock;
	Pcon* greenrock;
	Pcon* pie;
	Pcon* cupcake;
	Pcon* apple;
	Pcon* corn;
	Pcon* egg;
	Pcon* kabob;
	Pcon* warsupply;
	Pcon* lunars;
	Pcon* skalesoup;
	Pcon* pahnai;
	Pcon* city;
	std::vector<Pcon*> pcons;
	bool initialized;	// true if the feature is initialized
	bool enabled;		// true if the feature is enabled, false otherwise 
	GW::Constants::InstanceType current_map_type;
	clock_t scan_inventory_timer;

public:
	PconPanel(OSHGui::Control* parent);

	bool SetActive(bool active);
	inline void ToggleActive() { SetActive(!enabled); }

	void BuildUI() override;

	// Update. Will always be called every frame.
	void Main() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;
};
