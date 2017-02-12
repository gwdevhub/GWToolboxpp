#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxModule.h"

class TimerWindow : public ToolboxModule {
public:
	const char* Name() override { return "Timer"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	bool visible;
};
