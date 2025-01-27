#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Utils/FontLoader.h>
#include <Widgets/DistanceWidget.h>

namespace {
    bool hide_in_outpost = false;
    float font_size_header = static_cast<float>(FontLoader::FontSize::header1);
    float font_size_perc_value = static_cast<float>(FontLoader::FontSize::widget_small);
    float font_size_abs_value = static_cast<float>(FontLoader::FontSize::widget_label);

    Color color_adjacent = 0xFFFFFFFF;
    Color color_nearby = 0xFFFFFFFF;
    Color color_area = 0xFFFFFFFF;
    Color color_earshot = 0xFFFFFFFF;
    Color color_cast = 0xFFFFFFFF;
    Color color_spirit = 0xFFFFFFFF;
    Color color_compass = 0xFFFFFFFF;
}

void DistanceWidget::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::Text("Text sizes:");
    ImGui::ShowHelp("A text size of 0 means that it's not drawn.");
    ImGui::Indent();
    ImGui::DragFloat("'Distance' header", &font_size_header, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("Percent value", &font_size_perc_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("Absolute value", &font_size_abs_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::Unindent();
    ImGui::Text("Colors:");
    ImGui::Indent();
    Colors::DrawSettingHueWheel("Adjacent Range", &color_adjacent, 0);
    Colors::DrawSettingHueWheel("Nearby Range", &color_nearby, 0);
    Colors::DrawSettingHueWheel("Area Range", &color_area, 0);
    Colors::DrawSettingHueWheel("Earshot Range", &color_earshot, 0);
    Colors::DrawSettingHueWheel("Cast Range", &color_cast, 0);
    Colors::DrawSettingHueWheel("Spirit Range", &color_spirit, 0);
    Colors::DrawSettingHueWheel("Compass Range", &color_compass, 0);
    ImGui::Unindent();
}

void DistanceWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_FLOAT(font_size_header);
    LOAD_FLOAT(font_size_perc_value);
    LOAD_FLOAT(font_size_abs_value);
    LOAD_BOOL(hide_in_outpost);
    LOAD_COLOR(color_adjacent);
    LOAD_COLOR(color_nearby);
    LOAD_COLOR(color_area);
    LOAD_COLOR(color_earshot);
    LOAD_COLOR(color_cast);
    LOAD_COLOR(color_spirit);
    LOAD_COLOR(color_compass);
    auto_size = true;
}

void DistanceWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_FLOAT(font_size_header);
    SAVE_FLOAT(font_size_perc_value);
    SAVE_FLOAT(font_size_abs_value);
    SAVE_BOOL(hide_in_outpost);
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
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        const auto target = GW::Agents::GetTarget();
        const auto me = target ? GW::Agents::GetObservingAgent() : nullptr;
        if (me && me != target) {
            const float dist = GetDistance(me->pos, target->pos);

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

            ImVec2 cur = ImGui::GetCursorPos();
            constexpr auto background = ImColor(Colors::Black());
            // 'distance'
            if (font_size_header > 0.f) {
                ImGui::PushFont(FontLoader::GetFont(FontLoader::FontSize::header1));
                ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
                ImGui::TextColored(background, "Distance");
                ImGui::SetCursorPos(cur);
                ImGui::Text("Distance");
                ImGui::PopFont();
            }

            // perc
            if (font_size_perc_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFontByPx(font_size_perc_value));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                const auto dist_perc = std::format("{:2.0f} %%", dist * 100 / GW::Constants::Range::Compass);
                ImGui::TextColored(background, dist_perc.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::TextColored(color, dist_perc.c_str());
                ImGui::PopFont();
            }

            // abs
            if (font_size_abs_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFontByPx(font_size_abs_value));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                const auto dist_abs = std::format("{:.0f}", dist);
                ImGui::TextColored(background, dist_abs.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::Text(dist_abs.c_str());
                ImGui::PopFont();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
