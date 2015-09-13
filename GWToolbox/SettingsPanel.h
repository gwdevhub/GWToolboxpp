#pragma once

#include "ToolboxPanel.h"

class SettingsPanel : public ToolboxPanel {
public:
	SettingsPanel();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override {};
};

