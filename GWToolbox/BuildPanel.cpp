#include "BuildPanel.h"

#include "../API/APIMain.h"

#include "GWToolbox.h"
#include "Config.h"

using namespace std;
using namespace OSHGui;

void BuildPanel::Build::BuildUI() {
	SetBackColor(Drawing::Color::Empty());

	const int edit_button_width = 60;

	Button* button = new Button();
	button->SetText(name_);
	button->SetSize(GetWidth() - edit_button_width - DefaultBorderPadding, GetHeight());
	button->SetLocation(0, 0);
	button->GetClickEvent() += ClickEventHandler([this](Control*) {
		SendTeamBuild();
	});
	AddControl(button);

	Button* edit = new Button();
	edit->SetText("Edit");
	edit->SetSize(edit_button_width, GetHeight());
	edit->SetLocation(GetWidth() - edit->GetWidth(), 0);
	edit->GetClickEvent() += ClickEventHandler([this, button](Control*) {
		edit_build_->SetEditedBuild(index_, button);
	});
	AddControl(edit);
}

void BuildPanel::Build::SendTeamBuild() {
	Config* config = GWToolbox::instance()->config();
	wstring section = wstring(L"builds") + to_wstring(index_);
	wstring key;

	key = L"buildname";
	wstring buildname = config->iniRead(section.c_str(), key.c_str(), L"");
	if (!buildname.empty()) {
		panel_->Enqueue(buildname);
	}

	bool show_numbers = config->iniReadBool(section.c_str(), L"showNumbers", true);

	for (int i = 0; i < edit_build_->N_PLAYERS; ++i) {
		key = L"name" + to_wstring(i + 1);
		wstring name = config->iniRead(section.c_str(), key.c_str(), L"");

		key = L"template" + to_wstring(i + 1);
		wstring temp = config->iniRead(section.c_str(), key.c_str(), L"");
		
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

BuildPanel::BuildPanel() {
	builds = vector<Build*>();
	first_shown_ = 0;
	send_timer = TBTimer::init();
}

void BuildPanel::OnMouseScroll(const MouseMessage &mouse) {
	int delta = mouse.GetDelta() > 0 ? 1 : -1;
	scrollbar_->SetValue(scrollbar_->GetValue() + delta);
}

void BuildPanel::set_first_shown(int first) {
	if (first < 0) {
		first_shown_ = 0;
	} else if (first > N_BUILDS - MAX_SHOWN) {
		first_shown_ = N_BUILDS - MAX_SHOWN;
	} else {
		first_shown_ = first;
	}

	for (int i = 0; i < N_BUILDS; ++i) {
		builds[i]->SetLocation(DefaultBorderPadding,
			DefaultBorderPadding + (i - first_shown_) * (BUILD_HEIGHT + DefaultBorderPadding));
	
		builds[i]->SetVisible(i >= first_shown_ && i < first_shown_ + MAX_SHOWN);
	}
}

void BuildPanel::BuildUI() {

	Config* config = GWToolbox::instance()->config();

	edit_build_ = new EditBuild();
	edit_build_->SetVisible(false);
	AddControl(edit_build_);

	ScrollBar* scrollbar = new ScrollBar();
	scrollbar->SetSize(scrollbar->GetWidth(), GetHeight());
	scrollbar->SetLocation(GetWidth() - scrollbar->GetWidth(), 0);
	scrollbar->SetMaximum(N_BUILDS - MAX_SHOWN);
	scrollbar->GetScrollEvent() += ScrollEventHandler([this, scrollbar](Control*, ScrollEventArgs) {
		this->set_first_shown(scrollbar->GetValue());
	});
	scrollbar_ = scrollbar;
	AddControl(scrollbar);

	for (int i = 0; i < N_BUILDS; ++i) {
		int index = i + 1;
		wstring section = wstring(L"builds") + to_wstring(index);
		wstring wname = config->iniRead(section.c_str(), L"buildname", L"");
		if (wname.empty()) wname = wstring(L"<Build ") + to_wstring(index) + wstring(L">");
		string name = string(wname.begin(), wname.end());
		Build* build = new Build(index, name, edit_build_, this);
		build->SetSize(GetWidth() - scrollbar->GetWidth() - 2 * DefaultBorderPadding, BUILD_HEIGHT);
		build->BuildUI();
		builds.push_back(build);
		AddControl(builds[i]);
	}

	set_first_shown(0);
}

void BuildPanel::MainRoutine() {
	if (!queue.empty() && TBTimer::diff(send_timer) > 600) {
		send_timer = TBTimer::init();

		GWAPIMgr* api = GWAPIMgr::instance();
		if (api->Map()->GetInstanceType() != GwConstants::InstanceType::Loading
			&& api->Agents()->GetPlayer()) {

			api->Chat()->SendChat(queue.front().c_str(), L'#');
			queue.pop();
		}
	}
}
