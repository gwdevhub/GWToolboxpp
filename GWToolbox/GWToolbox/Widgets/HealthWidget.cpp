#include <time.h>
#include <stdint.h>

#include <Windows.h>

#include <fstream>
#include <functional>

#include <imgui.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include "GuiUtils.h"
#include "ToolboxWidget.h"

#include "Modules/ToolboxSettings.h"

#include "Widgets/HealthWidget.h"

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
                ImVec2 size = ImGui::GetWindowSize();
                ImVec2 min = ImGui::GetWindowPos();
                ImVec2 max(min.x + size.x, min.y + size.y);
                if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
                    if (target) {
                        std::wstring name;
                        GW::Agents::AsyncGetAgentName(target, name);
                        if (name.size()) {
                            char buffer[512];
                            int current_hp = (int)(target->HP * target->MaxHP);
                            snprintf(buffer, sizeof(buffer), "%S's Health is %d of %d. (%.0f %%)", name.c_str(), current_hp, target->MaxHP, target->HP * 100.f);
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
