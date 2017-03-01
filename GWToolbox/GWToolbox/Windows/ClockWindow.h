#pragma once

#include "ToolboxWindow.h"

class ClockWindow : public ToolboxWindow {
	ClockWindow() {};
	~ClockWindow() {};
public:
	static ClockWindow& Instance() {
		static ClockWindow instance;
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
