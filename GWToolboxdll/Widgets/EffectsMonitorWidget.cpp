#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameContainers/GamePos.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Color.h>
#include <Defines.h>
#include "EffectsMonitorWidget.h"

#include <Utils/FontLoader.h>

namespace {
    auto font_effects = 18.f;
    Color color_text_effects = Colors::White();
    Color color_background = Colors::ARGB(128, 0, 0, 0);
    Color color_text_shadow = Colors::Black();

    // duration -> string settings
    int decimal_threshold = 600;   // when to start displaying decimals
    int only_under_seconds = 3600; // hide effect durations over n seconds
    bool round_up = true;          // round up or down?
    bool show_vanquish_counter = true;
    bool track_spirit_effects = false;

    GW::UI::Frame* effects_frame = nullptr;

    ImGuiViewport* viewport = nullptr;
    ImDrawList* draw_list = nullptr;

    // Maps an agent's encoded name to the skill_id of the spirit it represents.
    // TODO: @3vcloud - populate this with the actual encoded names for tracked spirits.
    // To find enc_names: use GW::Agents::GetAgentEncName(agent) in-game and log the values.
    const std::unordered_map<std::wstring, GW::Constants::SkillID> spirit_enc_name_to_skill_id = {
        {L"\x416F\xD141\x9F0B\x5276", GW::Constants::SkillID::Disenchantment},
        {L"\x4164\x825C\xA2F2\x1235", GW::Constants::SkillID::Pain}, 
        {L"\x4171\xCD7A\xD7A6\x386D", GW::Constants::SkillID::Bloodsong}, 
        {L"\x8102\x5F66\xBE02\xB9AB\x1073",GW::Constants::SkillID::Signet_of_Spirits}
    };

    // Maps spirit agent_id -> custom effect_id for currently tracked spirits.
    std::unordered_map<uint32_t, uint32_t> tracked_spirit_effects;

    GW::HookEntry OnAgentRemove_SpiritEntry;

    // Calculates the spirit duration in seconds based on the spirit skill and NPC level.
    // Uses duration0/duration15 from skill data as a linear interpolation over NPC level.
    // NB: Fine-tune the level-to-attribute mapping as needed.
    float GetSpiritDuration(const GW::Constants::SkillID skill_id, const uint32_t npc_level)
    {
        const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!skill || skill->duration0 == 0) return 60.f;
        const float d0 = static_cast<float>(skill->duration0);
        const float d15 = static_cast<float>(skill->duration15);
        const float level = std::min(static_cast<float>(npc_level), 20.f);
        return d0 + (d15 - d0) * level / 15.f;
    }

    void RemoveTrackedSpiritEffect(const uint32_t agent_id)
    {
        const auto it = tracked_spirit_effects.find(agent_id);
        if (it == tracked_spirit_effects.end()) return;
        GW::Effects::RemoveCustomEffect(it->second);
        tracked_spirit_effects.erase(it);
    }

    void OnSpiritAgentRemoved(const GW::HookStatus*, const GW::Packet::StoC::AgentRemove* packet)
    {
        RemoveTrackedSpiritEffect(packet->agent_id);
    }

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

    void DrawTextOverlay(const char* text, const GW::UI::Frame* frame)
    {
        if (!(frame && text && *text && effects_frame)) return;
        auto skill_bottom_right = frame->position.GetBottomRightOnScreen();
        const auto skill_frame_size = frame->position.GetSizeOnScreen();

        skill_bottom_right.x += viewport->Pos.x;
        skill_bottom_right.y += viewport->Pos.y;

        ImFont* overridden_font = nullptr;

        GW::Vec2f label_size = ImGui::CalcTextSize(text);
        if (label_size.x > skill_frame_size.x) {
            // If the label is wider than the frame, scale text size.
            const auto scale_factor = skill_frame_size.x / label_size.x;
            const auto scaled_size = scale_factor * font_effects;
            overridden_font = FontLoader::GetFont();
            ImGui::PushFont(overridden_font, draw_list, scaled_size);
            label_size *= scale_factor;
        }

        const ImVec2 label_pos(skill_bottom_right.x - label_size.x, skill_bottom_right.y - label_size.y);
        if ((color_background & IM_COL32_A_MASK) != 0) {
            draw_list->AddRectFilled(label_pos, skill_bottom_right, color_background);
        }
        if ((color_text_shadow & IM_COL32_A_MASK) != 0) {
            draw_list->AddText({label_pos.x + 1, label_pos.y + 1}, color_text_shadow, text);
        }
        draw_list->AddText(label_pos, color_text_effects, text);
        if (overridden_font) {
            ImGui::PopFont(draw_list);
        }
    }
    
    std::unordered_map<uint32_t, clock_t> effect_timestamps;
    
    GW::HookEntry OnPreUIMessage_HookEntry;
    void OnPreUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kEffectAdd: {
            const auto packet = (GW::UI::UIPacket::kEffectAdd*)wparam;
            switch (packet->effect->skill_id) {
            case GW::Constants::SkillID::Aspect_of_Exhaustion:
            case GW::Constants::SkillID::Aspect_of_Depletion_energy_loss:
            case GW::Constants::SkillID::Scorpion_Aspect:
                if (!effect_timestamps.contains((uint32_t)packet->effect->skill_id))
                    effect_timestamps[(uint32_t)packet->effect->skill_id] = GW::MemoryMgr::GetSkillTimer();
                break;
            }
        } break;
        }
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
    DummyWindow();
    viewport = ImGui::GetMainViewport();
    draw_list = ImGui::GetBackgroundDrawList(viewport);
    const auto font = FontLoader::GetFont();
    ImGui::PushFont(font, draw_list, font_effects);

    const auto hard_mode_frame = GW::UI::GetChildFrame(effects_frame, 1);
    // const auto minion_count_frame = GW::UI::GetChildFrame(effects_frame, 2);
    // const auto morale_frame = GW::UI::GetChildFrame(effects_frame, 3);

    if (hard_mode_frame && show_vanquish_counter) {
        if (const auto foes_left = GW::Map::GetFoesToKill()) {
            const auto foes_killed = GW::Map::GetFoesKilled();
            std::array<char, 16> remaining_str;
            snprintf(remaining_str.data(), 16, "%d/%d", foes_killed, foes_killed + foes_left);
            DrawTextOverlay(remaining_str.data(), hard_mode_frame);
        }
    }
    const auto effects = GW::Effects::GetPlayerEffects();

    if (effects) {
        std::unordered_map<GW::Constants::SkillID, DWORD> time_remaining_by_effect;
        for (auto& effect : *effects) {
            if (effect.duration <= 0) continue;
            const auto remaining = effect.GetTimeRemaining();
            if (remaining <= 0) continue;
            const auto found = time_remaining_by_effect.find(effect.skill_id);
            if (found == time_remaining_by_effect.end() || found->second < remaining) {
                time_remaining_by_effect[effect.skill_id] = remaining;
            }
        }
        for (auto& [skill_id, remaining] : time_remaining_by_effect) {
            const auto skill_frame = GW::UI::GetChildFrame(effects_frame, (uint32_t)skill_id + 0x4);
            if (!skill_frame) continue;
            std::array<char, 16> remaining_str;
            if (UptimeToString(remaining_str.data(), static_cast<int>(remaining)) > 0) DrawTextOverlay(remaining_str.data(), skill_frame);
        }
    }

    ImGui::PopFont(draw_list);
    ImGui::End();
}

void EffectsMonitorWidget::Initialize()
{
    ToolboxWidget::Initialize();
    GW::UI::UIMessage message_ids[] = {
        GW::UI::UIMessage::kEffectAdd
    };
    for (auto message_id : message_ids) {
        GW::UI::RegisterUIMessageCallback(&OnPreUIMessage_HookEntry, message_id, OnPreUIMessage, -0x6000);
    }
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(
        &OnAgentRemove_SpiritEntry, OnSpiritAgentRemoved);
}
void EffectsMonitorWidget::Terminate()
{
    ToolboxWidget::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnPreUIMessage_HookEntry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&OnAgentRemove_SpiritEntry);
    for (const auto& [agent_id, effect_id] : tracked_spirit_effects) {
        GW::Effects::RemoveCustomEffect(effect_id);
    }
    tracked_spirit_effects.clear();
}
void EffectsMonitorWidget::Update(float delta)
{
    static float update_timer = 0.f;
    update_timer += delta;
    if (update_timer < 1.f / 30.f) return;
    update_timer = 0.f;

    for (auto [skill_id, timestamp] : effect_timestamps) {
        auto effect = GW::Effects::GetPlayerEffectBySkillId((GW::Constants::SkillID)skill_id);
        if (!effect) {
            effect_timestamps.erase(skill_id);
            break;
        }
        const auto time_elapsed = effect->GetTimeElapsed();
        if (!effect->duration || (time_elapsed / 1000.f) > effect->duration) {
            const auto now = GW::MemoryMgr::GetSkillTimer();
            const clock_t diff = (now - timestamp) / 1000;

            // a 30s timer starts when you enter the aspect
            // a 30s timer starts 100s after you enter the aspect
            // a 30s timer starts 200s after you enter the aspect
            long duration = 30 - diff % 30;
            if (diff > 100) duration = std::min(duration, 30 - (diff - 100) % 30);
            if (diff > 200) duration = std::min(duration, 30 - (diff - 200) % 30);
            effect->timestamp = now;
            effect->duration = (float)duration;
        }
    }

    if (!track_spirit_effects || spirit_enc_name_to_skill_id.empty()) return;

    const auto* me = GW::Agents::GetControlledCharacter();
    if (!me) return;
    const auto* agents = GW::Agents::GetAgentArray();
    if (!agents) return;

    const auto spirit_flags = GW::AgentTargetFlags::Include_SpiritPet | GW::AgentTargetFlags::Exclude_DeadSpiritPet;
    std::unordered_map<uint32_t, GW::Constants::SkillID> spirits_in_range;
    for (const auto* agent : *agents) {
        if (!GW::Agents::GetAgentMatchesFlags(agent, spirit_flags)) continue;
        const auto* living = static_cast<const GW::AgentLiving*>(agent);
        if (GetSquareDistance(me->pos, living->pos) > GW::Constants::SqrRange::Earshot) continue;
        const auto* enc_name = GW::Agents::GetAgentEncName(agent);
        if (!enc_name) continue;
        const auto it = spirit_enc_name_to_skill_id.find(enc_name);
        if (it == spirit_enc_name_to_skill_id.end()) continue;
        spirits_in_range[agent->agent_id] = it->second;
    }

    std::vector<uint32_t> to_remove;
    for (const auto& [agent_id, effect_id] : tracked_spirit_effects) {
        if (!spirits_in_range.contains(agent_id)) to_remove.push_back(agent_id);
    }
    for (const auto agent_id : to_remove) RemoveTrackedSpiritEffect(agent_id);

    for (const auto& [agent_id, skill_id] : spirits_in_range) {
        if (tracked_spirit_effects.contains(agent_id)) continue;
        const auto* agent = GW::Agents::GetAgentByID(agent_id);
        if (!agent) continue;
        const float duration = GetSpiritDuration(skill_id, static_cast<const GW::AgentLiving*>(agent)->level);
        if (duration <= 0.f) continue;
        const uint32_t effect_id = GW::Effects::AddCustomEffect(skill_id, duration);
        if (effect_id) tracked_spirit_effects[agent_id] = effect_id;
    }
}

void EffectsMonitorWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    LOAD_UINT(decimal_threshold);
    LOAD_UINT(only_under_seconds);
    LOAD_BOOL(round_up);
    LOAD_BOOL(show_vanquish_counter);
    LOAD_BOOL(track_spirit_effects);
    LOAD_FLOAT(font_effects);
    LOAD_COLOR(color_text_effects);
    LOAD_COLOR(color_text_shadow);
    LOAD_COLOR(color_background);

    font_effects = std::clamp(font_effects, 16.f, 48.f);
}

void EffectsMonitorWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);

    SAVE_UINT(decimal_threshold);
    SAVE_UINT(only_under_seconds);
    SAVE_BOOL(round_up);
    SAVE_BOOL(show_vanquish_counter);
    SAVE_BOOL(track_spirit_effects);
    SAVE_FLOAT(font_effects);
    SAVE_COLOR(color_text_effects);
    SAVE_COLOR(color_text_shadow);
    SAVE_COLOR(color_background);
}

void EffectsMonitorWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    ImGui::PushID("effects_monitor_overlay_settings");

    ImGui::DragFloat("Text size", &font_effects, 1.f, 16.f, 48.f, "%.f");
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
    ImGui::Checkbox("Track nearby spirit timers (Bloodsong, Vampirism, etc.)", &track_spirit_effects);
    ImGui::PopID();
}
