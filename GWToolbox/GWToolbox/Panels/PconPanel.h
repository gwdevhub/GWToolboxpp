#pragma once

#include <list>
#include <vector>

#include <GWCA\Constants\Constants.h>

#include "Timer.h"
#include "Pcons.h"
#include "ToolboxPanel.h"

class PconPanel : public ToolboxPanel {
public:
	static PconPanel* Instance();

	const char* Name() const override { return "Pcon Panel"; }
	const char* TabButtonText() const override { return "Pcons"; }

	PconPanel();
	~PconPanel() {};

	bool SetEnabled(bool b);
	inline void ToggleEnable() { SetEnabled(!enabled); }

	void Update() override;

	bool DrawTabButton(IDirect3DDevice9* device) override;
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

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
	GW::Constants::InstanceType current_map_type;
	clock_t scan_inventory_timer;
	bool enabled = false;
	
	// Interface Settings
	bool tick_with_pcons = false;
	int items_per_row = 3;

	// Pcon Settings
	// todo: tonic pop?
	// todo: morale / dp removal
	// todo: replenish character pcons from chest?
};
