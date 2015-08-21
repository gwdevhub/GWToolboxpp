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
	static const int PIC_SIZE = 64;

private:
	OSHGui::PictureBox* pic;
	OSHGui::Label* shadow;
	int quantity;
	bool enabled;
	const wchar_t* iniName;
	const wchar_t* chatName;
	UINT itemID;
	UINT effectID;
	int threshold;
	timer_t timer;

public:
	Pcon(const wchar_t* ini, int xOffset, int yOffset);
	virtual void DrawSelf(OSHGui::Drawing::RenderContext &context) override;
	virtual void CalculateLabelLocation() override {};

	void setIcon(const char* icon);
	inline void setChatName(const wchar_t* chat) { chatName = chat; }
	inline void setItemID(UINT item) { itemID = item; }
	inline void setEffectID(UINT effect) { effectID = effect; }
	inline void setThreshold(int t) { threshold = t; }

	void checkAndUse();
	void scanInventory();
	void toggleActive();
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
