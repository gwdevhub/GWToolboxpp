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


void SkillbarWidget::skill_cooldown_to_string(std::array<char, 16>& arr, uint32_t cd) const
{
    if (cd > 180000u || cd == 0) {
        snprintf(arr.data(), sizeof(arr), "");
    } else if (cd >= static_cast<uint32_t>(decimal_threshold)) {
        if (round_up) cd += 1000;
        snprintf(arr.data(), sizeof(arr), "%d", cd / 1000);
    } else {
        snprintf(arr.data(), sizeof(arr), "%.1f", cd / 1000.f);
    }
}

std::vector<SkillbarWidget::Effect> SkillbarWidget::get_effects(const uint32_t skillId) const
{
    std::vector<Effect> ret;
    for (const GW::Effect& effect : GW::Effects::GetPlayerEffectArray()) {
        if (effect.skill_id == skillId && effect.effect_type != 7 /*hex*/) {
            Effect e;
            e.remaining = effect.GetTimeRemaining();
            e.progress = (e.remaining / 1000.0f) / effect.duration;
            ret.emplace_back(e);
        }
    }
    return ret;
}

SkillbarWidget::Effect SkillbarWidget::get_longest_effect(const uint32_t skillId) const
{
    SkillbarWidget::Effect ret;
    for (const GW::Effect& effect : GW::Effects::GetPlayerEffectArray()) {
        if (effect.skill_id == skillId && effect.effect_type != 7 /*hex*/) {
            auto remaining = effect.GetTimeRemaining();
            if (ret.remaining < remaining) {
                ret.remaining = remaining;
                ret.progress = (ret.remaining / 1000.0f) / effect.duration;
            }
        }
    }
    return ret;
}

void SkillbarWidget::Update(float)
{
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (skillbar == nullptr) return;

    bool has_sf = false;
    for (size_t i = 0; i < 8 && !has_sf; i++) {
        has_sf = skillbar->skills[i].skill_id == static_cast<uint32_t>(GW::Constants::SkillID::Shadow_Form);
    }

    for (size_t it = 0u; it < 8; it++) {
        skill_cooldown_to_string(m_skills[it].cooldown, skillbar->skills[it].GetRecharge());
        const uint32_t &skill_id = skillbar->skills[it].skill_id;
        const Effect& effect = get_longest_effect(skill_id);
        m_skills[it].color = UptimeToColor(effect.remaining);

        if (display_effect_monitor) {

            m_skills[it].effects.clear();
            const auto& skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (display_multiple_effects && has_sf 
                && skill_data.profession == static_cast<uint8_t>(GW::Constants::Profession::Assassin) 
                && skill_data.type == static_cast<uint32_t>(GW::Constants::SkillType::Enchantment)) {

                m_skills[it].effects = get_effects(skill_id);
                std::sort(m_skills[it].effects.begin(), m_skills[it].effects.end(),
                    [](const Effect& a, const Effect& b) { return a.remaining > b.remaining; });

            } else if (effect.remaining > 0) {
                m_skills[it].effects.push_back(effect);
            }

            for (auto& e : m_skills[it].effects) {
                skill_cooldown_to_string(e.text, e.remaining);
                e.color = UptimeToColor(e.remaining);
            }
        }
    }
}

void SkillbarWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
    
    const auto window_flags = GetWinFlags();
    Color col_border = (window_flags & ImGuiWindowFlags_NoMove) ? color_border : Colors::White();

    ImGui::PushFont(GuiUtils::GetFont(font_recharge));

    const ImVec2 skillsize(static_cast<float>(m_skill_width), static_cast<float>(m_skill_height));
    ImVec2 winsize = skillsize;
    if (vertical) {
        winsize.y *= 8;
    } else {
        winsize.x *= 8;
    }

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::SetNextWindowSize(winsize);
    ImGui::Begin(Name(), nullptr, window_flags);
    ImVec2 winpos = ImGui::GetWindowPos();
    for (size_t i = 0; i < m_skills.size(); ++i) {
        const Skill& skill = m_skills[i];

        // position of this skill
        ImVec2 pos1(winpos.x, winpos.y);
        if (vertical) {
            pos1.y += (i * skillsize.y);
        } else {
            pos1.x += (i * skillsize.x);
        }
        ImVec2 pos2 = ImVec2(pos1.x + skillsize.x, pos1.y + skillsize.y);

        // draw overlay
        if (display_skill_overlay) {
            ImGui::GetBackgroundDrawList()->AddRectFilled(pos1, pos2, skill.color);
        }

        // border
        if (i != 7) {
            if (vertical) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos2.y), ImVec2(pos2.x, pos2.y), col_border);
            } else {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos2.x, pos1.y), ImVec2(pos2.x, pos2.y), col_border);
            }
        }
        // label
        ImVec2 label_size = ImGui::CalcTextSize(skill.cooldown.data());
        ImVec2 label_pos(pos1.x + skillsize.x / 2 - label_size.x / 2, 
            pos1.y + skillsize.y / 2 - label_size.y / 2);
        ImGui::GetWindowDrawList()->AddText(label_pos, color_text_recharge, skill.cooldown.data());

        if (display_effect_monitor) {
            DrawEffect(i, pos1);
        }
    }
    
    ImGui::GetBackgroundDrawList()->AddRect(winpos, 
        ImVec2(winpos.x + winsize.x, winpos.y + winsize.y), col_border);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}

void SkillbarWidget::DrawEffect(int skill_idx, const ImVec2& pos) const
{
    ImGui::PushFont(GuiUtils::GetFont(font_effects));

    const Skill& skill = m_skills[skill_idx];

    ImVec2 pos1 = pos; // base position
    if (vertical) {
        pos1.x += effect_monitor_offset;
    } else {
        pos1.y += effect_monitor_offset;
    }
    ImVec2 size;
    if (vertical) {
        size.x = ImGui::GetTextLineHeightWithSpacing(); // not really correct but works
        size.y = static_cast<float>(m_skill_height);
    } else {
        size.x = static_cast<float>(m_skill_width);
        size.y = ImGui::GetTextLineHeightWithSpacing();
    }

    for (size_t i = 0; i < skill.effects.size(); ++i) {
        const Effect& effect = skill.effects[i];

        if (i != 0) {
            if (vertical) {
                pos1.x += size.x;
            } else {
                pos1.y -= size.y;
            }
        }
        ImVec2 pos2(pos1.x + size.x, pos1.y + size.y); // base + size

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            pos1, pos2, color_effect_background);
        
        if (effect.progress >= 0.0f && effect.progress <= 1.0f) {
            ImVec2 pos3, pos4;
            if (vertical) {
                pos3 = ImVec2(pos1.x, pos2.y - size.y * effect.progress);
                pos4 = pos2;
            } else {
                pos3 = pos1;
                pos4 = ImVec2(pos1.x + size.x * effect.progress, pos2.y);
            }
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                pos3, pos4, effect_progress_bar_color ? effect.color : color_effect_progress);
        }

        ImGui::GetBackgroundDrawList()->AddRect(pos1, pos2, color_effect_border);

        ImVec2 label_size = ImGui::CalcTextSize(effect.text.data());
        ImVec2 label_pos(pos1.x + size.x / 2 - label_size.x / 2, pos1.y + size.y / 2 - label_size.y / 2);
        ImGui::GetBackgroundDrawList()->AddText(
            label_pos, effect_text_color ? effect.color : color_text_effects, effect.text.data());
    }

    ImGui::PopFont();
}

void SkillbarWidget::LoadSettings(CSimpleIni *ini)
{
    ToolboxWidget::LoadSettings(ini);
    vertical = ini->GetBoolValue(Name(), VAR_NAME(vertical), false);
    m_skill_height = ini->GetLongValue(Name(), "height", m_skill_height);
    m_skill_width = ini->GetLongValue(Name(), "width", m_skill_width);

    const long medium_treshold_i =
        ini->GetLongValue(Name(), VAR_NAME(medium_treshold), static_cast<int>(medium_treshold));
    if (medium_treshold_i > 0) {
        medium_treshold = static_cast<uint32_t>(medium_treshold_i);
    }
    const long short_treshold_i = ini->GetLongValue(Name(), VAR_NAME(short_treshold), static_cast<int>(short_treshold));
    if (short_treshold_i > 0) {
        short_treshold = static_cast<uint32_t>(short_treshold_i);
    }
    color_long = Colors::Load(ini, Name(), VAR_NAME(color_long), color_long);
    color_medium = Colors::Load(ini, Name(), VAR_NAME(color_medium), color_medium);
    color_short = Colors::Load(ini, Name(), VAR_NAME(color_short), color_short);

    decimal_threshold = ini->GetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
    round_up = ini->GetBoolValue(Name(), VAR_NAME(round_up), round_up);

    display_skill_overlay = ini->GetBoolValue(Name(), VAR_NAME(display_skill_overlay), display_skill_overlay);
    font_recharge = static_cast<GuiUtils::FontSize>(
        ini->GetLongValue(Name(), VAR_NAME(font_recharge), static_cast<long>(font_recharge)));
    color_text_recharge = Colors::Load(ini, Name(), VAR_NAME(color_text_recharge), color_text_recharge);
    color_border = Colors::Load(ini, Name(), VAR_NAME(color_border), color_border);

    display_effect_monitor = ini->GetBoolValue(Name(), VAR_NAME(display_effect_monitor), display_effect_monitor);
    effect_monitor_offset = ini->GetLongValue(Name(), VAR_NAME(effect_monitor_offset), effect_monitor_offset);
    display_multiple_effects = ini->GetBoolValue(Name(), VAR_NAME(display_multiple_effects), display_multiple_effects);
    effect_text_color = ini->GetBoolValue(Name(), VAR_NAME(effect_text_color), effect_text_color);
    effect_progress_bar_color =
        ini->GetBoolValue(Name(), VAR_NAME(effect_progress_bar_color), effect_progress_bar_color);
    font_effects = static_cast<GuiUtils::FontSize>(
        ini->GetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects)));
    color_text_effects = Colors::Load(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    color_effect_background = Colors::Load(ini, Name(), VAR_NAME(color_effect_background), color_effect_background);
    color_effect_progress = Colors::Load(ini, Name(), VAR_NAME(color_effect_progress), color_effect_progress);    
    color_effect_border = Colors::Load(ini, Name(), VAR_NAME(color_effect_border), color_effect_border);
}

void SkillbarWidget::SaveSettings(CSimpleIni *ini)
{
    ToolboxWidget::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(vertical), vertical);
    ini->SetLongValue(Name(), "height", static_cast<long>(m_skill_height));
    ini->SetLongValue(Name(), "width", static_cast<long>(m_skill_width));

    ini->SetLongValue(Name(), "medium_treshold", static_cast<long>(medium_treshold));
    ini->SetLongValue(Name(), "short_treshold", static_cast<long>(short_treshold));
    Colors::Save(ini, Name(), VAR_NAME(color_long), color_long);
    Colors::Save(ini, Name(), VAR_NAME(color_medium), color_medium);
    Colors::Save(ini, Name(), VAR_NAME(color_short), color_short);

    ini->SetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
    ini->SetBoolValue(Name(), VAR_NAME(round_up), round_up);

    ini->SetBoolValue(Name(), VAR_NAME(display_skill_overlay), display_skill_overlay);
    ini->SetLongValue(Name(), VAR_NAME(font_recharge), static_cast<long>(font_recharge));
    Colors::Save(ini, Name(), VAR_NAME(color_text_recharge), color_text_recharge);
    Colors::Save(ini, Name(), VAR_NAME(color_border), color_border);

    ini->SetBoolValue(Name(), VAR_NAME(display_effect_monitor), display_effect_monitor);
    ini->SetLongValue(Name(), VAR_NAME(effect_monitor_offset), effect_monitor_offset);
    ini->SetBoolValue(Name(), VAR_NAME(display_multiple_effects), display_multiple_effects);
    ini->SetBoolValue(Name(), VAR_NAME(effect_text_color), effect_text_color);
    ini->SetBoolValue(Name(), VAR_NAME(effect_progress_bar_color), effect_progress_bar_color);
    ini->SetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects));
    Colors::Save(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_background), color_effect_background);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_progress), color_effect_progress);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_border), color_effect_border);
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

    static constexpr char* font_sizes[] = {"16", "18", "20", "24", "42", "48"};

    ImGui::Spacing();
    ImGui::Text("Duration -> Color");
    Colors::DrawSettingHueWheel("Long color", &color_long);
    ImGui::DragInt("Medium Threshold", &medium_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the medium color is used.");
    Colors::DrawSettingHueWheel("Medium color", &color_medium);
    ImGui::DragInt("Short Threshold", &short_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the short color is used.");
    Colors::DrawSettingHueWheel("Short color", &color_short);

    ImGui::Spacing();
    ImGui::Text("Duration -> Text");
    ImGui::InputInt("Decimal threshold", &decimal_threshold);
    ImGui::ShowHelp("When should decimal numbers start to show (in milliseconds)");
    ImGui::Checkbox("Round up integers", &round_up);

    ImGui::Spacing();
    ImGui::Text("Skill overlay");
    ImGui::Checkbox("Paint skills according to effect duration", &display_skill_overlay);
    ImGui::Combo("Recharge font size", (int*)&font_recharge, font_sizes, 6);
    Colors::DrawSettingHueWheel("Recharge text color", &color_text_recharge);
    Colors::DrawSettingHueWheel("Skill border color", &color_border);
    
    ImGui::Spacing();
    ImGui::Text("Effect monitor");
    ImGui::Checkbox("Display effect monitor", &display_effect_monitor);
    ImGui::DragInt("Offset", &effect_monitor_offset, 1, -200, 200);
    ImGui::Checkbox("Display multiple effects", &display_multiple_effects);
    ImGui::ShowHelp("Show multiple casted enchantment per skill, when applicable");
    ImGui::Checkbox("Use the progress color for the text", &effect_text_color);
    ImGui::Checkbox("Use the progress color for the progress bar", &effect_progress_bar_color);
    ImGui::Combo("Effects font size", (int*)&font_effects, font_sizes, 6);
    Colors::DrawSettingHueWheel("Effect font color", &color_text_effects);
    Colors::DrawSettingHueWheel("Effect background", &color_effect_background);
    Colors::DrawSettingHueWheel("Effect progress bar color", &color_effect_progress);
    Colors::DrawSettingHueWheel("Effect border color", &color_effect_border);
    
    ImGui::ShowHelp("Whether effect uptime timers should be displayed above the skill.");
}

Color SkillbarWidget::UptimeToColor(const uint32_t uptime) const
{
    if (uptime > static_cast<uint32_t>(medium_treshold)) {
        return color_long;
    }

    if (uptime > static_cast<uint32_t>(short_treshold)) {
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
