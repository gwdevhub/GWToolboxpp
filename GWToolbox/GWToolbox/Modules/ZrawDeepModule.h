#pragma once

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Constants/Maps.h>

#include <GWCA/Packets/StoC.h>

#include "mp3.h"

class ZrawDeepModule : public ToolboxModule {
	ZrawDeepModule() {};
	~ZrawDeepModule() {
		if (mp3) delete mp3;
	};
private:
	Mp3* mp3;
	bool transmog_done = false;
	bool checked_enabled = false;
	bool valid_for_map = false;
	bool disabled = false;
	clock_t pending_transmog = 0;
public:
	static ZrawDeepModule& Instance() {
		static ZrawDeepModule instance;
		return instance;
	}

	const char* Name() const override { return "ZRaw 24h Deep!"; }
	void Initialize() override;
	void Terminate() override;
	void Update(float delta) override;
	bool HasSettings() override { return false; };
	void DisplayDialogue(GW::Packet::StoC::DisplayDialogue*);
	void PlayKanaxaiDialog(uint8_t idx);

	void SetTransmogs(bool enabled);
	void SetValidForMap(bool v);
	void Reset() {
		transmog_done = false;
		checked_enabled = false;
		valid_for_map = false;
	}
	void Disable() {
		disabled = true;
		Reset();
	}
	void Enable() {
		disabled = false;
		Reset();
	}
	bool IsEnabled() {
		return disabled == false;
	}
	void Toggle() {
		disabled = !disabled;
		Reset();
	}

	GW::HookEntry ZrawDeepModule_StoCs;
};
