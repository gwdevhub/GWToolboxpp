#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Timer.h>
#include <Utils/AoeEffects.h>
#include <Utils/SettingsDoc.h>

namespace {
    using namespace AoeEffects;

    enum SkillEffect {
        Chaos_storm          = 131,
        Meteor_Shower        = 341,
        Savannah_heat        = 346,
        Lava_font            = 347,
        Lava_font_overcast   = 348, // Lava Font cast while overcast - larger radius, distinct effect id
        Fire_Storm           = 350,
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
        bool from_enemy;
        const EffectSettings* settings;
    };

    // A recent skill activation, kept briefly so a caster-less ground effect (Fire Storm, Meteor Shower -
    // their PlayEffect carries no caster) can be attributed to enemy/ally by matching its landing coords.
    struct PendingCast {
        GW::Vec2f pos;
        bool from_enemy;
        clock_t time;
    };
    constexpr size_t kMaxPendingCasts = 32;
    constexpr clock_t kPendingCastTtl = 8000; // ms; long enough to cover Meteor Shower's cast + fall

    std::recursive_mutex effects_mutex;
    std::vector<EffectRecord> effects;
    std::vector<PendingCast> pending_casts;
    std::unordered_map<uint32_t, EffectSettings*> effect_settings;
    std::unordered_map<uint32_t, EffectTrigger*> effect_triggers;
    uint64_t next_uid = 1;

    bool callbacks_registered = false;
    GW::HookEntry StoC_Hook;

    bool InExplorable()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    // 1 = enemy (red), 0 = one of our own (green), -1 = ignore (neutral / minions / spirits / no agent).
    int SourceOf(const GW::AgentLiving* caster)
    {
        if (!caster) return -1;
        if (caster->allegiance == GW::Constants::Allegiance::Enemy) return 1;
        if (caster->allegiance == GW::Constants::Allegiance::Ally_NonAttackable) return 0; // player, party, heroes, henchmen
        return -1;
    }

    void AddEffect(const uint32_t effect_id, const float x, const float y, const uint32_t zplane, const bool from_enemy, const EffectSettings* settings)
    {
        const GW::Vec2f pos(x, y);
        // Overcast Lava Font (348) fires alongside the normal one (347) at the same spot; show only the larger
        // overcast ring - it supersedes the normal one, in either arrival order.
        if (effect_id == Lava_font) {
            for (const auto& e : effects)
                if (e.effect_id == Lava_font_overcast && GetSquareDistance(pos, e.pos) < GW::Constants::SqrRange::Adjacent) return;
        }
        else if (effect_id == Lava_font_overcast) {
            std::erase_if(effects, [&](const EffectRecord& e) {
                return e.effect_id == Lava_font && GetSquareDistance(pos, e.pos) < GW::Constants::SqrRange::Adjacent;
            });
        }
        // Meteor Shower / Fire Storm fire a PlayEffect per wave at the same spot; refresh the existing ring
        // rather than stacking copies, keeping the first wave's (correctly attributed) allegiance.
        for (auto& e : effects) {
            if (e.effect_id == effect_id && GetSquareDistance(pos, e.pos) < GW::Constants::SqrRange::Adjacent) {
                e.start = TIMER_INIT();
                return;
            }
        }
        effects.push_back({next_uid++, effect_id, pos, zplane, TIMER_INIT(), settings->duration, settings->range, from_enemy, settings});
    }

    // Record where a skill was cast (the victim's position, or the caster's if there is no victim) tagged with
    // the caster's side, so a following caster-less PlayEffect at those coords can be coloured correctly.
    void RecordPendingCast(const uint32_t caster_agent_id, const uint32_t location_agent_id)
    {
        const auto caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(caster_agent_id));
        const int src = SourceOf(caster);
        if (src < 0) return;
        const GW::Agent* loc = GW::Agents::GetAgentByID(location_agent_id ? location_agent_id : caster_agent_id);
        if (!loc) return;
        const auto now = TIMER_INIT();
        std::erase_if(pending_casts, [now](const PendingCast& p) { return now - p.time > kPendingCastTtl; });
        pending_casts.push_back({{loc->pos.x, loc->pos.y}, src == 1, now});
        if (pending_casts.size() > kMaxPendingCasts) pending_casts.erase(pending_casts.begin());
    }

    // Attribute a caster-less effect at `pos` to the nearest recent skill cast. 1 = enemy, 0 = ally, -1 = none.
    int MatchPendingCast(const GW::Vec2f& pos)
    {
        int best = -1;
        float best_d = GW::Constants::SqrRange::Area;
        const auto now = TIMER_INIT();
        for (const auto& pc : pending_casts) {
            if (now - pc.time > kPendingCastTtl) continue;
            const float d = GetSquareDistance(pos, pc.pos);
            if (d <= best_d) {
                best_d = d;
                best = pc.from_enemy ? 1 : 0;
            }
        }
        return best;
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
        if (!InExplorable()) {
            return;
        }
        std::scoped_lock lock(effects_mutex); // the settings table can be rebuilt from the render thread
        if (pak->value_id == GW::Packet::StoC::GenericValueID::skill_activated) {
            RecordPendingCast(pak->agent_id, 0); // no victim in this variant - land at the caster
            return;
        }
        if (pak->value_id != GW::Packet::StoC::GenericValueID::effect_on_agent) {
            return;
        }
        const auto it = effect_settings.find(pak->value);
        if (it == effect_settings.end()) {
            return;
        }
        const auto settings = it->second;
        if (settings->stoc_header && settings->stoc_header != pak->header) {
            return;
        }
        const auto* caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
        const int src = SourceOf(caster);
        if (src < 0) {
            return;
        }
        AddEffect(pak->value, caster->pos.x, caster->pos.y, caster->pos.zplane, src == 1, settings);
    }

    void OnGenericValueTarget(GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* pak)
    {
        if (!InExplorable()) {
            return;
        }
        std::scoped_lock lock(effects_mutex);
        if (pak->Value_id == GW::Packet::StoC::GenericValueID::skill_activated) {
            // skill_activated swaps the fields: the actual caster is pak->target, the victim is pak->caster.
            RecordPendingCast(pak->target, pak->caster);
            return;
        }
        if (pak->Value_id != GW::Packet::StoC::GenericValueID::effect_on_target) {
            return;
        }
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
        const int src = SourceOf(caster);
        if (src < 0) {
            return;
        }
        const GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
        if (!target) {
            return;
        }
        AddEffect(pak->value, target->pos.x, target->pos.y, target->pos.zplane, src == 1, settings);
    }

    void OnPlayEffect(GW::HookStatus*, GW::Packet::StoC::PlayEffect* pak)
    {
        if (!InExplorable()) {
            return;
        }
        std::scoped_lock lock(effects_mutex);
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
        int src = SourceOf(a);
        if (src < 0) {
            // Fire Storm / Meteor Shower carry no caster in their PlayEffect; attribute by matching the
            // landing coords to a recent skill cast. Unknown -> show as enemy danger rather than drop it.
            src = MatchPendingCast(pak->coords);
            if (src < 0) src = 1;
        }
        AddEffect(pak->effect_id, pak->coords.x, pak->coords.y, pak->plane, src == 1, settings);
    }
} // namespace

Color AoeEffects::color_enemy = Colors::ARGB(51, 235, 40, 40); // red, ~20% alpha
Color AoeEffects::color_ally = Colors::ARGB(0, 40, 210, 70);   // green, hidden by default (alpha 0)

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
    std::scoped_lock lock(effects_mutex);
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
    std::scoped_lock lock(effects_mutex);
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
    effect_settings.emplace(Lava_font_overcast, new EffectSettings{0xFFFF0000, "Lava Font (overcast)", Lava_font_overcast, GW::Constants::Range::Nearby, 0, 5000});
    effect_settings.emplace(Churning_earth, new EffectSettings{0xFFFF0000, "Churning Earth", Churning_earth, GW::Constants::Range::Nearby, 0, 5000});
    // Caster-less ground showers - each fires a PlayEffect per wave; a short duration + wave dedup keeps one ring.
    effect_settings.emplace(Meteor_Shower, new EffectSettings{0xFFFF0000, "Meteor Shower", Meteor_Shower, GW::Constants::Range::Adjacent, 0, 4000});
    effect_settings.emplace(Fire_Storm, new EffectSettings{0xFFFF0000, "Fire Storm", Fire_Storm, GW::Constants::Range::Adjacent, 0, 4000});
    effect_settings.emplace(Bed_of_coals, new EffectSettings{0xFFFF0000, "Bed of Coals", Bed_of_coals, GW::Constants::Range::Adjacent, 0, 5000});

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
    std::scoped_lock lock(effects_mutex);
    std::erase_if(effects, [](const EffectRecord& e) {
        return TIMER_DIFF(e.start) > static_cast<clock_t>(e.duration);
    });
    out.reserve(effects.size());
    for (const auto& e : effects) {
        out.push_back({e.uid, e.effect_id, e.pos, e.zplane, e.start, e.duration, e.range,
                       e.from_enemy ? color_enemy : color_ally, e.from_enemy});
    }
}

void AoeEffects::Clear()
{
    std::scoped_lock lock(effects_mutex);
    effects.clear();
    pending_casts.clear();
    for (const auto& trigger : effect_triggers) {
        trigger.second->triggers_handled.clear();
    }
}

bool AoeEffects::DrawColorSettings()
{
    bool changed = false;
    changed |= Colors::DrawSettingHueWheel("Enemy AoE", &color_enemy, ImGuiColorEditFlags_AlphaBar);
    changed |= Colors::DrawSettingHueWheel("Our AoE", &color_ally, ImGuiColorEditFlags_AlphaBar);
    ImGui::TextDisabled("Alpha 0 hides that side. Drives both the minimap circles and the in-world danger rings.");
    return changed;
}

void AoeEffects::LoadColorSettings(const SettingsDoc& doc, const ToolboxIni* legacy, const char* section)
{
    Colors::SettingColor enemy(color_enemy), ally(color_ally);
    if (doc.Get(section, "color_aoe_enemy", enemy)) color_enemy = enemy.value;
    else if (legacy) color_enemy = Colors::Load(legacy, section, "color_aoe_enemy", color_enemy);
    if (doc.Get(section, "color_aoe_ally", ally)) color_ally = ally.value;
    else if (legacy) color_ally = Colors::Load(legacy, section, "color_aoe_ally", color_ally);
}

void AoeEffects::SaveColorSettings(SettingsDoc& doc, const char* section)
{
    doc.Set(section, "color_aoe_enemy", Colors::SettingColor(color_enemy));
    doc.Set(section, "color_aoe_ally", Colors::SettingColor(color_ally));
}
