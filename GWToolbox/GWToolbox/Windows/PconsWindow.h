#pragma once

#include <Defines.h>

#include <GWCA\Utilities\Hook.h>
#include <GWCA\Constants\Constants.h>

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
	GW::Constants::InstanceType current_map_type = GW::Constants::InstanceType::Outpost;
	clock_t scan_inventory_timer = 0;
	bool enabled = false;
	
	// Interface Settings
	bool tick_with_pcons = false;
	int items_per_row = 3;
	bool show_enable_button = true;

	// Pcon Settings
	// todo: tonic pop?
	// todo: morale / dp removal
	// todo: replenish character pcons from chest?
	GW::HookEntry AgentSetPlayer_Entry;
	GW::HookEntry AddExternalBond_Entry;
	GW::HookEntry PostProcess_Entry;
	GW::HookEntry GenericValue_Entry;
	GW::HookEntry AgentState_Entry;
	GW::HookEntry SpeechBubble_Entry;
};
