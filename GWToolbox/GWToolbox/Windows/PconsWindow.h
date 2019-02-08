#pragma once

#include <list>
#include <vector>
#include <Defines.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\GameEntities\Position.h>
#include <GWCA\GameEntities\Agent.h>

#include "Timer.h"
#include "Pcons.h"
#include "ToolboxWindow.h"

class PconsWindow : public ToolboxWindow {
	PconsWindow() {};
	~PconsWindow() {};
public:
	static PconsWindow& Instance() {
		static PconsWindow instance;
		return instance;
	}

	const char* Name() const override { return "Pcons"; }

	void Initialize() override;

	bool SetEnabled(bool b);
	bool GetEnabled();

	inline void ToggleEnable() { SetEnabled(!enabled); }

	void Update(float delta) override;

	bool DrawTabButton(IDirect3DDevice9* device, bool show_icon, bool show_text) override;
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	void CheckIfWeJustEnabledAlcoholWithLunarsOn();


	std::vector<Pcon*> pcons;
	PconAlcohol* pcon_alcohol = nullptr;
	GW::Constants::InstanceType current_map_type;
	clock_t scan_inventory_timer;
	bool enabled = false;

	// Interface Settings
	bool tick_with_pcons = false;
	int items_per_row = 3;
	bool show_enable_button = true;
	bool show_auto_refill_pcons_tickbox = true;
	GW::Agent* player;

	// Pcon Settings
	// todo: tonic pop?
	// todo: morale / dp removal
	// todo: replenish character pcons from chest?

	// Elite area auto disable
	void CheckBossRangeAutoDisable();	// Trigger Elite area auto disable if applicable
	void MapChanged();
	void CheckObjectivesCompleteAutoDisable();

	GW::Constants::MapID map_id;
	GW::Constants::InstanceType instance_type;
	bool elite_area_disable_triggered = false;	// Already triggered in this run?
	clock_t elite_area_check_timer;

	std::vector<DWORD> objectives_complete;
	// Fissue of Woe
	bool fow_disable_when_all_objs_complete = false;
	std::vector<DWORD> fow_objectives{ 309,310,311,312,313,314,315,316,317,318,319 };
	char* fow_disable_hint = "Fissure of Woe: Pcons auto disabled on final objective completion";
	// Underworld
	bool uw_disable_when_all_objs_complete = false;
	std::vector<DWORD> uw_objectives{ 157 };
	char* uw_disable_hint = "The Underworld: Pcons auto disabled on final objective completion";
	// Urgoz Warren
	GW::Vector2f urgoz_location;
	bool urgoz_disable_in_range_of_boss = false;	// Urgoz
	char* urgoz_disable_hint = "Urgoz Warren: Pcons auto disabled after entering final room";
	// The Deep
	GW::Vector2f kanaxai_location;
	bool deep_disable_in_range_of_boss = false;	// Deep
	char* deep_disable_hint = "The Deep: Pcons auto disabled after entering final room";
};
