#include "stdafx.h"

#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/WorldContext.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Utils/GuiUtils.h>
#include <Color.h>
#include <Defines.h>
#include "EffectsMonitorWidget.h"


namespace {
    GuiUtils::FontSize font_effects = GuiUtils::FontSize::header2;
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

    void DrawTextOverlay(const char* text, GW::UI::Frame* frame) {
        if (!(frame && text && *text && effects_frame)) return;
        const auto skill_top_left = frame->position.GetTopLeftOnScreen();
        const auto skill_bottom_right = frame->position.GetBottomRightOnScreen();

        const ImVec2 label_size = ImGui::CalcTextSize(text);
        const ImVec2 label_pos(skill_bottom_right.x - label_size.x, skill_bottom_right.y - label_size.y);
        if ((color_background & IM_COL32_A_MASK) != 0) {
            ImGui::GetBackgroundDrawList()->AddRectFilled(label_pos, { skill_bottom_right.x, skill_bottom_right.y }, color_background);
        }
        if ((color_text_shadow & IM_COL32_A_MASK) != 0) {
            ImGui::GetBackgroundDrawList()->AddText({ label_pos.x + 1, label_pos.y + 1 }, color_text_shadow, text);
        }
        ImGui::GetBackgroundDrawList()->AddText(label_pos, color_text_effects, text);
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
    ImGui::PushFont(GetFont(font_effects));

    char remaining_str[16];

    const auto hard_mode_frame = GW::UI::GetChildFrame(effects_frame, 1);
    // const auto minion_count_frame = GW::UI::GetChildFrame(effects_frame, 2);
    // const auto morale_frame = GW::UI::GetChildFrame(effects_frame, 3);

    if (hard_mode_frame && show_vanquish_counter) {
        const auto foes_left = GW::Map::GetFoesToKill();
        if (foes_left) {
            const auto foes_killed = GW::Map::GetFoesKilled();
            snprintf(remaining_str, 16, "%d/%d", foes_killed, foes_killed + foes_left);
            DrawTextOverlay(remaining_str, hard_mode_frame);
        }
    }
    const auto effects = GW::Effects::GetPlayerEffects();
    if (effects) {
        for (auto& effect : *effects) {
            const auto skill_frame = GW::UI::GetChildFrame(effects_frame, (uint32_t)effect.skill_id + 0x4);
            if (!skill_frame)
                continue;
            if (effect.duration <= 0)
                continue;
            const auto remaining = effect.GetTimeRemaining();
            if (remaining <= 0)
                continue;
            UptimeToString(remaining_str, static_cast<int>(remaining));
            DrawTextOverlay(remaining_str, skill_frame);
        }
    }

    ImGui::PopFont();
}

void EffectsMonitorWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    LOAD_UINT(decimal_threshold);
    LOAD_UINT(only_under_seconds);
    LOAD_BOOL(round_up);
    LOAD_BOOL(show_vanquish_counter);
    font_effects = static_cast<GuiUtils::FontSize>(
        ini->GetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects)));
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

    ini->SetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects));
    SAVE_COLOR(color_text_effects);
    SAVE_COLOR(color_text_shadow);
    SAVE_COLOR(color_background);
}

void EffectsMonitorWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    constexpr const char* font_sizes[] = {"16", "18", "20", "24", "42", "48"};

    ImGui::PushID("effects_monitor_overlay_settings");
    ImGui::Combo("Text size", reinterpret_cast<int*>(&font_effects), font_sizes, 6);
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
