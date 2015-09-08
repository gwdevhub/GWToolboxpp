#include "MaterialsPanel.h"

using namespace OSHGui;

MaterialsPanel::MaterialsPanel() {
}

void MaterialsPanel::BuildUI() {
	SetSize(250, 300);

	Label* label = new Label();
	label->SetText("Materials panel under construction");
	AddControl(label);
}
