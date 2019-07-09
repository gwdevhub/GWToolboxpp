#include "stdafx.h"
#include "HealthWidget.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include "GuiUtils.h"
#include "Modules/ToolboxSettings.h"


void HealthWidget::LoadSettings(CSimpleIni *ini) {
	ToolboxWidget::LoadSettings(ini);
	click_to_print_health = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_health), false);
}

void HealthWidget::SaveSettings(CSimpleIni *ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(click_to_print_health), click_to_print_health);
}

void HealthWidget::DrawSettingInternal() {
	ToolboxWidget::DrawSettingInternal();
	ImGui::Checkbox("Ctrl+Click to print target health", &click_to_print_health);
}

void HealthWidget::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiSetCond_FirstUseEver);
    bool ctrl_pressed = ImGui::IsKeyDown(VK_CONTROL);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(ctrl_pressed && click_to_print_health)))) {
		static char health_perc[32];
		static char health_abs[32];
		GW::Agent* target = GW::Agents::GetTarget();
		if (target && target->GetIsCharacterType()) {
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

			// 'health'
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(0, 0, 0), "Health");
			ImGui::SetCursorPos(cur);
			ImGui::Text("Health");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f42));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), health_perc);
			ImGui::SetCursorPos(cur);
			ImGui::Text(health_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), health_abs);
			ImGui::SetCursorPos(cur);
			ImGui::Text(health_abs);
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
