#pragma once

#include <list>
#include <vector>

#include "Timer.h"

using namespace std;

//enum class Pcon { Cons, Alcohol, RRC, BRC, GRC, Pie, Cupcake, Apple, Corn, Egg, 
//	Kabob, Warsupply, Lunars, Res, Skalesoup, Mobstoppers, Panhai, City};

namespace pcons {
	const int Cons = 0;
	const int Alcohol = 1;
	const int RRC = 2;
	const int BRC = 3;
	const int GRC = 4;
	const int Pie = 5;
	const int Cupcake = 6;
	const int Apple = 7;
	const int Corn = 8;
	const int Egg = 9;
	const int Kabob = 10;
	const int Warsupply = 11;
	const int Lunars = 12;
	const int Res = 13;
	const int Skalesoup = 14;
	const int Mobstoppers = 15;
	const int Panhai = 16;
	const int City = 17;
}

class Pcons {
private:

	unsigned int pconsCount = 18;

	// true if the feature is enabled, false otherwise 
	bool enabled;

	// map from each pcons to ini name
	vector<string> pconsName;

	// map from pcons to item id
	vector<int> pconsItemID;

	// map from pcons to status active (bool) 
	vector<bool> pconsActive;

	// list of pcons timers 
	vector<timer_t> pconsTimer;
 
	// list of pcons effects 
	vector<int> pconsEffect;

	// checks if the pcon is in inventory 
	bool pconsFind(unsigned int ModelID);

	// it will use if needed the pcon. Same as main routine,
	// but only for one pcon
	void checkAndUsePcon(int PconID);

	// scans inventory, updates UI and the pconsActive array 
	void scanInventory();

public:
	Pcons();
	~Pcons();

	void enable() { enabled = true; }
	void disable() { enabled = false; }
	
	// load settings from ini file 
	void loadIni();

	// create user interface
	void buildUI();

	// runs one loop of the main routine (checking each pcon once)
	void mainRoutine();
};
