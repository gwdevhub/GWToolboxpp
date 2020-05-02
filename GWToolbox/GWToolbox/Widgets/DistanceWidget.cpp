#include "stdafx.h"
#include "DistanceWidget.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>


#include "GuiUtils.h"
#include "Modules/ToolboxSettings.h"

void DistanceWidget::DrawSettingInternal() {
	ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
}
void DistanceWidget::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
}
void DistanceWidget::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
}
void DistanceWidget::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
		return;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
		static char dist_perc[32];
		static char dist_abs[32];
		GW::Agent* me = GW::Agents::GetPlayer();
		GW::Agent* target = GW::Agents::GetTarget();
		if (me && target && me != target) {
			float dist = GW::GetDistance(me->pos, target->pos);
			snprintf(dist_perc, 32, "%2.0f %s", dist * 100 / GW::Constants::Range::Compass, "%%");
			snprintf(dist_abs, 32, "%.0f", dist);

			ImVec2 cur;

			// 'distance'
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(0, 0, 0), "Distance");
			ImGui::SetCursorPos(cur);
			ImGui::Text("Distance");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f42));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), dist_perc);
			ImGui::SetCursorPos(cur);
			ImGui::Text(dist_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), dist_abs);
			ImGui::SetCursorPos(cur);
			ImGui::Text(dist_abs);
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
