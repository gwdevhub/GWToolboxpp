#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Widgets/DistanceWidget.h>

void DistanceWidget::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::SameLine();
    ImGui::Checkbox("Show percentage value", &show_perc_value);
    ImGui::SameLine();
    ImGui::Checkbox("Show absolute value", &show_abs_value);
    Colors::DrawSettingHueWheel("Adjacent Range", &color_adjacent, 0);
    Colors::DrawSettingHueWheel("Nearby Range", &color_nearby, 0);
    Colors::DrawSettingHueWheel("Area Range", &color_area, 0);
    Colors::DrawSettingHueWheel("Earshot Range", &color_earshot, 0);
    Colors::DrawSettingHueWheel("Cast Range", &color_cast, 0);
    Colors::DrawSettingHueWheel("Spirit Range", &color_spirit, 0);
    Colors::DrawSettingHueWheel("Compass Range", &color_compass, 0);
}

void DistanceWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(hide_in_outpost);
    LOAD_BOOL(show_perc_value);
    LOAD_BOOL(show_abs_value);
    LOAD_COLOR(color_adjacent);
    LOAD_COLOR(color_nearby);
    LOAD_COLOR(color_area);
    LOAD_COLOR(color_earshot);
    LOAD_COLOR(color_cast);
    LOAD_COLOR(color_spirit);
    LOAD_COLOR(color_compass);
}

void DistanceWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(hide_in_outpost);
    SAVE_BOOL(show_perc_value);
    SAVE_BOOL(show_abs_value);
    SAVE_COLOR(color_adjacent);
    SAVE_COLOR(color_nearby);
    SAVE_COLOR(color_area);
    SAVE_COLOR(color_earshot);
    SAVE_COLOR(color_cast);
    SAVE_COLOR(color_spirit);
    SAVE_COLOR(color_compass);
}

void DistanceWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    if (!show_perc_value && !show_abs_value) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        const GW::Agent* me = GW::Agents::GetPlayer();
        const GW::Agent* target = GW::Agents::GetTarget();
        if (me && target && me != target) {
            constexpr size_t buffer_size = 32;
            static char dist_perc[buffer_size];
            static char dist_abs[buffer_size];
            const float dist = GetDistance(me->pos, target->pos);
            if (show_perc_value) {
                snprintf(dist_perc, buffer_size, "%2.0f %s", dist * 100 / GW::Constants::Range::Compass, "%%");
            }
            if (show_abs_value) {
                snprintf(dist_abs, buffer_size, "%.0f", dist);
            }

            ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (dist <= GW::Constants::Range::Adjacent) {
                color = ImColor(color_adjacent);
            }
            else if (dist <= GW::Constants::Range::Nearby) {
                color = ImColor(color_nearby);
            }
            else if (dist <= GW::Constants::Range::Area) {
                color = ImColor(color_area);
            }
            else if (dist <= GW::Constants::Range::Earshot) {
                color = ImColor(color_earshot);
            }
            else if (dist <= GW::Constants::Range::Spellcast) {
                color = ImColor(color_cast);
            }
            else if (dist <= GW::Constants::Range::Spirit) {
                color = ImColor(color_spirit);
            }
            else if (dist <= GW::Constants::Range::Compass) {
                color = ImColor(color_compass);
            }

            const auto background = ImColor(Colors::Black());
            // 'distance'
            ImGui::PushFont(GetFont(GuiUtils::FontSize::header1));
            ImVec2 cur = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
            ImGui::TextColored(background, "Distance");
            ImGui::SetCursorPos(cur);
            ImGui::Text("Distance");
            ImGui::PopFont();

            // perc
            if (show_perc_value) {
                ImGui::PushFont(GetFont(GuiUtils::FontSize::widget_small));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                ImGui::TextColored(background, dist_perc);
                ImGui::SetCursorPos(cur);
                ImGui::TextColored(color, dist_perc);
                ImGui::PopFont();
            }

            // abs
            if (show_abs_value) {
                ImGui::PushFont(GetFont(GuiUtils::FontSize::widget_label));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                ImGui::TextColored(background, dist_abs);
                ImGui::SetCursorPos(cur);
                ImGui::Text(dist_abs);
                ImGui::PopFont();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
