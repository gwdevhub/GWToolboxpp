#pragma once

#include <list>
#include <vector>

#include "Timer.h"

using namespace std;

typedef unsigned char BYTE;

class Pcons {
private:
	BYTE initializer;
	const BYTE Cons;
	const BYTE Alcohol;
	const BYTE RRC;
	const BYTE BRC;
	const BYTE GRC;
	const BYTE Pie;
	const BYTE Cupcake;
	const BYTE Apple;
	const BYTE Corn;
	const BYTE Egg;
	const BYTE Kabob;
	const BYTE Warsupply;
	const BYTE Lunars;
	const BYTE Res;
	const BYTE Skalesoup;
	const BYTE Mobstoppers;
	const BYTE Panhai;
	const BYTE City;
	const BYTE count;
	
	bool enabled;						// true if the feature is enabled, false otherwise 

	vector<string> pconsName;			// map from each pcon to ini name
	vector<wstring> pconsChatName;		// map from each pcon to chat name
	vector<int> pconsItemID;			// map from pcons to item id
	vector<bool> pconsActive;			// map from pcons to status active (bool) 
	vector<timer_t> pconsTimer;			// list of pcons timers 
	vector<int> pconsEffect;			// list of pcons effects 

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
	
	void loadIni();			// load settings from ini file 
	void buildUI();			// create user interface
	void mainRoutine();		// runs one loop of the main routine (checking each pcon once)
};
