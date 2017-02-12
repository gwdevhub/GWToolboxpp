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
	const char* Name() override { return "Materials Panel"; }

	MaterialsPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}

	void BuildUI() override;
	
	// Update. Will always be called every frame.
	void Update() override {}

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override {}

private:
};

