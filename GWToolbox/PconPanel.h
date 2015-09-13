#pragma once

#include <list>
#include <vector>
#include "Timer.h"
#include "Pcons.h"
#include "../API/GwConstants.h"
#include "ToolboxPanel.h"

class PconPanel : public ToolboxPanel {
private:
	static const int WIDTH = 6 * 2 + Pcon::WIDTH * 3;
	static const int HEIGHT = 6 * 2 + Pcon::HEIGHT * 6;

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
	
	bool initialized;	// true if the feature is initialized
	bool enabled;		// true if the feature is enabled, false otherwise 
	GwConstants::InstanceType current_map_type;
	clock_t scan_inventory_timer;

	// scans inventory and updates UI
	void ScanInventory();

public:
	PconPanel();

	void Enable() { enabled = true; }
	void Disable() { enabled = false; }
	bool ToggleActive() { return enabled = !enabled; }
	
	void BuildUI();	// create user interface
	void MainRoutine();			// runs one loop of the main routine (checking each pcon once)
};
