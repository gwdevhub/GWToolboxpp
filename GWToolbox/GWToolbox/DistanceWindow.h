#pragma once

#include "ToolboxModule.h"

class DistanceWindow : public ToolboxModule {
public:
	DistanceWindow() {};
	~DistanceWindow() {};

	const char* Name() const override { return "Distance"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) const override;

	bool visible;
};
