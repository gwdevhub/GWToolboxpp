#pragma once

#include "ToolboxWidget.h"

#include <GWCA\Packets\StoC.h>


class AlcoholWidget : public ToolboxWidget {
	AlcoholWidget() {};
	~AlcoholWidget() {};
private:
	DWORD alcohol_level;
	time_t last_alcohol;
	long alcohol_time;
public:
	static AlcoholWidget& Instance() {
		static AlcoholWidget instance;
		return instance;
	}

	const char* Name() const override { return "Alcohol"; }

	void Initialize() override;

	bool AlcUpdate(GW::Packet::StoC::P095 * packet);

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettingInternal() override;
};
