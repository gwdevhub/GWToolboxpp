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
	Colors::DrawSetting("Widget Color", &color_widget);
	Colors::DrawSetting("Adjacent Range", &color_adjacent);
	Colors::DrawSetting("Nearby Range", &color_nearby);
	Colors::DrawSetting("Area Range", &color_area);
	Colors::DrawSetting("Earshot Range", &color_earshot);
	Colors::DrawSetting("Cast Range", &color_cast);
	Colors::DrawSetting("Spirit Range", &color_spirit);
	Colors::DrawSetting("Compass Range", &color_compass);
}
void DistanceWidget::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
	color_widget = Colors::Load(ini, Name(), VAR_NAME(color_widget), ImColor(255, 255, 255));
	color_adjacent = Colors::Load(ini, Name(), VAR_NAME(color_adjacent), ImColor(255, 255, 255));
	color_nearby = Colors::Load(ini, Name(), VAR_NAME(color_nearby), ImColor(255, 255, 255));
	color_area = Colors::Load(ini, Name(), VAR_NAME(color_area), ImColor(255, 255, 255));
	color_earshot = Colors::Load(ini, Name(), VAR_NAME(color_earshot), ImColor(255, 255, 255));
	color_cast = Colors::Load(ini, Name(), VAR_NAME(color_cast), ImColor(255, 255, 255));
	color_spirit = Colors::Load(ini, Name(), VAR_NAME(color_spirit), ImColor(255, 255, 255));
	color_compass = Colors::Load(ini, Name(), VAR_NAME(color_compass), ImColor(255, 255, 255));
}
void DistanceWidget::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
	Colors::Save(ini, Name(), VAR_NAME(color_widget), color_widget);
	Colors::Save(ini, Name(), VAR_NAME(color_adjacent), color_adjacent);
	Colors::Save(ini, Name(), VAR_NAME(color_nearby), color_nearby);
	Colors::Save(ini, Name(), VAR_NAME(color_area), color_area);
	Colors::Save(ini, Name(), VAR_NAME(color_earshot), color_earshot);
	Colors::Save(ini, Name(), VAR_NAME(color_cast), color_cast);
	Colors::Save(ini, Name(), VAR_NAME(color_spirit), color_spirit);
	Colors::Save(ini, Name(), VAR_NAME(color_compass), color_compass);
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

			Color color;
			if (dist <= GW::Constants::Range::Adjacent) {
				color = color_adjacent;
			} else if (dist <= GW::Constants::Range::Nearby) {
				color = color_nearby;
			} else if (dist <= GW::Constants::Range::Area) {
				color = color_area;
			} else if (dist <= GW::Constants::Range::Earshot) {
				color = color_earshot;
			} else if (dist <= GW::Constants::Range::Spellcast) {
				color = color_cast;
			} else if (dist <= GW::Constants::Range::Spirit) {
				color = color_spirit;
			} else if (dist <= GW::Constants::Range::Compass) {
				color = color_compass;
			} else {
				color = Colors::RGB(255, 255, 255);
			}

			Color background = Colors::RGB(0, 0, 0);

			// 'distance'
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(background), "Distance");
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color_widget), "Distance");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f42));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(background), dist_perc);
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), dist_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(background), dist_abs);
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color_widget), dist_abs);
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
