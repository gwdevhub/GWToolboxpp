#pragma once

#include "ToolboxWidget.h"

class HealthWidget : public ToolboxWidget {
	HealthWidget() {};
	~HealthWidget() {};
public:
	static HealthWidget& Instance() {
		static HealthWidget instance;
		return instance;
	}

	const char* Name() const override { return "Health"; }

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;
};
