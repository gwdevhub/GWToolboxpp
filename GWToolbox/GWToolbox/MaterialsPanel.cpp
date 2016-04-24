#include "MaterialsPanel.h"

#include "GWToolbox.h"

using namespace OSHGui;

MaterialsPanel::MaterialsPanel() {
}

void MaterialsPanel::BuildUI() {

	const int textbox_x = 180;
	const int buybutton_x = 220;

	auto GetName = [](enum Item item) -> wstring {
		switch (item) {
		case MaterialsPanel::Essence: return L"Essence of Celrity";
		case MaterialsPanel::Grail: return L"Grail of Might";
		case MaterialsPanel::Armor: return L"Armor of Salvation";
		case MaterialsPanel::Powerstone: return L"Powerstone of Courage";
		case MaterialsPanel::ResScroll: return L"Ressurrection Scroll";
		case MaterialsPanel::Any: return L"";
		default: return L"";
		}
	};
	const int group_width = (GetWidth() - 3 * DefaultBorderPadding) / 2;
	const int group_height = 50;
	const int item_width = (group_width - 2 * DefaultBorderPadding) / 3;

	auto MakeRow = [&](enum Item item, string file, int size, int x, int y) -> void {
		GroupBox* group = new GroupBox();
		group->SetSize(group_width, group_height);
		group->SetLocation(DefaultBorderPadding + x * (group_width + DefaultBorderPadding),
			DefaultBorderPadding + y * (group_height + DefaultBorderPadding));
		group->SetBackColor(Drawing::Color::Empty());
		AddControl(group);

		PictureBox* pic = new PictureBox();
		pic->SetEnabled(false);
		pic->SetBackColor(Color::Empty());
		pic->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
		pic->SetImage(Drawing::Image::FromFile(GuiUtils::getSubPathA(file, "img")));
		pic->SetStretch(true);
		pic->SetSize(size, size);
		pic->SetLocation(0,-10);
		group->AddControl(pic);
		
		TextBox* textbox = new TextBox();
		textbox->SetText(L"1");
		textbox->SetSize(item_width, textbox->GetHeight());
		textbox->SetLocation(item_width + 3, 5);
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
		group->AddControl(textbox);

		Button* button = new Button();
		button->SetSize(item_width, textbox->GetHeight());
		button->SetLocation(textbox->GetRight() + 3, textbox->GetTop());
		button->SetText(L"Buy");
		group->AddControl(button);
	};

	MakeRow(Essence, "Essence_of_Celerity.png", 60, 0, 0);
	MakeRow(Grail, "Grail_of_Might.png", 50, 0, 1);
	MakeRow(Armor, "Armor_of_Salvation.png", 52, 0, 2);

	MakeRow(Powerstone, "Armor_of_Salvation.png", 60, 1, 0);
	MakeRow(ResScroll, "Armor_of_Salvation.png", 60, 1, 1);

	GroupBox* group = new GroupBox();
	group->SetSize(GetWidth() - 2 * DefaultBorderPadding, 40);
	group->SetLocation(DefaultBorderPadding,
		DefaultBorderPadding + 3 * (group_height + DefaultBorderPadding));
	group->SetBackColor(Drawing::Color::Empty());
	AddControl(group);

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
	combo->SetLocation(0, 0);
	combo->SetSize(176, combo->GetHeight());
	group->AddControl(combo);

	TextBox* textbox = new TextBox();
	textbox->SetText(L"1");
	textbox->SetSize(item_width, combo->GetHeight());
	textbox->SetLocation(combo->GetRight() + 3, 0);
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
	group->AddControl(textbox);

	Button* button = new Button();
	button->SetSize(item_width, combo->GetHeight());
	button->SetLocation(textbox->GetRight() + 3, textbox->GetTop());
	button->SetText(L"Buy");
	group->AddControl(button);

	Label* log = new Label();
	log->SetText(L"here there will be bought x/x");
	log->SetLocation(DefaultBorderPadding, DefaultBorderPadding + 4 * (group_height + DefaultBorderPadding));
	AddControl(log);
}
