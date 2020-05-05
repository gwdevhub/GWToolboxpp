#include "stdafx.h"
#include "HealthWidget.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include "GuiUtils.h"
#include "Modules/ToolboxSettings.h"



void HealthWidget::LoadSettings(CSimpleIni *ini) {
	ToolboxWidget::LoadSettings(ini);
	click_to_print_health = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_health), click_to_print_health);
	hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
	color_default = Colors::Load(ini, Name(), VAR_NAME(below_half), ImColor(255, 255, 255));
	color_below_ninety = Colors::Load(ini, Name(), VAR_NAME(color_below_ninety), ImColor(255, 255, 255));
	color_below_half = Colors::Load(ini, Name(), VAR_NAME(color_below_half), ImColor(255, 255, 255));
}

void HealthWidget::SaveSettings(CSimpleIni *ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(click_to_print_health), click_to_print_health);
	ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
	Colors::Save(ini, Name(), VAR_NAME(color_default), color_default);
	Colors::Save(ini, Name(), VAR_NAME(color_below_ninety), color_below_ninety);
	Colors::Save(ini, Name(), VAR_NAME(color_below_half), color_below_half);
}

void HealthWidget::DrawSettingInternal() {
	ToolboxWidget::DrawSettingInternal();
	ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
	ImGui::Checkbox("Ctrl+Click to print target health", &click_to_print_health);
	Colors::DrawSetting("Default", &color_default);
	Colors::DrawSetting("<90%%", &color_below_ninety);
	Colors::DrawSetting("<50%%", &color_below_half);
}

void HealthWidget::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
		return;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiSetCond_FirstUseEver);
    bool ctrl_pressed = ImGui::IsKeyDown(VK_CONTROL);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(ctrl_pressed && click_to_print_health)))) {
		static char health_perc[32];
		static char health_abs[32];
		GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
		if (target) {
			if (target->hp >= 0) {
				snprintf(health_perc, 32, "%.0f %s", target->hp * 100, "%%");
			} else {
				snprintf(health_perc, 32, "-");
			}
			if (target->max_hp > 0) {
				float abs = target->hp * target->max_hp;
				snprintf(health_abs, 32, "%.0f / %d", abs, target->max_hp);
			} else {
				snprintf(health_abs, 32, "-");
			}

			ImVec2 cur;

			Color color = color_default;
			if (target->hp < .9) {
				color = color_below_ninety;
				if (target->hp < .5) {
					color = color_below_half;
				}
			}

			// 'health'
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(0, 0, 0), "Health");
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), "Health");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f42));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), health_perc);
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), health_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), health_abs);
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), health_abs);
			ImGui::PopFont();

            if (click_to_print_health) {
			    if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringWindow()) {
                    if (target) {
                        GW::Agents::AsyncGetAgentName(target, agent_name_ping);
                        if (agent_name_ping.size()) {
                            char buffer[512];
                            int current_hp = (int)(target->hp * target->max_hp);
                            snprintf(buffer, sizeof(buffer), "%S's Health is %d of %d. (%.0f %%)", agent_name_ping.c_str(), current_hp, target->max_hp, target->hp * 100.f);
                            GW::Chat::SendChat('#', buffer);
                        }
                    }
			    }
		    }
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
