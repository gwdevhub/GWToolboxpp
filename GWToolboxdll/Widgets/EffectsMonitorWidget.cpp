#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/GameContext.h>
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

#include "EffectsMonitorWidget.h"

#include "Timer.h"

void EffectsMonitorWidget::RefreshEffects() {
    GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
    if (!effects)
        return;
    std::vector<GW::Effect> readd_effects;
    for (GW::Effect& effect : *effects) {
        readd_effects.push_back(effect);
    }
    struct Packet : GW::Packet::StoC::PacketBase {
        uint32_t agent_id;
        uint32_t skill_id;
        uint32_t effect_type;
        uint32_t effect_id;
        float duration;
    } add;
    add.header = GAME_SMSG_EFFECT_APPLIED;
    struct Packet2 : GW::Packet::StoC::PacketBase {
        uint32_t agent_id;
        uint32_t effect_id;
    } remove;
    remove.header = GAME_SMSG_EFFECT_REMOVED;
    add.agent_id = remove.agent_id = GW::Agents::GetPlayerId();

    for (GW::Effect& effect : readd_effects) {
        remove.effect_id = effect.effect_id;
        GW::StoC::EmulatePacket(&remove);
        add.skill_id = effect.skill_id;
        add.effect_id = effect.effect_id;
        add.duration = effect.duration;
        add.effect_type = effect.effect_type;
        GW::StoC::EmulatePacket(&add);
    }
    Instance().SetMoralePercent(GW::GameContext::instance()->world->morale);
}
void EffectsMonitorWidget::CheckSetMinionCount() {
    auto& minions_arr = GW::GameContext::instance()->world->controlled_minion_count;
    uint32_t me = GW::Agents::GetPlayerId();
    minion_count = 0;
    for (size_t i = minions_arr.size() - 1; i < minions_arr.size(); i--) {
        if (minions_arr[i].agent_id == me) {
            minion_count = minions_arr[i].minion_count;
            return;
        }
    }
}
void EffectsMonitorWidget::OnEffectUIMessage(GW::HookStatus*, uint32_t message_id, void* wParam, void* lParam) {
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(message_id);
    
    switch (message_id) {
    case 0x10000046: { // Minion count updated on effects monitor
        Instance().CheckSetMinionCount();
    } break;
    case GW::UI::kEffectAdd: {
        struct Payload {
            uint32_t agent_id;
            GW::Effect* e;
        } *details = (Payload*)wParam;
        uint32_t player_id = GW::Agents::GetPlayerId();
        if (player_id && player_id != details->agent_id)
            break;
        Instance().SetEffect(details->e);
    }break;
    case GW::UI::kEffectRenew: {
        const GW::Effect* e = Instance().GetEffect(*(uint32_t*)wParam);
        if (e)
            Instance().SetEffect(e);
    }break;
    case GW::UI::kMoraleChange: { // Morale boost/DP change
        struct Payload {
            uint32_t agent_id;
            uint32_t percent;
        } *details = (Payload*)wParam;
        uint32_t player_id = GW::Agents::GetPlayerId();
        if (player_id && player_id != details->agent_id)
            break;
        Instance().SetMoralePercent(details->percent);
    } break;
    case GW::UI::kEffectRemove: {// Remove effect
        Instance().RemoveEffect((uint32_t)wParam);
    }break;
    case GW::UI::kMapChange: { // Map change
        Instance().cached_effects.clear();
        Instance().hard_mode = false;
    } break;
    case GW::UI::kPreferenceChanged: // Refresh preference e.g. window X/Y position
    case GW::UI::kUIPositionChanged: // Refresh GW UI element position
        Instance().RefreshPosition();
        break;
    }
}
void EffectsMonitorWidget::SetMoralePercent(uint32_t _morale_percent) {
    // The in-game effect monitor does something stupid to "inject" the morale boost because its not really a skill
    // We're going to spoof it as a skill with effect id 0 to put it in the right place.
    morale_percent = _morale_percent;
}
bool EffectsMonitorWidget::RemoveEffect(uint32_t effect_id) {
    if (GetEffect(effect_id))
        return false; // Game hasn't removed the effect yet.
    for (auto& by_type : cached_effects) {
        auto& effects = by_type.second;
        for (size_t i = 0; i < effects.size();i++) {
            if (effects[i].effect_id != effect_id)
                continue;
            const GW::Effect* existing = GetLongestEffectBySkillId(effects[i].skill_id);
            if (existing) {
                memcpy(&effects[i], existing, sizeof(*existing));
                return false; // Game hasn't removed the effect yet (copy the effect over)
            }
            by_type.second.erase(effects.begin() + i);
            if (effects.size() == 0)
                cached_effects.erase(by_type.first);
            return true;
        }
    }
    return false;
}
const GW::Effect* EffectsMonitorWidget::GetEffect(uint32_t effect_id) {
    const GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
    if (!effects)
        return nullptr;
    for (const GW::Effect& effect : *effects) {
        if (effect.effect_id == effect_id)
            return &effect;
    }
    return nullptr;
}
const GW::Effect* EffectsMonitorWidget::GetLongestEffectBySkillId(uint32_t skill_id) {
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
uint32_t EffectsMonitorWidget::GetEffectSortOrder(uint32_t skill_id) {
    switch (static_cast<GW::Constants::SkillID>(skill_id)) {
    case GW::Constants::SkillID::Hard_mode:
        return 0;
    case GW::Constants::SkillID::No_Skill:
        return 1; // Morale boost from ui message 0x10000047
    }
    const GW::Skill* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    // Lifted from GmEffect::ActivateEffect(), removed assertion and instead whack everything else into 0xd
    switch (skill->type) {
    case 3:
        return 9;
    case 4:
        return 5;
    case 5:
        return 7;
    case 6:
        return 0xc;
    case 8:
        return 4;
    case 10:
        return 8;
    case 0xc:
        return 0xb;
    case 0x13:
        return 10;
    case 0x16:
        return 6;
    }
    return 0xd;
}
void EffectsMonitorWidget::SetEffect(const GW::Effect* effect) {
    uint32_t type = GetEffectSortOrder(effect->skill_id);
    if (cached_effects.find(type) == cached_effects.end())
        cached_effects[type] = std::vector<GW::Effect>();

    // Player can stand in range of more than 1 spirit; use the longest effect duration for the effect monitor
    if (effect->duration) {
        effect = GetLongestEffectBySkillId(effect->skill_id);
    }

    size_t current = GetEffectIndex(cached_effects[type], effect->skill_id);
    if (current != 0xFF) {
        cached_effects[type].erase(cached_effects[type].begin() + current);
        current = 0xFF;
    }
    cached_effects[type].push_back(*effect);
    // Trigger durations for aspects etc
    if (!effect->duration)
        DurationExpired(cached_effects[type].back());
    hard_mode |= effect->skill_id == (uint32_t)GW::Constants::SkillID::Hard_mode;
}
bool EffectsMonitorWidget::DurationExpired(GW::Effect& effect) {
    uint32_t timer = GW::MemoryMgr::GetSkillTimer();
    switch (static_cast<GW::Constants::SkillID>(effect.skill_id)) {
        case GW::Constants::SkillID::Aspect_of_Exhaustion:
        case GW::Constants::SkillID::Aspect_of_Depletion_energy_loss:
        case GW::Constants::SkillID::Scorpion_Aspect:
            effect.duration = 30.f;
            effect.timestamp = timer;
            return false;
    }
    if (!effect.duration)
        return &effect;
    // Effect expired
    return RemoveEffect(effect.effect_id);
    
}
size_t EffectsMonitorWidget::GetEffectIndex(const std::vector<GW::Effect>& arr, uint32_t skill_id) {
    for (size_t i = 0; i < arr.size(); i++) {
        if (arr[i].skill_id == skill_id)
            return i;
    }
    return 0xFF;
}
void EffectsMonitorWidget::RefreshPosition() {
    GW::UI::WindowPosition* pos = GW::UI::GetWindowPosition(GW::UI::WindowID_EffectsMonitor);
    if (pos) {
        if (pos->state & 0x2) {
            // Default layout
            pos->state = 0xd;
            pos->p1 = { 0.f,0.f };
            pos->p2 = { 312.f, 104.f };
        }
        float uiscale = GuiUtils::GetGWScaleMultiplier();
        GW::Vec2f xAxis = pos->xAxis(uiscale);
        GW::Vec2f yAxis = pos->yAxis(uiscale);
        window_size = { std::roundf(xAxis.y - xAxis.x), std::roundf(yAxis.y - yAxis.x) };
        window_pos = { std::roundf(xAxis.x),std::roundf(yAxis.x) };
        layout = window_size.y > window_size.x ? Layout::Columns : Layout::Rows;
        m_skill_width = std::roundf(std::min<float>(default_skill_width * uiscale, std::min<float>(window_size.x, window_size.y)));
        if (layout == Layout::Rows) {
            row_count = std::floorf(window_size.y / m_skill_width);
            skills_per_row = std::floorf(window_size.x / m_skill_width);
        }
        else {
            row_count = std::floorf(window_size.x / m_skill_width);
            skills_per_row = std::floorf(window_size.y / m_skill_width);
        }

        GW::Vec2f mid_point(window_pos.x + window_size.x / 2.f, window_pos.y + window_size.y / 2.f);
        GW::Vec2f screen_size = { static_cast<float>(GW::Render::GetViewportWidth()) , static_cast<float>(GW::Render::GetViewportHeight()) };
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
}

void EffectsMonitorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) return;

    if (!initialised) {
        initialised = true;
        GW::UI::RegisterUIMessageCallback(&OnEffect_Entry, OnEffectUIMessage,0x8000);
        GW::GameThread::Enqueue([]() {
            Instance().RefreshEffects();
            });
        RefreshPosition();
    }

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

    float row_skills_drawn = 0.f;
    float row_idx = 1.f;
    int draw = 0;
    auto next_effect = [&]() {
        row_skills_drawn++;
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
    auto skip_effects = [&]() {
        if (skipped_effects) return;
        if (morale_percent != 100)
            next_effect();
        if (minion_count)
            next_effect();
        skipped_effects = true;
    };
    if (!hard_mode) {
        skip_effects();
    }

    for (auto& it : cached_effects) {
        for (size_t i = 0; i < it.second.size();i++) {
            GW::Effect& effect = it.second[i];
            if (effect.duration) {
                DWORD remaining = effect.GetTimeRemaining();
                draw = remaining < (DWORD)(effect.duration * 1000.f);
                if (draw) {
                    draw = UptimeToString(remaining_str, remaining);
                }
                else if (DurationExpired(effect)) {
                    goto enddraw; // cached_effects is now invalidated; skip to end and redraw next frame
                }
            }
            else if(effect.skill_id == static_cast<uint32_t>(GW::Constants::SkillID::Hard_mode)) {
                if (show_vanquish_counter) {
                    size_t left = GW::Map::GetFoesToKill();
                    size_t killed = GW::Map::GetFoesKilled();
                    draw = left ? snprintf(remaining_str, 16, "%d/%d", killed, killed + left) : 0;
                }
            }
            else {
                draw = 0;
            }
            if (draw > 0) {
                const ImVec2 label_size = ImGui::CalcTextSize(remaining_str);
                ImVec2 label_pos(skill_top_left.x + m_skill_width - label_size.x - 1.f, skill_top_left.y + m_skill_width - label_size.y - 1.f);
                if ((color_background & IM_COL32_A_MASK) != 0)
                    ImGui::GetWindowDrawList()->AddRectFilled({ skill_top_left.x + m_skill_width - label_size.x - 2.f, skill_top_left.y + m_skill_width - label_size.y }, { skill_top_left.x + m_skill_width, skill_top_left.y + m_skill_width }, color_background);
                if ((color_text_shadow & IM_COL32_A_MASK) != 0)
                    ImGui::GetWindowDrawList()->AddText({ label_pos.x + 1, label_pos.y + 1 }, color_text_shadow, remaining_str);
                ImGui::GetWindowDrawList()->AddText(label_pos, color_text_effects, remaining_str);
            }
            skip_effects();
            next_effect();

        }
    }
    enddraw:
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
}
void EffectsMonitorWidget::LoadSettings(CSimpleIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    decimal_threshold = ini->GetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
    round_up = ini->GetBoolValue(Name(), VAR_NAME(round_up), round_up);
    show_vanquish_counter = ini->GetBoolValue(Name(), VAR_NAME(show_vanquish_counter), show_vanquish_counter);
    font_effects = static_cast<GuiUtils::FontSize>(
        ini->GetLongValue(Name(), VAR_NAME(font_effects), static_cast<long>(font_effects)));
    color_text_effects = Colors::Load(ini, Name(), VAR_NAME(color_text_effects), color_text_effects);
    color_text_shadow = Colors::Load(ini, Name(), VAR_NAME(color_text_shadow), color_text_shadow);
    color_background = Colors::Load(ini, Name(), VAR_NAME(color_background), color_background);
}
void EffectsMonitorWidget::SaveSettings(CSimpleIni* ini)
{
    ToolboxWidget::SaveSettings(ini);


    ini->SetLongValue(Name(), VAR_NAME(decimal_threshold), decimal_threshold);
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

    constexpr char* font_sizes[] = { "16", "18", "20", "24", "42", "48" };

    ImGui::PushID("effects_monitor_overlay_settings");
    ImGui::Combo("Text size", reinterpret_cast<int*>(&font_effects), font_sizes, 6);
    Colors::DrawSettingHueWheel("Text color", &color_text_effects);
    Colors::DrawSettingHueWheel("Text shadow", &color_text_shadow);
    Colors::DrawSettingHueWheel("Effect duration background", &color_background);
    ImGui::InputInt("Text decimal threshold", &decimal_threshold);
    ImGui::ShowHelp("When should decimal numbers start to show (in milliseconds)");
    ImGui::Checkbox("Round up integers", &round_up);
    ImGui::SameLine();
    ImGui::Checkbox("Show vanquish counter on Hard Mode effect icon", &show_vanquish_counter);
    ImGui::PopID();
}
int EffectsMonitorWidget::UptimeToString(char arr[8], int cd) const
{
    cd = std::abs(cd);
    if (cd >= decimal_threshold) {
        if (round_up) cd += 1000;
        return snprintf(arr, 8, "%d", cd / 1000);
    }
    return snprintf(arr, 8, "%.1f", cd / 1000.f);
}