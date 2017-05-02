#pragma once

#include "ToolboxWidget.h"

class VanquishWindow : public ToolboxWidget {

	VanquishWindow() {};
	~VanquishWindow() {};

public:
	static VanquishWindow& Instance() {
		static VanquishWindow instance;
		return instance;
	}

	const char* Name() const override { return "Vanquish"; }

	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettingInternal() override;
};
