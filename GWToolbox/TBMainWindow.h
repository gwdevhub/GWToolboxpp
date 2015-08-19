#pragma once

#include "../include/OSHGui/OSHGui.hpp"

using namespace OSHGui::Drawing;

class TBMainWindow : public OSHGui::Form {
private:
	const int width = 100;
	const int height = 300;

public:
	TBMainWindow() {
		SetText("GWToolbox++");
		SetSize(width, height);

		Button * pcons = new Button();
		pcons->SetText("Pcons");
		pcons->SetBounds(0, 0, width - DefaultBorderPadding * 2, 30);
		//pcons->SetBackColor(Color(0, 0, 0, 0));
		pcons->GetClickEvent() += ClickEventHandler([pcons](Control*) {
			LOG("Clicked on pcons!\n");
		});
		AddControl(pcons);

		Button * hotkeys = new Button();
		hotkeys->SetText("Hotkeys");
		hotkeys->SetBounds(0, 30, width - DefaultBorderPadding * 2, 30);
		//hotkeys->SetBackColor(Color(0, 0, 0, 0));
		hotkeys->GetClickEvent() += ClickEventHandler([pcons](Control*) {
			LOG("Clicked on hotkeys!\n");
		});
		AddControl(hotkeys);

		Panel * test = new Panel();
		test->SetLocation(150, 20);
		test->SetSize(200, 200);
		test->SetBackColor(Color(1, 1, 1, 1));
		test->SetEnabled(true);
		AddControl(test);
	}

};
