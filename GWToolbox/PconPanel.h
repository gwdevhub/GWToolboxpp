#pragma once

#include <list>
#include <vector>
#include "../include/OSHGui/OSHGui.hpp"
#include "Timer.h"
#include "Pcons.h"

class PconPanel : public OSHGui::Panel {
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
	
	bool initialized;	// true if the feature is initialized
	bool enabled;		// true if the feature is enabled, false otherwise 

	// scans inventory and updates UI
	void scanInventory();

public:
	PconPanel();

	void enable() { enabled = true; }
	void disable() { enabled = false; }
	bool toggleActive() { return enabled = !enabled; }
	
	void buildUI();	// create user interface
	void mainRoutine();			// runs one loop of the main routine (checking each pcon once)
};
