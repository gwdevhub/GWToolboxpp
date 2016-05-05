#include "SettingsPanel.h"

#include <Windows.h>

#include <GWCA\GWCA.h>

#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"
#include "HealthWindow.h"
#include "TimerWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"

#include "Settings.h"

using namespace OSHGui;

void SettingsPanel::BuildUI() {
	Config& config = GWToolbox::instance().config();

	location_current_map_ = GwConstants::MapID::None;
	location_timer_ = TBTimer::init();
	location_active_ = false;

	const int item_width = GetWidth() - 2 * Padding;

	Label* version = new Label(this);
	version->SetText(wstring(L"GWToolbox++ version ") + GWToolbox::Version);
	version->SetLocation(PointI(GetWidth() / 2 - version->GetWidth() / 2, Padding / 2));
	AddControl(version);

	Label* authors = new Label(this);
	authors->SetText(L"by Has and KAOS");
	authors->SetFont(GuiUtils::getTBFont(8.0f, true));
	authors->SetLocation(PointI(GetWidth() / 2 - authors->GetWidth() / 2, version->GetBottom()));
	AddControl(authors);

	ScrollPanel* panel = new ScrollPanel(this);
	panel->SetLocation(PointI(Padding, authors->GetBottom() + Padding / 2));
	panel->SetWidth(GetWidth() - Padding * 2);
	panel->GetContainer()->SetBackColor(Color::Empty());
	panel->SetDeltaFactor(5);
	panel->SetAutoSize(true);
	AddControl(panel);

	// should be in the same order as the Settings_enum
	boolsettings.push_back(new OpenTabsLeft(panel->GetContainer()));
	boolsettings.push_back(new FreezeWidgets(panel->GetContainer()));
	boolsettings.push_back(new HideTargetWidgets(panel->GetContainer()));
	boolsettings.push_back(new MinimizeToAltPos(panel->GetContainer()));
	boolsettings.push_back(new AdjustOnResize(panel->GetContainer()));
	boolsettings.push_back(new BorderlessWindow(panel->GetContainer()));
	boolsettings.push_back(new SuppressMessages(panel->GetContainer()));
	boolsettings.push_back(new TickWithPcons(panel->GetContainer()));
	boolsettings.push_back(new OpenTemplateLinks(panel->GetContainer()));
	boolsettings.push_back(new SaveLocationData(panel->GetContainer()));

	for (size_t i = 0; i < boolsettings.size(); ++i) {
		boolsettings[i]->SetWidth(panel->GetContainer()->GetWidth());
		if (i == 0) {
			boolsettings[i]->SetLocation(PointI(0, 0));
		} else {
			boolsettings[i]->SetLocation(PointI(0, boolsettings[i - 1]->GetBottom() + Padding));
		}
		panel->AddControl(boolsettings[i]);
	}

	Button* folder = new Button(this);
	folder->SetText(L"Open Settings Folder");
	folder->SetSize(SizeI(item_width, GuiUtils::ROW_HEIGHT));
	folder->SetLocation(PointI(Padding, GetHeight() - Padding - folder->GetHeight()));
	folder->GetClickEvent() += ClickEventHandler([](Control*) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteW(NULL, L"open", GuiUtils::getSettingsFolder().c_str(), NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(folder);

	Button* website = new Button(this);
	website->SetText(L"Open GWToolbox++ Website");
	website->SetSize(SizeI(item_width, GuiUtils::ROW_HEIGHT));
	website->SetLocation(PointI(Padding, folder->GetTop() - Padding - website->GetHeight()));
	website->GetClickEvent() += ClickEventHandler([](Control*) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteW(NULL, L"open", GWToolbox::Host, NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(website);

	panel->SetHeight(website->GetTop() - authors->GetBottom() - Padding);
}

void SettingsPanel::ApplySettings() {
	for (BoolSetting* setting : boolsettings) {
		setting->ApplySetting(setting->ReadSetting());
	}
}


void SettingsPanel::MainRoutine() {
	// save location data
	if (location_active_ && TBTimer::diff(location_timer_) > 1000) {
		location_timer_ = TBTimer::init();
		if (GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Explorable
			&& GWCA::Agents().GetPlayer() != nullptr
			&& GWCA::Map().GetInstanceTime() > 3000) {
			GwConstants::MapID current = GWCA::Map().GetMapID();
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
				GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
				if (me) {
					prof_string += " - ";
					prof_string += GWCA::Agents().GetProfessionAcronym(
						static_cast<GwConstants::Profession>(me->Primary));
					prof_string += "-";
					prof_string += GWCA::Agents().GetProfessionAcronym(
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

			GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
			if (location_file_.is_open() && me != nullptr) {
				location_file_ << "Time=" << GWCA::Map().GetInstanceTime();
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

