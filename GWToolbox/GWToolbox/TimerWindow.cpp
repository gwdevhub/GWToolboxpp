#include "TimerWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>

#include "GuiUtils.h"
#include "OtherSettings.h"
#include "GWToolbox.h"

void TimerWindow::Draw() {
	unsigned long time = GW::Map().GetInstanceTime() / 1000;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
	if (GWToolbox::instance().settings().freeze_widgets.value) {
		flags |= ImGuiWindowFlags_NoInputs;
	}
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f));
	if (ImGui::Begin("Timer", nullptr, flags)) {
		static char timer[32];
		static char urgoz_timer[32];
		sprintf_s(timer, "%d:%02d:%02d", time / (60 * 60), (time / 60) % 60, time % 60);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f30]);
		ImVec2 cur = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
		ImGui::TextColored(ImColor(0, 0, 0), timer);
		ImGui::SetCursorPos(cur);
		ImGui::Text(timer);
		ImGui::PopFont();
		if (GW::Map().GetMapID() == GW::Constants::MapID::Urgozs_Warren
			&& GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable) {
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f16]);
			ImVec2 cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), urgoz_timer);
			ImGui::SetCursorPos(cur);
			int temp = (time - 1) % 25;
			if (temp < 15) {
				sprintf_s(urgoz_timer, "Open - %02d", 15 - temp);
				ImGui::TextColored(ImColor(0, 1, 0), urgoz_timer);
			} else {
				sprintf_s(urgoz_timer, "Closed - %02d", 25 - temp);
				ImGui::TextColored(ImColor(1, 0, 0), urgoz_timer);
			}
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
