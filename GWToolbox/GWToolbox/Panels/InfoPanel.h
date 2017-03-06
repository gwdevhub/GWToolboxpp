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

	void DrawSettingInternal() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
	DWORD mapfile = 0;
	std::vector<std::string> resigned;

	bool show_widgets = true;
	bool show_open_chest = true;
	bool show_player = true;
	bool show_target = true;
	bool show_map = true;
	bool show_dialog = true;
	bool show_item = true;
	bool show_mobcount = true;
	bool show_resignlog = true;
};
