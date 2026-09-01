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
    DistanceWidget::Settings settings;
}

void DistanceWidget::DrawSettingsInternal()
{
    ImGui::SameLine();
    ImGui::Checkbox("在前哨站隐藏", &settings.hide_in_outpost);
    ImGui::Text("文字大小：");
    ImGui::ShowHelp("文字大小为 0 表示不绘制。");
    ImGui::Indent();
    ImGui::DragFloat("'距离' 标题", &settings.font_size_header, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("百分比数值", &settings.font_size_perc_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("绝对数值", &settings.font_size_abs_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::Unindent();
    ImGui::Text("颜色：");
    ImGui::Indent();
    Colors::DrawSettingHueWheel("邻近范围", &settings.color_adjacent.value, 0);
    Colors::DrawSettingHueWheel("附近范围", &settings.color_nearby.value, 0);
    Colors::DrawSettingHueWheel("区域范围", &settings.color_area.value, 0);
    Colors::DrawSettingHueWheel("听觉范围", &settings.color_earshot.value, 0);
    Colors::DrawSettingHueWheel("施法范围", &settings.color_cast.value, 0);
    Colors::DrawSettingHueWheel("灵范围", &settings.color_spirit.value, 0);
    Colors::DrawSettingHueWheel("罗盘范围", &settings.color_compass.value, 0);
    ImGui::Unindent();
}

void DistanceWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
}

void DistanceWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    auto_size = true;
}

void DistanceWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void DistanceWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (settings.hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
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
                color = ImColor(settings.color_adjacent.value);
            }
            else if (dist <= GW::Constants::Range::Nearby) {
                color = ImColor(settings.color_nearby.value);
            }
            else if (dist <= GW::Constants::Range::Area) {
                color = ImColor(settings.color_area.value);
            }
            else if (dist <= GW::Constants::Range::Earshot) {
                color = ImColor(settings.color_earshot.value);
            }
            else if (dist <= GW::Constants::Range::Spellcast) {
                color = ImColor(settings.color_cast.value);
            }
            else if (dist <= GW::Constants::Range::Spirit) {
                color = ImColor(settings.color_spirit.value);
            }
            else if (dist <= GW::Constants::Range::Compass) {
                color = ImColor(settings.color_compass.value);
            }

            ImVec2 cur = ImGui::GetCursorPos();
            constexpr auto background = ImColor(Colors::Black());
            if (settings.font_size_header > 0.f && show_titlebar) {
                ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header1));
                ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
                ImGui::TextColored(background, "距离");
                ImGui::SetCursorPos(cur);
                ImGui::Text("距离");
                ImGui::PopFont();
            }

            if (settings.font_size_perc_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFont(), settings.font_size_perc_value);
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                const auto dist_perc = std::format("{:2.0f} %%", dist * 100 / GW::Constants::Range::Compass);
                ImGui::TextColored(background, dist_perc.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::TextColored(color, dist_perc.c_str());
                ImGui::PopFont();
            }

            if (settings.font_size_abs_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFont(), settings.font_size_abs_value);
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
