#include "MaterialsPanel.h"

using namespace OSHGui;

MaterialsPanel::MaterialsPanel() {
}

void MaterialsPanel::BuildUI() {

	Label* label = new Label();
	label->SetText(L"Materials panel under construction");
	AddControl(label);

	//Button* button = new Button();
	//button->SetText(L"It will be done sooner or later");
	//button->SetSize(200, 25);
	//button->SetLocation(GetWidth() / 2 - 100, GetHeight() / 2 - 12);
	//button->GetClickEvent() += ClickEventHandler([](Control*) {
	//	ShellExecuteW(NULL, L"open", L"https://youtu.be/dQw4w9WgXcQ", NULL, NULL, SW_SHOWNORMAL);
	//});
	//AddControl(button);
}
