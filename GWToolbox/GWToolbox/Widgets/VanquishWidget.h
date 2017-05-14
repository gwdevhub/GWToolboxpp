#pragma once

#include "ToolboxWidget.h"

class VanquishWidget : public ToolboxWidget {
	VanquishWidget() {};
	~VanquishWidget() {};

public:
	static VanquishWidget& Instance() {
		static VanquishWidget instance;
		return instance;
	}

	const char* Name() const override { return "Vanquish"; }

	void Draw(IDirect3DDevice9* pDevice) override;

	void DrawSettingInternal() override;
};
