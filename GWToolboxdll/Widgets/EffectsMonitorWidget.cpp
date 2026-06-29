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

#include <GWCA/GameContainers/GamePos.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Color.h>
#include <Defines.h>
#include "EffectsMonitorWidget.h"

#include <Utils/FontLoader.h>

namespace {
    EffectsMonitorWidget::Settings settings;

    GW::UI::Frame* effects_frame = nullptr;

    ImGuiViewport* viewport = nullptr;
    ImDrawList* draw_list = nullptr;

    // Maps an agent's encoded name to the skill_id of the spirit it represents.
    // TODO: @3vcloud - populate with actual encoded names for all desired tracked spirits.
    const std::unordered_map<std::wstring, GW::Constants::SkillID> spirit_enc_name_to_skill_id = {
        {L"\x416F\xD141\x9F0B\x5276", GW::Constants::SkillID::Disenchantment},
        {L"\x4164\x825C\xA2F2\x1235", GW::Constants::SkillID::Pain},
        {L"\x4171\xCD7A\xD7A6\x386D", GW::Constants::SkillID::Bloodsong},
        {L"\x8102\x5F66\xBE02\xB9AB\x1073", GW::Constants::SkillID::Signet_of_Spirits}
    };

    // Deterministic effect ID per spirit skill: high byte 0x0f avoids collision with real effects.
    constexpr uint32_t SpiritEffectId(const GW::Constants::SkillID skill_id)
    {
        return 0x0f000000 | static_cast<uint32_t>(skill_id);
    }

    // Maps agent_id -> skill_id for currently tracked spirit agents.
    std::unordered_map<uint32_t, GW::Constants::SkillID> tracked_spirits;

    // Set when the player completes a spirit-summoning cast; cleared once the spirit is detected.
    struct PendingSpiritSpawn {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t timestamp_ms = 0;
    } pending_spirit_spawn;

    // Set when a spirit agent spawns before the skill activation message arrives.
    struct PendingAgentSpawn {
        uint32_t agent_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t timestamp_ms = 0;
    } pending_agent_spawn;

    bool IsAlliedSpirit(const GW::Agent* agent) {
        return GW::Agents::GetAgentMatchesFlags(agent, GW::AgentTargetFlags::Include_SpiritPet | GW::AgentTargetFlags::Exclude_DeadSpiritPet);
    }

    GW::HookEntry spirit_ui_hook;

    float GetSpiritDuration(const GW::Constants::SkillID skill_id)
    {
        const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!(skill && skill->duration0)) return 60.f;
        const auto att = GW::SkillbarMgr::GetPlayerAttribute((GW::Constants::Attribute)skill->attribute);
        return !att || att->level == 0 ? skill->duration0 : skill->duration0 + (skill->duration15 - skill->duration0) * att->level / 15.f;
    }

    void RemoveTrackedSpirit(const uint32_t agent_id)
    {
        const auto it = tracked_spirits.find(agent_id);
        if (it == tracked_spirits.end()) return;
        GW::Effects::RemoveCustomEffect(SpiritEffectId(it->second));
        tracked_spirits.erase(it);
    }

    void TrackSpirit(const uint32_t agent_id, const GW::Constants::SkillID skill_id)
    {
        // Evict any previously tracked spirit for this skill_id.
        // The old agent's kAgentDestroy may arrive after us, so we must unlink it
        // from tracked_spirits now so RemoveTrackedSpirit doesn't kill our new effect.
        for (auto it = tracked_spirits.begin(); it != tracked_spirits.end();) {
            if (it->second == skill_id && it->first != agent_id) {
                tracked_spirits.erase(it); // don't call RemoveTrackedSpirit - that would remove the effect
                break;
            }
            else
                ++it;
        }
        const float duration = GetSpiritDuration(skill_id);
        GW::Effects::AddCustomEffect(skill_id, duration);
        tracked_spirits[agent_id] = skill_id;
    }

    void OnSpiritUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
        case GW::UI::UIMessage::kAgentSkillActivated: {
            if (!settings.track_spirit_effects) break;
            const auto* packet = static_cast<GW::UI::UIPacket::kAgentSkillPacket*>(wparam);
            if (packet->agent_id != GW::Agents::GetControlledCharacterId()) break;
            const bool is_spirit = std::any_of(
                spirit_enc_name_to_skill_id.begin(), spirit_enc_name_to_skill_id.end(),
                [&](const auto& kv) { return kv.second == packet->skill_id; });
            if (!is_spirit) break;
            // Agent may have already spawned before this activation message arrived.
            if (pending_agent_spawn.skill_id == packet->skill_id
                && GW::MemoryMgr::GetSkillTimer() - pending_agent_spawn.timestamp_ms <= 500
                && IsAlliedSpirit(GW::Agents::GetAgentByID(pending_agent_spawn.agent_id))) {
                TrackSpirit(pending_agent_spawn.agent_id, packet->skill_id);
                pending_agent_spawn = {};
            }
            else {
                pending_spirit_spawn = {packet->skill_id, GW::MemoryMgr::GetSkillTimer()};
            }
        } break;
        case GW::UI::UIMessage::kAgentUpdate: {
            if (!settings.track_spirit_effects) break;
            const auto agent_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            if (tracked_spirits.contains(agent_id)) break;
            const auto* agent = GW::Agents::GetAgentByID(agent_id);
            if (!IsAlliedSpirit(agent)) break;
            const auto* enc_name = GW::Agents::GetAgentEncName(agent);
            if (!enc_name) break;
            const auto name_it = spirit_enc_name_to_skill_id.find(enc_name);
            if (name_it == spirit_enc_name_to_skill_id.end()) break;
            if (pending_spirit_spawn.skill_id != GW::Constants::SkillID::No_Skill) {
                // Skill activation arrived first: resolve now.
                if (GW::MemoryMgr::GetSkillTimer() - pending_spirit_spawn.timestamp_ms > 500) {
                    pending_spirit_spawn = {};
                    break;
                }
                if (name_it->second != pending_spirit_spawn.skill_id) break;
                TrackSpirit(agent_id, name_it->second);
                pending_spirit_spawn = {};
            }
            else {
                // Agent spawned before skill activation: store and wait.
                pending_agent_spawn = {agent_id, name_it->second, GW::MemoryMgr::GetSkillTimer()};
            }
        } break;
        case GW::UI::UIMessage::kAgentDestroy: {
            const auto agent_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            RemoveTrackedSpirit(agent_id);
        } break;
        }
    }

    int UptimeToString(char arr[8], int cd)
    {
        cd = std::abs(cd);
        if (cd > settings.only_under_seconds * 1000) {
            return 0;
        }
        if (cd >= settings.decimal_threshold) {
            if (settings.round_up) {
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
            const auto scaled_size = scale_factor * settings.font_effects;
            overridden_font = FontLoader::GetFont();
            ImGui::PushFont(overridden_font, draw_list, scaled_size);
            label_size *= scale_factor;
        }

        const ImVec2 label_pos(skill_bottom_right.x - label_size.x, skill_bottom_right.y - label_size.y);
        if ((settings.color_background.value & IM_COL32_A_MASK) != 0) {
            draw_list->AddRectFilled(label_pos, skill_bottom_right, settings.color_background);
        }
        if ((settings.color_text_shadow.value & IM_COL32_A_MASK) != 0) {
            draw_list->AddText({label_pos.x + 1, label_pos.y + 1}, settings.color_text_shadow, text);
        }
        draw_list->AddText(label_pos, settings.color_text_effects, text);
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
    ImGui::PushFont(font, draw_list, settings.font_effects);

    const auto hard_mode_frame = GW::UI::GetChildFrame(effects_frame, 1);
    // const auto minion_count_frame = GW::UI::GetChildFrame(effects_frame, 2);
    // const auto morale_frame = GW::UI::GetChildFrame(effects_frame, 3);

    if (hard_mode_frame && settings.show_vanquish_counter) {
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
    SettingsRegistry::Register(this, settings);
    RegisterUIMessageCallback(&OnPreUIMessage_HookEntry, GW::UI::UIMessage::kEffectAdd, OnPreUIMessage, -0x6000);

    GW::UI::UIMessage spirit_messages[] = {
        GW::UI::UIMessage::kAgentSkillActivated,
        GW::UI::UIMessage::kAgentUpdate,
        GW::UI::UIMessage::kAgentDestroy
    };
    for (auto msg : spirit_messages) {
        RegisterUIMessageCallback(&spirit_ui_hook, msg, OnSpiritUIMessage, 0x100);
    }
}
void EffectsMonitorWidget::Terminate()
{
    ToolboxWidget::Terminate();
    GW::UI::RemoveUIMessageCallback(&OnPreUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&spirit_ui_hook);
    for (const auto& [agent_id, skill_id] : tracked_spirits) {
        GW::Effects::RemoveCustomEffect(SpiritEffectId(skill_id));
    }
    tracked_spirits.clear();
    pending_spirit_spawn = {};
    pending_agent_spawn = {};
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

    // Expire pending states that never produced a matching counterpart.
    const auto now_ms = GW::MemoryMgr::GetSkillTimer();
    if (pending_spirit_spawn.skill_id != GW::Constants::SkillID::No_Skill && now_ms - pending_spirit_spawn.timestamp_ms > 500) {
        pending_spirit_spawn = {};
    }
    if (pending_agent_spawn.skill_id != GW::Constants::SkillID::No_Skill && now_ms - pending_agent_spawn.timestamp_ms > 500) {
        pending_agent_spawn = {};
    }

    // Validate that tracked spirits still exist and are alive.
    if (!tracked_spirits.empty()) {
        std::vector<uint32_t> to_remove;
        for (const auto& [agent_id, skill_id] : tracked_spirits) {
            if (!IsAlliedSpirit(GW::Agents::GetAgentByID(agent_id))) {
                to_remove.push_back(agent_id);
            }
        }
        for (const auto id : to_remove) RemoveTrackedSpirit(id);
    }
}

void EffectsMonitorWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    settings.font_effects = std::clamp(settings.font_effects, 16.f, 48.f);
}

void EffectsMonitorWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void EffectsMonitorWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();

    ImGui::PushID("effects_monitor_overlay_settings");

    ImGui::DragFloat("Text size", &settings.font_effects, 1.f, 16.f, 48.f, "%.f");
    Colors::DrawSettingHueWheel("Text color", &settings.color_text_effects.value);
    Colors::DrawSettingHueWheel("Text shadow", &settings.color_text_shadow.value);
    Colors::DrawSettingHueWheel("Effect duration background", &settings.color_background.value);
    ImGui::Text("Don't show effect durations longer than");
    ImGui::SameLine();
    ImGui::PushItemWidth(64.f * ImGui::FontScale());
    ImGui::InputInt("###only_under_seconds", &settings.only_under_seconds, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("seconds");
    ImGui::Text("Show decimal places when duration is less than");
    ImGui::SameLine();
    ImGui::PushItemWidth(64.f * ImGui::FontScale());
    ImGui::InputInt("###decimal_threshold", &settings.decimal_threshold, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("milliseconds");
    ImGui::Checkbox("Round up integers", &settings.round_up);
    ImGui::SameLine();
    ImGui::Checkbox("Show vanquish counter on Hard Mode effect icon", &settings.show_vanquish_counter);
    ImGui::Checkbox("Track nearby spirit timers (Bloodsong, Vampirism, etc.)", &settings.track_spirit_effects);
    ImGui::PopID();
}
