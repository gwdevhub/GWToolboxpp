#pragma once

#include "ToolboxWindow.h"

class ClockWindow : public ToolboxWindow {
public:
	const char* Name() const override { return "Clock"; }

	ClockWindow() {};
	~ClockWindow() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	bool use_24h_clock = true;
};
