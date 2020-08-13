#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include "SkillbarWidget.h"

#include "Timer.h"

/*
 * Based off of @JuliusPunhal April skill timer - https://github.com/JuliusPunhal/April-old/blob/master/Source/April/SkillbarOverlay.cpp
 */

namespace
{
    int skill_cooldown_to_string(std::array<char, 16> &arr, const uint32_t cd)
    {
        if (cd > 180000u || cd <= 0)
            return snprintf(arr.data(), sizeof(arr), "");
        if (cd >= 10000u)
            return snprintf(arr.data(), sizeof(arr), "%d", cd / 1000);
        return snprintf(arr.data(), sizeof(arr), "%.1f", cd / 1000.f);
    }

    std::vector<uint32_t> get_effect_durations(const uint32_t skillId)
    {
        std::vector<uint32_t> durations;
        for (const GW::Effect &effect : GW::Effects::GetPlayerEffectArray()) {
            if (effect.skill_id != skillId)
                continue;
            if (effect.effect_type == 7) // hex
                continue;
            durations.emplace_back(effect.GetTimeRemaining());
        }
        return durations;
    }

    uint32_t get_longest_effect_duration(const uint32_t skillId)
    {
        const std::vector<uint32_t> durations = get_effect_durations(skillId);
        auto const it = std::max_element(durations.begin(), durations.end());
        if (it != durations.end())
            return *it;
        return 0u;
    }
} // namespace

void SkillbarWidget::Update(float)
{
    if (!visible || GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        return;
    const GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (skillbar == nullptr)
        return;
    bool has_sf = false;
    for (size_t i = 0; i < 8 && !has_sf; i++) {
        has_sf = skillbar->skills[i].skill_id == static_cast<uint32_t>(GW::Constants::SkillID::Shadow_Form);
    }

    for (size_t it = 0u; it < 8; it++) {
        skill_cooldown_to_string(m_skills[it].cooldown, skillbar->skills[it].GetRecharge());
        const uint32_t &skill_id = skillbar->skills[it].skill_id;
        const uint32_t effect_duration = get_longest_effect_duration(skill_id);
        m_skills[it].color = UptimeToColor(effect_duration);
        if (display_effect_times) {
            m_skills[it].effects.clear();
            auto const create_pair = [this](const uint32_t duration) -> std::pair<std::array<char, 16>, Color> {
                std::pair<std::array<char, 16>, Color> pair;
                skill_cooldown_to_string(pair.first, duration);
                pair.second = UptimeToColor(duration);
                return pair;
            };
            auto const skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (skill_data.profession == static_cast<uint8_t>(GW::Constants::Profession::Assassin) && has_sf && skill_data.type == static_cast<uint32_t>(GW::Constants::SkillType::Enchantment)) {
                std::vector<uint32_t> durations = get_effect_durations(skill_id);
                std::sort(durations.begin(), durations.end(), [](const uint32_t a, const uint32_t b) { return b < a; });
                std::transform(durations.begin(), durations.end(), std::back_inserter(m_skills[it].effects), create_pair);
            } else {
                if (effect_duration > 0)
                    m_skills[it].effects.emplace_back(create_pair(effect_duration));
            }
        }
    }

}

void SkillbarWidget::Draw(IDirect3DDevice9 *)
{
    if (!visible || GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        return;
    const auto wnd_flags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;

    ImGuiStyle &style = ImGui::GetStyle();
    const ImVec2 old_padding = style.WindowPadding;
    style.WindowPadding = {0, 0};

    ImGui::SetNextWindowBgAlpha(0.0f);
    const ImVec2 skillsize(static_cast<float>(m_skill_width), static_cast<float>(m_skill_height));
    ImVec2 winsize = skillsize;
    if (vertical) {
        winsize.y *= 8;
    } else {
        winsize.x *= 8;
    }
    ImGui::SetNextWindowSize(winsize);
    ImGui::Begin(Name(), nullptr, GetWinFlags());

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_Text, color_text);
    ImGui::PushFont(GuiUtils::GetFont(m_font_size));
    ImGui::PushStyleColor(ImGuiCol_Border, color_border);
    char imgui_id_buf[32];
    for (const SkillbarWidget::Skill& skill : m_skills) {
        ImGui::PushID(&skill);
        ImGui::PushStyleColor(ImGuiCol_Button, skill.color);
        const ImVec2 button_pos = ImGui::GetCursorPos();
        ImGui::Button(skill.cooldown.data(), skillsize);
        if (!vertical)
            ImGui::SameLine();
        auto const next_button_pos = ImGui::GetCursorPos();
        if (display_effect_times) {
            const ImVec2 wnd_size = vertical ? ImVec2{skillsize.x * 2, skillsize.y} : ImVec2{skillsize.x, skillsize.y * 2};
            const float font_scale = (1 + static_cast<float>(m_font_size) / 5.f);
            const ImVec2 smallsize(skillsize.x / 3.f * font_scale, skillsize.y / 3.f * font_scale);
            if (vertical)
                ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + static_cast<float>(m_effect_offset) - (m_effect_offset < 0 ? wnd_size.x / 2.f : 0), ImGui::GetWindowPos().y + button_pos.y});
            else
                ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + button_pos.x, ImGui::GetWindowPos().y + static_cast<float>(m_effect_offset) - (m_effect_offset < 0 ? wnd_size.y / 2.f : 0)});
            ImGui::SetNextWindowSize({wnd_size.x * font_scale, wnd_size.y * font_scale});
            ImGui::SetNextWindowBgAlpha(0.f);
            snprintf(imgui_id_buf, sizeof(imgui_id_buf), "Skill###%p", &skill);
            ImGui::Begin(imgui_id_buf, nullptr, wnd_flags);
            for (size_t i = 0u; i < skill.effects.size(); i++) {
                if (vertical) {
                    ImGui::SameLine();
                    if (m_effect_offset < 0) {
                        ImGui::SetCursorPosX(wnd_size.x - (i + 1) * smallsize.x);
                    } else {
                        ImGui::SetCursorPosX((i + 1) * smallsize.x);
                    }
                } else {
                    if (m_effect_offset < 0) {
                        ImGui::SetCursorPosY(wnd_size.y - (i + 1) * smallsize.y);
                    } else {
                        ImGui::SetCursorPosY((i + 1) * smallsize.y);
                    }
                }
                int colbuf[4];
                Colors::ConvertU32ToInt4(skill.effects.at(i).second, colbuf);
                colbuf[0] = 255;
                ImGui::PushStyleColor(ImGuiCol_Text, Colors::ConvertInt4ToU32(colbuf));
                ImGui::PushStyleColor(ImGuiCol_Button, color_effect_background);
                ImGui::PushStyleColor(ImGuiCol_Border, 0);
                if (vertical) {
                    ImGui::Button(skill.effects.at(i).first.data(), {smallsize.x, skillsize.y});
                } else {
                    ImGui::Button(skill.effects.at(i).first.data(), {skillsize.x, smallsize.y});
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::End();
        }
        ImGui::SetCursorPos(next_button_pos);
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    ImGui::End();
    style.WindowPadding = old_padding;
}

void SkillbarWidget::LoadSettings(CSimpleIni *ini)
{
    ToolboxWidget::LoadSettings(ini);
    color_text = Colors::Load(ini, Name(), VAR_NAME(color_text), color_text);
    color_border = Colors::Load(ini, Name(), VAR_NAME(color_border), color_border);
    color_long = Colors::Load(ini, Name(), VAR_NAME(color_long), color_long);
    color_medium = Colors::Load(ini, Name(), VAR_NAME(color_medium), color_medium);
    color_short = Colors::Load(ini, Name(), VAR_NAME(color_short), color_short);
    color_effect_background = Colors::Load(ini, Name(), VAR_NAME(color_effect_background), color_effect_background);
    m_skill_height = ini->GetLongValue(Name(), "height", m_skill_height);
    m_skill_width = ini->GetLongValue(Name(), "width", m_skill_width);
    vertical = ini->GetBoolValue(Name(), "vertical", false);
    display_effect_times = ini->GetBoolValue(Name(), "effect_text", false);
    m_effect_offset = ini->GetLongValue(Name(), "effect_offset", m_effect_offset);
    m_font_size = static_cast<GuiUtils::FontSize>(ini->GetLongValue(Name(), "font_size", m_font_size));
    const long medium_treshold_i = ini->GetLongValue(Name(), "medium_treshold", static_cast<int>(medium_treshold));
    if (medium_treshold_i > 0)
        medium_treshold = static_cast<uint32_t>(medium_treshold);
    const long short_treshold_i = ini->GetLongValue(Name(), "short_treshold", static_cast<int>(short_treshold));
    if (short_treshold_i > 0)
        short_treshold = static_cast<uint32_t>(short_treshold_i);
}

void SkillbarWidget::SaveSettings(CSimpleIni *ini)
{
    ToolboxWidget::SaveSettings(ini);
    Colors::Save(ini, Name(), VAR_NAME(color_text), color_text);
    Colors::Save(ini, Name(), VAR_NAME(color_border), color_border);
    Colors::Save(ini, Name(), VAR_NAME(color_long), color_long);
    Colors::Save(ini, Name(), VAR_NAME(color_medium), color_medium);
    Colors::Save(ini, Name(), VAR_NAME(color_short), color_short);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_background), color_effect_background);
    ini->SetLongValue(Name(), "height", static_cast<long>(m_skill_height));
    ini->SetLongValue(Name(), "width", static_cast<long>(m_skill_width));
    ini->SetBoolValue(Name(), "vertical", vertical);
    ini->SetLongValue(Name(), "medium_treshold", static_cast<long>(medium_treshold));
    ini->SetLongValue(Name(), "short_treshold", static_cast<long>(short_treshold));
    ini->SetBoolValue(Name(), "effect_text", display_effect_times);
    ini->SetLongValue(Name(), "effect_offset", m_effect_offset);
    ini->SetLongValue(Name(), "font_size", m_font_size);
}

void SkillbarWidget::DrawSettingInternal()
{
    ToolboxWidget::DrawSettingInternal();
    ImGui::SameLine();
    ImGui::Checkbox("Vertical", &vertical);
    int size[2]{m_skill_width, m_skill_height};
    if (ImGui::DragInt2("Skill size", size, 1.f, 1, 100)) {
        m_skill_width = size[0];
        m_skill_height = size[1];
    }
    int fontsize = static_cast<int>(m_font_size);
    if (ImGui::DragInt("Font size", &fontsize, 1, static_cast<int>(GuiUtils::FontSize::f16), static_cast<int>(GuiUtils::FontSize::f48))) {
        m_font_size = static_cast<GuiUtils::FontSize>(fontsize);
    }
    if (ImGui::TreeNode("Colors")) {
        Colors::DrawSettingHueWheel("Text color", &color_text);
        Colors::DrawSettingHueWheel("Border color", &color_border);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Effect uptime")) {
        int mediumdur = static_cast<int>(medium_treshold);
        if (ImGui::DragInt("Medium Treshold", &mediumdur, 1.f, 1, 180000)) {
            medium_treshold = static_cast<uint32_t>(mediumdur);
        }
        ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the medium color is used.");
        int shortdur = static_cast<int>(short_treshold);
        if (ImGui::DragInt("Short Treshold", &shortdur, 1.f, 1, 180000)) {
            short_treshold = static_cast<uint32_t>(shortdur);
        }
        ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the short color is used.");
        Colors::DrawSettingHueWheel("Long uptime", &color_long);
        Colors::DrawSettingHueWheel("Medium uptime", &color_medium);
        Colors::DrawSettingHueWheel("Short uptime", &color_short);
        Colors::DrawSettingHueWheel("Effect background", &color_effect_background);
        ImGui::Checkbox("Effect text", &display_effect_times);
        ImGui::DragInt("Text offset", &m_effect_offset, 1, -200, 200);
        ImGui::ShowHelp("Whether effect uptime timers should be displayed above the skill.");
        ImGui::TreePop();
    }
}

Color SkillbarWidget::UptimeToColor(const uint32_t uptime) const
{
    if (uptime > medium_treshold) {
        return color_long;
    }

    if (uptime > short_treshold) {
        const float diff = static_cast<float>(medium_treshold - short_treshold);
        const float fraction = 1.f - (medium_treshold - uptime) / diff;
        int colold[4], colnew[4], colout[4];
        Colors::ConvertU32ToInt4(color_long, colold);
        Colors::ConvertU32ToInt4(color_medium, colnew);
        for (size_t i = 0; i < 4; i++) {
            colout[i] = static_cast<int>((1.f - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    if (uptime > 0) {
        const float fraction = uptime / static_cast<float>(short_treshold);
        int colold[4], colnew[4], colout[4];
        Colors::ConvertU32ToInt4(color_medium, colold);
        Colors::ConvertU32ToInt4(color_short, colnew);
        for (auto i = 0; i < 4; i++) {
            colout[i] = static_cast<int>((1.f - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    return 0x00000000;
}
