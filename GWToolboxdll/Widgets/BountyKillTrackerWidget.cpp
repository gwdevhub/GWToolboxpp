#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Defines.h>
#include <Utils/FontLoader.h>
#include <Utils/GuiUtils.h>

#include "BountyKillTrackerWidget.h"

namespace {
    float font_size = 18.f;
    Color color_text = Colors::White();
    Color color_text_shadow = Colors::Black();
    Color color_background = Colors::ARGB(128, 0, 0, 0);

    GW::UI::Frame* effects_frame = nullptr;
    ImGuiViewport* viewport = nullptr;
    ImDrawList* draw_list = nullptr;

    struct BountyState {
        GW::Constants::TitleID title_id;
        uint32_t points_per_kill;
        uint32_t kill_count;
        uint32_t last_title_points;
        uint32_t effect_id;
    };

    std::unordered_map<GW::Constants::SkillID, BountyState> active_bounties;
    std::unordered_map<uint32_t, GW::Constants::SkillID> effect_id_to_skill;

    // EotN bounties award 25/50/75/100 title points per kill for tiers 1-4.
    // Nightfall and Factions bounties award 1 point per kill by default;
    // exact values should be verified in-game and updated here.
    uint32_t GetPointsPerKill(const GW::Constants::SkillID skill_id)
    {
        using S = GW::Constants::SkillID;
        switch (skill_id) {
            case S::Asuran_Bodyguard_Rank2:
            case S::Dwarven_Raider_Rank2:
            case S::Vanguard_Patrol_Rank2:
            case S::Norn_Hunting_Party_Rank2:
                return 2;
            case S::Asuran_Bodyguard_Rank3:
            case S::Dwarven_Raider_Rank3:
            case S::Vanguard_Patrol_Rank3:
            case S::Norn_Hunting_Party_Rank3:
                return 3;
            case S::Asuran_Bodyguard_Rank4:
            case S::Dwarven_Raider_Rank4:
            case S::Vanguard_Patrol_Rank4:
            case S::Norn_Hunting_Party_Rank4:
                return 4;
            default:
                return 1;
        }
    }

    bool IsBountyTitle(const GW::Constants::TitleID title_id)
    {
        using T = GW::Constants::TitleID;
        switch (title_id) {
            case T::Sunspear:
            case T::Lightbringer:
            case T::Kurzick:
            case T::Luxon:
            case T::Asuran:
            case T::Deldrimor:
            case T::Vanguard:
            case T::Norn:
                return true;
            default:
                return false;
        }
    }

    uint32_t GetCurrentTitlePoints(const GW::Constants::TitleID title_id)
    {
        const auto* title = GW::PlayerMgr::GetTitleTrack(title_id);
        return title ? title->current_points : 0;
    }

    void OnBountyAdded(const GW::Constants::SkillID skill_id, const uint32_t effect_id)
    {
        const auto* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!skill_data) return;
        if (skill_data->type != GW::Constants::SkillType::Bounty) return;
        const auto title_id = static_cast<GW::Constants::TitleID>(skill_data->title);
        if (!IsBountyTitle(title_id)) return;

        BountyState state{};
        state.title_id = title_id;
        state.points_per_kill = GetPointsPerKill(skill_id);
        state.kill_count = 0;
        state.last_title_points = GetCurrentTitlePoints(title_id);
        state.effect_id = effect_id;

        active_bounties[skill_id] = state;
        effect_id_to_skill[effect_id] = skill_id;
    }

    void OnBountyRemoved(const uint32_t effect_id)
    {
        const auto it = effect_id_to_skill.find(effect_id);
        if (it == effect_id_to_skill.end()) return;
        active_bounties.erase(it->second);
        effect_id_to_skill.erase(it);
    }

    void OnTitleProgressUpdated(const GW::Constants::TitleID title_id)
    {
        for (auto& [skill_id, state] : active_bounties) {
            if (state.title_id != title_id) continue;
            const uint32_t current_points = GetCurrentTitlePoints(title_id);
            if (current_points <= state.last_title_points) {
                state.last_title_points = current_points;
                continue;
            }
            const uint32_t delta = current_points - state.last_title_points;
            state.last_title_points = current_points;
            if (state.points_per_kill > 0) {
                state.kill_count += delta / state.points_per_kill;
            }
        }
    }

    void DrawTextOverlay(const char* text, const GW::UI::Frame* frame)
    {
        if (!(frame && text && *text && effects_frame)) return;
        auto bottom_right = frame->position.GetBottomRightOnScreen();
        const auto frame_size = frame->position.GetSizeOnScreen();

        bottom_right.x += viewport->Pos.x;
        bottom_right.y += viewport->Pos.y;

        ImFont* overridden_font = nullptr;
        GW::Vec2f label_size = ImGui::CalcTextSize(text);
        if (label_size.x > frame_size.x) {
            const auto scale_factor = frame_size.x / label_size.x;
            overridden_font = FontLoader::GetFont();
            ImGui::PushFont(overridden_font, draw_list, scale_factor * font_size);
            label_size *= scale_factor;
        }

        const ImVec2 label_pos(bottom_right.x - label_size.x, bottom_right.y - label_size.y);
        if ((color_background & IM_COL32_A_MASK) != 0) {
            draw_list->AddRectFilled(label_pos, bottom_right, color_background);
        }
        if ((color_text_shadow & IM_COL32_A_MASK) != 0) {
            draw_list->AddText({label_pos.x + 1, label_pos.y + 1}, color_text_shadow, text);
        }
        draw_list->AddText(label_pos, color_text, text);
        if (overridden_font) {
            ImGui::PopFont(draw_list);
        }
    }

    GW::HookEntry UIMessage_HookEntry;

    void OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kEffectAdd: {
                const auto* packet = static_cast<GW::UI::UIPacket::kEffectAdd*>(wparam);
                if (packet->agent_id != GW::Agents::GetControlledCharacterId()) return;
                OnBountyAdded(packet->effect->skill_id, packet->effect->effect_id);
            } break;
            case GW::UI::UIMessage::kEffectRemove: {
                OnBountyRemoved(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam)));
            } break;
            case GW::UI::UIMessage::kTitleProgressUpdated: {
                OnTitleProgressUpdated(static_cast<GW::Constants::TitleID>(reinterpret_cast<uintptr_t>(wparam)));
            } break;
            case GW::UI::UIMessage::kMapChange: {
                active_bounties.clear();
                effect_id_to_skill.clear();
            } break;
            // TODO: Add case for chat message(s) that announce kill milestones (e.g. "35 insects slain")
            // and reset kill_count for the relevant active bounty. Encoded string IDs are not yet known.
        }
    }
}

void BountyKillTrackerWidget::Initialize()
{
    ToolboxWidget::Initialize();

    // Pick up bounties already active when the module loads
    GW::GameThread::Enqueue([] {
        const auto* effects = GW::Effects::GetPlayerEffects();
        if (!effects) return;
        for (const auto& effect : *effects) {
            OnBountyAdded(effect.skill_id, effect.effect_id);
        }
    });

    constexpr GW::UI::UIMessage messages[] = {
        GW::UI::UIMessage::kEffectAdd,
        GW::UI::UIMessage::kEffectRemove,
        GW::UI::UIMessage::kTitleProgressUpdated,
        GW::UI::UIMessage::kMapChange,
    };
    for (const auto msg : messages) {
        GW::UI::RegisterUIMessageCallback(&UIMessage_HookEntry, msg, OnUIMessage, -0x6000);
    }
}

void BountyKillTrackerWidget::Terminate()
{
    ToolboxWidget::Terminate();
    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
    active_bounties.clear();
    effect_id_to_skill.clear();
}

void BountyKillTrackerWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    if (active_bounties.empty()) return;

    effects_frame = GW::UI::GetFrameByLabel(L"Effects");
    if (!effects_frame) return;

    DummyWindow();
    viewport = ImGui::GetMainViewport();
    draw_list = ImGui::GetBackgroundDrawList(viewport);
    ImGui::PushFont(FontLoader::GetFont(), draw_list, font_size);

    for (const auto& [skill_id, state] : active_bounties) {
        if (state.kill_count == 0) continue;
        const auto* skill_frame = GW::UI::GetChildFrame(effects_frame, static_cast<uint32_t>(skill_id) + 0x4);
        if (!skill_frame) continue;
        std::array<char, 16> count_str{};
        snprintf(count_str.data(), count_str.size(), "%u", state.kill_count);
        DrawTextOverlay(count_str.data(), skill_frame);
    }

    ImGui::PopFont(draw_list);
    ImGui::End();
}

void BountyKillTrackerWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_FLOAT(font_size);
    LOAD_COLOR(color_text);
    LOAD_COLOR(color_text_shadow);
    LOAD_COLOR(color_background);
    font_size = std::clamp(font_size, 16.f, 48.f);
}

void BountyKillTrackerWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_FLOAT(font_size);
    SAVE_COLOR(color_text);
    SAVE_COLOR(color_text_shadow);
    SAVE_COLOR(color_background);
}

void BountyKillTrackerWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::PushID("bounty_kill_tracker_settings");
    ImGui::DragFloat("Text size", &font_size, 1.f, 16.f, 48.f, "%.f");
    Colors::DrawSettingHueWheel("Text color", &color_text);
    Colors::DrawSettingHueWheel("Text shadow", &color_text_shadow);
    Colors::DrawSettingHueWheel("Background", &color_background);
    ImGui::PopID();
}
