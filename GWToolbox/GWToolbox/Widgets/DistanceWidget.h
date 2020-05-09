#pragma once

#include "ToolboxWidget.h"
#include <Color.h>

class DistanceWidget : public ToolboxWidget {
	DistanceWidget() {};
	~DistanceWidget() {};
public:
	static DistanceWidget& Instance() {
		static DistanceWidget instance;
		return instance;
	}

	const char* Name() const override { return "Distance"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	bool hide_in_outpost = false;

	Color color_adjacent = 0xFFFFFFFF;
	Color color_nearby = 0xFFFFFFFF;
	Color color_area = 0xFFFFFFFF;
	Color color_earshot = 0xFFFFFFFF;
	Color color_cast = 0xFFFFFFFF;
	Color color_spirit = 0xFFFFFFFF;
	Color color_compass = 0xFFFFFFFF;
};
