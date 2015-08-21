#pragma once

#include <list>
#include <vector>
#include "../include/OSHGui/OSHGui.hpp"
#include "Timer.h"

using namespace std;

typedef unsigned int UINT;

class Pcon : public OSHGui::Button {
public:
	static const int WIDTH = 50;
	static const int HEIGHT = 50;

private:
	OSHGui::Label* back;
	OSHGui::PictureBox* pic;
	OSHGui::Label* shadow;
	const wchar_t* iniName;

protected:
	const wchar_t* chatName;
	int quantity;
	bool enabled;
	UINT itemID;
	UINT effectID;
	int threshold;
	timer_t timer;

public:
	Pcon(const wchar_t* ini);
	virtual void DrawSelf(OSHGui::Drawing::RenderContext &context) override;
	virtual void CalculateLabelLocation() override {};

	void setIcon(const char* icon, int xOff, int yOff, int size);
	inline void setChatName(const wchar_t* chat) { chatName = chat; }
	inline void setItemID(UINT item) { itemID = item; }
	inline void setEffectID(UINT effect) { effectID = effect; }
	inline void setThreshold(int t) { threshold = t; }

	virtual void checkAndUse();		// checks if need to use pcon, uses if needed
	virtual void scanInventory();	// scans inventory, updates quantity field
	void updateLabel();		// updates the label with quantity and color
	void toggleActive();	// disables if enabled, enables if disabled
};

class PconCons : public Pcon {
public:
	PconCons(const wchar_t* ini)
		: Pcon(ini) {}

	void checkAndUse() override;
};

class PconCity : public Pcon {
public:
	PconCity(const wchar_t* ini)
		: Pcon(ini) {}

	void checkAndUse() override;
	void scanInventory() override;
};

class PconAlcohol : public Pcon {
public:
	PconAlcohol(const wchar_t* ini)
		: Pcon(ini) {}

	void checkAndUse() override;
	void scanInventory() override;
};

class PconLunar : public Pcon {
public:
	PconLunar(const wchar_t* ini)
		: Pcon(ini) {}

	void checkAndUse() override;
	void scanInventory() override;
};

class Pcons {
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
	
	// true if the feature is enabled, false otherwise 
	bool enabled;

	// scans inventory and updates UI
	void scanInventory();

public:
	Pcons();
	~Pcons();

	void enable() { enabled = true; }
	void disable() { enabled = false; }
	
	void loadIni();				// load settings from ini file 
	OSHGui::Panel* buildUI();	// create user interface
	void mainRoutine();			// runs one loop of the main routine (checking each pcon once)
};
