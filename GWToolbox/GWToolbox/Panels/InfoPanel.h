#pragma once

#include <vector>
#include <string>

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

private:
	DWORD mapfile = 0;
	std::vector<std::string> resigned;
};
