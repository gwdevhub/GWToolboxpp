#pragma once

#include "ToolboxModule.h"

class HealthWindow : public ToolboxModule {
public:
	const char* Name() const override { return "Target Health"; }

	HealthWindow() {};
	~HealthWindow() {};

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) const override;
};
