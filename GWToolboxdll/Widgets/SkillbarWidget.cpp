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

namespace {
    GW::UI::FramePosition skillbar_skill_positions[8];
    ImVec2 skill_positions_calculated[8];

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
                skillbar_position_dirty = true; // 强制重新计算
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
        // ImGui 视口可能不限于游戏区域
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

    const bool want_effects = settings.display_skill_overlay || settings.display_effect_monitor;

    std::array<const GW::Skill*, 8> skill_data{};
    std::array<Effect, 8> longest{};
    std::array<std::vector<Effect>, 8> stacked;

    if (want_effects) {
        for (auto i = 0; i < _countof(skillbar->skills); i++) {
            skill_data[i] = GW::SkillbarMgr::GetSkillConstantData(skillbar->skills[i].skill_id);
        }
    }
    if (const auto* effects = want_effects ? GW::Effects::GetPlayerEffects() : nullptr) {
        for (const GW::Effect& effect : *effects) {
            for (auto i = 0; i < _countof(skillbar->skills); i++) {
                if (effect.skill_id != skillbar->skills[i].skill_id)
                    continue;
                const auto* sd = skill_data[i];
                if (sd && sd->type == GW::Constants::SkillType::Hex)
                    continue;
                const auto remaining = effect.GetTimeRemaining();
                Effect& slot_longest = longest[i];
                if (slot_longest.remaining < remaining) {
                    slot_longest.remaining = remaining;
                    slot_longest.progress = effect.duration ? slot_longest.remaining / 1000.0f / effect.duration : 1.f;
                }
                if (settings.display_multiple_effects && has_sf && sd
                    && sd->profession == GW::Constants::ProfessionByte::Assassin && sd->type == GW::Constants::SkillType::Enchantment) {
                    Effect e;
                    e.remaining = remaining;
                    e.progress = effect.duration ? remaining / 1000.0f / effect.duration : 1.f;
                    stacked[i].push_back(e);
                }
            }
        }
    }

    for (auto i = 0; i < _countof(skillbar->skills); i++) {
        skill_cooldown_to_string(m_skills[i].cooldown, skillbar->skills[i].GetRecharge());
        if (!want_effects) {
            continue;
        }
        const Effect& effect = longest[i];
        m_skills[i].color = UptimeToColor(effect.remaining);
        if (settings.display_effect_monitor) {
            m_skills[i].effects.clear();
            const auto* sd = skill_data[i];
            if (sd && settings.display_multiple_effects && has_sf
                && sd->profession == GW::Constants::ProfessionByte::Assassin && sd->type == GW::Constants::SkillType::Enchantment) {
                m_skills[i].effects = std::move(stacked[i]);
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
        return; // 获取技能栏位置失败
    }

    const auto font_size = ImMin(settings.font_recharge, m_skill_width);

    DummyWindow();

    const auto draw_list = ImGui::GetBackgroundDrawList();
    for (size_t i = 0; i < m_skills.size(); i++) {
        const Skill& skill = m_skills[i];
        // 注意：ImGui 的 Y 轴是反转的
        const ImVec2& top_left = skill_positions_calculated[i];
        const ImVec2 bottom_right = {skill_positions_calculated[i].x + m_skill_width, skill_positions_calculated[i].y + m_skill_height};

        if (settings.display_skill_overlay) {
            draw_list->AddRectFilled(top_left, bottom_right, skill.color);
        }
        draw_list->AddRect(top_left, bottom_right, settings.color_border);

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
            base.x -= widget_height; // 不完全准确但能工作
            base.x += settings.effect_monitor_offset;
        }
        else {
            base.x -= settings.effect_monitor_offset;
        }
    }

    ImVec2 size;
    if (layout == Layout::Column || layout == Layout::Columns) {
        size.x = widget_height; // 不完全准确但能工作
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
    ImGui::Text("技能持续时间阈值");
    const float width = 150.f * ImGui::FontScale();
    ImGui::PushID("long");
    ImGui::Text("长：");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("颜色", &settings.color_long.value);
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::PushID("medium");
    ImGui::Text("中：");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("颜色", &settings.color_medium.value);
    ImGui::NewLine();
    ImGui::SameLine(width);
    ImGui::DragInt("阈值", &settings.medium_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("效果剩余时间的毫秒数阈值，达到该值后使用中等颜色。");
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::PushID("short");
    ImGui::Text("短：");
    ImGui::SameLine(width);
    Colors::DrawSettingHueWheel("颜色", &settings.color_short.value);
    ImGui::NewLine();
    ImGui::SameLine(width);
    ImGui::DragInt("阈值", &settings.short_treshold, 1.f, 1, 180000);
    ImGui::ShowHelp("效果剩余时间的毫秒数阈值，达到该值后使用短颜色。");
    ImGui::PopID();
    ImGui::Spacing();

    ImGui::Unindent();
}

void SkillbarWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();


    const bool is_vertical = layout == Layout::Column || layout == Layout::Columns;

    ImGui::Separator();
    ImGui::Text("技能覆盖层设置");
    ImGui::Spacing();
    ImGui::Indent();
    ImGui::PushID("skill_overlay_settings");
    ImGui::DragFloat("文字大小", &settings.font_recharge, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
    Colors::DrawSettingHueWheel("文字颜色", &settings.color_text_recharge.value);
    Colors::DrawSettingHueWheel("文字描边颜色", &settings.color_text_outline.value);
    Colors::DrawSettingHueWheel("边框颜色", &settings.color_border.value);
    ImGui::CheckboxWithHelp("根据效果持续时间给技能着色", &settings.display_skill_overlay, "根据长/中/短持续时间颜色改变技能的颜色");
    if (settings.display_skill_overlay) {
        DrawDurationThresholds();
    }
    ImGui::InputInt("小数阈值（毫秒）", &settings.decimal_threshold);
    ImGui::ShowHelp("何时开始显示小数（单位：毫秒）");
    ImGui::Checkbox("整数向上取整", &settings.round_up);
    ImGui::PopID();
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("效果监控设置");
    ImGui::Spacing();
    ImGui::Indent();
    ImGui::PushID("effect_monitor_settings");
    ImGui::Checkbox("显示效果监控", &settings.display_effect_monitor);
    if (settings.display_effect_monitor) {
        ImGui::DragFloat(is_vertical ? "效果宽度" : "效果高度", &settings.effect_monitor_size, 1.f, FontLoader::text_size_min, FontLoader::text_size_max,"%.f");
        ImGui::ShowHelp(is_vertical ? "效果监控中单个效果的宽度（像素）。\n0 匹配字体大小。" : "效果监控中单个效果的高度（像素）。\n0 匹配字体大小。");
        ImGui::DragInt("偏移量", &settings.effect_monitor_offset, 1, -200, 200);
        ImGui::ShowHelp(is_vertical ? "效果相对于技能栏上对应技能的左右距离" : "效果相对于技能栏上对应技能的上下距离");
        if (layout == Layout::Columns) {
            ImGui::Checkbox("在技能栏两侧显示效果", &settings.effects_symmetric);
        }
        else if (layout == Layout::Rows) {
            ImGui::Checkbox("在技能栏上下方显示效果", &settings.effects_symmetric);
        }
        ImGui::CheckboxWithHelp("显示多个效果", &settings.display_multiple_effects, "显示已施放增益的叠加效果，例如暗影形态下的痛苦之幕");
        if (settings.display_multiple_effects) {
            ImGui::Indent();
            ImGui::CheckboxWithHelp("翻转效果顺序", &settings.effects_flip_order, "新效果显示在最后而非最前");
            ImGui::Unindent();
            ImGui::Spacing();
        }

        ImGui::CheckboxWithHelp("根据效果持续时间给文字着色", &settings.effect_text_color, "根据长/中/短持续时间颜色改变字体颜色");
        ImGui::CheckboxWithHelp("根据效果持续时间给进度条着色", &settings.effect_progress_bar_color, "根据长/中/短持续时间颜色改变效果进度条颜色");
        if (settings.effect_text_color || settings.effect_progress_bar_color) {
            DrawDurationThresholds();
        }
        ImGui::DragFloat("文字大小", &settings.font_effects, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
        if (!settings.effect_text_color) {
            Colors::DrawSettingHueWheel("文字颜色", &settings.color_text_effects.value);
        }
        Colors::DrawSettingHueWheel("背景颜色", &settings.color_effect_background.value);
        if (!settings.effect_progress_bar_color) {
            Colors::DrawSettingHueWheel("进度条颜色", &settings.color_effect_progress.value);
        }
        Colors::DrawSettingHueWheel("边框颜色", &settings.color_effect_border.value);
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
