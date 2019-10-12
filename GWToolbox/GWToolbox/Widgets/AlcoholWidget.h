#pragma once

#include "ToolboxWidget.h"

#include <GWCA/Packets/StoC.h>
#include <GWCA/Utilities/Hook.h>

class AlcoholWidget : public ToolboxWidget {
	AlcoholWidget() {};
	~AlcoholWidget() {};
private:
	DWORD alcohol_level = 0;
	time_t last_alcohol = 0;
	long alcohol_time = 0;
	GW::HookEntry PostProcess_Entry;
public:
	static AlcoholWidget& Instance() {
		static AlcoholWidget instance;
		return instance;
	}

	const char* Name() const override { return "Alcohol"; }

	void Initialize() override;

	void AlcUpdate(GW::HookStatus *, GW::Packet::StoC::PostProcess *packet);

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettingInternal() override;
};
