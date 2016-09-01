#include "SettingsPanel.h"

#include <Windows.h>

#include <GWCA\GWCA.h>

#include "Defines.h"

#include "Config.h"
#include "GuiUtils.h"

#include "Settings.h"

using namespace OSHGui;

void SettingsPanel::BuildUI() {
	location_current_map_ = GW::Constants::MapID::None;
	location_timer_ = TBTimer::init();
	location_active_ = false;

	const int item_width = GetWidth() - 2 * Padding;

	Label* version = new Label(this);
	version->SetText(wstring(L"GWToolbox++ version ") + GWTOOLBOX_VERSION);
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
	Panel* container = panel->GetContainer();
	boolsettings.push_back(new OpenTabsLeft(container));
	boolsettings.push_back(new FreezeWidgets(container));
	boolsettings.push_back(new HideTargetWidgets(container));
	boolsettings.push_back(new MinimizeToAltPos(container));
	boolsettings.push_back(new AdjustOnResize(container));
	boolsettings.push_back(new BorderlessWindow(container));
	boolsettings.push_back(new SuppressMessages(container));
	boolsettings.push_back(new TickWithPcons(container));
	boolsettings.push_back(new OpenTemplateLinks(container));
	boolsettings.push_back(new SaveLocationData(container));
	boolsettings.push_back(new NoBackgroundWidgets(container));

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
		ShellExecuteW(NULL, L"open", GWTOOLBOX_HOST, NULL, NULL, SW_SHOWNORMAL);
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
		if (GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& GW::Agents().GetPlayer() != nullptr
			&& GW::Map().GetInstanceTime() > 3000) {
			GW::Constants::MapID current = GW::Map().GetMapID();
			if (location_current_map_ != current) {
				location_current_map_ = current;

				string map_string;
				switch (current) {
				case GW::Constants::MapID::Domain_of_Anguish:
					map_string = "DoA";
					break;
				case GW::Constants::MapID::Urgozs_Warren:
					map_string = "Urgoz";
					break;
				case GW::Constants::MapID::The_Deep:
					map_string = "Deep";
					break;
				case GW::Constants::MapID::The_Underworld:
					map_string = "UW";
					break;
				case GW::Constants::MapID::The_Fissure_of_Woe:
					map_string = "FoW";
					break;
				default:
					map_string = string("Map-") + to_string(static_cast<long>(current));
				}

				string prof_string = "";
				GW::Agent* me = GW::Agents().GetPlayer();
				if (me) {
					prof_string += " - ";
					prof_string += GW::Constants::GetProfessionAcronym(
						static_cast<GW::Constants::Profession>(me->Primary));
					prof_string += "-";
					prof_string += GW::Constants::GetProfessionAcronym(
						static_cast<GW::Constants::Profession>(me->Secondary));
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

			GW::Agent* me = GW::Agents().GetPlayer();
			if (location_file_.is_open() && me != nullptr) {
				location_file_ << "Time=" << GW::Map().GetInstanceTime();
				location_file_ << " X=" << me->X;
				location_file_ << " Y=" << me->Y;
				location_file_ << "\n";
			}
		} else {
			location_current_map_ = GW::Constants::MapID::None;
			location_file_.close();
		}
	}
}

