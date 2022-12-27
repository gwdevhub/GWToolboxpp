#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include "SkillbarWidget.h"

/*
 * Based off of @JuliusPunhal April skill timer - https://github.com/JuliusPunhal/April-old/blob/master/Source/April/SkillbarOverlay.cpp
 */

void SkillbarWidget::skill_cooldown_to_string(char arr[16], uint32_t cd) const
{
    if (cd > 1800'000u || cd == 0) {
        arr[0] = 0;
    }
    else if (cd >= static_cast<uint32_t>(decimal_threshold)) {
        if (round_up) cd += 1000;
        snprintf(arr, 16, "%d", cd / 1000);
    }
    else {
        snprintf(arr, 16, "%.1f", cd / 1000.f);
    }
}

std::vector<SkillbarWidget::Effect> SkillbarWidget::get_effects(const GW::Constants::SkillID skillId)
{
    std::vector<Effect> ret;
    auto* effects = GW::Effects::GetPlayerEffects();
    if (!effects) return ret;
    const auto& skill_data = GW::SkillbarMgr::GetSkillConstantData(skillId);
    if (skill_data && skill_data->type == GW::Constants::SkillType::Hex) return ret;
    for (const GW::Effect& effect : *effects) {
        if (effect.skill_id == skillId) {
            Effect e;
            e.remaining = effect.GetTimeRemaining();
            if (effect.duration) {
                e.progress = (e.remaining / 1000.0f) / effect.duration;
            }
            else {
                e.progress = 1.f;
            }
            ret.emplace_back(e);
        }
    }
    return ret;
}

SkillbarWidget::Effect SkillbarWidget::get_longest_effect(const GW::Constants::SkillID skillId)
{
    SkillbarWidget::Effect ret;
    auto* effects = GW::Effects::GetPlayerEffects();
    if (!effects) return ret;
    const auto& skill_data = GW::SkillbarMgr::GetSkillConstantData(skillId);
    if (skill_data && skill_data->type == GW::Constants::SkillType::Hex) return ret;
    for (const GW::Effect& effect : *effects) {
        if (effect.skill_id == skillId) {
            const auto remaining = effect.GetTimeRemaining();
            if (ret.remaining < remaining) {
                ret.remaining = remaining;
                if (effect.duration) {
                    ret.progress = (ret.remaining / 1000.0f) / effect.duration;
                }
                else {
                    ret.progress = 1.f;
                }
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
        has_sf = skillbar->skills[i].skill_id == GW::Constants::SkillID::Shadow_Form;
    }

    for (size_t idx = 0u; idx < 8; idx++) {
        skill_cooldown_to_string(m_skills[idx].cooldown, skillbar->skills[idx].GetRecharge());
        if (!display_skill_overlay && !display_effect_monitor) continue;
        const GW::Constants::SkillID& skill_id = skillbar->skills[idx].skill_id;
        const Effect& effect = get_longest_effect(skill_id);
        m_skills[idx].color = UptimeToColor(effect.remaining);
        if (display_effect_monitor) {
            const auto* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (!skill_data) continue;
            m_skills[idx].effects.clear();
            if (display_multiple_effects && has_sf && skill_data->profession == static_cast<uint8_t>(GW::Constants::Profession::Assassin) && skill_data->type == GW::Constants::SkillType::Enchantment) {
                m_skills[idx].effects = get_effects(skill_id);
                std::ranges::sort(m_skills[idx].effects, [](const Effect& a, const Effect& b) { return a.remaining > b.remaining; });
            }
            else if (effect.remaining > 0) {
                m_skills[idx].effects.push_back(effect);
            }

            for (auto& e : m_skills[idx].effects) {
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
    const Color col_border = (window_flags & ImGuiWindowFlags_NoMove) ? color_border : Colors::White();

    ImGui::PushFont(GuiUtils::GetFont(font_recharge));

    GW::UI::WindowPosition* pos = GW::UI::GetWindowPosition(GW::UI::WindowID_Skillbar);
    ImVec2 skillsize(m_skill_width, m_skill_height);
    ImVec2 winsize;
    if (snap_to_skillbar && pos) {
        if (pos->state & 0x2) {
            // Default layout
            pos->state = 0x21;
            pos->p1 = {224.f, 56.f};
            pos->p2 = {224.f, 0.f};
        }
        const float uiscale = GuiUtils::GetGWScaleMultiplier();
        GW::Vec2f xAxis = pos->xAxis(uiscale);
        const GW::Vec2f yAxis = pos->yAxis(uiscale);
        float width = xAxis.y - xAxis.x;
        float height = yAxis.y - yAxis.x;
        if (width > height) {
            if (width / 5.f > height) {
                layout = Layout::Row;
            }
            else {
                layout = Layout::Rows;
            }
        }
        else {
            if (height / 5.f > width) {
                layout = Layout::Column;
            }
            else {
                layout = Layout::Columns;
            }
        }
        switch (layout) {
            case Layout::Columns: skillsize.x = width / 2.f; break;
            case Layout::Row: skillsize.x = width / 8.f; break;
            case Layout::Rows: skillsize.x = width / 4.f; break;
        }
        m_skill_width = m_skill_height = skillsize.y = skillsize.x;
        // NB: Skillbar is 1px off on x axis
        ImGui::SetNextWindowPos({xAxis.x, yAxis.x});
        winsize = {width, height};
    }
    else {
        winsize = skillsize;
        if (layout == Layout::Row) {
            winsize.x *= 8;
        }
        else if (layout == Layout::Rows) {
            winsize.x *= 4;
            winsize.y *= 2;
        }
        else if (layout == Layout::Column) {
            winsize.y *= 8;
        }
        else if (layout == Layout::Columns) {
            winsize.x *= 2;
            winsize.y *= 4;
        }
    }

    ImGui::SetNextWindowSize(winsize);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin(Name(), nullptr, window_flags);
    const ImVec2 winpos = ImGui::GetWindowPos();
    for (size_t i = 0; i < m_skills.size(); ++i) {
        const Skill& skill = m_skills[i];

        // position of this skill
        ImVec2 pos1(winpos.x, winpos.y);

        if (layout == Layout::Row) {
            pos1.x += (i * skillsize.x);
        }
        else if (layout == Layout::Rows) {
            pos1.x += (i % 4 * skillsize.x);
            pos1.y += (std::floor(i / 4.f) * skillsize.y);
        }
        else if (layout == Layout::Column) {
            pos1.y += (i * skillsize.y);
        }
        else if (layout == Layout::Columns) {
            pos1.x += (i % 2 * skillsize.x);
            pos1.y += (std::floor(i / 2.f) * skillsize.y);
        }

        ImVec2 pos2 = ImVec2(pos1.x + skillsize.x, pos1.y + skillsize.y);

        // draw overlay
        if (display_skill_overlay) {
            ImGui::GetBackgroundDrawList()->AddRectFilled(pos1, pos2, skill.color);
        }

        // border
        if (layout == Layout::Row) {
            if (i != 7) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos2.x, pos1.y), ImVec2(pos2.x, pos2.y), col_border);
            }
        }
        else if (layout == Layout::Rows) {
            if (i % 4 != 3) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos2.x, pos1.y), ImVec2(pos2.x, pos2.y), col_border);
            }
            if (std::floor(i / 4.f) == 0) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos2.y), ImVec2(pos2.x, pos2.y), col_border);
            }
        }
        else if (layout == Layout::Column) {
            if (i != 7) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos2.y), ImVec2(pos2.x, pos2.y), col_border);
            }
        }
        else if (layout == Layout::Columns) {
            if (i % 2 == 0) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos2.x, pos1.y), ImVec2(pos2.x, pos2.y), col_border);
            }
            if (i < 6) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos2.y), ImVec2(pos2.x, pos2.y), col_border);
            }
        }

        // label
        const ImVec2 label_size = ImGui::CalcTextSize(skill.cooldown);
        ImVec2 label_pos(pos1.x + skillsize.x / 2 - label_size.x / 2, pos1.y + skillsize.y / 2 - label_size.y / 2);
        ImGui::GetWindowDrawList()->AddText(label_pos, color_text_recharge, skill.cooldown);

        if (display_effect_monitor) {
            DrawEffect(i, pos1);
        }
    }

    ImGui::GetBackgroundDrawList()->AddRect(winpos, ImVec2(winpos.x + winsize.x, winpos.y + winsize.y), col_border);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}

void SkillbarWidget::DrawEffect(int skill_idx, const ImVec2& pos) const
{
    ImGui::PushFont(GuiUtils::GetFont(font_effects));

    const Skill& skill = m_skills[skill_idx];

    ImVec2 base = pos;

    if (layout == Layout::Row) {
        base.y += effect_monitor_offset;
    }
    else if (layout == Layout::Rows) {
        if (effects_symmetric && std::floor(skill_idx / 4) == 0) {
            base.y += m_skill_height;
            base.y -= effect_monitor_size == 0 ? ImGui::GetTextLineHeightWithSpacing() : effect_monitor_size;
            base.y += effect_monitor_offset;
        }
        else {
            base.y -= effect_monitor_offset;
        }
    }
    else if (layout == Layout::Column) {
        base.x += effect_monitor_offset;
    }
    else if (layout == Layout::Columns) {
        if (effects_symmetric && skill_idx % 2 == 0) {
            base.x += m_skill_width;
            base.x -= effect_monitor_size == 0 ? ImGui::GetTextLineHeightWithSpacing() : effect_monitor_size; // not really correct but works
            base.x += effect_monitor_offset;
        }
        else {
            base.x -= effect_monitor_offset;
        }
    }

    ImVec2 size;
    if (layout == Layout::Column || layout == Layout::Columns) {
        size.x = effect_monitor_size == 0 ? ImGui::GetTextLineHeightWithSpacing() : effect_monitor_size; // not really correct but works
        size.y = m_skill_height;
    }
    else {
        size.x = m_skill_width;
        size.y = effect_monitor_size == 0 ? ImGui::GetTextLineHeightWithSpacing() : effect_monitor_size;
    }

    for (size_t i = 0; i < skill.effects.size(); ++i) {
        const Effect& effect = skill.effects[i];

        ImVec2 pos1 = base;

        bool first_half = (layout == Layout::Rows && std::floor(skill_idx / 4) == 0) || (layout == Layout::Columns && skill_idx % 2 == 0);
        bool flip_order = effects_flip_order;

        bool shift_offset = (effects_symmetric && first_half) || effects_flip_direction;

        if (effects_symmetric && !first_half) {
            flip_order = !flip_order;
        }

        size_t index = flip_order ? i : skill.effects.size() - i - 1;

        if (layout == Layout::Row || layout == Layout::Rows) {
            pos1.y += size.y * index;
            if (shift_offset) {
                pos1.y -= size.y * (skill.effects.size() - 1);
            }
        }
        else if (layout == Layout::Column || layout == Layout::Columns) {
            pos1.x += size.x * index;
            if (shift_offset) {
                pos1.x -= size.x * (skill.effects.size() - 1);
            }
        }

        ImVec2 pos2(pos1.x + size.x, pos1.y + size.y); // base + size

        ImGui::GetBackgroundDrawList()->AddRectFilled(pos1, pos2, color_effect_background);

        if (effect.progress >= 0.0f && effect.progress <= 1.0f) {
            ImVec2 pos3, pos4;
            if (layout == Layout::Column || layout == Layout::Columns) {
                pos3 = ImVec2(pos1.x, pos2.y - size.y * effect.progress);
                pos4 = pos2;
            }
            else {
                pos3 = pos1;
                pos4 = ImVec2(pos1.x + size.x * effect.progress, pos2.y);
            }
            ImGui::GetBackgroundDrawList()->AddRectFilled(pos3, pos4, effect_progress_bar_color ? effect.color : color_effect_progress);
        }

        ImGui::GetBackgroundDrawList()->AddRect(pos1, pos2, color_effect_border);

        const ImVec2 label_size = ImGui::CalcTextSize(effect.text);
        const ImVec2 label_pos(pos1.x + size.x / 2 - label_size.x / 2, pos1.y + size.y / 2 - label_size.y / 2);
        ImGui::GetBackgroundDrawList()->AddText(label_pos, effect_text_color ? Colors::FullAlpha(effect.color) : color_text_effects, effect.text);
    }

    ImGui::PopFont();
}

void SkillbarWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    layout = static_cast<Layout>(ini->GetLongValue(Name(), VAR_NAME(layout), static_cast<long>(layout)));
    m_skill_height = static_cast<float>(ini->GetDoubleValue(Name(), "height", m_skill_height));
    m_skill_width = static_cast<float>(ini->GetDoubleValue(Name(), "width", m_skill_width));
    medium_treshold = ini->GetLongValue(Name(), VAR_NAME(medium_treshold), medium_treshold);
    short_treshold = ini->GetLongValue(Name(), VAR_NAME(short_treshold), short_treshold);
    color_long = Colors::Load(ini, Name(), VAR_NAME(color_long), color_long);
    color_medium = Colors::Load(ini, Name(), VAR_NAME(color_medium), color_medium);
    color_short = Colors::Load(ini, Name(), VAR_NAME(color_short), color_short);

    decimal_threshold = ini->GetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
    round_up = ini->GetBoolValue(Name(), VAR_NAME(round_up), round_up);

    display_skill_overlay = ini->GetBoolValue(Name(), VAR_NAME(display_skill_overlay), display_skill_overlay);
    font_recharge = static_cast<GuiUtils::FontSize>(ini->GetLongValue(Name(), VAR_NAME(font_recharge), static_cast<long>(font_recharge)));
    color_text_recharge = Colors::Load(ini, Name(), VAR_NAME(color_text_recharge), color_text_recharge);
    color_border = Colors::Load(ini, Name(), VAR_NAME(color_border), color_border);

    display_effect_monitor = ini->GetBoolValue(Name(), VAR_NAME(display_effect_monitor), display_effect_monitor);
    effect_monitor_size = ini->GetLongValue(Name(), VAR_NAME(effect_monitor_size), effect_monitor_size);
    effect_monitor_offset = ini->GetLongValue(Name(), VAR_NAME(effect_monitor_offset), effect_monitor_offset);
    effects_symmetric = ini->GetBoolValue(Name(), VAR_NAME(effects_symmetric), effects_symmetric);
    display_multiple_effects = ini->GetBoolValue(Name(), VAR_NAME(display_multiple_effects), display_multiple_effects);
    effects_flip_order = ini->GetBoolValue(Name(), VAR_NAME(effects_flip_order), effects_flip_order);
    effects_flip_direction = ini->GetBoolValue(Name(), VAR_NAME(effects_flip_direction), effects_flip_direction);
    effect_text_color = ini->GetBoolValue(Name(), VAR_NAME(effect_text_color), effect_text_color);
    effect_progress_bar_color = ini->GetBoolValue(Name(), VAR_NAME(effect_progress_bar_color), effect_progress_bar_color);
    font_effects = static_cast<GuiUtils::FontSize>(ini->GetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects)));
    color_text_effects = Colors::Load(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    color_effect_background = Colors::Load(ini, Name(), VAR_NAME(color_effect_background), color_effect_background);
    color_effect_progress = Colors::Load(ini, Name(), VAR_NAME(color_effect_progress), color_effect_progress);
    color_effect_border = Colors::Load(ini, Name(), VAR_NAME(color_effect_border), color_effect_border);
    snap_to_skillbar = ini->GetBoolValue(Name(), VAR_NAME(snap_to_skillbar), snap_to_skillbar);
    is_movable = is_resizable = !snap_to_skillbar;
}

void SkillbarWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    ini->SetLongValue(Name(), VAR_NAME(layout), static_cast<long>(layout));
    ini->SetDoubleValue(Name(), "height", m_skill_height);
    ini->SetDoubleValue(Name(), "width", m_skill_width);

    ini->SetLongValue(Name(), VAR_NAME(medium_treshold), medium_treshold);
    ini->SetLongValue(Name(), VAR_NAME(short_treshold), short_treshold);
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
    ini->SetLongValue(Name(), VAR_NAME(effect_monitor_size), effect_monitor_size);
    ini->SetLongValue(Name(), VAR_NAME(effect_monitor_offset), effect_monitor_offset);
    ini->SetBoolValue(Name(), VAR_NAME(effects_symmetric), effects_symmetric);
    ini->SetBoolValue(Name(), VAR_NAME(display_multiple_effects), display_multiple_effects);
    ini->SetBoolValue(Name(), VAR_NAME(effects_flip_order), effects_flip_order);
    ini->SetBoolValue(Name(), VAR_NAME(effects_flip_direction), effects_flip_direction);
    ini->SetBoolValue(Name(), VAR_NAME(effect_text_color), effect_text_color);
    ini->SetBoolValue(Name(), VAR_NAME(effect_progress_bar_color), effect_progress_bar_color);
    ini->SetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects));
    Colors::Save(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_background), color_effect_background);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_progress), color_effect_progress);
    Colors::Save(ini, Name(), VAR_NAME(color_effect_border), color_effect_border);
    ini->SetBoolValue(Name(), VAR_NAME(snap_to_skillbar), snap_to_skillbar);
}

void SkillbarWidget::DrawDurationThresholds()
{
    ImGui::Indent();
    ImGui::Text("Skill duration thresholds");
    const float width = 150.f * ImGui::GetIO().FontGlobalScale;
    ImGui::PushID("long");
    ImGui::Text("Long: ");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("Color", &color_long);
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::PushID("medium");
    ImGui::Text("Medium: ");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("Color", &color_medium);
    ImGui::NewLine();
    ImGui::SameLine(width);
    ImGui::DragInt("Threshold", &medium_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the medium color is used.");
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::PushID("short");
    ImGui::Text("Short: ");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("Color", &color_short);
    ImGui::NewLine();
    ImGui::SameLine(width);
    ImGui::DragInt("Threshold", &short_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the short color is used.");
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::Unindent();
}

void SkillbarWidget::DrawSettingInternal()
{
    if (ImGui::Checkbox("Attach to skill bar", &snap_to_skillbar)) {
        is_movable = is_resizable = !snap_to_skillbar;
    }
    ToolboxWidget::DrawSettingInternal();
    ImGui::ShowHelp("Skill overlay will match your skillbar position and orientation");
    if (snap_to_skillbar) {
        // TODO: Offsets, rely on user values for now
    }
    else {
        static const char* items[] = {"Row", "2 Rows", "Column", "2 Columns"};
        ImGui::Combo("Layout", reinterpret_cast<int*>(&layout), items, 4);
        float size[2]{m_skill_width, m_skill_height};
        if (ImGui::DragFloat2("Skill size", size, 0.1f, 1, 100)) {
            m_skill_width = size[0];
            m_skill_height = size[1];
        }
    }

    static constexpr const char* font_sizes[] = {"16", "18", "20", "24", "42", "48"};

    const bool is_vertical = (layout == Layout::Column || layout == Layout::Columns);

    ImGui::Separator();
    ImGui::Text("Skill overlay settings");
    ImGui::Spacing();
    ImGui::Indent();
    ImGui::PushID("skill_overlay_settings");
    ImGui::Combo("Text size", reinterpret_cast<int*>(&font_recharge), font_sizes, 6);
    Colors::DrawSettingHueWheel("Text color", &color_text_recharge);
    Colors::DrawSettingHueWheel("Border color", &color_border);
    ImGui::Checkbox("Paint skills according to effect duration", &display_skill_overlay);
    ImGui::ShowHelp("Change the color of the skill dependent on the long/medium/short duration colors");
    if (display_skill_overlay) {
        DrawDurationThresholds();
    }
    ImGui::InputInt("Text decimal threshold", &decimal_threshold);
    ImGui::ShowHelp("When should decimal numbers start to show (in milliseconds)");
    ImGui::Checkbox("Round up integers", &round_up);
    ImGui::PopID();
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Effect monitor settings");
    ImGui::Spacing();
    ImGui::Indent();
    ImGui::PushID("effect_monitor_settings");
    ImGui::Checkbox("Display effect monitor", &display_effect_monitor);
    if (display_effect_monitor) {
        ImGui::DragInt(is_vertical ? "Effect width" : "Effect height", &effect_monitor_size, 1, 0);
        ImGui::ShowHelp(is_vertical ? "Width in pixels of a single effect on the effect monitor.\n0 matches font size." : "Height in pixels of a single effect on the effect monitor.\n0 matches font size.");
        ImGui::DragInt("Offset", &effect_monitor_offset, 1, -200, 200);
        ImGui::ShowHelp(is_vertical ? "Distance to the left or right of an effect relative to the related skill on your skillbar" : "Distance above or below of an effect relative to the related skill on your skillbar");
        if (layout == Layout::Columns) {
            ImGui::Checkbox("Show effects either side of your skillbar", &effects_symmetric);
        }
        else if (layout == Layout::Rows) {
            ImGui::Checkbox("Show effects above and below your skillbar", &effects_symmetric);
        }
        ImGui::Checkbox("Display multiple effects", &display_multiple_effects);
        ImGui::ShowHelp("Show stacking effects for casted enchantments e.g. Shroud of Distress with Shadow Form");
        if (display_multiple_effects) {
            ImGui::Indent();
            ImGui::Checkbox("Flip effects order", &effects_flip_order);
            ImGui::ShowHelp("Newest effect is displayed last instead of first");
            ImGui::Unindent();
            ImGui::Spacing();
        }

        ImGui::Checkbox("Color text according to effect duration", &effect_text_color);
        ImGui::ShowHelp("Change the color of the font dependent on the long/medium/short duration colors");
        ImGui::Checkbox("Paint effects according to effect duration", &effect_progress_bar_color);
        ImGui::ShowHelp("Change the color of the effect progress bar dependent on the long/medium/short duration colors");
        if (effect_text_color || effect_progress_bar_color) {
            DrawDurationThresholds();
        }
        ImGui::Combo("Text size", reinterpret_cast<int*>(&font_effects), font_sizes, 6);
        if (!effect_text_color) Colors::DrawSettingHueWheel("Text color", &color_text_effects);
        Colors::DrawSettingHueWheel("Background color", &color_effect_background);
        if (!effect_progress_bar_color) Colors::DrawSettingHueWheel("Progress bar color", &color_effect_progress);
        Colors::DrawSettingHueWheel("Border color", &color_effect_border);
    }
    ImGui::PopID();
    ImGui::Unindent();
}

Color SkillbarWidget::UptimeToColor(const uint32_t uptime) const
{
    if (uptime > static_cast<uint32_t>(medium_treshold)) {
        return color_long;
    }

    if (uptime > static_cast<uint32_t>(short_treshold)) {
        const auto diff = static_cast<float>(medium_treshold - short_treshold);
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
