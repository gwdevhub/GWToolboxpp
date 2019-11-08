#pragma once

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Constants/Maps.h>

#include <GWCA/Packets/StoC.h>

#include "mp3.h"

class FunModule : public ToolboxModule {
	FunModule() {};
	~FunModule() {
		if (mp3) delete mp3;
	};
private:
	Mp3* mp3;
public:
	static FunModule& Instance() {
		static FunModule instance;
		return instance;
	}

	const char* Name() const override { return "Just For FUN!"; }
	void Initialize() override;
	void Terminate() override;
	bool HasSettings() override { return false; };
	void DisplayDialogue(GW::Packet::StoC::DisplayDialogue*);
	void PlayKanaxaiDialog(uint8_t idx);

	GW::HookEntry DisplayDialogue_Entry;
	GW::HookEntry GameSrvTransfer_Entry;
};
