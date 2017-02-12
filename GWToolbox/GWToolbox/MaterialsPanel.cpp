#include "MaterialsPanel.h"

#include "GWToolbox.h"

using namespace OSHGui;

void MaterialsPanel::BuildUI() {

	const int textbox_x = 180;
	const int buybutton_x = 220;

	auto GetName = [](enum Item item) -> std::wstring {
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
	const int group_width = (GetWidth() - 3 * Padding) / 2;
	const int group_height = 50;
	const int item_width = (group_width - 2 * Padding) / 3;
	
	auto MakeRow = [&](enum Item item, std::string file, int size, int x, int y) -> void {
		GroupBox* group = new GroupBox(this);
		group->SetSize(SizeI(group_width, group_height));
		group->SetLocation(PointI(Padding + x * (group_width + Padding),
			Padding + y * (group_height + Padding)));
		group->SetBackColor(Drawing::Color::Empty());
		AddControl(group);

		PictureBox* pic = new PictureBox(group->GetContainer());
		pic->SetEnabled(false);
		pic->SetBackColor(Color::Empty());
		pic->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
		pic->SetImage(Drawing::Image::FromFile(GuiUtils::getSubPath(file, "img")));
		pic->SetStretch(true);
		pic->SetSize(SizeI(size, size));
		pic->SetLocation(PointI(0,-10));
		group->AddControl(pic);
		
		TextBox* textbox = new TextBox(group->GetContainer());
		textbox->SetText("1");
		textbox->SetSize(SizeI(item_width, textbox->GetHeight()));
		textbox->SetLocation(PointI(item_width + 3, 5));
		textbox->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
			GWToolbox::instance().set_capture_input(true);
		});
		textbox->GetFocusLostEvent() += FocusLostEventHandler([textbox](Control*, Control*) {
			GWToolbox::instance().set_capture_input(false);
			try {
				std::stof(textbox->GetText());
			} catch (...) {
				textbox->SetText("0");
			}
		});
		group->AddControl(textbox);

		Button* button = new Button(group->GetContainer());
		button->SetSize(SizeI(item_width, textbox->GetHeight()));
		button->SetLocation(PointI(textbox->GetRight() + 3, textbox->GetTop()));
		button->SetText("Buy");
		group->AddControl(button);
	};

	MakeRow(Essence, "Essence_of_Celerity.png", 60, 0, 0);
	MakeRow(Grail, "Grail_of_Might.png", 50, 0, 1);
	MakeRow(Armor, "Armor_of_Salvation.png", 52, 0, 2);

	MakeRow(Powerstone, "Armor_of_Salvation.png", 60, 1, 0);
	MakeRow(ResScroll, "Armor_of_Salvation.png", 60, 1, 1);

	GroupBox* group = new GroupBox(this);
	group->SetSize(SizeI(GetWidth() - 2 * Padding, 40));
	group->SetLocation(PointI(Padding, Padding + 3 * (group_height + Padding)));
	group->SetBackColor(Drawing::Color::Empty());
	AddControl(group);

	ComboBox* combo = new ComboBox(group->GetContainer());
	combo->AddItem("10 Bones");
	combo->AddItem("10 Iron Ingots");
	combo->AddItem("10 Tanned Hide Squares");
	combo->AddItem("10 Scales");
	combo->AddItem("10 Chitin Fragments");
	combo->AddItem("10 Bolts of Cloth");
	combo->AddItem("10 Wood Planks");
	combo->AddItem("10 Granite Slabs");
	combo->AddItem("10 Piles of Glittering Dust");
	combo->AddItem("10 Plant Fibers");
	combo->AddItem("10 Feathers");
	combo->AddItem("Glob of Ectoplasm");
	combo->AddItem("Obsidian Shard");
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {

	});
	combo->SetSelectedIndex(0);
	combo->SetLocation(PointI(0, 0));
	combo->SetSize(SizeI(176, combo->GetHeight()));
	group->AddControl(combo);

	TextBox* textbox = new TextBox(group->GetContainer());
	textbox->SetText("1");
	textbox->SetSize(SizeI(item_width, combo->GetHeight()));
	textbox->SetLocation(PointI(combo->GetRight() + 3, 0));
	textbox->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	textbox->GetFocusLostEvent() += FocusLostEventHandler([textbox](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stof(textbox->GetText());
		} catch (...) {
			textbox->SetText("0");
		}
	});
	group->AddControl(textbox);

	Button* button = new Button(group->GetContainer());
	button->SetSize(SizeI(item_width, combo->GetHeight()));
	button->SetLocation(PointI(textbox->GetRight() + 3, textbox->GetTop()));
	button->SetText("Buy");
	group->AddControl(button);

	Label* log = new Label(this);
	//log->SetText(L"here there will be bought x/x");
	log->SetText("Work in progress");
	log->SetLocation(PointI(Padding, Padding + 4 * (group_height + Padding)));
	AddControl(log);

	Label* log2 = new Label(this);
	log2->SetText("Materials buyer NOT working");
	log2->SetLocation(PointI(Padding, log->GetBottom() + Padding));
	AddControl(log2);
}

