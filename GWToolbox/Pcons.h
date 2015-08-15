#pragma once

#include <list>
#include <vector>

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
	bool pcons_enabled;

	// map from pcons to ini name
	vector<string> pconsName;

	// map from pcons to item id
	vector<int> pconsItemID;

	// map from pcons to status active
	vector<bool> pconsActive;

	// checks if the pcon is in inventory
	bool pconsFind(unsigned int ModelID);

	// scans inventory and updates UI
	void scanInventory();

public:
	Pcons();
	~Pcons() {}

	void enable() { pcons_enabled = true; }
	void disable() { pcons_enabled = false; }
	
	void pconsLoadIni();
	void pconsBuildUI();

};
