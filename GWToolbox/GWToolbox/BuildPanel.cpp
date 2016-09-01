#include "BuildPanel.h"

#include <OSHGui\Misc\Intersection.hpp>
#include <GWCA\GWCA.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include "Config.h"

using namespace std;
using namespace OSHGui;

void BuildPanel::Build::BuildUI() {
	SetBackColor(Drawing::Color::Empty());

	const int edit_button_width = 60;

	Button* button = new Button(this);
	button->SetText(name_);
	button->SetSize(Drawing::SizeI(GetWidth() - edit_button_width - Padding, GetHeight()));
	button->SetLocation(Drawing::PointI(0, 0));
	button->GetClickEvent() += ClickEventHandler([this](Control*) {
		SendTeamBuild();
	});
	AddControl(button);

	Button* edit = new Button(this);
	edit->SetText(L"Edit");
	edit->SetSize(Drawing::SizeI(edit_button_width, GetHeight()));
	edit->SetLocation(Drawing::PointI(GetWidth() - edit->GetWidth(), 0));
	edit->GetClickEvent() += ClickEventHandler([this, button](Control*) {
		edit_build_->SetEditedBuild(index_, button);
	});
	AddControl(edit);
}

void BuildPanel::Build::SendTeamBuild() {
	wstring section = wstring(L"builds") + to_wstring(index_);
	wstring key;

	key = L"buildname";
	wstring buildname = Config::IniRead(section.c_str(), key.c_str(), L"");
	if (!buildname.empty()) {
		panel_->Enqueue(buildname);
	}

	bool show_numbers = Config::IniReadBool(section.c_str(), L"showNumbers", true);

	for (int i = 0; i < edit_build_->N_PLAYERS; ++i) {
		key = L"name" + to_wstring(i + 1);
		wstring name = Config::IniRead(section.c_str(), key.c_str(), L"");

		key = L"template" + to_wstring(i + 1);
		wstring temp = Config::IniRead(section.c_str(), key.c_str(), L"");
		
		if (!name.empty() && !temp.empty()) {
			wstring message = L"[";
			if (show_numbers) {
				message += to_wstring(i + 1);
				message += L" - ";
			}
			message += name;
			message += L";";
			message += temp;
			message += L"]";
			panel_->Enqueue(message);
		}
	}
}

BuildPanel::BuildPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {
	builds = vector<Build*>();
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
		wstring section = wstring(L"builds") + to_wstring(index);
		wstring name = Config::IniRead(section.c_str(), L"buildname", L"");
		if (name.empty()) name = wstring(L"<Build ") + to_wstring(index) + wstring(L">");
		Build* build = new Build(panel->GetContainer(), index, name, edit_build_, this);
		build->SetSize(Drawing::SizeI(panel->GetContainer()->GetWidth() - 2 * Padding, BUILD_HEIGHT));
		build->SetLocation(Drawing::PointI(Padding, Padding + i * (BUILD_HEIGHT + Padding)));
		build->BuildUI();
		builds.push_back(build);
		panel->AddControl(build);
	}
}

void BuildPanel::MainRoutine() {
	if (!queue.empty() && TBTimer::diff(send_timer) > 600) {
		send_timer = TBTimer::init();

		if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
			&& GW::Agents().GetPlayer()) {

			GW::Chat().SendChat(queue.front().c_str(), L'#');
			queue.pop();
		}
	}
}
