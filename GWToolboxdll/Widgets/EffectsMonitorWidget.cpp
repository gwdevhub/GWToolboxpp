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
#include "EffectsMonitorWidget.h"


namespace {


    GuiUtils::FontSize font_effects = GuiUtils::FontSize::header2;
    Color color_text_effects = Colors::White();
    Color color_background = Colors::ARGB(128, 0, 0, 0);
    Color color_text_shadow = Colors::Black();

    // duration -> string settings
    int decimal_threshold = 600; // when to start displaying decimals
    int only_under_seconds = 3600; // hide effect durations over n seconds
    bool round_up = true;        // round up or down?
    bool show_vanquish_counter = true;
    // Adds or removes the minion count "effect" depending on percent
    uint32_t minion_count = 0;
    uint32_t morale_percent = 100;
    // Overall settings
    enum class Layout
    {
        Rows,
        Columns
    };
    Layout layout = Layout::Rows;
    constexpr float default_skill_width = 52.f;
    // Runtime params
    float m_skill_width = 52.f;
    int row_count = 1;
    int skills_per_row = 99;
    ImVec2 window_pos = { 0.f, 0.f };
    ImVec2 window_size = { 0.f, 0.f };
    float x_translate = 1.f; // Multiplier for traversing effects on the x axis
    float y_translate = -1.f; // Multiplier for traversing effects on the y axis
    bool hard_mode = false;
    ImVec2 imgui_pos = { 0.f, 0.f };
    ImVec2 imgui_size = { 0.f, 0.f };
    bool map_ready = false;
    bool initialised = false;

    // Emulated effects in order of addition
    std::map<uint32_t, std::vector<GW::Effect>> cached_effects;

    struct MoraleChangeUIMessage {
        uint32_t agent_id;
        uint32_t percent;
    };

    constexpr GW::UI::UIMessage hook_messages[] = {
        GW::UI::UIMessage::kMinionCountUpdated,
        GW::UI::UIMessage::kEffectAdd,
        GW::UI::UIMessage::kEffectRenew,
        GW::UI::UIMessage::kMoraleChange,
        GW::UI::UIMessage::kEffectRemove,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kPreferenceChanged,
        GW::UI::UIMessage::kUIPositionChanged
    };
    GW::HookEntry OnEffect_Entry;

    uint32_t GetMinionCount() {
        const auto w = GW::GetWorldContext();
        if (!w) return 0;
        const auto& minions_arr = w->controlled_minion_count;
        const auto me = GW::Agents::GetPlayerId();
        const auto mine = std::ranges::find_if(minions_arr, [me](const auto& cur) {
            return cur.agent_id == me;
        });
        return mine != minions_arr.end() ? mine->minion_count : 0;
    }
    uint32_t GetMorale() {
        const auto w = GW::GetWorldContext();
        return w ? w->morale : 100;
    }
    uint32_t GetMorale(uint32_t agent_id) {
        const auto w = GW::GetWorldContext();
        if (!(w && w->party_morale_related.size()))
            return 100;
        for (const auto& m : w->party_morale_related) {
            if (m.party_member_info->agent_id == agent_id)
                return m.party_member_info->morale;
        }
        return 100;
    }

    // Get matching effect from gwtoolbox overlay
    const GW::Effect* GetEffect(uint32_t effect_id) {
        const GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects)
            return nullptr;
        for (const GW::Effect& effect : *effects) {
            if (effect.effect_id == effect_id)
                return &effect;
        }
        return nullptr;
    }
    // Get matching effect from gwtoolbox overlay
    const GW::Effect* GetLongestEffectBySkillId(GW::Constants::SkillID skill_id) {
        const GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects)
            return nullptr;
        const GW::Effect* found = nullptr;
        for (const GW::Effect& effect : *effects) {
            if (effect.skill_id == skill_id && (!found || effect.GetTimeRemaining() > found->GetTimeRemaining())) {
                found = &effect;
            }
        }
        return found;
    }
    // Find the drawing order of the skill based on the gw effect monitor
    uint32_t GetEffectSortOrder(GW::Constants::SkillID skill_id) {
        switch (skill_id) {
        case GW::Constants::SkillID::Hard_mode:
            return 0;
        case GW::Constants::SkillID::No_Skill:
            return 1; // Morale boost from ui message 0x10000047
        }
        const GW::Skill* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        // Lifted from GmEffect::ActivateEffect(), removed assertion and instead whack everything else into 0xd
        switch (skill->type) {
        case GW::Constants::SkillType::Stance:
            return 9;
        case GW::Constants::SkillType::Hex:
            return 5;
        case GW::Constants::SkillType::Spell:
            return 7;
        case GW::Constants::SkillType::Enchantment:
            return 0xc;
        case GW::Constants::SkillType::Condition:
            return 4;
        case GW::Constants::SkillType::Skill:
            return 8;
        case GW::Constants::SkillType::Glyph:
            return 0xb;
        case GW::Constants::SkillType::Preparation:
            return 10;
        case GW::Constants::SkillType::Ritual:
            return 6;
        default:
            return 0xd;
        }
    }
    // Recalculate position of widget based on gw effect monitor position
    void RefreshPosition() {
        GW::UI::WindowPosition* pos = GW::UI::GetWindowPosition(GW::UI::WindowID_EffectsMonitor);
        if (!pos)
            return;
        if (pos->state & 0x2) {
            // Default layout
            pos->state = 0xd;
            pos->p1 = { 0.f,0.f };
            pos->p2 = { 312.f, 104.f };
        }
        const float uiscale = GuiUtils::GetGWScaleMultiplier();
        const GW::Vec2f xAxis = pos->xAxis(uiscale);
        const GW::Vec2f yAxis = pos->yAxis(uiscale);
        window_size = { std::roundf(xAxis.y - xAxis.x), std::roundf(yAxis.y - yAxis.x) };
        window_pos = { std::roundf(xAxis.x),std::roundf(yAxis.x) };
        layout = window_size.y > window_size.x ? Layout::Columns : Layout::Rows;
        m_skill_width = std::roundf(std::min<float>(default_skill_width * uiscale, std::min<float>(window_size.x, window_size.y)));
        if (layout == Layout::Rows) {
            row_count = static_cast<int>(std::round(window_size.y / m_skill_width));
            skills_per_row = static_cast<int>(std::round(window_size.x / m_skill_width));
        }
        else {
            row_count = static_cast<int>(std::round(window_size.x / m_skill_width));
            skills_per_row = static_cast<int>(std::round(window_size.y / m_skill_width));
        }

        const GW::Vec2f mid_point(window_pos.x + window_size.x / 2.f, window_pos.y + window_size.y / 2.f);
        const GW::Vec2f screen_size = { static_cast<float>(GW::Render::GetViewportWidth()) , static_cast<float>(GW::Render::GetViewportHeight()) };
        imgui_pos = window_pos;
        imgui_size = { screen_size.x - window_pos.x, screen_size.y - window_pos.y };
        x_translate = 1.f;
        if (mid_point.x > screen_size.x / 2.f) {
            // Right aligned effects
            x_translate = -1.f;
            imgui_pos.x = 0.f;
            imgui_size.x = window_pos.x + window_size.x;
        }

        y_translate = 1.f;
        if (mid_point.y > screen_size.y / 2.f) {
            // Bottom aligned effects
            y_translate = -1.f;
            imgui_pos.y = 0.f;
            imgui_size.y = window_pos.y + window_size.y;
        }
    }
    int UptimeToString(char arr[8], int cd)
    {
        cd = std::abs(cd);
        if (cd > only_under_seconds * 1000) {
            return 0;
        }
        if (cd >= decimal_threshold) {
            if (round_up) cd += 1000;
            return snprintf(arr, 8, "%d", cd / 1000);
        }
        return snprintf(arr, 8, "%.1f", static_cast<double>(cd) / 1000.0);
    }
    // Find index of active effect from gwtoolbox overlay
    size_t GetEffectIndex(const std::vector<GW::Effect>& arr, GW::Constants::SkillID skill_id) {
        for (size_t i = 0; i < arr.size(); i++) {
            if (arr[i].skill_id == skill_id)
                return i;
        }
        return 0xFF;
    }
    // Forcefully removes then re-adds the current effects; used for initialising
    bool RefreshEffects() {
        GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects)
            return false;
        std::vector<GW::Effect> readd_effects;
        for (GW::Effect& effect : *effects) {
            readd_effects.push_back(effect);
        }
        struct Packet : GW::Packet::StoC::PacketBase {
            uint32_t agent_id;
            uint32_t skill_id;
            uint32_t attribute_level;
            uint32_t effect_id;
            float duration;
        } add{};
        add.header = GAME_SMSG_EFFECT_APPLIED;
        struct Packet2 : GW::Packet::StoC::PacketBase {
            uint32_t agent_id;
            uint32_t effect_id;
        } remove{};
        remove.header = GAME_SMSG_EFFECT_REMOVED;
        add.agent_id = remove.agent_id = GW::Agents::GetPlayerId();

        for (GW::Effect& effect : readd_effects) {
            remove.effect_id = effect.effect_id;
            GW::StoC::EmulatePacket(&remove);
            add.skill_id = static_cast<uint32_t>(effect.skill_id);
            add.effect_id = effect.effect_id;
            add.duration = effect.duration;
            add.attribute_level = effect.attribute_level;
            GW::StoC::EmulatePacket(&add);
        }
        minion_count = GetMinionCount();
        morale_percent = GetMorale();
        return true;
    }
    // Remove effect from gwtoolbox overlay. Will only remove if the game has also removed it, otherwise false.
    bool RemoveEffect(uint32_t effect_id) {
        if (GetEffect(effect_id))
            return false; // Game hasn't removed the effect yet.
        for (auto& by_type : cached_effects) {
            auto& effects = by_type.second;
            for (size_t i = 0; i < effects.size(); i++) {
                if (effects[i].effect_id != effect_id)
                    continue;
                const GW::Effect* existing = GetLongestEffectBySkillId(effects[i].skill_id);
                if (existing) {
                    memcpy(&effects[i], existing, sizeof(*existing));
                    return false; // Game hasn't removed the effect yet (copy the effect over)
                }
                by_type.second.erase(effects.begin() + i);
                if (effects.empty())
                    cached_effects.erase(by_type.first);
                return true;
            }
        }
        return false;
    }
    // Triggered when an effect has reached < 0 duration. Returns true if effect has been removed.
    bool DurationExpired(GW::Effect& effect) {
        const auto timer = GW::MemoryMgr::GetSkillTimer();
        switch (effect.skill_id) {
        case GW::Constants::SkillID::Aspect_of_Exhaustion:
        case GW::Constants::SkillID::Aspect_of_Depletion_energy_loss:
        case GW::Constants::SkillID::Scorpion_Aspect:
            effect.duration = 30.f;
            effect.timestamp = timer;
            return false;
        default: break;
        }
        if (effect.duration == 0.f)
            return true;
        // Effect expired
        return RemoveEffect(effect.effect_id);

    }
    // Update effect on the gwtoolbox overlay
    bool SetEffect(const GW::Effect* effect) {
        const uint32_t type = GetEffectSortOrder(effect->skill_id);
        if (!cached_effects.contains(type))
            cached_effects[type] = std::vector<GW::Effect>();

        // Player can stand in range of more than 1 spirit; use the longest effect duration for the effect monitor
        if (effect->duration > 0) {
            effect = GetLongestEffectBySkillId(effect->skill_id);
        }
        if (!effect) {
            return false;
        }

        const size_t current = GetEffectIndex(cached_effects[type], effect->skill_id);
        if (current != 0xFF) {
            cached_effects[type].erase(cached_effects[type].begin() + current);
        }
        cached_effects[type].push_back(*effect);
        // Trigger durations for aspects etc
        if (!effect->duration)
            DurationExpired(cached_effects[type].back());
        hard_mode |= effect->skill_id == GW::Constants::SkillID::Hard_mode;
        return true;
    }
    // Static handler for GW UI Message events. Updates ongoing effects and refreshes UI position.
    void OnEffectUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*) {

        switch (message_id) {
        case GW::UI::UIMessage::kMinionCountUpdated: { // Minion count updated on effects monitor
            minion_count = GetMinionCount();
        } break;
        case GW::UI::UIMessage::kMoraleChange: { // Morale boost/DP change
            const struct Packet {
                uint32_t agent_id;
                uint32_t percent;
            } *packet = static_cast<Packet*>(wParam);
            if (packet->agent_id == GW::Agents::GetPlayerId())
                morale_percent = packet->percent;
        } break;
        case GW::UI::UIMessage::kEffectAdd: {
            const struct Payload {
                uint32_t agent_id;
                GW::Effect* e;
            } *details = static_cast<Payload*>(wParam);
            const uint32_t agent_id = GW::Agents::GetPlayerId();
            if (agent_id && agent_id != details->agent_id)
                break;
            SetEffect(details->e);
        } break;
        case GW::UI::UIMessage::kEffectRenew: {
            const GW::Effect* e = GetEffect(*static_cast<uint32_t*>(wParam));
            if (e)
                SetEffect(e);
        } break;

        case GW::UI::UIMessage::kEffectRemove: {
            RemoveEffect(reinterpret_cast<uint32_t>(wParam));
        } break;
        case GW::UI::UIMessage::kMapChange: {
            cached_effects.clear();
            hard_mode = false; // will be reapplied in OnEffect callback
            minion_count = 0;
        } break;
        case GW::UI::UIMessage::kMapLoaded: { // not ready yet at kMapChange
            morale_percent = GetMorale();
        } break;
        case GW::UI::UIMessage::kPreferenceChanged: // Refresh preference e.g. window X/Y position
        case GW::UI::UIMessage::kUIPositionChanged: // Refresh GW UI element position
            RefreshPosition();
            break;
        default: break;
        }
    }
}

void EffectsMonitorWidget::Initialize()
{
    ToolboxWidget::Initialize();

    for (const auto& message_id : hook_messages) {
        GW::UI::RegisterUIMessageCallback(&OnEffect_Entry, message_id, OnEffectUIMessage, 0x8000);
    }

    GW::GameThread::Enqueue([] {
        RefreshEffects();
        });
    RefreshPosition();
}

void EffectsMonitorWidget::Terminate()
{
    ToolboxWidget::Terminate();
    for (size_t i = 0; i < _countof(hook_messages); i++) {
        GW::UI::RemoveUIMessageCallback(&OnEffect_Entry);
    }
    cached_effects.clear();
}

void EffectsMonitorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) return;

    const auto window_flags = GetWinFlags(ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::SetNextWindowSize(imgui_size);
    ImGui::SetNextWindowPos(imgui_pos);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f,0.0f));

    ImGui::Begin(Name(), nullptr, window_flags);

    ImVec2 skill_top_left({ imgui_pos.x, imgui_pos.y });
    if (y_translate < 0) {
        skill_top_left.y = imgui_pos.y + imgui_size.y - m_skill_width;
    }
    if (x_translate < 0) {
        skill_top_left.x = imgui_pos.x + imgui_size.x - m_skill_width;
    }
    char remaining_str[16];
    ImGui::PushFont(GuiUtils::GetFont(font_effects));

    int row_skills_drawn = 0;
    constexpr int row_idx = 1;
    int draw = 0;
    const auto next_effect = [&](std::string_view str = "") {
        row_skills_drawn++;
        if (!str.empty()) {
            const ImVec2 label_size = ImGui::CalcTextSize(str.data());
            const ImVec2 label_pos(skill_top_left.x + m_skill_width - label_size.x - 1.f, skill_top_left.y + m_skill_width - label_size.y - 1.f);
            ImGui::GetWindowDrawList()->AddText({label_pos.x + 1, label_pos.y + 1}, 0xFFFF0000, str.data());
        }
        if (layout == Layout::Rows) {
            skill_top_left.x += (m_skill_width * x_translate);
            if (row_skills_drawn == skills_per_row && row_idx < row_count) {
                skill_top_left.y += (m_skill_width * y_translate);
                skill_top_left.x = x_translate > 0 ? imgui_pos.x : imgui_pos.x + imgui_size.x - m_skill_width;
                row_skills_drawn = 0;
            }

        }
        else {
            skill_top_left.y += (m_skill_width * y_translate);
            if (row_skills_drawn == skills_per_row && row_idx < row_count) {
                skill_top_left.x += (m_skill_width * x_translate);
                skill_top_left.y = y_translate > 0 ? imgui_pos.y : imgui_pos.y + imgui_size.y - m_skill_width;
                row_skills_drawn = 0;
            }
        }
    };
    bool skipped_effects = false;
    const auto skip_effects = [&] {
        if (skipped_effects) return;
        if (morale_percent != 100) next_effect();
        if (minion_count) next_effect();
        skipped_effects = true;
    };

    if (!hard_mode) { // if hard mode, only skip effects after hard mode icon
        skip_effects();
    }

    for (auto& effects : cached_effects | std::views::values) {
        for (auto& effect : effects) {
            if (effect.duration > 0) {
                const auto remaining = effect.GetTimeRemaining();
                draw = remaining < static_cast<DWORD>(effect.duration * 1000.f);
                if (draw) {
                    draw = UptimeToString(remaining_str, static_cast<int>(remaining));
                }
                else if (DurationExpired(effect)) {
                    goto enddraw; // cached_effects is now invalidated; skip to end and redraw next frame
                }
            }
            else if (effect.skill_id == GW::Constants::SkillID::Hard_mode) {
                if (show_vanquish_counter) {
                    const auto left = GW::Map::GetFoesToKill();
                    const auto killed = GW::Map::GetFoesKilled();
                    draw = left ? snprintf(remaining_str, 16, "%d/%d", killed, killed + left) : 0;
                }
            }
            else {
                draw = 0;
            }
            if (draw > 0) {
                const ImVec2 label_size = ImGui::CalcTextSize(remaining_str);
                const ImVec2 label_pos(skill_top_left.x + m_skill_width - label_size.x - 1.f, skill_top_left.y + m_skill_width - label_size.y - 1.f);
                if ((color_background & IM_COL32_A_MASK) != 0)
                    ImGui::GetWindowDrawList()->AddRectFilled({ skill_top_left.x + m_skill_width - label_size.x - 2.f, skill_top_left.y + m_skill_width - label_size.y }, { skill_top_left.x + m_skill_width, skill_top_left.y + m_skill_width }, color_background);
                if ((color_text_shadow & IM_COL32_A_MASK) != 0)
                    ImGui::GetWindowDrawList()->AddText({ label_pos.x + 1, label_pos.y + 1 }, color_text_shadow, remaining_str);
                ImGui::GetWindowDrawList()->AddText(label_pos, color_text_effects, remaining_str);
            }
            next_effect();
            skip_effects();

        }
    }
    enddraw:
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void EffectsMonitorWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    decimal_threshold = ini->GetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
    only_under_seconds = ini->GetLongValue(Name(), VAR_NAME(only_under_seconds), only_under_seconds);
    round_up = ini->GetBoolValue(Name(), VAR_NAME(round_up), round_up);
    show_vanquish_counter = ini->GetBoolValue(Name(), VAR_NAME(show_vanquish_counter), show_vanquish_counter);
    font_effects = static_cast<GuiUtils::FontSize>(
        ini->GetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects)));
    color_text_effects = Colors::Load(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    color_text_shadow = Colors::Load(ini, Name(), VAR_NAME(color_text_shadow), color_text_shadow);
    color_background = Colors::Load(ini, Name(), VAR_NAME(color_background), color_background);
}

void EffectsMonitorWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);


    ini->SetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
    ini->SetLongValue(Name(), VAR_NAME(only_under_seconds), only_under_seconds);
    ini->SetBoolValue(Name(), VAR_NAME(round_up), round_up);
    ini->SetBoolValue(Name(), VAR_NAME(show_vanquish_counter), show_vanquish_counter);

    ini->SetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects));
    Colors::Save(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    Colors::Save(ini, Name(), VAR_NAME(color_text_shadow), color_text_shadow);
    Colors::Save(ini, Name(), VAR_NAME(color_background), color_background);
}

void EffectsMonitorWidget::DrawSettingInternal()
{
    ToolboxWidget::DrawSettingInternal();

    constexpr const char* font_sizes[] = { "16", "18", "20", "24", "42", "48" };

    ImGui::PushID("effects_monitor_overlay_settings");
    ImGui::Combo("Text size", reinterpret_cast<int*>(&font_effects), font_sizes, 6);
    Colors::DrawSettingHueWheel("Text color", &color_text_effects);
    Colors::DrawSettingHueWheel("Text shadow", &color_text_shadow);
    Colors::DrawSettingHueWheel("Effect duration background", &color_background);
    ImGui::Text("Don't show effect durations longer than");
    ImGui::SameLine();
    ImGui::PushItemWidth(64.f * ImGui::FontScale());
    ImGui::InputInt("###only_under_seconds", &only_under_seconds,0);
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
