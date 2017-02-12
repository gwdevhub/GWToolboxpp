#include "EditBuild.h"

#include <string>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ChatMgr.h>

#include "Config.h"
#include "logger.h"
#include "MainWindow.h"
#include "GWToolbox.h"

using namespace OSHGui;
using namespace OSHGui::Drawing;

EditBuild::EditBuild(OSHGui::Control* parent) : OSHGui::Panel(parent) {
	names = std::vector<TextBox*>(N_PLAYERS);
	templates = std::vector<TextBox*>(N_PLAYERS);

	SetSize(SizeI(450, 500));

	Label* label_name = new Label(this);
	label_name->SetText("Build Name:");
	label_name->SetLocation(PointI(Padding, Padding + 3));
	AddControl(label_name);

	Button* button = new Button(this);
	button->SetSize(SizeI(ITEM_HEIGHT, ITEM_HEIGHT));
	button->SetLocation(PointI(GetWidth() - ITEM_HEIGHT - Padding, Padding));
	button->SetText("^");
	button->GetClickEvent() += ClickEventHandler([button, this](Control*) {
		up_ = !up_;
		UpdateLocation();
		Config::IniWrite(MainWindow::IniSection(), EditBuild::IniKeyUp(), up_);
	});
	AddControl(button);

	name = new TextBox(this);
	name->SetSize(SizeI(button->GetLeft() - label_name->GetRight() - 2 * Padding, ITEM_HEIGHT));
	name->SetLocation(PointI(label_name->GetRight() + Padding, Padding));
	EnableTextInput(name);
	AddControl(name);

	Label* label_names = new Label(this);
	label_names->SetText("Names:");
	label_names->SetLocation(PointI(NAME_LEFT, name->GetBottom() + Padding * 2));
	AddControl(label_names);

	const int starty = label_names->GetBottom() + Padding;
	const int row_height = ITEM_HEIGHT + Padding;
	for (int i = 0; i < N_PLAYERS; ++i) {
		Label* label_index = new Label(this);
		label_index->SetText(std::string("#") + std::to_string(i + 1));
		label_index->SetLocation(PointI(Padding, starty + row_height * i + 3));
		AddControl(label_index);

		names[i] = new TextBox(this);
		names[i]->SetLocation(PointI(NAME_LEFT, starty + row_height * i));
		names[i]->SetSize(SizeI(NAME_WIDTH, ITEM_HEIGHT));
		EnableTextInput(names[i]);
		AddControl(names[i]);

		Button* send = new Button(this);
		send->SetText("Send");
		send->SetSize(SizeI(SEND_WIDTH, names[i]->GetHeight()));
		send->SetLocation(PointI(GetWidth() - send->GetWidth() - Padding / 2,
			starty + row_height * i));
		send->GetClickEvent() += ClickEventHandler([this, i](Control*) {
			std::string message = "[";
			if (show_numbers->GetChecked()) {
				message += std::to_string(i + 1);
				message += " - ";
			}
			message += names[i]->GetText();
			message += ";";
			message += templates[i]->GetText();
			message += "]";
			GW::Chat().SendChat(message.c_str(), '#');
		});
		AddControl(send);

		templates[i] = new TextBox(this);
		templates[i]->SetLocation(PointI(names[i]->GetRight() + Padding / 2, starty + row_height * i));
		templates[i]->SetSize(SizeI(send->GetLeft() - names[i]->GetRight() - Padding, ITEM_HEIGHT));
		EnableTextInput(templates[i]);
		AddControl(templates[i]);
	}

	Label* label_templates = new Label(this);
	label_templates->SetText("Templates:");
	label_templates->SetLocation(PointI(templates[0]->GetLeft(), name->GetBottom() + Padding * 2));
	AddControl(label_templates);

	const int last_row_y = names[11]->GetBottom() + Padding;
	const int buttons_width = (GetWidth() - templates[0]->GetLeft() - Padding) / 2;

	show_numbers = new CheckBox(this);
	show_numbers->SetText("Show Build numbers");
	show_numbers->SetLocation(PointI(Padding, last_row_y));
	AddControl(show_numbers);
	
	Button* cancel = new Button(this);
	cancel->SetText("Cancel");
	cancel->SetLocation(PointI(templates[0]->GetLeft(), last_row_y));
	cancel->SetSize(SizeI(buttons_width, ITEM_HEIGHT));
	cancel->GetClickEvent() += ClickEventHandler([this](Control*) {
		this->SetVisible(false);
	});
	AddControl(cancel);

	Button* ok = new Button(this);
	ok->SetText("Ok");
	ok->SetLocation(PointI(cancel->GetRight() + Padding, last_row_y));
	ok->SetSize(SizeI(buttons_width, ITEM_HEIGHT));
	ok->GetClickEvent() += ClickEventHandler([this](Control*) {
		this->SetVisible(false);
		SaveBuild();
	});
	AddControl(ok);

	SetSize(SizeI(templates[0]->GetRight() + SEND_WIDTH + 2 * Padding,
		ok->GetBottom() + Padding));

	up_ = Config::IniRead(MainWindow::IniSection(), EditBuild::IniKeyUp(), true);

	left_ = false;
	UpdateLocation();
}

void EditBuild::EnableTextInput(TextBox* tb) {
	tb->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	tb->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
	});
}

void EditBuild::SetEditedBuild(int index, Button* button) {
	editing_index = index;
	editing_button = button;

	std::string section = std::string("builds") + std::to_string(index);
	std::string key;
	
	key = "buildname";
	std::string buildname = Config::IniRead(section.c_str(), key.c_str(), "");
	name->SetText(buildname);
	for (int i = 0; i < N_PLAYERS; ++i) {
		key = "name" + std::to_string(i + 1);
		std::string name = Config::IniRead(section.c_str(), key.c_str(), "");
		names[i]->SetText(name);

		key = "template" + std::to_string(i + 1);
		std::string temp = Config::IniRead(section.c_str(), key.c_str(), "");
		templates[i]->SetText(temp);
	}
	show_numbers->SetChecked(Config::IniRead(section.c_str(), "showNumbers", true));

	SetVisible(true);
}

void EditBuild::SaveBuild() {
	std::string section = std::string("builds") + std::to_string(editing_index);
	std::string key;

	std::string s_name = name->GetText();
	key = "buildname";
	Config::IniWrite(section.c_str(), key.c_str(), s_name.c_str());
	editing_button->SetText(s_name);

	for (int i = 0; i < N_PLAYERS; ++i) {
		std::string s_name = names[i]->GetText();
		key = "name" + std::to_string(i + 1);
		Config::IniWrite(section.c_str(), key.c_str(), s_name.c_str());

		std::string s_template = templates[i]->GetText();
		key = "template" + std::to_string(i + 1);
		Config::IniWrite(section.c_str(), key.c_str(), s_template.c_str());
	}

	Config::IniWrite(section.c_str(), "showNumbers", show_numbers->GetChecked());
}

void EditBuild::UpdateLocation() {
	int x = left_ ? -GetWidth() : MainWindow::SIDE_PANEL_WIDTH;
	int y = up_ ? 0 : MainWindow::HEIGHT - GetHeight();

	SetLocation(PointI(x, y));
}
