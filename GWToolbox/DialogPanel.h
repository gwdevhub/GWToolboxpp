#pragma once

#include "ToolboxPanel.h"

class DialogPanel : public ToolboxPanel {
public:
	DialogPanel();

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override {};
};

