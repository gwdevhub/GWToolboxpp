#pragma once

#include "ToolboxPanel.h"

class MaterialsPanel : public ToolboxPanel {
public:
	MaterialsPanel();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override {};
};

