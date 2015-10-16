#include "SettingsPanel.h"

#include <Windows.h>

#include "GWCA\APIMain.h"

#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"
#include "HealthWindow.h"
#include "TimerWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"

using namespace OSHGui;

SettingsPanel::SettingsPanel() {
}

void SettingsPanel::BuildUI() {
	GWToolbox* tb = GWToolbox::instance();
	Config* config = tb->config();

	location_current_map_ = GwConstants::MapID::None;
	location_timer_ = TBTimer::init();
	location_active_ = config->iniReadBool(MainWindow::IniSection(), MainWindow::IniKeySaveLocation(), false);

	const int item_width = GetWidth() - 2 * DefaultBorderPadding;
	const int item_height = 25;

	Label* version = new Label();
	version->SetText(wstring(L"GWToolbox++ version ") + GWToolbox::Version);
	version->SetLocation(GetWidth() / 2 - version->GetWidth() / 2, DefaultBorderPadding);
	AddControl(version);

	Label* authors = new Label();
	authors->SetText(L"by Has and KAOS");
	authors->SetLocation(GetWidth() / 2 - authors->GetWidth() / 2, version->GetBottom() + DefaultBorderPadding);
	AddControl(authors);

	CheckBox* tabsleft = new CheckBox();
	tabsleft->SetText(L"Open tabs on the left");
	tabsleft->SetLocation(DefaultBorderPadding, authors->GetBottom() + DefaultBorderPadding * 2);
	tabsleft->SetSize(item_width, item_height);
	tabsleft->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyTabsLeft(), false));
	tabsleft->GetCheckedChangedEvent() += CheckedChangedEventHandler([tabsleft](Control*) {
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyTabsLeft(), tabsleft->GetChecked());
		GWToolbox::instance()->main_window()->SetPanelPositions(tabsleft->GetChecked());
	});
	AddControl(tabsleft);

	CheckBox* freeze = new CheckBox();
	freeze->SetText(L"Freeze info widget positions");
	freeze->SetLocation(DefaultBorderPadding, tabsleft->GetBottom());
	freeze->SetSize(item_width, item_height);
	freeze->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyFreeze(), false));
	freeze->GetCheckedChangedEvent() += CheckedChangedEventHandler([freeze](Control*) {
		bool b = freeze->GetChecked();
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyFreeze(), b);
		GWToolbox::instance()->timer_window()->SetFreeze(b);
		GWToolbox::instance()->bonds_window()->SetFreze(b);
		GWToolbox::instance()->health_window()->SetFreeze(b);
		GWToolbox::instance()->distance_window()->SetFreeze(b);
	});
	AddControl(freeze);

	CheckBox* hidetarget = new CheckBox();
	hidetarget->SetText(L"Hide target windows");
	hidetarget->SetLocation(DefaultBorderPadding, freeze->GetBottom());
	hidetarget->SetSize(item_width, item_height);
	hidetarget->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyHideTarget(), false));
	hidetarget->GetCheckedChangedEvent() += CheckedChangedEventHandler([hidetarget](Control*) {
		bool hide = hidetarget->GetChecked();
		GWToolbox::instance()->health_window()->SetHideTarget(hide);
		GWToolbox::instance()->distance_window()->SetHideTarget(hide);
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyHideTarget(), hide);
	});
	AddControl(hidetarget);

	CheckBox* minimizealtpos = new CheckBox();
	minimizealtpos->SetText(L"Minimize to Alt Position");
	minimizealtpos->SetLocation(DefaultBorderPadding, hidetarget->GetBottom());
	minimizealtpos->SetSize(item_width, item_height);
	minimizealtpos->SetChecked(config->iniReadBool(MainWindow::IniSection(),
		MainWindow::IniKeyMinAltPos(), false));
	minimizealtpos->GetCheckedChangedEvent() += CheckedChangedEventHandler([minimizealtpos](Control*) {
		bool enabled = minimizealtpos->GetChecked();
		GWToolbox::instance()->main_window()->set_use_minimized_alt_pos(enabled);
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyMinAltPos(), enabled);
	});
	AddControl(minimizealtpos);

	CheckBox* tickwithpcons = new CheckBox();
	tickwithpcons->SetText(L"Tick with pcon status");
	tickwithpcons->SetLocation(DefaultBorderPadding, minimizealtpos->GetBottom());
	tickwithpcons->SetSize(item_width, item_height);
	tickwithpcons->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyTickWithPcons(), false));
	tickwithpcons->GetCheckedChangedEvent() += CheckedChangedEventHandler([tickwithpcons](Control*) {
		bool enabled = tickwithpcons->GetChecked();
		GWToolbox::instance()->main_window()->set_tick_with_pcons(enabled);
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyTickWithPcons(), enabled);
	});
	AddControl(tickwithpcons);

	CheckBox* savelocation = new CheckBox();
	savelocation->SetText(L"Save location data");
	savelocation->SetLocation(DefaultBorderPadding, tickwithpcons->GetBottom());
	savelocation->SetSize(item_width, item_height);
	savelocation->SetChecked(location_active_);
	savelocation->GetCheckedChangedEvent() += CheckedChangedEventHandler(
		[savelocation, this](Control*) {
		location_active_ = savelocation->GetChecked();
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeySaveLocation(), location_active_);
	});
	AddControl(savelocation);


	Button* folder = new Button();
	folder->SetText(L"Open Settings Folder");
	folder->SetSize(item_width, item_height);
	folder->SetLocation(DefaultBorderPadding, GetHeight() - DefaultBorderPadding - folder->GetHeight());
	folder->GetClickEvent() += ClickEventHandler([](Control*) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteW(NULL, L"open", GuiUtils::getSettingsFolder().c_str(), NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(folder);

	Button* website = new Button();
	website->SetText(L"Open GWToolbox++ Website");
	website->SetSize(item_width, item_height);
	website->SetLocation(DefaultBorderPadding, folder->GetTop() - DefaultBorderPadding - website->GetHeight());
	website->GetClickEvent() += ClickEventHandler([](Control*) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteW(NULL, L"open", GWToolbox::Host, NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(website);
}


void SettingsPanel::MainRoutine() {
	if (location_active_ && TBTimer::diff(location_timer_) > 1000) {
		location_timer_ = TBTimer::init();
		GWAPIMgr* api = GWAPIMgr::instance();
		if (api->Map()->GetInstanceType() == GwConstants::InstanceType::Explorable
			&& api->Agents()->GetPlayer() != nullptr
			&& api->Map()->GetInstanceTime() > 3000) {
			GwConstants::MapID current = api->Map()->GetMapID();
			if (location_current_map_ != current) {
				location_current_map_ = current;

				string map_string;
				switch (current) {
				case GwConstants::MapID::Domain_of_Anguish:
					map_string = "DoA";
					break;
				case GwConstants::MapID::Urgozs_Warren:
					map_string = "Urgoz";
					break;
				case GwConstants::MapID::The_Deep:
					map_string = "Deep";
					break;
				case GwConstants::MapID::The_Underworld:
					map_string = "UW";
					break;
				case GwConstants::MapID::The_Fissure_of_Woe:
					map_string = "FoW";
					break;
				default:
					map_string = string("Map-") + to_string(static_cast<long>(current));
				}

				string prof_string = "";
				GW::Agent* me = api->Agents()->GetPlayer();
				if (me) {
					prof_string += " - ";
					prof_string += api->Agents()->GetProfessionAcronym(
						static_cast<GwConstants::Profession>(me->Primary));
					prof_string += "-";
					prof_string += api->Agents()->GetProfessionAcronym(
						static_cast<GwConstants::Profession>(me->Secondary));
				}

				SYSTEMTIME localtime;
				GetLocalTime(&localtime);
				string filename = to_string(localtime.wYear)
					+ "-" + to_string(localtime.wMonth)
					+ "-" + to_string(localtime.wDay)
					+ " - " + to_string(localtime.wHour)
					+ "-" + to_string(localtime.wMinute)
					+ "-" + to_string(localtime.wSecond)
					+ " - " + map_string + prof_string + ".log";

				if (location_file_ && location_file_.is_open()) {
					location_file_.close();
				}
				location_file_.open(GuiUtils::getSubPathA(filename, "location logs").c_str());
			}

			GW::Agent* me = api->Agents()->GetPlayer();
			if (location_file_.is_open() && me != nullptr) {
				location_file_ << "Time=" << api->Map()->GetInstanceTime();
				location_file_ << " X=" << me->X;
				location_file_ << " Y=" << me->Y;
				location_file_ << "\n";
			}
		} else {
			location_current_map_ = GwConstants::MapID::None;
			location_file_.close();
		}
	}
}

