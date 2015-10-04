#pragma once

#include <stdint.h>

#include "OSHGui\OSHGui.hpp"

#include "GWCA\GwConstants.h"

#include "Timer.h"



class Pcon : public OSHGui::Button {
public:
	static const int WIDTH = 46;
	static const int HEIGHT = WIDTH;

private:
	OSHGui::PictureBox* pic;
	OSHGui::PictureBox* tick;
	OSHGui::Label* shadow;
	const wchar_t* iniName;

protected:
	const wchar_t* chatName;
	int quantity;
	bool enabled;
	unsigned int itemID;
	GwConstants::SkillID effectID;
	int threshold;
	clock_t timer;
	clock_t update_timer;
	bool update_ui;
	void CheckUpdateTimer();

public:
	Pcon(const wchar_t* ini);
	void DrawSelf(OSHGui::Drawing::RenderContext &context) override;
	void CalculateLabelLocation() override {};

	void setIcon(const char* icon, int xOff, int yOff, int size);
	inline void setChatName(const wchar_t* chat) { chatName = chat; }
	inline void setItemID(unsigned int item) { itemID = item; }
	inline void setEffectID(GwConstants::SkillID effect) { effectID = effect; }
	inline void setThreshold(int t) { threshold = t; }

	virtual bool checkAndUse();		// checks if need to use pcon, uses if needed. Returns true if was used.
	void UpdateUI();
	virtual void scanInventory();	// scans inventory, updates quantity field
	void toggleActive();	// disables if enabled, enables if disabled
};

class PconCons : public Pcon {
public:
	PconCons(const wchar_t* ini)
		: Pcon(ini) {
	}

	bool checkAndUse() override;
};

class PconCity : public Pcon {
public:
	PconCity(const wchar_t* ini)
		: Pcon(ini) {
	}

	bool checkAndUse() override;
	void scanInventory() override;
};

class PconAlcohol : public Pcon {
public:
	PconAlcohol(const wchar_t* ini)
		: Pcon(ini) {
	}

	bool checkAndUse() override;
	void scanInventory() override;
};

class PconLunar : public Pcon {
public:
	PconLunar(const wchar_t* ini)
		: Pcon(ini) {
	}

	bool checkAndUse() override;
	void scanInventory() override;
};
