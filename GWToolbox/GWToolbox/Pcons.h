#pragma once

#include <stdint.h>

#include "OSHGui\OSHGui.hpp"

#include <GWCA\Constants\Skills.h>

#include "Timer.h"


class Pcon : public OSHGui::Button {
public:
	static const int WIDTH = 46;
	static const int HEIGHT = WIDTH;

private:
	OSHGui::PictureBox* pic;
	OSHGui::PictureBox* tick;
	OSHGui::Label* shadow;
	const char* iniName;

protected:
	const char* chatName;
	int quantity;
	bool enabled;
	unsigned int itemID;
	GW::Constants::SkillID effectID;
	int threshold; // quantity at which the number color goes from green to yellow
	clock_t timer;
	clock_t update_timer;
	bool update_ui;
	void CheckUpdateTimer();

public:
	Pcon(OSHGui::Control* parent, const char* ini);
	void CalculateLabelLocation() override {};

	void setIcon(const char* icon, int xOff, int yOff, int size);
	inline void setChatName(const char* chat) { chatName = chat; }
	inline void setItemID(unsigned int item) { itemID = item; }
	inline void setEffectID(GW::Constants::SkillID effect) { effectID = effect; }
	inline void setThreshold(int t) { threshold = t; }

	virtual bool checkAndUse();		// checks if need to use pcon, uses if needed. Returns true if was used.
	void UpdateUI();
	virtual void scanInventory();	// scans inventory, updates quantity field
	void toggleActive();			// disables if enabled, enables if disabled
};

class PconCons : public Pcon {
public:
	PconCons(OSHGui::Control* parent, const char* ini) : Pcon(parent, ini) {}

	bool checkAndUse() override;
};

class PconCity : public Pcon {
public:
	PconCity(OSHGui::Control* parent, const char* ini) : Pcon(parent, ini) {}

	bool checkAndUse() override;
	void scanInventory() override;
};

class PconAlcohol : public Pcon {
public:
	PconAlcohol(OSHGui::Control* parent, const char* ini) : Pcon(parent, ini) {}

	bool checkAndUse() override;
	void scanInventory() override;
};

class PconLunar : public Pcon {
public:
	PconLunar(OSHGui::Control* parent, const char* ini) : Pcon(parent, ini) {}

	bool checkAndUse() override;
	void scanInventory() override;
};
