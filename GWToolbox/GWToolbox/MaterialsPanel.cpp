#include "MaterialsPanel.h"

#include "GWToolbox.h"

using namespace OSHGui;

MaterialsPanel::MaterialsPanel() {
}

void MaterialsPanel::BuildUI() {

	Label* label;
	Button* button;
	TextBox* textbox;

	int textbox_x = 180;
	int buybutton_x = 220;
	int row_height = 25;
	int cur_row = 0;

	label = new Label();
	label->SetText(L"Essence of Celerity");
	label->SetLocation(DefaultBorderPadding, DefaultBorderPadding + row_height * cur_row);
	AddControl(label);

	textbox = new TextBox();
	textbox->SetSize(40, textbox->GetHeight());
	textbox->SetLocation(textbox_x, DefaultBorderPadding + row_height * cur_row);
	textbox->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	textbox->GetFocusLostEvent() += FocusLostEventHandler([textbox](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stof(textbox->GetText());
		} catch (...) {
			textbox->SetText(L"0");
		}
	});
	AddControl(textbox);

	button = new Button();
	button->SetSize(50, button->GetHeight());
	button->SetLocation(buybutton_x, DefaultBorderPadding + row_height * cur_row);
	button->SetText(L"Buy");
	AddControl(button);
	

	Label* grail = new Label();
	grail->SetText(L"Grail of Might");
	grail->SetLocation(DefaultBorderPadding, DefaultBorderPadding + row_height * 1);
	AddControl(grail);

	Label* armor = new Label();
	armor->SetText(L"Armor of Salvation");
	armor->SetLocation(DefaultBorderPadding, DefaultBorderPadding + row_height * 2);
	AddControl(armor);

	Label* pstone = new Label();
	pstone->SetText(L"Powerstone of Courage");
	pstone->SetLocation(DefaultBorderPadding, DefaultBorderPadding + row_height * 3);
	AddControl(pstone);

	Label* res = new Label();
	res->SetText(L"Ressurrection Scroll");
	res->SetLocation(DefaultBorderPadding, DefaultBorderPadding + row_height * 4);
	AddControl(res);

	ComboBox* combo = new ComboBox();
	combo->AddItem(L"10 Bones");
	combo->AddItem(L"10 Iron Ingots");
	combo->AddItem(L"10 Tanned Hide Squares");
	combo->AddItem(L"10 Scales");
	combo->AddItem(L"10 Chitin Fragments");
	combo->AddItem(L"10 Bolts of Cloth");
	combo->AddItem(L"10 Wood Planks");
	combo->AddItem(L"10 Granite Slabs");
	combo->AddItem(L"10 Piles of Glittering Dust");
	combo->AddItem(L"10 Plant Fibers");
	combo->AddItem(L"10 Feathers");
	combo->AddItem(L"Glob of Ectoplasm");
	combo->AddItem(L"Obsidian Shard");
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {

	});
	combo->SetSelectedIndex(0);
	combo->SetLocation(DefaultBorderPadding, DefaultBorderPadding + row_height * 5);
	AddControl(combo);
}
