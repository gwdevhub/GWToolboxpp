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
		CoUninitialize();
	};
private:
	Mp3* mp3;
    
	bool enabled = false;
    bool transmo_team = true;
    bool rewrite_npc_dialogs = true;
    bool kanaxais_true_form = true;

	clock_t pending_transmog = 0;
	bool can_terminate = true;
	bool terminating = false;
public:
	static ZrawDeepModule& Instance() {
		static ZrawDeepModule instance;
		return instance;
	}

	const char* Name() const override { return "24h Deep Mode"; }
	void Initialize() override;
	void SignalTerminate() override;
	bool CanTerminate() { return can_terminate; }
	bool HasSettings() { return enabled; }
	void SetEnabled(bool enabled);
	void Update(float delta) override;
    void DrawSettingInternal() override;
	void DisplayDialogue(GW::Packet::StoC::DisplayDialogue*);
	void PlayKanaxaiDialog(uint8_t idx);
	void SaveSettings(CSimpleIni* ini);
	void LoadSettings(CSimpleIni* ini);

	void SetTransmogs();
	bool IsEnabled() {
        return enabled;
	}

	GW::HookEntry ZrawDeepModule_StoCs;
};
