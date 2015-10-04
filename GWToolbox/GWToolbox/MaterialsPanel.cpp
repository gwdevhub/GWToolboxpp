#include "MaterialsPanel.h"

using namespace OSHGui;

MaterialsPanel::MaterialsPanel() {
}

void MaterialsPanel::BuildUI() {

	Label* label = new Label();
	label->SetText(L"Materials panel under construction");
	AddControl(label);
}
