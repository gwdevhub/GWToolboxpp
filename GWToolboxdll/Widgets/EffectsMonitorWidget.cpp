#include "stdafx.h"

#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/GuiUtils.h>
#include <Color.h>
#include <Defines.h>
#include "EffectsMonitorWidget.h"

#include "Utils/FontLoader.h"


namespace {
    auto font_effects = 18.f;
    Color color_text_effects = Colors::White();
    Color color_background = Colors::ARGB(128, 0, 0, 0);
    Color color_text_shadow = Colors::Black();

    // duration -> string settings
    int decimal_threshold = 600;   // when to start displaying decimals
    int only_under_seconds = 3600; // hide effect durations over n seconds
    bool round_up = true;          // round up or down?
    bool show_vanquish_counter = true;

    GW::UI::Frame* effects_frame = nullptr;

    int UptimeToString(char arr[8], int cd)
    {
        cd = std::abs(cd);
        if (cd > only_under_seconds * 1000) {
            return 0;
        }
        if (cd >= decimal_threshold) {
            if (round_up) {
                cd += 1000;
            }
            return snprintf(arr, 8, "%d", cd / 1000);
        }
        return snprintf(arr, 8, "%.1f", static_cast<double>(cd) / 1000.0);
    }

    void DrawTextOverlay(const char* text, const GW::UI::Frame* frame)
    {
        if (!(frame && text && *text && effects_frame)) return;
        auto skill_bottom_right = frame->position.GetBottomRightOnScreen();

        const auto viewport = ImGui::GetMainViewport();
        skill_bottom_right.x += viewport->Pos.x;
        skill_bottom_right.y += viewport->Pos.y;

        const auto draw_list = ImGui::GetBackgroundDrawList(viewport);

        const ImVec2 label_size = ImGui::CalcTextSize(text);
        const ImVec2 label_pos(skill_bottom_right.x - label_size.x, skill_bottom_right.y - label_size.y);
        if ((color_background & IM_COL32_A_MASK) != 0) {
            draw_list->AddRectFilled(label_pos, skill_bottom_right, color_background);
        }
        if ((color_text_shadow & IM_COL32_A_MASK) != 0) {
            draw_list->AddText({label_pos.x + 1, label_pos.y + 1}, color_text_shadow, text);
        }
        draw_list->AddText(label_pos, color_text_effects, text);
    }
}

void EffectsMonitorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    effects_frame = GW::UI::GetFrameByLabel(L"Effects");
    if (!effects_frame) {
        return;
    }
    const auto viewport = ImGui::GetMainViewport();
    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);
    const auto font = FontLoader::GetFontByPx(font_effects);
    ImGui::PushFont(font, draw_list, font_effects);

    const auto hard_mode_frame = GW::UI::GetChildFrame(effects_frame, 1);
    // const auto minion_count_frame = GW::UI::GetChildFrame(effects_frame, 2);
    // const auto morale_frame = GW::UI::GetChildFrame(effects_frame, 3);

    if (hard_mode_frame && show_vanquish_counter) {
        if (const auto foes_left = GW::Map::GetFoesToKill()) {
            const auto foes_killed = GW::Map::GetFoesKilled();
            std::array<char, 16> remaining_str;
            snprintf(remaining_str.data(), 16, "%d/%d", foes_killed, foes_killed + foes_left);
            DrawTextOverlay(remaining_str.data(), hard_mode_frame);
        }
    }
    const auto effects = GW::Effects::GetPlayerEffects();

    if (effects) {
        std::unordered_map<GW::Constants::SkillID, DWORD> time_remaining_by_effect;
        for (auto& effect : *effects) {
            if (effect.duration <= 0)
                continue;
            const auto remaining = effect.GetTimeRemaining();
            if (remaining <= 0)
                continue;
            const auto found = time_remaining_by_effect.find(effect.skill_id);
            if (found == time_remaining_by_effect.end() || found->second < remaining) {
                time_remaining_by_effect[effect.skill_id] = remaining;
            }
        }
        for (auto& [skill_id, remaining] : time_remaining_by_effect) {
            const auto skill_frame = GW::UI::GetChildFrame(effects_frame, (uint32_t)skill_id + 0x4);
            if (!skill_frame)
                continue;
            std::array<char, 16> remaining_str;
            if (UptimeToString(remaining_str.data(), static_cast<int>(remaining)) > 0)
                DrawTextOverlay(remaining_str.data(), skill_frame);
        }
    }

    ImGui::PopFont(draw_list);
}

void EffectsMonitorWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    LOAD_UINT(decimal_threshold);
    LOAD_UINT(only_under_seconds);
    LOAD_BOOL(round_up);
    LOAD_BOOL(show_vanquish_counter);
    LOAD_FLOAT(font_effects);
    LOAD_COLOR(color_text_effects);
    LOAD_COLOR(color_text_shadow);
    LOAD_COLOR(color_background);
}

void EffectsMonitorWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);

    SAVE_UINT(decimal_threshold);
    SAVE_UINT(only_under_seconds);
    SAVE_BOOL(round_up);
    SAVE_BOOL(show_vanquish_counter);
    SAVE_FLOAT(font_effects);
    SAVE_COLOR(color_text_effects);
    SAVE_COLOR(color_text_shadow);
    SAVE_COLOR(color_background);
}

void EffectsMonitorWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    ImGui::PushID("effects_monitor_overlay_settings");

    ImGui::DragFloat("Text size", &font_effects, 1.f, 16.f, 48.f, "%.f");
    Colors::DrawSettingHueWheel("Text color", &color_text_effects);
    Colors::DrawSettingHueWheel("Text shadow", &color_text_shadow);
    Colors::DrawSettingHueWheel("Effect duration background", &color_background);
    ImGui::Text("Don't show effect durations longer than");
    ImGui::SameLine();
    ImGui::PushItemWidth(64.f * ImGui::FontScale());
    ImGui::InputInt("###only_under_seconds", &only_under_seconds, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("seconds");
    ImGui::Text("Show decimal places when duration is less than");
    ImGui::SameLine();
    ImGui::PushItemWidth(64.f * ImGui::FontScale());
    ImGui::InputInt("###decimal_threshold", &decimal_threshold, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("milliseconds");
    ImGui::Checkbox("Round up integers", &round_up);
    ImGui::SameLine();
    ImGui::Checkbox("Show vanquish counter on Hard Mode effect icon", &show_vanquish_counter);
    ImGui::PopID();
}
