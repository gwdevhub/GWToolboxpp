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
    BountyKillTrackerWidget::Settings settings;

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

    GW::Constants::TitleID GetBountyTitle(const GW::Constants::SkillID skill_id)
    {
        using S = GW::Constants::SkillID;
        using T = GW::Constants::TitleID;
        switch (skill_id) {
            // EotN - Asuran
            case S::Asuran_Bodyguard:
            case S::Asuran_Bodyguard_Rank2:
            case S::Asuran_Bodyguard_Rank3:
            case S::Asuran_Bodyguard_Rank4:
            case S::Veteran_Asuran_Bodyguard:
                return T::Asuran;
            // EotN - Deldrimor
            case S::Dwarven_Raider:
            case S::Dwarven_Raider_Rank2:
            case S::Dwarven_Raider_Rank3:
            case S::Dwarven_Raider_Rank4:
            case S::Veteran_Dwarven_Raider:
            case S::Dwarven_Raider4:
            case S::Dwarven_Raider5:
            case S::Dwarven_Raider6:
            case S::Dwarven_Raider7:
                return T::Deldrimor;
            // EotN - Vanguard
            case S::Vanguard_Patrol:
            case S::Vanguard_Patrol_Rank2:
            case S::Vanguard_Patrol_Rank3:
            case S::Vanguard_Patrol_Rank4:
            case S::Veteran_Vanguard_Patrol:
            case S::Vanguard_Patrol4:
                return T::Vanguard;
            // EotN - Norn
            case S::Norn_Hunting_Party:
            case S::Norn_Hunting_Party_Rank2:
            case S::Norn_Hunting_Party_Rank3:
            case S::Norn_Hunting_Party_Rank4:
            case S::Veteran_Norn_Hunting_Party:
            case S::Norn_Hunting_Party4:
            case S::Norn_Hunting_Party5:
            case S::Norn_Hunting_Party6:
            case S::Norn_Hunting_Party7:
                return T::Norn;
            // Nightfall - Lightbringer (Abaddon's demons)
            case S::Demon_Hunt:
            case S::Margonite_Battle:
            case S::Margonite_Battle1:
            case S::Margonite_Battle2:
            case S::Titan_Hunt:
            case S::Titan_Hunt1:
            case S::Titan_Hunt2:
                return T::Lightbringer;
            // Nightfall - Sunspear
            case S::Skale_Hunt:
            case S::Skale_Hunt1:
            case S::Skale_Hunt2:
            case S::Mandragor_Hunt:
            case S::Mandragor_Hunt1:
            case S::Mandragor_Hunt2:
            case S::Mandragor_Hunt3:
            case S::Skree_Battle:
            case S::Skree_Battle1:
            case S::Insect_Hunt:
            case S::Insect_Hunt1:
            case S::Insect_Hunt2:
            case S::Corsair_Bounty:
            case S::Corsair_Bounty1:
            case S::Corsair_Bounty2:
            case S::Corsair_Bounty3:
            case S::Plant_Hunt:
            case S::Plant_Hunt1:
            case S::Plant_Hunt2:
            case S::Undead_Hunt:
            case S::Undead_Hunt1:
            case S::Undead_Hunt2:
            case S::Undead_Hunt3:
            case S::Monster_Hunt:
            case S::Monster_Hunt1:
            case S::Monster_Hunt2:
            case S::Monster_Hunt3:
            case S::Monster_Hunt4:
            case S::Monster_Hunt5:
            case S::Monster_Hunt6:
            case S::Monster_Hunt7:
            case S::Monster_Hunt8:
            case S::Elemental_Hunt:
            case S::Elemental_Hunt1:
            case S::Elemental_Hunt2:
            case S::Minotaur_Hunt:
            case S::Heket_Hunt:
            case S::Heket_Hunt1:
            case S::Kournan_Bounty:
            case S::Kournan_Bounty1:
            case S::Kournan_Bounty2:
            case S::Kournan_Bounty3:
            case S::Monolith_Hunt:
            case S::Monolith_Hunt1:
            case S::Giant_Hunt:
                return T::Sunspear;
            // Factions - Kurzick
            case S::Blessing_of_the_Kurzicks:
            case S::Blessing_of_the_Kurzicks1:
                return T::Kurzick;
            // Factions - Luxon
            case S::Blessing_of_the_Luxons:
                return T::Luxon;
            default:
                return T::None;
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
        const auto title_id = GetBountyTitle(skill_id);
        if (title_id == GW::Constants::TitleID::None) return;

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
            ImGui::PushFont(overridden_font, draw_list, scale_factor * settings.font_size);
            label_size *= scale_factor;
        }

        const ImVec2 label_pos(bottom_right.x - label_size.x, bottom_right.y - label_size.y);
        if ((settings.color_background & IM_COL32_A_MASK) != 0) {
            draw_list->AddRectFilled(label_pos, bottom_right, settings.color_background);
        }
        if ((settings.color_text_shadow & IM_COL32_A_MASK) != 0) {
            draw_list->AddText({label_pos.x + 1, label_pos.y + 1}, settings.color_text_shadow, text);
        }
        draw_list->AddText(label_pos, settings.color_text, text);
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
    SettingsRegistry::Register(this, settings);

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
        RegisterUIMessageCallback(&UIMessage_HookEntry, msg, OnUIMessage, -0x6000);
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
    ImGui::PushFont(FontLoader::GetFont(), draw_list, settings.font_size);

    for (const auto& [skill_id, state] : active_bounties) {
        const auto* skill_frame = GW::UI::GetChildFrame(effects_frame, static_cast<uint32_t>(skill_id) + 0x4);
        if (!skill_frame) continue;
        std::array<char, 16> count_str{};
        snprintf(count_str.data(), count_str.size(), "%u", state.kill_count);
        DrawTextOverlay(count_str.data(), skill_frame);
    }

    ImGui::PopFont(draw_list);
    ImGui::End();
}

void BountyKillTrackerWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    settings.font_size = std::clamp(settings.font_size, 16.f, 48.f);
}

void BountyKillTrackerWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void BountyKillTrackerWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::PushID("bounty_kill_tracker_settings");
    ImGui::DragFloat("Text size", &settings.font_size, 1.f, 16.f, 48.f, "%.f");
    Colors::DrawSettingHueWheel("Text color", &settings.color_text.value);
    Colors::DrawSettingHueWheel("Text shadow", &settings.color_text_shadow.value);
    Colors::DrawSettingHueWheel("Background", &settings.color_background.value);
    ImGui::PopID();
}
