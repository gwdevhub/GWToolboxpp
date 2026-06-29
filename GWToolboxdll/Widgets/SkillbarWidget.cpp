#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hooker.h>

#include <Color.h>
#include <Defines.h>
#include <ImGuiAddons.h>
#include <Utils/FontLoader.h>
#include "SkillbarWidget.h"
#include <Modules/ChatCommands.h>

/*
 * Based off of @JuliusPunhal April skill timer - https://github.com/JuliusPunhal/April-old/blob/master/Source/April/SkillbarOverlay.cpp
 */
namespace {
    GW::UI::FramePosition skillbar_skill_positions[8];
    ImVec2 skill_positions_calculated[8];

    // Overall settings
    enum class Layout {
        Row,
        Rows,
        Column,
        Columns
    };

    Layout layout = Layout::Row;
    float m_skill_width = 50.f;
    float m_skill_height = 50.f;

    SkillbarWidget::Settings settings;

    GW::UI::Frame* skillbar_frame = nullptr;
    bool skillbar_position_dirty = true;
    GW::UI::UIInteractionCallback OnSkillbar_UICallback_Ret = 0, OnSkillbar_UICallback_Func = 0;

    void __cdecl OnSkillbar_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        OnSkillbar_UICallback_Ret(message, wParam, lParam);
        switch (message->message_id) {
            case GW::UI::UIMessage::kDestroyFrame:
                skillbar_frame = nullptr;
                skillbar_position_dirty = true;
                break;
            case GW::UI::UIMessage::kFrameMessage_0x13:
            case GW::UI::UIMessage::kRenderFrame_0x30:
            case GW::UI::UIMessage::kSetLayout:
                skillbar_position_dirty = true; // Forces a recalculation
                break;
        }
        GW::Hook::LeaveHook();
    }

    GW::UI::Frame* GetSkillbarFrame()
    {
        if (skillbar_frame)
            return skillbar_frame;
        skillbar_frame = GW::UI::GetFrameByLabel(L"Skillbar");
        if (!skillbar_frame) skillbar_frame = GW::UI::GetFrameByLabel(L"MobileActionCluster");
        if (skillbar_frame) {
            ASSERT(skillbar_frame->frame_callbacks.size());
            if (!OnSkillbar_UICallback_Func) {
                OnSkillbar_UICallback_Func = skillbar_frame->frame_callbacks[0].callback;
                GW::Hook::CreateHook((void**)&OnSkillbar_UICallback_Func, OnSkillbar_UICallback, reinterpret_cast<void**>(&OnSkillbar_UICallback_Ret));
                GW::Hook::EnableHooks(OnSkillbar_UICallback_Func);
            }
        }
        return skillbar_frame;
    }

    bool GetSkillbarPos()
    {
        if (!skillbar_position_dirty)
            return true;
        const auto frame = GetSkillbarFrame();
        if (!(frame && frame->IsVisible() && frame->IsCreated())) {
            return false;
        }
        if (!GImGui)
            return false;
        // Imgui viewport may not be limited to the game area.
        const auto imgui_viewport = ImGui::GetMainViewport();

        for (size_t i = 0; i < _countof(skillbar_skill_positions); i++) {
            const auto skillframe = GW::UI::GetChildFrame(frame, i);
            if (!skillframe)
                return false;
            skillbar_skill_positions[i] = skillframe->position;
            skill_positions_calculated[i] = skillbar_skill_positions[i].GetTopLeftOnScreen();
            skill_positions_calculated[i].y += imgui_viewport->Pos.y;
            skill_positions_calculated[i].x += imgui_viewport->Pos.x;
            if (i == 0) {
                m_skill_width = skillbar_skill_positions[0].GetSizeOnScreen().x;
                m_skill_height = skillbar_skill_positions[0].GetSizeOnScreen().y;
            }
        }

        // Calculate columns/rows
        if (skillbar_skill_positions[0].screen_top == skillbar_skill_positions[7].screen_top) {
            layout = Layout::Row;
        }
        else if (skillbar_skill_positions[0].screen_left == skillbar_skill_positions[7].screen_left) {
            layout = Layout::Column;
        }
        else if (skillbar_skill_positions[0].screen_top == skillbar_skill_positions[3].screen_top) {
            layout = Layout::Rows;
        }
        else {
            layout = Layout::Columns;
        }
        skillbar_position_dirty = false;
        return true;
    }

    GW::HookEntry OnUIMessage_HookEntry;

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        skillbar_frame = nullptr;
        skillbar_position_dirty = true;
        
    }

    ToolboxUIElement& Instance()
    {
        return SkillbarWidget::Instance();
    }
}

void SkillbarWidget::skill_cooldown_to_string(char arr[16], uint32_t cd) const
{
    if (cd > 1800'000u || cd == 0) {
        arr[0] = 0;
    }
    else if (cd >= static_cast<uint32_t>(settings.decimal_threshold)) {
        if (settings.round_up) {
            cd += 1000;
        }
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
    if (!effects) {
        return ret;
    }
    const auto& skill_data = GW::SkillbarMgr::GetSkillConstantData(skillId);
    if (skill_data && skill_data->type == GW::Constants::SkillType::Hex) {
        return ret;
    }
    for (const GW::Effect& effect : *effects) {
        if (effect.skill_id == skillId) {
            Effect e;
            e.remaining = effect.GetTimeRemaining();
            if (effect.duration) {
                e.progress = e.remaining / 1000.0f / effect.duration;
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
    Effect ret;
    auto* effects = GW::Effects::GetPlayerEffects();
    if (!effects) {
        return ret;
    }
    const auto& skill_data = GW::SkillbarMgr::GetSkillConstantData(skillId);
    if (skill_data && skill_data->type == GW::Constants::SkillType::Hex) {
        return ret;
    }
    for (const GW::Effect& effect : *effects) {
        if (effect.skill_id == skillId) {
            const auto remaining = effect.GetTimeRemaining();
            if (ret.remaining < remaining) {
                ret.remaining = remaining;
                if (effect.duration) {
                    ret.progress = ret.remaining / 1000.0f / effect.duration;
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
    if (!visible) {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }

    const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (skillbar == nullptr) {
        return;
    }

    bool has_sf = false;
    for (size_t i = 0; i < 8 && !has_sf; i++) {
        has_sf = skillbar->skills[i].skill_id == GW::Constants::SkillID::Shadow_Form;
    }

    for (auto i = 0; i < _countof(skillbar->skills); i++) {
        skill_cooldown_to_string(m_skills[i].cooldown, skillbar->skills[i].GetRecharge());
        if (!settings.display_skill_overlay && !settings.display_effect_monitor) {
            continue;
        }
        const GW::Constants::SkillID& skill_id = skillbar->skills[i].skill_id;
        const Effect& effect = get_longest_effect(skill_id);
        m_skills[i].color = UptimeToColor(effect.remaining);
        if (settings.display_effect_monitor) {
            const auto* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (!skill_data) {
                continue;
            }
            m_skills[i].effects.clear();
            if (settings.display_multiple_effects && has_sf && skill_data->profession == GW::Constants::ProfessionByte::Assassin && skill_data->type == GW::Constants::SkillType::Enchantment) {
                m_skills[i].effects = get_effects(skill_id);
                std::ranges::sort(m_skills[i].effects, [](const Effect& a, const Effect& b) { return a.remaining > b.remaining; });
            }
            else if (effect.remaining > 0) {
                m_skills[i].effects.push_back(effect);
            }

            for (auto& e : m_skills[i].effects) {
                skill_cooldown_to_string(e.text, e.remaining);
                e.color = UptimeToColor(e.remaining);
            }
        }
    }
}

void SkillbarWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }
    if (skillbar_position_dirty && !GetSkillbarPos()) {
        return; // Failed to get skillbar pos
    }

    const auto font_size = ImMin(settings.font_recharge, m_skill_width);
    
    DummyWindow();

    const auto draw_list = ImGui::GetBackgroundDrawList();
    for (size_t i = 0; i < m_skills.size(); i++) {
        const Skill& skill = m_skills[i];
        // NB: Y axis inverted for imgui
        const ImVec2& top_left = skill_positions_calculated[i];
        const ImVec2 bottom_right = {skill_positions_calculated[i].x + m_skill_width, skill_positions_calculated[i].y + m_skill_height};

        // draw overlay
        if (settings.display_skill_overlay) {
            draw_list->AddRectFilled(top_left, bottom_right, skill.color);
        }
        draw_list->AddRect(top_left, bottom_right, settings.color_border);

        // label
        if (*skill.cooldown) {
            ImGui::PushFont(NULL, draw_list, font_size);
            const ImVec2 label_size = ImGui::CalcTextSize(skill.cooldown);
            ImVec2 label_pos(top_left.x + m_skill_width / 2 - label_size.x / 2, top_left.y + m_skill_width / 2 - label_size.y / 2);
            if (IM_COL32_A_MASK & settings.color_text_outline) {
                ImGui::DrawTextWithOutline(draw_list, skill.cooldown, label_pos, settings.color_text_recharge, settings.color_text_outline);
            }
            else {
                draw_list->AddText(label_pos, settings.color_text_recharge, skill.cooldown);
            }
            ImGui::PopFont(draw_list);
        }

        if (settings.display_effect_monitor) {
            DrawEffect(i, top_left);
        }
    }
    ImGui::End();
}

void SkillbarWidget::DrawEffect(const int skill_idx, const ImVec2& pos) const
{
    const auto draw_list = ImGui::GetBackgroundDrawList();
    ImGui::PushFont(NULL, draw_list, settings.font_effects);

    const auto widget_height = ImMax(settings.font_effects, settings.effect_monitor_size);

    const Skill& skill = m_skills[skill_idx];

    ImVec2 base = pos;

    if (layout == Layout::Row) {
        base.y += settings.effect_monitor_offset;
    }
    else if (layout == Layout::Rows) {
        if (settings.effects_symmetric && std::floor(skill_idx / 4) == 0) {
            base.y += m_skill_height;
            base.y -= widget_height;
            base.y += settings.effect_monitor_offset;
        }
        else {
            base.y -= settings.effect_monitor_offset;
        }
    }
    else if (layout == Layout::Column) {
        base.x += settings.effect_monitor_offset;
    }
    else if (layout == Layout::Columns) {
        if (settings.effects_symmetric && skill_idx % 2 == 0) {
            base.x += m_skill_width;
            base.x -= widget_height; // not really correct but works
            base.x += settings.effect_monitor_offset;
        }
        else {
            base.x -= settings.effect_monitor_offset;
        }
    }

    ImVec2 size;
    if (layout == Layout::Column || layout == Layout::Columns) {
        size.x = widget_height; // not really correct but works
        size.y = m_skill_height;
    }
    else {
        size.x = m_skill_width;
        size.y = widget_height;
    }

    for (size_t i = 0; i < skill.effects.size(); i++) {
        const Effect& effect = skill.effects[i];

        ImVec2 pos1 = base;

        const bool first_half = (layout == Layout::Rows && std::floor(skill_idx / 4) == 0) || (layout == Layout::Columns && skill_idx % 2 == 0);
        bool flip_order = settings.effects_flip_order;

        const bool shift_offset = (settings.effects_symmetric && first_half) || settings.effects_flip_direction;

        if (settings.effects_symmetric && !first_half) {
            flip_order = !flip_order;
        }

        const size_t index = flip_order ? i : skill.effects.size() - i - 1;

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

        draw_list->AddRectFilled(pos1, pos2, settings.color_effect_background);

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
            draw_list->AddRectFilled(pos3, pos4, settings.effect_progress_bar_color ? effect.color : settings.color_effect_progress.value);
        }

        draw_list->AddRect(pos1, pos2, settings.color_effect_border);

        const ImVec2 label_size = ImGui::CalcTextSize(effect.text);
        const ImVec2 label_pos(pos1.x + size.x / 2 - label_size.x / 2, pos1.y + size.y / 2 - label_size.y / 2);
        draw_list->AddText(label_pos, settings.effect_text_color ? Colors::FullAlpha(effect.color) : settings.color_text_effects.value, effect.text);
    }
    ImGui::PopFont(draw_list);
}

void SkillbarWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    settings.font_recharge = std::clamp(settings.font_recharge, FontLoader::text_size_min, FontLoader::text_size_max);
    settings.font_effects = std::clamp(settings.font_effects, FontLoader::text_size_min, FontLoader::text_size_max);
}

void SkillbarWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void SkillbarWidget::DrawDurationThresholds()
{
    ImGui::Indent();
    ImGui::Text("Skill duration thresholds");
    const float width = 150.f * ImGui::FontScale();
    ImGui::PushID("long");
    ImGui::Text("Long: ");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("Color", &settings.color_long.value);
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::PushID("medium");
    ImGui::Text("Medium: ");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("Color", &settings.color_medium.value);
    ImGui::NewLine();
    ImGui::SameLine(width);
    ImGui::DragInt("Threshold", &settings.medium_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the medium color is used.");
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::PushID("short");
    ImGui::Text("Short: ");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("Color", &settings.color_short.value);
    ImGui::NewLine();
    ImGui::SameLine(width);
    ImGui::DragInt("Threshold", &settings.short_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the short color is used.");
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::Unindent();
}

void SkillbarWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();


    const bool is_vertical = layout == Layout::Column || layout == Layout::Columns;

    ImGui::Separator();
    ImGui::Text("Skill overlay settings");
    ImGui::Spacing();
    ImGui::Indent();
    ImGui::PushID("skill_overlay_settings");
    ImGui::DragFloat("Text size", &settings.font_recharge, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
    Colors::DrawSettingHueWheel("Text color", &settings.color_text_recharge.value);
    Colors::DrawSettingHueWheel("Text outline color", &settings.color_text_outline.value);
    Colors::DrawSettingHueWheel("Border color", &settings.color_border.value);
    ImGui::CheckboxWithHelp("Paint skills according to effect duration", &settings.display_skill_overlay, "Change the color of the skill dependent on the long/medium/short duration colors");
    if (settings.display_skill_overlay) {
        DrawDurationThresholds();
    }
    ImGui::InputInt("Text decimal threshold", &settings.decimal_threshold);
    ImGui::ShowHelp("When should decimal numbers start to show (in milliseconds)");
    ImGui::Checkbox("Round up integers", &settings.round_up);
    ImGui::PopID();
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Effect monitor settings");
    ImGui::Spacing();
    ImGui::Indent();
    ImGui::PushID("effect_monitor_settings");
    ImGui::Checkbox("Display effect monitor", &settings.display_effect_monitor);
    if (settings.display_effect_monitor) {
        ImGui::DragFloat(is_vertical ? "Effect width" : "Effect height", &settings.effect_monitor_size, 1.f, FontLoader::text_size_min, FontLoader::text_size_max,"%.f");
        ImGui::ShowHelp(is_vertical ? "Width in pixels of a single effect on the effect monitor.\n0 matches font size." : "Height in pixels of a single effect on the effect monitor.\n0 matches font size.");
        ImGui::DragInt("Offset", &settings.effect_monitor_offset, 1, -200, 200);
        ImGui::ShowHelp(is_vertical ? "Distance to the left or right of an effect relative to the related skill on your skillbar" : "Distance above or below of an effect relative to the related skill on your skillbar");
        if (layout == Layout::Columns) {
            ImGui::Checkbox("Show effects either side of your skillbar", &settings.effects_symmetric);
        }
        else if (layout == Layout::Rows) {
            ImGui::Checkbox("Show effects above and below your skillbar", &settings.effects_symmetric);
        }
        ImGui::CheckboxWithHelp("Display multiple effects", &settings.display_multiple_effects, "Show stacking effects for casted enchantments e.g. Shroud of Distress with Shadow Form");
        if (settings.display_multiple_effects) {
            ImGui::Indent();
            ImGui::CheckboxWithHelp("Flip effects order", &settings.effects_flip_order, "Newest effect is displayed last instead of first");
            ImGui::Unindent();
            ImGui::Spacing();
        }

        ImGui::CheckboxWithHelp("Color text according to effect duration", &settings.effect_text_color, "Change the color of the font dependent on the long/medium/short duration colors");
        ImGui::CheckboxWithHelp("Paint effects according to effect duration", &settings.effect_progress_bar_color, "Change the color of the effect progress bar dependent on the long/medium/short duration colors");
        if (settings.effect_text_color || settings.effect_progress_bar_color) {
            DrawDurationThresholds();
        }
        ImGui::DragFloat("Text size", &settings.font_effects, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
        if (!settings.effect_text_color) {
            Colors::DrawSettingHueWheel("Text color", &settings.color_text_effects.value);
        }
        Colors::DrawSettingHueWheel("Background color", &settings.color_effect_background.value);
        if (!settings.effect_progress_bar_color) {
            Colors::DrawSettingHueWheel("Progress bar color", &settings.color_effect_progress.value);
        }
        Colors::DrawSettingHueWheel("Border color", &settings.color_effect_border.value);
    }
    ImGui::PopID();
    ImGui::Unindent();
}

void SkillbarWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kUIPositionChanged, OnUIMessage, 0x8000);
    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kPreferenceValueChanged, OnUIMessage, 0x8000);
}

void SkillbarWidget::Terminate()
{
    ToolboxWidget::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);

    if (OnSkillbar_UICallback_Func) {
        GW::Hook::RemoveHook(OnSkillbar_UICallback_Func);
        OnSkillbar_UICallback_Func = nullptr;
    }
}

Color SkillbarWidget::UptimeToColor(const uint32_t uptime) const
{
    if (uptime > static_cast<uint32_t>(settings.medium_treshold)) {
        return settings.color_long;
    }

    if (uptime > static_cast<uint32_t>(settings.short_treshold)) {
        const auto diff = static_cast<float>(settings.medium_treshold - settings.short_treshold);
        const float fraction = 1.f - (settings.medium_treshold - uptime) / diff;
        int colold[4], colnew[4], colout[4];
        Colors::ConvertU32ToInt4(settings.color_long, colold);
        Colors::ConvertU32ToInt4(settings.color_medium, colnew);
        for (size_t i = 0; i < 4; i++) {
            colout[i] = static_cast<int>((1.f - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    if (uptime > 0) {
        const float fraction = uptime / static_cast<float>(settings.short_treshold);
        int colold[4], colnew[4], colout[4];
        Colors::ConvertU32ToInt4(settings.color_medium, colold);
        Colors::ConvertU32ToInt4(settings.color_short, colnew);
        for (auto i = 0; i < 4; i++) {
            colout[i] = static_cast<int>((1.f - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    return 0x00000000;
}
