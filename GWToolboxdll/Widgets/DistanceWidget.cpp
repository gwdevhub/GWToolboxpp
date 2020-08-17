#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GuiUtils.h>

#include <Modules/ToolboxSettings.h>
#include <Widgets/DistanceWidget.h>

void DistanceWidget::DrawSettingInternal() {
    ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    Colors::DrawSettingHueWheel("Adjacent Range", &color_adjacent, 0);
    Colors::DrawSettingHueWheel("Nearby Range", &color_nearby, 0);
    Colors::DrawSettingHueWheel("Area Range", &color_area, 0);
    Colors::DrawSettingHueWheel("Earshot Range", &color_earshot, 0);
    Colors::DrawSettingHueWheel("Cast Range", &color_cast, 0);
    Colors::DrawSettingHueWheel("Spirit Range", &color_spirit, 0);
    Colors::DrawSettingHueWheel("Compass Range", &color_compass, 0);
}
void DistanceWidget::LoadSettings(CSimpleIni* ini) {
    ToolboxWidget::LoadSettings(ini);
    hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    color_adjacent = Colors::Load(ini, Name(), VAR_NAME(color_adjacent), color_adjacent);
    color_nearby = Colors::Load(ini, Name(), VAR_NAME(color_nearby), color_nearby);
    color_area = Colors::Load(ini, Name(), VAR_NAME(color_area), color_area);
    color_earshot = Colors::Load(ini, Name(), VAR_NAME(color_earshot), color_earshot);
    color_cast = Colors::Load(ini, Name(), VAR_NAME(color_cast), color_cast);
    color_spirit = Colors::Load(ini, Name(), VAR_NAME(color_spirit), color_spirit);
    color_compass = Colors::Load(ini, Name(), VAR_NAME(color_compass), color_compass);
}
void DistanceWidget::SaveSettings(CSimpleIni* ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    Colors::Save(ini, Name(), VAR_NAME(color_adjacent), color_adjacent);
    Colors::Save(ini, Name(), VAR_NAME(color_nearby), color_nearby);
    Colors::Save(ini, Name(), VAR_NAME(color_area), color_area);
    Colors::Save(ini, Name(), VAR_NAME(color_earshot), color_earshot);
    Colors::Save(ini, Name(), VAR_NAME(color_cast), color_cast);
    Colors::Save(ini, Name(), VAR_NAME(color_spirit), color_spirit);
    Colors::Save(ini, Name(), VAR_NAME(color_compass), color_compass);
}
void DistanceWidget::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        static char dist_perc[32];
        static char dist_abs[32];
        GW::Agent* me = GW::Agents::GetPlayer();
        GW::Agent* target = GW::Agents::GetTarget();
        if (me && target && me != target) {
            float dist = GW::GetDistance(me->pos, target->pos);
            snprintf(dist_perc, 32, "%2.0f %s", dist * 100 / GW::Constants::Range::Compass, "%%");
            snprintf(dist_abs, 32, "%.0f", dist);

            ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (dist <= GW::Constants::Range::Adjacent) {
                color = ImColor(color_adjacent);
            } else if (dist <= GW::Constants::Range::Nearby) {
                color = ImColor(color_nearby);
            } else if (dist <= GW::Constants::Range::Area) {
                color = ImColor(color_area);
            } else if (dist <= GW::Constants::Range::Earshot) {
                color = ImColor(color_earshot);
            } else if (dist <= GW::Constants::Range::Spellcast) {
                color = ImColor(color_cast);
            } else if (dist <= GW::Constants::Range::Spirit) {
                color = ImColor(color_spirit);
            } else if (dist <= GW::Constants::Range::Compass) {
                color = ImColor(color_compass);
            }

            ImColor background = ImColor(Colors::Black());
            // 'distance'
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f20));
            ImVec2 cur = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
            ImGui::TextColored(background, "Distance");
            ImGui::SetCursorPos(cur);
            ImGui::Text("Distance");
            ImGui::PopFont();

            // perc
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f42));
            cur = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
            ImGui::TextColored(background, dist_perc);
            ImGui::SetCursorPos(cur);
            ImGui::TextColored(color, dist_perc);
            ImGui::PopFont();

            // abs
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f24));
            cur = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
            ImGui::TextColored(background, dist_abs);
            ImGui::SetCursorPos(cur);
            ImGui::Text(dist_abs);
            ImGui::PopFont();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
