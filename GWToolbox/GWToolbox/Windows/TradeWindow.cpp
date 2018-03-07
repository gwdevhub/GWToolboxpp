#include "TradeWindow.h"

#include <GWCA\GWCA.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <json.hpp>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

#include <list>

void TradeWindow::Initialize() {
	// skip the Window initialize to avoid registering with ourselves
	ToolboxWindow::Initialize();
}

void TradeWindow::DrawSettingInternal() {

}

void TradeWindow::Update(float delta) {
	chat.fetch();
}

void TradeWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) { return; }
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		ImGui::PushTextWrapPos();
		ImGui::Columns(2);
		ImGui::Text("Player Name");
		ImGui::SetColumnWidth(-1, 175);
		ImGui::NextColumn();
		ImGui::Text("Message");
		ImGui::SetColumnWidth(-1, 500);
		ImGui::NextColumn();
		std::string name;
		std::string message;
		for (unsigned int i = 0; i < chat.messages.size(); i++) {
			name = chat.messages.at(i)["name"].dump();
			name = name.substr(1, name.size() - 2);
			message = chat.messages.at(i)["message"].dump();
			message = message.substr(1, message.size() - 2);
			ImGui::Text("%s", name.c_str());
			ImGui::NextColumn();
			ImGui::Text("%s", message.c_str());
			ImGui::NextColumn();
		}
		ImGui::PopTextWrapPos();
	}
	ImGui::End();
}

void TradeWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
}

void TradeWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
}

void TradeWindow::Terminate() {
	chat.stop();
	ToolboxWindow::Terminate();
}