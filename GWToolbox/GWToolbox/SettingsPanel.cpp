#include "SettingsPanel.h"

#include <Windows.h>

#include <GWCA\GWCA.h>

#include "Defines.h"

#include "Config.h"
#include "GuiUtils.h"

#include "GWToolbox.h"
#include "MainWindow.h"
#include "SettingManager.h"


SettingsPanel::SettingsPanel(OSHGui::Control* parent) : ToolboxPanel(parent),
	save_location_data(false, MainWindow::IniSection(), L"save_location") {

	save_location_data.SetText("Save location data");
	save_location_data.SetTooltip("When enabled, toolbox will save character location every second in a file in settings folder / location logs");
}

void SettingsPanel::BuildUI() {
	using namespace OSHGui;

	location_current_map_ = GW::Constants::MapID::None;
	location_timer_ = TBTimer::init();
	save_location_data.value = false;

	const int item_width = GetWidth() - 2 * Padding;

	Label* version = new Label(this);
	version->SetText(std::wstring(L"GWToolbox++ version ") + GWTOOLBOX_VERSION);
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
		ShellExecuteW(NULL, L"open", GWTOOLBOX_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(website);

	panel->SetHeight(website->GetTop() - authors->GetBottom() - Padding);
}


void SettingsPanel::Main() {
	// save location data
	if (save_location_data.value && TBTimer::diff(location_timer_) > 1000) {
		location_timer_ = TBTimer::init();
		if (GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& GW::Agents().GetPlayer() != nullptr
			&& GW::Map().GetInstanceTime() > 3000) {
			GW::Constants::MapID current = GW::Map().GetMapID();
			if (location_current_map_ != current) {
				location_current_map_ = current;

				std::string map_string;
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
					map_string = std::string("Map-") + std::to_string(static_cast<long>(current));
				}

				std::string prof_string = "";
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
				std::string filename = std::to_string(localtime.wYear)
					+ "-" + std::to_string(localtime.wMonth)
					+ "-" + std::to_string(localtime.wDay)
					+ " - " + std::to_string(localtime.wHour)
					+ "-" + std::to_string(localtime.wMinute)
					+ "-" + std::to_string(localtime.wSecond)
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

void SettingsPanel::Draw() {
	//ImGui::SetNextWindowSize(ImVec2(280, 285));
	ImGui::Begin("Settings Panel");
	if (ImGui::CollapsingHeader("Guild Wars General")) {
		GWToolbox::instance().settings().borderless_window.Draw();
		GWToolbox::instance().settings().open_template_links.Draw();
	}
	if (ImGui::CollapsingHeader("Toolbox++ General")) {
		GWToolbox::instance().settings().freeze_widgets.Draw();
		save_location_data.Draw();
	}
	if (ImGui::CollapsingHeader("Main Window")) {
		GWToolbox::instance().main_window().DrawSettings();
	}
	if (ImGui::CollapsingHeader("Minimap")) {

	}
	if (ImGui::CollapsingHeader("Chat Filter")) {
		GWToolbox::instance().chat_commands().chat_filter().DrawSettings();
	}
	if (ImGui::CollapsingHeader("Theme")) {
		
	}


	ImGui::End();
}
