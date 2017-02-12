#include "BuildPanel.h"

#include <OSHGui\Misc\Intersection.hpp>
#include <GWCA\GWCA.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include "Config.h"

using namespace OSHGui;

void BuildPanel::Build::BuildUI() {
	SetBackColor(Drawing::Color::Empty());

	const int edit_button_width = 60;

	Button* button = new Button(this);
	//button->SetText(name_);
	button->SetSize(Drawing::SizeI(GetWidth() - edit_button_width - Padding, GetHeight()));
	button->SetLocation(Drawing::PointI(0, 0));
	button->GetClickEvent() += ClickEventHandler([this](Control*) {
		SendTeamBuild();
	});
	AddControl(button);

	Button* edit = new Button(this);
	edit->SetText("Edit");
	edit->SetSize(Drawing::SizeI(edit_button_width, GetHeight()));
	edit->SetLocation(Drawing::PointI(GetWidth() - edit->GetWidth(), 0));
	edit->GetClickEvent() += ClickEventHandler([this, button](Control*) {
		edit_build_->SetEditedBuild(index_, button);
	});
	AddControl(edit);
}

void BuildPanel::Build::SendTeamBuild() {
	using namespace std;

	string section = std::string("builds") + to_string(index_);
	string key;

	key = "buildname";
	string buildname = Config::IniRead(section.c_str(), key.c_str(), "");
	if (!buildname.empty()) {
		panel_->Enqueue(buildname);
	}

	bool show_numbers = Config::IniRead(section.c_str(), "showNumbers", true);

	for (int i = 0; i < edit_build_->N_PLAYERS; ++i) {
		key = "name" + to_string(i + 1);
		string name = Config::IniRead(section.c_str(), key.c_str(), "");

		key = "template" + to_string(i + 1);
		string temp = Config::IniRead(section.c_str(), key.c_str(), "");

		if (!name.empty() && !temp.empty()) {
			string message = "[";
			if (show_numbers) {
				message += to_string(i + 1);
				message += " - ";
			}
			message += name;
			message += ";";
			message += temp;
			message += "]";
			panel_->Enqueue(message);
		}
	}
}

BuildPanel::BuildPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {
	builds = std::vector<Build*>();
	first_shown_ = 0;
	send_timer = TBTimer::init();
}

bool BuildPanel::Intersect(const Drawing::PointI &point) const {
	if (edit_build_->GetVisible()) {
		return Control::Intersect(point) || edit_build_->Intersect(point);
	} else {
		return Control::Intersect(point);
	}
}

void BuildPanel::BuildUI() {

	clip_ = Clipping::None;

	edit_build_ = new EditBuild(this);
	edit_build_->SetVisible(false);
	AddControl(edit_build_);

	ScrollPanel* panel = new ScrollPanel(this);
	panel->SetLocation(Drawing::PointI(0, 0));
	panel->SetSize(GetSize());
	panel->GetContainer()->SetBackColor(Drawing::Color::Empty());
	panel->SetInternalHeight(N_BUILDS * (BUILD_HEIGHT + Padding) + Padding);
	AddControl(panel);

	for (int i = 0; i < N_BUILDS; ++i) {
		int index = i + 1;
		std::string section = std::string("builds") + std::to_string(index);
		std::string name = Config::IniRead(section.c_str(), "buildname", "");
		if (name.empty()) name = std::string("<Build ") + std::to_string(index) + std::string(">");
		Build* build = new Build(panel->GetContainer(), index, name, edit_build_, this);
		build->SetSize(Drawing::SizeI(panel->GetContainer()->GetWidth() - 2 * Padding, BUILD_HEIGHT));
		build->SetLocation(Drawing::PointI(Padding, Padding + i * (BUILD_HEIGHT + Padding)));
		build->BuildUI();
		builds.push_back(build);
		panel->AddControl(build);
	}
}

void BuildPanel::Update() {
	if (!queue.empty() && TBTimer::diff(send_timer) > 600) {
		send_timer = TBTimer::init();

		if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
			&& GW::Agents().GetPlayer()) {

			//GW::Chat().SendChat(queue.front().c_str(), L'#');
			queue.pop();
		}
	}
}
