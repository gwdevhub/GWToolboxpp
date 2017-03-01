#pragma once

#include "ToolboxPanel.h"

class InfoPanel : public ToolboxPanel {
	InfoPanel() {};
	~InfoPanel() {};
public:
	static InfoPanel& Instance() {
		static InfoPanel instance;
		return instance;
	}

	const char* Name() const override { return "Info"; }

	void Initialize() override;

	void Draw(IDirect3DDevice9* pDevice) override;
	
	void DrawSettings() override {};
};
