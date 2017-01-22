#pragma once

#include "ToolboxPanel.h"

class MaterialsPanel : public ToolboxPanel {
	enum Item {
		Essence,
		Grail,
		Armor,
		Powerstone,
		ResScroll,
		Any
	};

public:
	MaterialsPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}

	void BuildUI() override;
	
	// Update. Will always be called every frame.
	void Main() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override {}

private:
};

