#include "TradeWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
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
	// skip the Window initialize to avoid registering with ourselves
	ToolboxWindow::Initialize();
	alert_ini = new CSimpleIni(false, false, false);
	alert_ini->LoadFile(Resources::GetPath(ini_filename).c_str());
}

void TradeWindow::DrawSettingInternal() {

}

void TradeWindow::Update(float delta) {
	chat.fetch();
	std::string message;
	std::string final_chat_message;
	for (unsigned int i = 0; i < chat.new_messages.size(); i++) {
		message = chat.new_messages.at(i)["message"].dump();
		std::transform(message.begin(), message.end(), message.begin(), ::tolower);
		for (unsigned j = 0; j < alerts.size(); j++) {
			// ensure the alert isnt empty
			if (strncmp(alerts.at(j).match_string, "", 128)) {
				if (message.find(alerts.at(j).match_string) != std::string::npos) {
					final_chat_message = "<c=#f96677><a=1> " + chat.new_messages.at(i)["name"].get<std::string>() + "</a>: " +
						chat.new_messages.at(i)["message"].get<std::string>() + "</c>";
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
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		ImGui::PushTextWrapPos();
		if (ImGui::Button("Alerts", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
			show_alert_window = true;
		}
		ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 80.0f - 80.0f - ImGui::GetStyle().ItemInnerSpacing.x * 4));
		if (ImGui::InputText("", search_buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
			chat.search(search_buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Search", ImVec2(80.0f, 0))) {
			chat.search(search_buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear", ImVec2(80.0f, 0))) {
			strncpy(search_buffer, "", 256);
			chat.search("");
		}
		ImGui::BeginChild("trade_scroll");
		ImGui::Columns(3);
		ImGui::Text("Player Name");
		ImGui::SetColumnWidth(-1, 175);
		ImGui::NextColumn();
		ImGui::Text("Time");
		ImGui::SetColumnWidth(-1, 140);
		ImGui::NextColumn();
		ImGui::Text("Message");
		ImGui::SetColumnWidth(-1, 500);
		ImGui::NextColumn();
		std::string name;
		std::string message;
		time_t now = time(0);
		for (unsigned int i = 0; i < chat.messages.size(); i++) {
			ImGui::PushID(i);
			name = chat.messages.at(i)["name"].get<std::string>();
			message = chat.messages.at(i)["message"].get<std::string>();
			

			if (ImGui::Button(name.c_str())) {
				GW::GameThread::Enqueue([name]() {
					wchar_t ws[100];
					swprintf(ws, 100, L"%hs", name.c_str());
					GW::UI::SendUIMessage(GW::UI::kOpenWhisper, ws, nullptr);
				});
			}
			ImGui::NextColumn();
			// add 5 so we dont get negative numbers from server not being synced
			int time_since_message = 5 + (int)now - stoi(chat.messages.at(i)["timestamp"].get<std::string>());

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
			ImGui::NextColumn();
			ImGui::PushTextWrapPos();
			ImGui::Text("%s", message.c_str());
			ImGui::PopTextWrapPos();
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::EndChild();
		if (show_alert_window) {
			if (ImGui::Begin("Trade Alerts", &show_alert_window)) {
				if (ImGui::Button("New Alert", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
					alerts.insert(alerts.begin(), Alert());
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Click to add a new keyword.\n" \
						"\t- Trade messages with matched keywords will be send to the Guild Wars chat.\n" \
						"\t- The keywords are not case sensitive.\n" \
						"\t- The Trade checkbox in the Guild Wars chat must be selected for messages to show up."
					);
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
	chat.stop();
	ToolboxWindow::Terminate();
}