#include "TradeWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <Modules\Resources.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <json.hpp>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

#include <list>

unsigned int TradeWindow::Alert::uid_count = 0;

void TradeWindow::Initialize() {
	ToolboxWindow::Initialize();
	// used for the alerts
	all_trade = new TradeChat();
	// used for the window
	trade_searcher = new TradeChat();
	all_trade->search("");
}

void TradeWindow::DrawSettingInternal() {

}

void TradeWindow::Update(float delta) {
	all_trade->fetch();

	if (!all_trade->is_active() && alerts.size() > 0) {
		all_trade->search("");
	}
	else if (all_trade->is_active() && alerts.size() == 0) {
		all_trade->stop();
	}
	if (!visible && trade_searcher->is_active()) {
		trade_searcher->stop();
	}
	else if (visible && trade_searcher->is_timed_out()) {
		trade_searcher->search(search_buffer);
	}

	if (trade_searcher->is_active()) {
		trade_searcher->fetch();
	} 
	
	// do not display trade chat while in kamadan AE district 1
	if (GW::Map::GetMapID() == GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost &&
		GW::Map::GetDistrict() == 1 &&
		GW::Map::GetRegion() == GW::Constants::Region::America) {
		return;
	}
	std::string message;
	std::string final_chat_message;
	for (unsigned int i = 0; i < all_trade->new_messages.size(); i++) {
		message = all_trade->new_messages.at(i)["message"].dump();
		std::transform(message.begin(), message.end(), message.begin(), ::tolower);
		for (unsigned j = 0; j < alerts.size(); j++) {
			// ensure the alert isnt empty
			if (strncmp(alerts.at(j).match_string, "", 128)) {
				// check if the trade message matches the keyword, or if the alert matches the all messages keyword
				if (message.find(alerts.at(j).match_string) != std::string::npos || all_keyword.compare(alerts.at(j).match_string) == 0) {
					final_chat_message = "<c=#" + chat_color + "><a=1>" + all_trade->new_messages.at(i)["name"].get<std::string>() + "</a>: " +
						all_trade->new_messages.at(i)["message"].get<std::string>() + "</c>";
					GW::Chat::WriteChat(GW::Chat::CHANNEL_TRADE, final_chat_message.c_str());
					// break to stop multiple alerts triggering the same message
					break;
				}
			}
		}
	}
}

void TradeWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) { return; }
	// start the trade_searcher if its not active
	if (!trade_searcher->is_active()) trade_searcher->search("");
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		ImGui::PushTextWrapPos();

		/* Search bar header */
		ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 80.0f - 80.0f - 80.0f - ImGui::GetStyle().ItemInnerSpacing.x * 6));
		if (ImGui::InputText("", search_buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
			trade_searcher->search(search_buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Search", ImVec2(80.0f, 0))) {
			trade_searcher->search(search_buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear", ImVec2(80.0f, 0))) {
			strncpy(search_buffer, "", 256);
			trade_searcher->search("");
		}
		ImGui::SameLine();
		if (ImGui::Button("Alerts", ImVec2(80.0f, 0))) {
			show_alert_window = true;
		}

		/* Main trade chat area */
		ImGui::BeginChild("trade_scroll", ImVec2(0, -20.0f - ImGui::GetStyle().ItemInnerSpacing.y));
		/* Connection checks */
		if (trade_searcher->is_timed_out()) {
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("The connection to kamadan.decltype.com has timed out.").x) / 2);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
			ImGui::Text("The connection to kamadan.decltype.com has timed out.");
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Click to reconnect").x) / 2);
			if (ImGui::Button("Click to reconnect")) {
				trade_searcher->search(search_buffer);
			}
			ImGui::End();
			ImGui::End();
			return;
		}
		else if (trade_searcher->is_connecting()) {
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Connecting...").x)/2);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
			ImGui::Text("Connecting...");
			ImGui::End();
			ImGui::End();
			return;
		}

		/* Display trade messages */
		ImGui::Columns(3, NULL, false);
		// HasKha didn't want these headers
		//ImGui::Text("Time");
		ImGui::SetColumnWidth(-1, 100);
		ImGui::NextColumn();
		//ImGui::Text("Player Name");
		ImGui::SetColumnWidth(-1, 175);
		ImGui::NextColumn();
		//ImGui::Text("Message");
		ImGui::SetColumnWidth(-1, 500);
		ImGui::NextColumn();
		std::string name;
		std::string message;
		time_t now = time(0);

		for (unsigned int i = 0; i < trade_searcher->messages.size(); i++) {
			ImGui::PushID(i);
			
			// negative numbers have came from this before, it is probably just server client desync
			int time_since_message = (int)now - stoi(trade_searcher->messages.at(i)["timestamp"].get<std::string>());

			// smaller font for time column
			ImFont* small_font = (ImFont*)malloc(sizeof(ImFont));
			memcpy(small_font, ImGui::GetFont(), sizeof(ImFont));
			small_font->Scale = 0.83f;
			ImGui::PushFont(small_font);

			// decide if days, hours, minutes, seconds...
			if ((int)(time_since_message / (60 * 60 * 24))) {
				int days = (int)(time_since_message / (60 * 60 * 24));
				ImGui::Text("%d %s ago", days, days > 1 ? "days" : "day");
			}
			else if ((int)(time_since_message / (60 * 60))) {
				int hours = (int)(time_since_message / (60 * 60));
				ImGui::Text("%d %s ago", hours, hours > 1 ? "hours" : "hour");
			}
			else if ((int)(time_since_message / (60))) {
				int minutes = (int)(time_since_message / 60);
				ImGui::Text("%d %s ago", minutes, minutes > 1 ? "minutes" : "minute");
			}
			else {
				ImGui::Text("%d %s ago", time_since_message, time_since_message > 1 ? "seconds" : "second");
			}

			ImGui::PopFont();
			ImGui::NextColumn();

			name = trade_searcher->messages.at(i)["name"].get<std::string>();

			if (ImGui::Button(name.c_str())) {
				// open whisper to player
				GW::GameThread::Enqueue([name]() {
					wchar_t ws[100];
					swprintf(ws, 100, L"%hs", name.c_str());
					GW::UI::SendUIMessage(GW::UI::kOpenWhisper, ws, nullptr);
				});
			}

			ImGui::NextColumn();
			message = trade_searcher->messages.at(i)["message"].get<std::string>();
			ImGui::PushTextWrapPos();
			ImGui::Text("%s", message.c_str());
			ImGui::PopTextWrapPos();
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::EndChild();

		/* Link to website footer */
		if (ImGui::Button("https://kamadan.decltype.org", ImVec2(ImGui::GetWindowContentRegionWidth(), 20.0f))){ 
			CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			ShellExecute(NULL, "open", "https://kamadan.decltype.org", NULL, NULL, SW_SHOWNORMAL);
		}

		/* Alerts window */
		if (show_alert_window) {
			if (ImGui::Begin("Trade Alerts", &show_alert_window)) {
				if (ImGui::Button("New Alert", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
					alerts.insert(alerts.begin(), Alert());
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(alerts_tooltip.c_str());
				}
				for (unsigned int i = 0; i < alerts.size(); i++) {
					ImGui::PushID(alerts.at(i).uid);
					ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 24.0f - ImGui::GetStyle().ItemInnerSpacing.x * 2));
					ImGui::InputText("", alerts.at(i).match_string, 128);
					ImGui::SameLine();
					if (ImGui::Button("x", ImVec2(24.0f, 0))) {
						alerts.erase(alerts.begin() + i);
					}
					ImGui::PopID();
				}
			}
			ImGui::End();
		}
		ImGui::PopTextWrapPos();
	}
	ImGui::End();
}

void TradeWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	alert_ini = new CSimpleIni(false, false, false);
	alert_ini->LoadFile(Resources::GetPath(ini_filename).c_str());
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

	LoadAlerts();
}

void TradeWindow::LoadAlerts() {
	alerts.clear();
	CSimpleIni::TNamesDepend sections;
	alert_ini->GetAllSections(sections);
	for (CSimpleIni::Entry& ini_alert : sections) {
		const char* section = ini_alert.pItem;
		if (strncmp(section, "alert", 5) == 0) {
			alerts.push_back(Alert(alert_ini->GetValue(section, "match_string", "")));
		}
	}
}

void TradeWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	SaveAlerts();
}

void TradeWindow::SaveAlerts() {
	alert_ini->Reset();
	for (unsigned int i = 0; i < alerts.size(); i++) {
		char section[16];
		snprintf(section, 16, "alert%03d", i);
		alert_ini->SetValue(section, "match_string", alerts.at(i).match_string);
	}
	alert_ini->SaveFile(Resources::GetPath(ini_filename).c_str());
}

void TradeWindow::Terminate() {
	all_trade->stop();
	trade_searcher->stop();
	ToolboxWindow::Terminate();
}