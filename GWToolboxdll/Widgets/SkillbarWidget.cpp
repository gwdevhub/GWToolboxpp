#include "stdafx.h"

#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include "SkillbarWidget.h"

#include "Timer.h"

using namespace std::chrono_literals;

/*
 * Based off of @JuliusPunhal April skill timer - https://github.com/JuliusPunhal/April-old/blob/master/Source/April/SkillbarOverlay.cpp
 */

namespace
{
    auto ms_to_string_hmmss(std::chrono::milliseconds const time, const char *fmt) -> std::string
    {
        auto const hour = std::chrono::duration_cast<std::chrono::hours>(time);
        auto const mins = std::chrono::duration_cast<std::chrono::minutes>(time - hour);
        auto const secs = std::chrono::duration_cast<std::chrono::duration<int>>(time - hour - mins);

        char buf[16];
        snprintf(buf, sizeof(buf), fmt, hour.count(), mins.count(), secs.count());

        return std::string{buf};
    }

    auto ms_to_string_mmss(std::chrono::milliseconds const time, const char *fmt) -> std::string
    {
        auto const mins = std::chrono::duration_cast<std::chrono::minutes>(time);
        auto const secs = std::chrono::duration_cast<std::chrono::duration<int>>(time - mins);

        char buf[16];
        snprintf(buf, sizeof(buf), fmt, mins.count(), secs.count());

        return std::string{buf};
    }

    auto ms_to_string_hhmm(std::chrono::milliseconds const time, const char *fmt) -> std::string
    {
        auto const hour = std::chrono::duration_cast<std::chrono::hours>(time);
        auto const mins = std::chrono::duration_cast<std::chrono::minutes>(time - hour);

        char buf[16];
        snprintf(buf, sizeof(buf), fmt, hour.count(), mins.count());

        return std::string{buf};
    }

    auto ms_to_string_sec(std::chrono::milliseconds const time, const char *fmt = "%d") -> std::string
    {
        char buf[16];
        snprintf(buf, sizeof(buf), fmt, std::chrono::duration_cast<std::chrono::duration<int>>(time).count());
        return buf;
    }

    auto ms_to_string_secf(std::chrono::milliseconds const cd, const char *fmt = "%.1f") -> std::string
    {
        auto const secs = std::chrono::duration_cast<std::chrono::duration<float>>(cd);
        char buf[16];
        snprintf(buf, sizeof(buf), fmt, static_cast<int>(secs.count() * 10) / 10.f);
        return buf;
    }

    auto skill_cooldown_to_string(std::chrono::milliseconds const cd) -> std::string
    {
        if (cd >= 10s)
            return ms_to_string_sec(cd);
        if (cd > 0s)
            return ::ms_to_string_secf(cd);
        if (cd == 0s)
            return "";
        else              // cd < 0, happens occasionally due to ping
            return "0.0"; // avoid negative numbers in overlay
    }

    auto get_longest_effect_duration(GW::Constants::SkillID const skillId) -> std::chrono::milliseconds
    {
        auto longest = 0ms;
        for (auto const &effect : GW::Effects::GetPlayerEffectArray()) {
            if (static_cast<GW::Constants::SkillID>(effect.skill_id) != skillId)
                continue;
            longest = std::max(longest, std::chrono::milliseconds{effect.GetTimeRemaining()});
        }
        return longest;
    }

}

void SkillbarWidget::Draw(IDirect3DDevice9 *)
{
    if (!visible)
        return;
    [this]() {
        auto const *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (skillbar == nullptr)
            return;

        for (auto it = 0u; it < 8; it++) {
            m_skills[it].cooldown = skill_cooldown_to_string(std::chrono::milliseconds{skillbar->skills[it].GetRecharge()});

            auto const effect_duration = get_longest_effect_duration(static_cast<GW::Constants::SkillID>(skillbar->skills[it].skill_id));
            m_skills[it].color = UptimeToColor(effect_duration);
        }
    }();

    const auto wnd_flags = GetWinFlags();

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin(Name(), nullptr, wnd_flags);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_Text, color_text);
    ImGui::PushStyleColor(ImGuiCol_Border, color_border);
    {
        for (auto const &skill : m_skills) {
            ImGui::PushID(&skill);
            ImGui::PushStyleColor(ImGuiCol_Button, skill.color);
            {
                ImGui::Button(skill.cooldown.c_str(), {static_cast<float>(m_skill_width), static_cast<float>(m_skill_height)});
                if (!vertical)
                    ImGui::SameLine();
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::End();
}

void SkillbarWidget::LoadSettings(CSimpleIni *ini)
{
    ToolboxWidget::LoadSettings(ini);
    color_text = Colors::Load(ini, Name(), VAR_NAME(color_text), Colors::White());
    color_border = Colors::Load(ini, Name(), VAR_NAME(color_border), Colors::White());
    color_long = Colors::Load(ini, Name(), VAR_NAME(color_long), Colors::ARGB(50, 0, 255, 0));
    color_medium = Colors::Load(ini, Name(), VAR_NAME(color_medium), Colors::ARGB(50, 255, 255, 0));
    color_short = Colors::Load(ini, Name(), VAR_NAME(color_short), Colors::ARGB(50, 255, 0, 0));
    m_skill_height = static_cast<int>(ini->GetLongValue(Name(), "height", 50));
    m_skill_width = static_cast<int>(ini->GetLongValue(Name(), "width", 50));
    vertical = ini->GetBoolValue(Name(), "vertical", false);
    medium_treshold = std::chrono::milliseconds{ini->GetLongValue(Name(), "medium_treshold", 5000)};
    short_treshold = std::chrono::milliseconds{ini->GetLongValue(Name(), "short_treshold", 2500)};
}

void SkillbarWidget::SaveSettings(CSimpleIni *ini)
{
    ToolboxWidget::SaveSettings(ini);
    Colors::Save(ini, Name(), VAR_NAME(color), color_text);
    Colors::Save(ini, Name(), VAR_NAME(color_border), color_border);
    Colors::Save(ini, Name(), VAR_NAME(color_long), color_long);
    Colors::Save(ini, Name(), VAR_NAME(color_medium), color_medium);
    Colors::Save(ini, Name(), VAR_NAME(color_short), color_short);
    ini->SetLongValue(Name(), "height", static_cast<long>(m_skill_height));
    ini->SetLongValue(Name(), "width", static_cast<long>(m_skill_width));
    ini->SetBoolValue(Name(), "vertical", vertical);
    ini->SetLongValue(Name(), "medium_treshold", static_cast<long>(medium_treshold.count()));
    ini->SetLongValue(Name(), "short_treshold", static_cast<long>(short_treshold.count()));
}

void SkillbarWidget::DrawSettingInternal()
{
    ToolboxWidget::DrawSettingInternal();
    ImGui::SameLine();
    auto vertical_copy = vertical;
    if (ImGui::Checkbox("Vertical", &vertical_copy)) {
        vertical = vertical_copy;
    }

    if (ImGui::TreeNode("Colors")) {
        Colors::DrawSettingHueWheel("Text color", &color_text);
        Colors::DrawSettingHueWheel("Border color", &color_border);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Effect uptime")) {
        int mediumdur = static_cast<int>(medium_treshold.count());
        if (ImGui::DragInt("Medium Treshold", &mediumdur)) {
            medium_treshold = std::chrono::milliseconds{mediumdur};
        }
        ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the medium color is used.");
        int shortdur = static_cast<int>(short_treshold.count());
        if (ImGui::DragInt("Short Treshold", &shortdur)) {
            short_treshold = std::chrono::milliseconds{shortdur};
        }
        ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the short color is used.");
        Colors::DrawSettingHueWheel("Long uptime", &color_long);
        Colors::DrawSettingHueWheel("Medium uptime", &color_medium);
        Colors::DrawSettingHueWheel("Short uptime", &color_short);
        ImGui::TreePop();
    }
    auto height = m_skill_height;
    if (ImGui::DragInt("Skill height", &height)) {
        m_skill_height = height;
    }
    auto width = m_skill_width;
    if (ImGui::DragInt("Skill width", &width)) {
        m_skill_width = width;
    }
}

Color SkillbarWidget::UptimeToColor(std::chrono::milliseconds const uptime) const
{
    if (uptime > medium_treshold) {
        return color_long;
    }

    if (uptime > short_treshold) {
        auto const diff = static_cast<float>((medium_treshold - short_treshold).count());
        auto const fraction = 1 - ((medium_treshold.count() - uptime.count()) / diff);
        int colold[4], colnew[4], colout[4];
        Colors::ConvertU32ToInt4(color_long, colold);
        Colors::ConvertU32ToInt4(color_medium, colnew);
        for (auto i = 0; i < 4; i++) {
            colout[i] = static_cast<int>(static_cast<float>(1 - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    if (uptime > 0s) {
        auto const fraction = uptime.count() / static_cast<float>(short_treshold.count());
        int colold[4], colnew[4], colout[4];
        ;
        Colors::ConvertU32ToInt4(color_medium, colold);
        Colors::ConvertU32ToInt4(color_short, colnew);
        for (auto i = 0; i < 4; i++) {
            colout[i] = static_cast<int>(static_cast<float>(1 - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    return 0x00000000;
}