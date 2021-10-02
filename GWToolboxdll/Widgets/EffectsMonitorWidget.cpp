#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include "EffectsMonitorWidget.h"

#include "Timer.h"

void EffectsMonitorWidget::Update(float)
{
    //if (!visible) return;
    if (map_ready && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        cached_effects.clear();
        map_ready = false;
    }

    if (!initialised) {
        initialised = true;
        GW::UI::RegisterUIMessageCallback(&OnEffect_Entry, OnEffectUIMessage);
        Refresh();
    }
    if (!map_ready && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        map_ready = true;
    }
}

void EffectsMonitorWidget::Refresh() {
    GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();

    if (effects.valid()) {
        std::vector<GW::Effect> readd_effects;
        for (GW::Effect& effect : effects) {
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
    }
}
void EffectsMonitorWidget::OnEffectUIMessage(GW::HookStatus*, uint32_t message_id, void* wParam, void* lParam) {
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(message_id);
    
    switch (message_id) {
    case 0x10000055: {// Add effect
        struct Payload {
            uint32_t agent_id;
            GW::Effect* e;
        } *details = (Payload*)wParam;
        uint32_t player_id = GW::Agents::GetPlayerId();
        if (player_id && player_id != details->agent_id)
            break;
        Instance().SetEffect(details->e, false);
    }break;
    case 0x10000056: {// Renew effect
        const GW::Effect* e = Instance().GetEffect(*(uint32_t*)wParam);
        if (e)
            Instance().SetEffect(e, true);
    }break;
    case 0x10000057: {// Remove effect
        Instance().RemoveEffect((uint32_t)wParam);
    }break;
    }
}
void EffectsMonitorWidget::RemoveEffect(uint32_t effect_id) {
    for (auto& by_type : cached_effects) {
        auto& effects = by_type.second;
        for (size_t i = 0; i < effects.size();i++) {
            if (effects[i].effect_id == effect_id) {
                by_type.second.erase(effects.begin() + i);
                if (effects.size() == 0)
                    cached_effects.erase(by_type.first);
                return;
            } 
        }
    }
}
const GW::Effect* EffectsMonitorWidget::GetEffect(uint32_t effect_id) {
    const GW::EffectArray& effects = GW::Effects::GetPlayerEffectArray();
    if (!effects.valid())
        return nullptr;
    for (const GW::Effect& effect : effects) {
        if (effect.effect_id == effect_id)
            return &effect;
    }
    return nullptr;
}
uint32_t EffectsMonitorWidget::GetEffectType(uint32_t skill_id) {
    if (skill_id == static_cast<uint32_t>(GW::Constants::SkillID::Hard_mode))
        return 0;
    const GW::Skill& skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    switch (static_cast<GW::Constants::SkillType>(skill.type)) {
    case GW::Constants::SkillType::Condition:
        return 1;
    case GW::Constants::SkillType::Hex:
        return 2;
    case GW::Constants::SkillType::Ritual:
        return 3;
    case GW::Constants::SkillType::Skill:
      if(skill.duration15)
        return 4;
      break;
    case GW::Constants::SkillType::Stance:
        return 5;
    case GW::Constants::SkillType::Enchantment:
        return 6;
    }
    return 0xFF;
}
void EffectsMonitorWidget::SetEffect(const GW::Effect* effect, bool renew) {
    uint32_t type = GetEffectType(effect->skill_id);
    if (cached_effects.find(type) == cached_effects.end())
        cached_effects[type] = std::vector<GW::Effect>();
    size_t current = GetEffectIndex(cached_effects[type], effect->skill_id);
    if (current != 0xFF && !renew) {
        cached_effects[type].erase(cached_effects[type].begin() + current);
        current = 0xFF;
    }
    if (current != 0xFF)
        cached_effects[type][current] = *effect;
    else
        cached_effects[type].push_back(*effect);
}

size_t EffectsMonitorWidget::GetEffectIndex(const std::vector<GW::Effect>& arr, uint32_t skill_id) {
    for (size_t i = 0; i < arr.size(); i++) {
        if (arr[i].skill_id == skill_id)
            return i;
    }
    return 0xFF;
}

void EffectsMonitorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    GW::UI::WindowPosition* pos = GW::UI::GetWindowPosition(GW::UI::WindowID_EffectsMonitor);
    ImVec2 skillsize(default_skill_width, default_skill_width);
    ImVec2 winsize;
    const auto window_flags = GetWinFlags();

    float row_count = 1.f;
    float skills_per_row = 99.f;
    if (snap_to_gw_interface && pos) {
        if (pos->state & 0x2) {
            // Default layout
            pos->state = 0x21;
            pos->p1 = { 224.5f,56.f };
            pos->p2 = { 223.5f, 0.f };
        }
        float uiscale = GuiUtils::GetGWScaleMultiplier();
        GW::Vec2f xAxis = pos->xAxis(uiscale);
        GW::Vec2f yAxis = pos->yAxis(uiscale);
        float width = xAxis.y - xAxis.x;
        float height = yAxis.y - yAxis.x;
        layout = width > height ? Layout::Rows : Layout::Columns;
        m_skill_width = skillsize.y = skillsize.x = std::min<float>(default_skill_width * uiscale, std::min<float>(width, height));
        if (layout == Layout::Rows) {
            row_count = height / m_skill_width;
            skills_per_row = std::floor(width / m_skill_width);
        }
        else {
            row_count = width / m_skill_width;
            skills_per_row = std::floor(height / m_skill_width);
        }
        ImGui::SetNextWindowPos({ xAxis.x,yAxis.x });
        winsize = { width,height };
    }

    ImGui::SetNextWindowSize(winsize);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    
    ImGui::Begin(Name(), nullptr, window_flags);
    const ImVec2 winpos = ImGui::GetWindowPos();

    ImVec2 pos1(winpos.x, winpos.y);
    

    char remaining_str[8];
draw_effects:
    float row_skills_drawn = 0.f;
        for (auto& it : cached_effects) {
            for (const GW::Effect& effect : it.second) {
                if (effect.duration) {
                    float remaining = effect.GetTimeRemaining() / 1000.f;
                    if (remaining < effect.duration) {
                        snprintf(remaining_str, _countof(remaining_str), remaining < 10.f ? "%.1f" : "%.0f", remaining);
                        const ImVec2 label_size = ImGui::CalcTextSize(remaining_str);
                        ImVec2 label_pos((pos1.x + skillsize.x / 2 - label_size.x / 2) + 1, (pos1.y + skillsize.y / 2 - label_size.y / 2) + 1);
                        ImGui::GetWindowDrawList()->AddText(label_pos, Colors::Black(), remaining_str);
                        label_pos.x--;
                        label_pos.y--;
                        ImGui::GetWindowDrawList()->AddText(label_pos, Colors::White(), remaining_str);
                    }
                    else {
                        // Effect expired
                        const GW::Effect* e = GetEffect(effect.effect_id);
                        if (!e) {
                            RemoveEffect(effect.effect_id);
                            goto draw_effects;
                        }
                        // Got here; effect has expired, but game hasn't removed it yet e.g. lag. Still need to draw it.
                    }

                }
                row_skills_drawn++;
                ImVec2 pos2 = ImVec2(pos1.x + skillsize.x, pos1.y + skillsize.y);
                if (layout == Layout::Rows) {
                    pos1.x += skillsize.x;
                    if (row_skills_drawn == skills_per_row) {
                        pos1.y += skillsize.y;
                        pos1.x = winpos.x;
                    }
                        
                }
                else {
                    pos1.y += skillsize.y;
                    if (row_skills_drawn == skills_per_row) {
                        pos1.x += skillsize.x;
                        pos1.y = winpos.y;
                    }
                        
                }
            }
        }
    
    

    ImGui::End();
    ImGui::PopStyleVar();
}