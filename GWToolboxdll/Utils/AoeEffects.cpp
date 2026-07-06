#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <ImGuiAddons.h>
#include <Timer.h>
#include <Utils/AoeEffects.h>

namespace {
    using namespace AoeEffects;

    enum SkillEffect {
        Chaos_storm          = 131,
        Meteor_Shower        = 341,
        Savannah_heat        = 346,
        Lava_font            = 347,
        Breath_of_fire       = 351,
        Maelstrom            = 381,
        Barbed_Trap          = 772,
        Barbed_Trap_Activate = 773,
        Flame_Trap           = 774,
        Flame_Trap_Activate  = 775,
        Spike_Trap           = 777,
        Spike_Trap_Activate  = 778,
        Bed_of_coals         = 875,
        Churning_earth       = 994
    };

    struct pair_hash {
        template <class T1, class T2>
        size_t operator()(const std::pair<T1, T2>& pair) const
        {
            return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
        }
    };

    struct EffectTrigger {
        uint32_t triggered_effect_id = 0;
        uint32_t duration = 2000;
        float range = GW::Constants::Range::Nearby;
        std::unordered_map<std::pair<float, float>, clock_t, pair_hash> triggers_handled{};

        EffectTrigger(const uint32_t _triggered_effect_id, const uint32_t _duration, const float _range)
            : triggered_effect_id(_triggered_effect_id), duration(_duration), range(_range) { }
    };

    struct EffectRecord {
        uint64_t uid;
        uint32_t effect_id;
        GW::Vec2f pos;
        uint32_t zplane;
        clock_t start;
        uint32_t duration;
        float range;
        const EffectSettings* settings;
    };

    std::recursive_mutex effects_mutex;
    std::vector<EffectRecord> effects;
    std::unordered_map<uint32_t, EffectSettings*> effect_settings;
    std::unordered_map<uint32_t, EffectTrigger*> effect_triggers;
    uint64_t next_uid = 1;

    bool callbacks_registered = false;
    GW::HookEntry StoC_Hook;

    bool InExplorable()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    void AddEffect(const uint32_t effect_id, const float x, const float y, const uint32_t zplane, const EffectSettings* settings)
    {
        effects.push_back({next_uid++, effect_id, {x, y}, zplane, TIMER_INIT(), settings->duration, settings->range, settings});
    }

    void RemoveTriggeredEffect(const uint32_t effect_id, const GW::Vec2f& pos)
    {
        const auto it1 = effect_triggers.find(effect_id);
        if (it1 == effect_triggers.end()) {
            return;
        }
        const auto trigger = it1->second;
        const auto settings_it = effect_settings.find(trigger->triggered_effect_id);
        if (settings_it == effect_settings.end()) {
            return;
        }
        std::pair posp = {pos.x, pos.y};
        const auto trap_handled = trigger->triggers_handled.find(posp);
        if (trap_handled != trigger->triggers_handled.end() && TIMER_DIFF(trap_handled->second) < 5000) {
            return; // Already handled this trap, e.g. Spike Trap triggers 3 times over 2 seconds; we only care about the first.
        }
        trigger->triggers_handled.emplace(posp, TIMER_INIT());
        EffectRecord* closest = nullptr;
        float closest_distance = GW::Constants::SqrRange::Nearby;
        for (auto& effect : effects) {
            if (effect.effect_id != settings_it->second->effect_id) {
                continue;
            }
            // Need to estimate position; player may have moved on cast slightly.
            const float new_distance = GetSquareDistance(pos, effect.pos);
            if (new_distance > closest_distance) {
                continue;
            }
            closest = &effect;
            closest_distance = new_distance;
        }
        if (closest) {
            // Trigger this trap to time out in 2 seconds' time. Increase damage radius from adjacent to nearby.
            closest->start = TIMER_INIT();
            closest->duration = trigger->duration;
            closest->range = trigger->range;
        }
    }

    void OnGenericValue(GW::HookStatus*, const GW::Packet::StoC::GenericValue* pak)
    {
        if (pak->value_id != 21) {
            // Not "effect on agent"
            return;
        }
        if (!InExplorable()) {
            return;
        }
        std::lock_guard lock(effects_mutex); // the settings table can be rebuilt from the render thread
        const auto it = effect_settings.find(pak->value);
        if (it == effect_settings.end()) {
            return;
        }
        const auto settings = it->second;
        if (settings->stoc_header && settings->stoc_header != pak->header) {
            return;
        }
        const auto* caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
        if (!caster || caster->allegiance != GW::Constants::Allegiance::Enemy) {
            return;
        }
        AddEffect(pak->value, caster->pos.x, caster->pos.y, caster->pos.zplane, settings);
    }

    void OnGenericValueTarget(GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* pak)
    {
        if (pak->Value_id != 20) {
            // Not "effect on target"
            return;
        }
        if (!InExplorable()) {
            return;
        }
        std::lock_guard lock(effects_mutex);
        const auto it = effect_settings.find(pak->value);
        if (it == effect_settings.end()) {
            return;
        }
        const auto settings = it->second;
        if (settings->stoc_header && settings->stoc_header != pak->header) {
            return;
        }
        if (pak->caster == pak->target) {
            return;
        }
        const auto caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->caster));
        if (!caster || caster->allegiance != GW::Constants::Allegiance::Enemy) {
            return;
        }
        const GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
        if (!target) {
            return;
        }
        AddEffect(pak->value, target->pos.x, target->pos.y, target->pos.zplane, settings);
    }

    void OnPlayEffect(GW::HookStatus*, GW::Packet::StoC::PlayEffect* pak)
    {
        if (!InExplorable()) {
            return;
        }
        std::scoped_lock lock(effects_mutex);
        // TODO: Fire storm and Meteor shower have no caster!
        // Need to record GenericValueTarget with value_id matching these skills, then roughly match the coords after.
        RemoveTriggeredEffect(pak->effect_id, pak->coords);
        const auto it = effect_settings.find(pak->effect_id);
        if (it == effect_settings.end()) {
            return;
        }
        const auto settings = it->second;
        if (settings->stoc_header && settings->stoc_header != pak->header) {
            return;
        }
        const auto a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
        if (!a || a->allegiance != GW::Constants::Allegiance::Enemy) {
            return;
        }
        AddEffect(pak->effect_id, pak->coords.x, pak->coords.y, pak->plane, settings);
    }
} // namespace

void AoeEffects::Initialize()
{
    if (callbacks_registered) {
        return;
    }
    callbacks_registered = true;
    if (effect_settings.empty()) {
        LoadDefaults();
    }
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&StoC_Hook, OnGenericValue);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&StoC_Hook, OnGenericValueTarget);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&StoC_Hook, OnPlayEffect);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&StoC_Hook, [](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) {
        Clear();
    });
}

void AoeEffects::Terminate()
{
    if (callbacks_registered) {
        GW::StoC::RemoveCallbacks(&StoC_Hook);
        callbacks_registered = false;
    }
    std::lock_guard lock(effects_mutex);
    effects.clear();
    for (const auto& settings : effect_settings) {
        delete settings.second;
    }
    effect_settings.clear();
    for (const auto& triggers : effect_triggers) {
        delete triggers.second;
    }
    effect_triggers.clear();
}

void AoeEffects::LoadDefaults()
{
    std::lock_guard lock(effects_mutex);
    effects.clear(); // records hold pointers into the settings table below
    for (const auto& settings : effect_settings) {
        delete settings.second;
    }
    effect_settings.clear();
    for (const auto& triggers : effect_triggers) {
        delete triggers.second;
    }
    effect_triggers.clear();

    effect_settings.emplace(Maelstrom, new EffectSettings{0xFFFF0000, "Maelstrom", Maelstrom, GW::Constants::Range::Adjacent, 0, 10000});
    effect_settings.emplace(Chaos_storm, new EffectSettings{0xFFFF0000, "Chaos Storm", Chaos_storm, GW::Constants::Range::Adjacent, 0, 10000});
    effect_settings.emplace(Savannah_heat, new EffectSettings{0xFFFF0000, "Savannah Heat", Savannah_heat, GW::Constants::Range::Adjacent, 0, 5000});
    effect_settings.emplace(Breath_of_fire, new EffectSettings{0xFFFF0000, "Breath of Fire", Breath_of_fire, GW::Constants::Range::Adjacent, 0, 5000});
    effect_settings.emplace(Lava_font, new EffectSettings{0xFFFF0000, "Lava font", Lava_font, GW::Constants::Range::Adjacent, 0, 5000});
    effect_settings.emplace(Churning_earth, new EffectSettings{0xFFFF0000, "Churning Earth", Churning_earth, GW::Constants::Range::Nearby, 0, 5000});

    effect_settings.emplace(Barbed_Trap, new EffectSettings{0xFFFF0000, "Barbed Trap", Barbed_Trap, GW::Constants::Range::Adjacent, GW::Packet::StoC::GenericValue::STATIC_HEADER, 90000});
    effect_triggers.emplace(Barbed_Trap_Activate, new EffectTrigger(Barbed_Trap, 2000, GW::Constants::Range::Nearby));
    effect_settings.emplace(Flame_Trap, new EffectSettings{0xFFFF0000, "Flame Trap", Flame_Trap, GW::Constants::Range::Adjacent, GW::Packet::StoC::GenericValue::STATIC_HEADER, 90000});
    effect_triggers.emplace(Flame_Trap_Activate, new EffectTrigger(Flame_Trap, 2000, GW::Constants::Range::Nearby));
    effect_settings.emplace(Spike_Trap, new EffectSettings{0xFFFF0000, "Spike Trap", Spike_Trap, GW::Constants::Range::Adjacent, GW::Packet::StoC::GenericValue::STATIC_HEADER, 90000});
    effect_triggers.emplace(Spike_Trap_Activate, new EffectTrigger(Spike_Trap, 2000, GW::Constants::Range::Nearby));
}

std::unordered_map<uint32_t, AoeEffects::EffectSettings*>& AoeEffects::GetEffectSettings()
{
    if (effect_settings.empty()) {
        LoadDefaults();
    }
    return effect_settings;
}

void AoeEffects::GetActiveEffects(std::vector<ActiveEffect>& out)
{
    out.clear();
    std::lock_guard lock(effects_mutex);
    std::erase_if(effects, [](const EffectRecord& e) {
        return TIMER_DIFF(e.start) > static_cast<clock_t>(e.duration);
    });
    out.reserve(effects.size());
    for (const auto& e : effects) {
        out.push_back({e.uid, e.effect_id, e.pos, e.zplane, e.start, e.duration, e.range, e.settings->color});
    }
}

void AoeEffects::Clear()
{
    std::lock_guard lock(effects_mutex);
    effects.clear();
    for (const auto& trigger : effect_triggers) {
        trigger.second->triggers_handled.clear();
    }
}

void AoeEffects::DrawColorSettings()
{
    ImGui::SmallConfirmButton("Restore Defaults", "Are you sure?", [](const bool result, void*) {
        if (result) {
            LoadDefaults();
        }
    });
    for (const auto& s : GetEffectSettings()) {
        ImGui::PushID(static_cast<int>(s.first));
        Colors::DrawSettingHueWheel("", &s.second->color, 0);
        ImGui::SameLine();
        ImGui::TextUnformatted(s.second->name.c_str());
        ImGui::PopID();
    }
}
