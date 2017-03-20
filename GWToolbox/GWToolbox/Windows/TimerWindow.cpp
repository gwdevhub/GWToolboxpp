#include "TimerWindow.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>

#include "GuiUtils.h"
#include "OtherModules\ToolboxSettings.h"

void TimerWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

	unsigned long time = GW::Map::GetInstanceTime() / 1000;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
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
		if (GW::Map::GetMapID() == GW::Constants::MapID::Urgozs_Warren
			&& GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f16]);
			ImVec2 cur = ImGui::GetCursorPos();
			int temp = (time - 1) % 25;
			if (temp < 15) {
				sprintf_s(urgoz_timer, "Open - %d", 15 - temp);
			}  else {
				sprintf_s(urgoz_timer, "Closed - %d", 25 - temp);
			}

			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), urgoz_timer);
			ImGui::SetCursorPos(cur);
			ImColor color = temp < 15 ? ImColor(0, 255, 0) : ImColor(255, 0, 0);
			ImGui::TextColored(color, urgoz_timer);
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
