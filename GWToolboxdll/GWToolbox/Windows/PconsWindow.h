#pragma once

#include <Defines.h>

#include <GWCA\Utilities\Hook.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA\Utilities\Hook.h>
#include <GWCA\Constants\Constants.h>

#include "Timer.h"
#include "Pcons.h"
#include "ToolboxWindow.h"

class PconsWindow : public ToolboxWindow {
	PconsWindow();
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
	bool show_storage_quantity = false;

	inline void ToggleEnable() { SetEnabled(!enabled); }

	void Update(float delta) override;

	bool DrawTabButton(IDirect3DDevice9* device, bool show_icon, bool show_text) override;
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;
	void RegisterSettingsContent() override;
	void DrawLunarsAndAlcoholSettings();

    std::vector<Pcon*> pcons;

private:

	
	PconAlcohol* pcon_alcohol = nullptr;
	clock_t scan_inventory_timer = 0;
	bool enabled = false;

	// Interface Settings
	bool tick_with_pcons = false;
	int items_per_row = 3;
	bool show_enable_button = true;

	bool disable_pcons_on_map_change = false;
    bool disable_cons_on_vanquish_completion = true;
    bool disable_cons_on_objective_completion = false;
    bool disable_cons_in_final_room = false;

	bool show_auto_refill_pcons_tickbox = true;
	bool show_auto_disable_pcons_tickbox = false;
    
	GW::Agent* player = nullptr;

	// Pcon Settings
	// todo: tonic pop?
	// todo: morale / dp removal
	GW::HookEntry AgentSetPlayer_Entry;
	GW::HookEntry AddExternalBond_Entry;
	GW::HookEntry PostProcess_Entry;
	GW::HookEntry GenericValue_Entry;
	GW::HookEntry AgentState_Entry;
	GW::HookEntry SpeechBubble_Entry;
	GW::HookEntry ObjectiveDone_Entry;
	GW::HookEntry VanquishComplete_Entry;

	void MapChanged(); // Called via Update() when map id changes
	// Elite area auto disable
	void CheckBossRangeAutoDisable();	// Trigger Elite area auto disable if applicable
	void CheckObjectivesCompleteAutoDisable();

	GW::Constants::MapID map_id = GW::Constants::MapID::None;
	GW::Constants::InstanceType instance_type = GW::Constants::InstanceType::Loading;
	GW::Constants::InstanceType previous_instance_type = GW::Constants::InstanceType::Loading;
    bool in_vanquishable_area = false;

	bool elite_area_disable_triggered = false;	// Already triggered in this run?
	clock_t elite_area_check_timer = 0;

	// Map of which objectives to check per map_id
	std::vector<DWORD> objectives_complete = {};
	std::map<GW::Constants::MapID, std::vector<DWORD>> objectives_to_complete_by_map_id = {
		{GW::Constants::MapID::The_Fissure_of_Woe,{ 309,310,311,312,313,314,315,316,317,318,319 }}, // Can be done in any order - check them all.
		{GW::Constants::MapID::The_Deep, { 421 }},
		{GW::Constants::MapID::Urgozs_Warren, { 357 }},
		{GW::Constants::MapID::The_Underworld,{ 157 }} // Only need to check for Nightman Cometh for Underworld.
	};
	std::vector<DWORD> current_objectives_to_check = {};
	
	// Map of which locations to turn off near by map_id e.g. Kanaxai, Urgoz
	std::map<GW::Constants::MapID, GW::Vec2f> final_room_location_by_map_id = {
		{GW::Constants::MapID::The_Deep, GW::Vec2f(30428.0f, -5842.0f)},		// Rough location of Kanaxai
		{GW::Constants::MapID::Urgozs_Warren, GW::Vec2f(-2800.0f, 14316.0f)} // Front entrance of Urgoz's room
	};
    GW::Vec2f current_final_room_location = GW::Vec2f(0, 0);

    char* disable_cons_on_objective_completion_hint = "Disable cons when final objective(s) completed";
	char* disable_cons_in_final_room_hint = "Disable cons when reaching the final room in Urgoz and Deep";
    char* disable_cons_on_vanquish_completion_hint = "Disable cons when completing a vanquish";
	
};
