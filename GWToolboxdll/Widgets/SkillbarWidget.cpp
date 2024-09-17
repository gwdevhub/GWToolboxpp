#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hooker.h>

#include <Defines.h>
#include <ImGuiAddons.h>
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

    // duration -> color settings
    int medium_treshold = 5000; // long to medium color
    int short_treshold = 2500;  // medium to short color
    Color color_long = Colors::ARGB(50, 0, 255, 0);
    Color color_medium = Colors::ARGB(50, 255, 255, 0);
    Color color_short = Colors::ARGB(80, 255, 0, 0);

    // duration -> string settings
    int decimal_threshold = 600; // when to start displaying decimals
    bool round_up = true;        // round up or down?

    // Skill overlay settings
    bool display_skill_overlay = true;
    FontLoader::FontSize font_recharge = FontLoader::FontSize::header1;
    Color color_text_recharge = Colors::White();
    Color color_border = Colors::ARGB(100, 255, 255, 255);

    // Effect monitor settings
    bool display_effect_monitor = false;
    int effect_monitor_size = 0;
    int effect_monitor_offset = -100;
    bool effects_symmetric = true;
    bool display_multiple_effects = false;
    bool effects_flip_order = false;
    bool effects_flip_direction = false;
    bool effect_text_color = false;
    bool effect_progress_bar_color = false;
    FontLoader::FontSize font_effects = FontLoader::FontSize::text;
    Color color_text_effects = Colors::White();
    Color color_effect_background = Colors::ARGB(100, 0, 0, 0);
    Color color_effect_border = Colors::ARGB(255, 0, 0, 0);
    Color color_effect_progress = Colors::Blue();


    GW::UI::Frame* skillbar_frame = nullptr;
    bool skillbar_position_dirty = true;
    GW::UI::UIInteractionCallback OnSkillbar_UICallback_Ret = nullptr;
    void __cdecl OnSkillbar_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        OnSkillbar_UICallback_Ret(message, wParam, lParam);
        switch (static_cast<uint32_t>(message->message_id)) {
        case 0xb:
            skillbar_frame = nullptr;
            skillbar_position_dirty = true;
            break;
        case 0x13:
        case 0x30:
        case 0x33:
            skillbar_position_dirty = true; // Forces a recalculation
            break;
        }
        GW::Hook::LeaveHook();
    }

    GW::UI::Frame* GetSkillbarFrame() {
        if (skillbar_frame)
            return skillbar_frame;
        skillbar_frame = GW::UI::GetFrameByLabel(L"Skillbar");
        if (skillbar_frame) {
            ASSERT(skillbar_frame->frame_callbacks.size());
            if (skillbar_frame->frame_callbacks[0] != OnSkillbar_UICallback) {
                OnSkillbar_UICallback_Ret = skillbar_frame->frame_callbacks[0];
                skillbar_frame->frame_callbacks[0] = OnSkillbar_UICallback;
            }
        }
        return skillbar_frame;
    }

    bool GetSkillbarPos() {
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
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        skillbar_position_dirty = true;
    }

    ToolboxUIElement& Instance() {
        return SkillbarWidget::Instance();
    }
}
void SkillbarWidget::skill_cooldown_to_string(char arr[16], uint32_t cd) const
{
    if (cd > 1800'000u || cd == 0) {
        arr[0] = 0;
    }
    else if (cd >= static_cast<uint32_t>(decimal_threshold)) {
        if (round_up) {
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
        if (!display_skill_overlay && !display_effect_monitor) {
            continue;
        }
        const GW::Constants::SkillID& skill_id = skillbar->skills[i].skill_id;
        const Effect& effect = get_longest_effect(skill_id);
        m_skills[i].color = UptimeToColor(effect.remaining);
        if (display_effect_monitor) {
            const auto* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (!skill_data) {
                continue;
            }
            m_skills[i].effects.clear();
            if (display_multiple_effects && has_sf && skill_data->profession == GW::Constants::ProfessionByte::Assassin && skill_data->type == GW::Constants::SkillType::Enchantment) {
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

    ImGui::PushFont(FontLoader::GetFont(font_recharge));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin(Name(), nullptr, GetWinFlags());
    const auto draw_list = ImGui::GetBackgroundDrawList();
    for (size_t i = 0; i < m_skills.size(); i++) {
        const Skill& skill = m_skills[i];
        // NB: Y axis inverted for imgui
        const ImVec2& top_left = skill_positions_calculated[i];
        const ImVec2& bottom_right = { skill_positions_calculated[i].x + m_skill_width, skill_positions_calculated[i].y + m_skill_height };

        // draw overlay
        if (display_skill_overlay) {
            draw_list->AddRectFilled(top_left, bottom_right, skill.color);
        }
        draw_list->AddRect(top_left, bottom_right, color_border);

        // label
        if (*skill.cooldown) {
            const ImVec2 label_size = ImGui::CalcTextSize(skill.cooldown);
            ImVec2 label_pos(top_left.x + m_skill_width / 2 - label_size.x / 2, top_left.y + m_skill_width / 2 - label_size.y / 2);
            draw_list->AddText(label_pos, color_text_recharge, skill.cooldown);
        }

        if (display_effect_monitor) {
            DrawEffect(i, top_left);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopFont();
}

void SkillbarWidget::DrawEffect(const int skill_idx, const ImVec2& pos) const
{
    ImGui::PushFont(FontLoader::GetFont(font_effects));

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

    for (size_t i = 0; i < skill.effects.size(); i++) {
        const Effect& effect = skill.effects[i];

        ImVec2 pos1 = base;

        const bool first_half = (layout == Layout::Rows && std::floor(skill_idx / 4) == 0) || (layout == Layout::Columns && skill_idx % 2 == 0);
        bool flip_order = effects_flip_order;

        const bool shift_offset = (effects_symmetric && first_half) || effects_flip_direction;

        if (effects_symmetric && !first_half) {
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
    LOAD_UINT(medium_treshold);
    LOAD_UINT(short_treshold);
    LOAD_COLOR(color_long);
    LOAD_COLOR(color_medium);
    LOAD_COLOR(color_short);

    LOAD_UINT(decimal_threshold);
    LOAD_BOOL(round_up);

    LOAD_BOOL(display_skill_overlay);
    font_recharge = static_cast<FontLoader::FontSize>(ini->GetLongValue(Name(), VAR_NAME(font_recharge), static_cast<long>(font_recharge)));
    LOAD_COLOR(color_text_recharge);
    LOAD_COLOR(color_border);

    LOAD_BOOL(display_effect_monitor);
    LOAD_UINT(effect_monitor_size);
    LOAD_UINT(effect_monitor_offset);
    LOAD_BOOL(effects_symmetric);
    LOAD_BOOL(display_multiple_effects);
    LOAD_BOOL(effects_flip_order);
    LOAD_BOOL(effects_flip_direction);
    LOAD_BOOL(effect_text_color);
    LOAD_BOOL(effect_progress_bar_color);
    font_effects = static_cast<FontLoader::FontSize>(ini->GetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects)));
    LOAD_COLOR(color_text_effects);
    LOAD_COLOR(color_effect_background);
    LOAD_COLOR(color_effect_progress);
    LOAD_COLOR(color_effect_border);
}

void SkillbarWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);

    SAVE_UINT(medium_treshold);
    SAVE_UINT(short_treshold);
    SAVE_COLOR(color_long);
    SAVE_COLOR(color_medium);
    SAVE_COLOR(color_short);

    SAVE_UINT(decimal_threshold);
    SAVE_BOOL(round_up);

    SAVE_BOOL(display_skill_overlay);
    ini->SetLongValue(Name(), VAR_NAME(font_recharge), static_cast<long>(font_recharge));
    SAVE_COLOR(color_text_recharge);
    SAVE_COLOR(color_border);

    SAVE_BOOL(display_effect_monitor);
    SAVE_UINT(effect_monitor_size);
    SAVE_UINT(effect_monitor_offset);
    SAVE_BOOL(effects_symmetric);
    SAVE_BOOL(display_multiple_effects);
    SAVE_BOOL(effects_flip_order);
    SAVE_BOOL(effects_flip_direction);
    SAVE_BOOL(effect_text_color);
    SAVE_BOOL(effect_progress_bar_color);
    ini->SetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects));
    SAVE_COLOR(color_text_effects);
    SAVE_COLOR(color_effect_background);
    SAVE_COLOR(color_effect_progress);
    SAVE_COLOR(color_effect_border);
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

void SkillbarWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    constexpr const char* font_sizes[] = {"16", "18", "20", "24", "42", "48"};

    const bool is_vertical = layout == Layout::Column || layout == Layout::Columns;

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
        if (!effect_text_color) {
            Colors::DrawSettingHueWheel("Text color", &color_text_effects);
        }
        Colors::DrawSettingHueWheel("Background color", &color_effect_background);
        if (!effect_progress_bar_color) {
            Colors::DrawSettingHueWheel("Progress bar color", &color_effect_progress);
        }
        Colors::DrawSettingHueWheel("Border color", &color_effect_border);
    }
    ImGui::PopID();
    ImGui::Unindent();
}

void SkillbarWidget::Initialize()
{
    ToolboxWidget::Initialize();
    GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kUIPositionChanged, OnUIMessage, 0x8000);
    GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kPreferenceValueChanged, OnUIMessage, 0x8000);

    ChatCommands::RegisterSettingChatCommand(L"skillbar_effects_overlay",&display_effect_monitor,L"Toggles the effect monitor overlay on the skillbar widget");
    ChatCommands::RegisterSettingChatCommand(L"skillbar_skills_overlay", &display_skill_overlay, L"Toggles the skill monitor overlay on the skillbar widget");
}
void SkillbarWidget::Terminate()
{
    ToolboxWidget::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);

    if (skillbar_frame && skillbar_frame->frame_callbacks[0] == OnSkillbar_UICallback) {
        skillbar_frame->frame_callbacks[0] = OnSkillbar_UICallback_Ret;
    }
    ChatCommands::RemoveSettingChatCommand(L"skillbar_effects_overlay");
    ChatCommands::RemoveSettingChatCommand(L"skillbar_skills_overlay");
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
