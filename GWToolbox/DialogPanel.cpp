#include "DialogPanel.h"

using namespace OSHGui;

DialogPanel::DialogPanel() {
}

void DialogPanel::BuildUI() {
	SetSize(250, 300);

	Label* label = new Label();
	label->SetText("Dialog panel under construction");
	AddControl(label);
}
