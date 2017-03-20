#include "HealthWindow.h"

#include <GWCA\Managers\AgentMgr.h>

#include "GuiUtils.h"
#include "OtherModules\ToolboxSettings.h"

void HealthWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
		static char health_perc[32];
		static char health_abs[32];
		GW::Agent* target = GW::Agents::GetTarget();
		if (target && target->GetIsLivingType()) {
			if (target->HP >= 0) {
				sprintf_s(health_perc, "%.0f %s", target->HP * 100, "%%");
			} else {
				sprintf_s(health_perc, "-");
			}
			if (target->MaxHP > 0) {
				float abs = target->HP * target->MaxHP;
				sprintf_s(health_abs, "%.0f / %d", abs, target->MaxHP);
			} else {
				sprintf_s(health_abs, "-");
			}

			ImVec2 cur;

			// 'health'
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f12]);
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(0, 0, 0), "Health");
			ImGui::SetCursorPos(cur);
			ImGui::Text("Health");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f26]);
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), health_perc);
			ImGui::SetCursorPos(cur);
			ImGui::Text(health_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f16]);
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), health_abs);
			ImGui::SetCursorPos(cur);
			ImGui::Text(health_abs);
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
