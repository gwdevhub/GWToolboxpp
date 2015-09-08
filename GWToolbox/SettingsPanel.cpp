#include "SettingsPanel.h"

using namespace OSHGui;

SettingsPanel::SettingsPanel() {
}

void SettingsPanel::BuildUI() {
	SetSize(250, 300);

	Label* label = new Label();
	label->SetText("Settings panel under construction");
	AddControl(label);
}
