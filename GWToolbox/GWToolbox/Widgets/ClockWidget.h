#pragma once

#include <Defines.h>
#include "ToolboxWidget.h"

class ClockWidget : public ToolboxWidget {
	ClockWidget() {};
	~ClockWidget() {};
public:
	static ClockWidget& Instance() {
		static ClockWidget instance;
		return instance;
	}

	const char* Name() const override { return "Clock"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	bool use_24h_clock = true;
};
