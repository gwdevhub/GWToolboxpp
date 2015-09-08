#include "OthersPanel.h"

using namespace OSHGui;

OthersPanel::OthersPanel() {
}

void OthersPanel::BuildUI() {
	SetSize(250, 300);

	Label* label = new Label();
	label->SetText("Others panel under construction");
	AddControl(label);
}
