#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include <Logger.h>
#include <Timer.h>
#include <d3d9.h>
#include <cmath>
#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Widgets/PartyDamage.h>
#include <Windows/EnemyStatisticsWindow.h>
#include <Utils/TextUtils.h>

namespace GW {
    namespace Agents {
        void AsyncGetAgentName(uint32_t agent_id, std::wstring& out);
        void AsyncGetAgentName(const Agent* agent, std::wstring& out);
    }
}

namespace {
    GW::HookEntry MapLoaded_Entry;
    GW::HookEntry GenericValueSelf_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry AddEffect_Entry;
    GW::HookEntry RemoveEffect_Entry;
    GW::HookEntry ProjectileLaunched_Entry;

    constexpr wchar_t NONE_PLAYER_NAME[] = L"Hero/Henchman Slot";
    constexpr uint32_t NONE_SKILL = static_cast<uint32_t>(GW::Constants::SkillID::No_Skill);
    constexpr wchar_t UNKNOWN_SKILL_NAME[] = L"Unknown Skill";
    constexpr wchar_t UNKNOWN_PLAYER_NAME[] = L"Unknown Player";

    std::unordered_map<GW::Constants::SkillID, std::unique_ptr<GuiUtils::EncString>> skill_names;

    GuiUtils::EncString* GetSkillName(const GW::Constants::SkillID skill_id)
    {
        const auto found_it = skill_names.find(skill_id);
        if (found_it == skill_names.end()) {
            const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            ASSERT(skill_data);
            skill_names[skill_id] = std::make_unique<GuiUtils::EncString>(skill_data->name);
        }
        return skill_names[skill_id].get();
    }

    IDirect3DTexture9* GetSkillImage(const GW::Constants::SkillID skill_id)
    {
        return *Resources::GetSkillImage(skill_id);
    }

    struct Skill {
        Skill(const GW::Constants::SkillID _id)
            : id(_id)
        {
            if (id != GW::Constants::SkillID::No_Skill) {
                name = GetSkillName(id);
                name->wstring();
            }
        }

        GW::Constants::SkillID id = static_cast<GW::Constants::SkillID>(0);
        uint32_t count = 0;
        uint32_t ended = 0;
        uint32_t attributed = 0;
        uint32_t damage = 0;
        uint32_t healing = 0;
        GuiUtils::EncString* name = nullptr;
    };

    struct PartyMember {
        PartyMember(const wchar_t* _name_enc, const uint32_t _agent_id, const uint32_t _party_idx)
            : name_enc(_name_enc), agent_id(_agent_id), party_idx(_party_idx)
        {
            name.reset(name_enc.c_str());
            name.wstring();
        }

        std::wstring name_enc;
        uint32_t agent_id = 0;
        uint32_t total_skills_used = 0;
        uint32_t party_idx = 0;
        GuiUtils::EncString name;
        std::vector<Skill> skills{};
        std::vector<Skill> incoming_skills{};
        uint32_t autoattack_count = 0;
        uint32_t autoattack_damage = 0;
        uint32_t autoattack_healing = 0;
        uint32_t unattributable_count = 0;
        uint32_t unattributable_damage = 0;
        uint32_t unattributable_healing = 0;
    };

    struct SavedSkillStats {
        GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
        uint32_t count = 0;
        uint32_t ended = 0;
        uint32_t attributed = 0;
        uint32_t damage = 0;
        uint32_t healing = 0;
    };

    struct SavedPartyMemberStats {
        uint32_t party_idx = 0;
        uint32_t total_skills_used = 0;
        uint32_t autoattack_count = 0;
        uint32_t autoattack_damage = 0;
        uint32_t autoattack_healing = 0;
        uint32_t unattributable_count = 0;
        uint32_t unattributable_damage = 0;
        uint32_t unattributable_healing = 0;
        std::vector<SavedSkillStats> skills;
        std::vector<SavedSkillStats> incoming_skills;
    };

    struct EnemyGroup {
        std::wstring name;
        uint32_t caster_id = 0;
        uint32_t player_number = 0;
        std::vector<Skill> skills;
        uint32_t autoattack_count = 0;
        uint32_t autoattack_damage = 0;
        uint32_t autoattack_healing = 0;
        uint32_t unattributable_count = 0;
        uint32_t unattributable_damage = 0;
        uint32_t unattributable_healing = 0;
    };

    std::vector<PartyMember*> party_members;
    std::vector<std::unique_ptr<EnemyGroup>> enemy_groups;
    std::unordered_map<uint32_t, EnemyGroup*> enemy_groups_by_caster_id;
    std::unordered_map<uint32_t, EnemyGroup*> enemy_groups_by_player_number;
    bool pending_party_members = true;
    bool in_explorable = false;
    PartyMember* player_party_member = nullptr;

    bool show_abs_values = true;
    bool show_perc_values = true;

    enum class ActionType {
        Skill,
        AttackSkill,
        Autoattack,
    };

    struct TrackedAction {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t target_id = 0;
        uint32_t timestamp = 0;
        ActionType type = ActionType::Skill;
        bool attributed = false;
    };

    struct SkillCastState {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t caster_id = 0;
        uint32_t target_id = 0;
        uint32_t timestamp = 0;
        uint32_t finish_timestamp = 0;
        bool finished = false;
        bool cancelled = false;
        bool attributed = false;
    };

    struct EffectTracker {
        uint32_t effect_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t cause_agent_id = 0;
        uint32_t timestamp = 0;
        uint32_t cast_timestamp = 0;
        bool active = true;
        bool attributed = false;
    };

    struct ProjectileTracker {
        uint32_t projectile_id = 0;
        uint32_t caster_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t target_id = 0;
        uint32_t timestamp = 0;
        uint32_t cast_timestamp = 0;
        uint32_t flight_time_ms = 0;
        uint32_t impact_timestamp = 0;
        uint32_t total_damage = 0;
        bool attributed = false;
        bool is_autoattack = false;
    };

    struct DamageEvent {
        uint32_t cause_id = 0;
        uint32_t target_id = 0;
        uint32_t type = 0;
        float value = 0.0f;
        uint32_t amount = 0;
        uint32_t timestamp = 0;
        bool handled = false;
        uint32_t group_player_number = 0;
        uint32_t group_caster_id = 0;
    };

    struct SkillEndEvent {
        uint32_t caster_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t target_id = 0;
        uint32_t timestamp = 0;
        bool is_real_end = false;
        bool is_dot_tick = false;
    };

    struct ProjectileImpactEvent {
        uint32_t projectile_id = 0;
        uint32_t caster_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t target_id = 0;
        bool is_autoattack = false;
        uint32_t impact_timestamp = 0;
    };

    struct DelayedSkillEffect {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t max_delay_ms = 0;
        uint32_t packet_type = 0;
    };

    struct DotSkillEffect {
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t packet_type = 0;
        uint32_t duration_ms = 0;
        std::vector<uint32_t> tick_offsets_ms;
    };

    const std::vector<DelayedSkillEffect> delayed_skill_effects = {
        {GW::Constants::SkillID::Mistrust, 6000, GW::Packet::StoC::GenericValueID::armorignoring},
        {GW::Constants::SkillID::Mistrust_PvP, 6000, GW::Packet::StoC::GenericValueID::armorignoring},
        {GW::Constants::SkillID::Clumsiness, 4000, GW::Packet::StoC::GenericValueID::armorignoring},
        {GW::Constants::SkillID::Wandering_Eye, 4000, GW::Packet::StoC::GenericValueID::armorignoring},
        {GW::Constants::SkillID::Ineptitude, 4000, GW::Packet::StoC::GenericValueID::armorignoring},
        {GW::Constants::SkillID::Incendiary_Bonds, 3000, GW::Packet::StoC::GenericValueID::armorignoring},
    };

    const std::vector<DotSkillEffect> dot_skill_effects = {
        {GW::Constants::SkillID::Agony, 0, 20000, {}},
        {GW::Constants::SkillID::Ancestors_Rage, 0, 0, {1000}},
        {GW::Constants::SkillID::Balthazars_Aura, 0, 10000, {}},
        {GW::Constants::SkillID::Bed_of_Coals, 0, 5000, {}},
        {GW::Constants::SkillID::Breath_of_Fire, 0, 5000, {}},
        {GW::Constants::SkillID::Chaos_Storm, 0, 10000, {}},
        {GW::Constants::SkillID::Churning_Earth, 0, 5000, {}},
        {GW::Constants::SkillID::Eruption, 0, 5000, {}},
        {GW::Constants::SkillID::Fire_Storm, 0, 10000, {}},
        {GW::Constants::SkillID::Glimmering_Mark, 0, 10000, {}},
        {GW::Constants::SkillID::Illusion_of_Pain, 0, 8000, {}},
        {GW::Constants::SkillID::Kirins_Wrath, 0, 5000, {}},
        {GW::Constants::SkillID::Lava_Font, 0, 5000, {}},
        {GW::Constants::SkillID::Maelstrom, 0, 10000, {}},
        {GW::Constants::SkillID::Meteor_Shower, 0, 0, {3000, 6000, 9000}},
        {GW::Constants::SkillID::Mystic_Sandstorm, 0, 3000, {}},
        {GW::Constants::SkillID::Ray_of_Judgment, 0, 5000, {}},
        {GW::Constants::SkillID::Renewing_Surge, 0, 8000, {}},
        {GW::Constants::SkillID::Sandstorm, 0, 10000, {}},
        {GW::Constants::SkillID::Savannah_Heat, 0, 5000, {}},
        {GW::Constants::SkillID::Searing_Heat, 0, 5000, {}},
        {GW::Constants::SkillID::Shatterstone, 0, 0, {1000, 3000}},
        {GW::Constants::SkillID::Spirit_Rift, 0, 0, {3000}},
        {GW::Constants::SkillID::Smoldering_Embers, 0, 3000, {}},
        {GW::Constants::SkillID::Snow_Storm, 0, 10000, {}},
        {GW::Constants::SkillID::Symbol_of_Wrath, 0, 5000, {}},
        {GW::Constants::SkillID::Unsteady_Ground, 0, 5000, {}},
        {GW::Constants::SkillID::Wastrels_Demise, 0, 5000, {}},
        {GW::Constants::SkillID::Wastrels_Worry, 0, 0, {1000, 3000}},
    };

    struct SkillCastTimelineEvent {
        uint32_t caster_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t target_id = 0;
        uint32_t start_timestamp = 0;
        uint32_t finish_timestamp = 0;
        bool finished = false;
        bool cancelled = false;
        bool is_autoattack = false;
    };

    struct ProjectileTimelineEvent {
        uint32_t projectile_id = 0;
        uint32_t caster_id = 0;
        GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill;
        uint32_t target_id = 0;
        uint32_t spawn_timestamp = 0;
        uint32_t impact_timestamp = 0;
        bool is_autoattack = false;
    };

    std::unordered_map<uint32_t, std::vector<TrackedAction>> tracked_actions;
    std::unordered_map<uint32_t, SkillCastState> last_skill_cast;
    std::vector<SkillCastTimelineEvent> recent_skill_cast_events;
    std::vector<ProjectileTimelineEvent> recent_projectile_timeline_events;
    std::unordered_map<uint32_t, std::vector<EffectTracker>> active_effects_by_agent;
    std::unordered_map<uint32_t, ProjectileTracker> active_projectiles;
    std::vector<SkillEndEvent> recent_skill_end_events;
    std::vector<ProjectileImpactEvent> recent_projectile_impacts;
    std::vector<DamageEvent> recent_damage_events;
    constexpr uint32_t effect_infer_window_ms = 300;
    constexpr uint32_t damage_attribution_delay_ms = 1000;
    constexpr uint32_t timeline_window_ms = 20000;
    constexpr size_t max_tracked_actions = 8;
    constexpr size_t max_party_skills = 8;

    uint32_t GetTimeMs()
    {
        return GetTickCount();
    }

    float TimestampToTimelineX(const uint32_t timestamp, const float left, const float right, const uint32_t now)
    {
        if (timestamp >= now) {
            return right;
        }
        const float t = std::clamp((now - timestamp) / static_cast<float>(timeline_window_ms), 0.0f, 1.0f);
        return right - t * (right - left);
    }

    Skill* GetOrAddSkill(PartyMember* party_member, const GW::Constants::SkillID skill_id);
    Skill* GetOrAddIncomingSkill(PartyMember* party_member, const GW::Constants::SkillID skill_id);
    Skill* GetOrAddEnemySkill(const uint32_t caster_id, const GW::Constants::SkillID skill_id);
    std::wstring GetEnemyName(const uint32_t caster_id);

    bool LogDamageMatchAttempt(const wchar_t* match_type, const DamageEvent& damage,
                               const uint32_t event_id, const uint32_t event_timestamp,
                               const bool result);
    bool IsCancelledSkillEnd(const SkillEndEvent& skill_end);
    const SkillEndEvent* FindBestDelayedSkillEndMatch(const DamageEvent& damage);

    TrackedAction* LookupActiveAutoattackAction(const uint32_t caster_id);
    void AddProjectileImpactEvent(const ProjectileTracker& tracker);
    void AddProjectileSpawnEvent(const ProjectileTracker& tracker);
    void ProcessPendingDamageEventsForProjectileImpact(const ProjectileImpactEvent& impact_event);

    PartyMember* GetPartyMemberByAgentId(const uint32_t agent_id);
    EnemyGroup* GetEnemyGroupByDamage(const DamageEvent& damage);
    const GW::Skillbar* GetAgentSkillbar(const uint32_t agent_id);

    std::vector<SavedPartyMemberStats> SavePartyMemberStats();
    void RestorePartyMemberStats(const std::vector<SavedPartyMemberStats>& saved_stats);
    void ExpireOldDamageEvents();
    bool TryAttributeDamageEvent(DamageEvent& damage, const GW::Packet::StoC::GenericModifier* packet);

    void AttributeDamageEvent(const DamageEvent& damage, const ProjectileImpactEvent& impact,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member);
    void AttributeDamageEvent(const DamageEvent& damage, const SkillEndEvent& skill_end,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member);

    uint32_t ToSkillValue(const float value)
    {
        return static_cast<uint32_t>(std::floor(std::fabs(value) + 0.5f));
    }

    uint32_t ToSkillValue(const GW::Packet::StoC::GenericModifier* packet)
    {
        if (!packet) {
            return 0;
        }
        const float abs_value = std::fabs(packet->value);
        if (abs_value > 1.0f) {
            return ToSkillValue(packet->value);
        }
        const auto agent = GW::Agents::GetAgentByID(packet->target_id);
        const auto target = agent ? agent->GetAsAgentLiving() : nullptr;
        if (target) {
            const uint32_t cached_max_hp = PartyDamage::GetMaxHp(target->player_number);
            if (cached_max_hp > 0) {
                return static_cast<uint32_t>(std::lround(abs_value * cached_max_hp));
            }
            if (target->max_hp > 0 && target->max_hp < 100000) {
                return static_cast<uint32_t>(std::lround(abs_value * target->max_hp));
            }
            const uint32_t approx_hp = target->level * 20 + 100;
            return static_cast<uint32_t>(std::lround(abs_value * approx_hp));
        }
        return ToSkillValue(packet->value);
    }

    IDirect3DTexture9* GetSkillImage(const GW::Constants::SkillID skill_id);

    bool IsPacketHealing(const GW::Packet::StoC::GenericModifier* packet)
    {
        return packet && packet->value > 0.0f;
    }

    void AddSkillValue(PartyMember* party_member, const GW::Constants::SkillID skill_id,
                       const GW::Packet::StoC::GenericModifier* packet, const bool is_healing,
                       const bool count_attributed)
    {
        if (!party_member || !packet) {
            return;
        }
        auto* skill = GetOrAddSkill(party_member, skill_id);
        if (!skill) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            skill->healing += amount;
        } else {
            skill->damage += amount;
        }
        if (count_attributed) {
            skill->attributed++;
        }
    }

    void AddIncomingSkillValue(PartyMember* party_member, const GW::Constants::SkillID skill_id,
                               const GW::Packet::StoC::GenericModifier* packet, const bool is_healing)
    {
        if (!party_member || !packet || skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        auto* skill = GetOrAddIncomingSkill(party_member, skill_id);
        if (!skill) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            skill->healing += amount;
        } else {
            skill->damage += amount;
        }
        skill->count++;
        skill->ended++;
        skill->attributed++;
    }

    void AddAutoattackValue(PartyMember* party_member, const GW::Packet::StoC::GenericModifier* packet,
                            const bool is_healing)
    {
        if (!party_member || !packet) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            party_member->autoattack_healing += amount;
        } else {
            party_member->autoattack_damage += amount;
        }
        party_member->autoattack_count++;
    }

    void AddEnemyAutoattackValue(EnemyGroup* group, const GW::Packet::StoC::GenericModifier* packet,
                                 const bool is_healing)
    {
        if (!group || !packet) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            group->autoattack_healing += amount;
        } else {
            group->autoattack_damage += amount;
        }
        group->autoattack_count++;
    }

    void AddUnattributableValue(PartyMember* party_member, const GW::Packet::StoC::GenericModifier* packet,
                                const bool is_healing)
    {
        if (!party_member || !packet) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            party_member->unattributable_healing += amount;
        } else {
            party_member->unattributable_damage += amount;
        }
        party_member->unattributable_count++;
    }

    void AddEnemyUnattributableValue(EnemyGroup* group, const GW::Packet::StoC::GenericModifier* packet,
                                     const bool is_healing)
    {
        if (!group || !packet) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            group->unattributable_healing += amount;
        } else {
            group->unattributable_damage += amount;
        }
        group->unattributable_count++;
    }

    void AddEnemySkillValue(EnemyGroup* group, const GW::Constants::SkillID skill_id,
                            const GW::Packet::StoC::GenericModifier* packet, const bool is_healing,
                            const bool count_attributed)
    {
        if (!group || !packet || skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        auto* skill = GetOrAddEnemySkill(group->caster_id, skill_id);
        if (!skill) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        if (is_healing) {
            skill->healing += amount;
        } else {
            skill->damage += amount;
        }
        if (count_attributed) {
            skill->attributed++;
        }
    }

    bool IsDamageOrHealingPacket(const GW::Packet::StoC::GenericModifier* packet)
    {
        if (!packet) {
            return false;
        }
        return packet->type == GW::Packet::StoC::GenericValueID::health ||
               packet->type == GW::Packet::StoC::GenericValueID::damage ||
               packet->type == GW::Packet::StoC::GenericValueID::critical ||
               packet->type == GW::Packet::StoC::GenericValueID::armorignoring;
    }

    bool IsDamageEvent(const DamageEvent& damage)
    {
        return damage.type == GW::Packet::StoC::GenericValueID::health ||
               damage.type == GW::Packet::StoC::GenericValueID::damage ||
               damage.type == GW::Packet::StoC::GenericValueID::critical ||
               damage.type == GW::Packet::StoC::GenericValueID::armorignoring;
    }

    bool IsDamageReadyForAttribution(const DamageEvent& damage)
    {
        const uint32_t now = GetTimeMs();
        return now >= damage.timestamp && now - damage.timestamp >= damage_attribution_delay_ms;
    }

    bool IsWithinInferWindow(const uint32_t event_timestamp, const uint32_t packet_timestamp)
    {
        const uint32_t diff = event_timestamp > packet_timestamp
                                  ? event_timestamp - packet_timestamp
                                  : packet_timestamp - event_timestamp;
        return diff <= effect_infer_window_ms;
    }

    void PruneOldTimelineEvents()
    {
        const uint32_t now = GetTimeMs();
        while (!recent_skill_end_events.empty() && now - recent_skill_end_events.front().timestamp > timeline_window_ms) {
            recent_skill_end_events.erase(recent_skill_end_events.begin());
        }
        while (!recent_projectile_impacts.empty()) {
            const auto& impact = recent_projectile_impacts.front();
            if (now <= impact.impact_timestamp) {
                break;
            }
            if (now - impact.impact_timestamp > timeline_window_ms) {
                recent_projectile_impacts.erase(recent_projectile_impacts.begin());
                continue;
            }
            break;
        }
        while (!recent_skill_cast_events.empty() && now - recent_skill_cast_events.front().start_timestamp > timeline_window_ms) {
            recent_skill_cast_events.erase(recent_skill_cast_events.begin());
        }
        while (!recent_projectile_timeline_events.empty() && now - recent_projectile_timeline_events.front().spawn_timestamp > timeline_window_ms &&
               now - recent_projectile_timeline_events.front().impact_timestamp > timeline_window_ms) {
            recent_projectile_timeline_events.erase(recent_projectile_timeline_events.begin());
        }
        ExpireOldDamageEvents();
    }

    EnemyGroup* GetEnemyGroupByPlayerNumber(const uint32_t player_number)
    {
        if (player_number == 0) {
            return nullptr;
        }
        const auto it = enemy_groups_by_player_number.find(player_number);
        return it == enemy_groups_by_player_number.end() ? nullptr : it->second;
    }

    EnemyGroup* GetEnemyGroupByCasterId(const uint32_t caster_id)
    {
        if (caster_id == 0) {
            return nullptr;
        }
        const auto it = enemy_groups_by_caster_id.find(caster_id);
        return it == enemy_groups_by_caster_id.end() ? nullptr : it->second;
    }

    void CacheEnemyGroup(EnemyGroup* group)
    {
        if (!group) {
            return;
        }
        if (group->caster_id != 0) {
            enemy_groups_by_caster_id[group->caster_id] = group;
        }
        if (group->player_number != 0) {
            enemy_groups_by_player_number[group->player_number] = group;
        }
    }

    EnemyGroup* GetOrAddEnemyGroup(const uint32_t caster_id)
    {
        if (caster_id == 0) {
            return nullptr;
        }

        const auto* agent = GW::Agents::GetAgentByID(caster_id);
        uint32_t player_number = 0;
        if (agent) {
            const auto* living = agent->GetAsAgentLiving();
            if (living) {
                if (living->allegiance != GW::Constants::Allegiance::Enemy) {
                    return nullptr;
                }
                player_number = living->player_number;
            }
        }

        if (auto* caster_group = GetEnemyGroupByCasterId(caster_id)) {
            if (player_number != 0 && caster_group->player_number == 0) {
                caster_group->player_number = player_number;
                CacheEnemyGroup(caster_group);
            }
            return caster_group;
        }

        if (player_number != 0) {
            if (auto* player_number_group = GetEnemyGroupByPlayerNumber(player_number)) {
                if (player_number_group->caster_id == 0 || player_number_group->caster_id == caster_id) {
                    if (player_number_group->caster_id == 0) {
                        player_number_group->caster_id = caster_id;
                    }
                    CacheEnemyGroup(player_number_group);
                    return player_number_group;
                }
            }
            for (auto& group_ptr : enemy_groups) {
                auto* candidate_group = group_ptr.get();
                if (candidate_group->player_number == player_number && (candidate_group->caster_id == 0 || candidate_group->caster_id == caster_id)) {
                    if (candidate_group->caster_id == 0) {
                        candidate_group->caster_id = caster_id;
                    }
                    CacheEnemyGroup(candidate_group);
                    if (player_number != 0 && candidate_group->player_number == 0) {
                        candidate_group->player_number = player_number;
                        CacheEnemyGroup(candidate_group);
                    }
                    return candidate_group;
                }
            }
        }

        EnemyGroup new_group;
        new_group.caster_id = caster_id;
        new_group.player_number = player_number;
        new_group.name = GetEnemyName(caster_id);
        new_group.skills.reserve(8);
        for (int i = 0; i < 8; ++i) {
            new_group.skills.emplace_back(GW::Constants::SkillID::No_Skill);
        }
        enemy_groups.emplace_back(std::make_unique<EnemyGroup>(std::move(new_group)));
        CacheEnemyGroup(enemy_groups.back().get());
        return enemy_groups.back().get();
    }

    Skill* GetOrAddEnemySkill(const uint32_t caster_id, const GW::Constants::SkillID skill_id)
    {
        if (skill_id == GW::Constants::SkillID::No_Skill) {
            return nullptr;
        }
        auto* group = GetOrAddEnemyGroup(caster_id);
        if (!group) {
            return nullptr;
        }
        for (auto& skill : group->skills) {
            if (skill.id == skill_id) {
                return &skill;
            }
        }
        for (auto& skill : group->skills) {
            if (skill.id == GW::Constants::SkillID::No_Skill) {
                skill = Skill(skill_id);
                return &skill;
            }
        }
        group->skills.emplace_back(skill_id);
        return &group->skills.back();
    }

    Skill* GetOrAddIncomingSkill(PartyMember* party_member, const GW::Constants::SkillID skill_id)
    {
        if (!party_member) {
            return nullptr;
        }
        for (auto& skill : party_member->incoming_skills) {
            if (skill.id == skill_id) {
                return &skill;
            }
        }
        party_member->incoming_skills.emplace_back(skill_id);
        return &party_member->incoming_skills.back();
    }

    Skill* GetOrAddSkill(PartyMember* party_member, const GW::Constants::SkillID skill_id)
    {
        for (auto& skill : party_member->skills) {
            if (skill.id == skill_id) {
                return &skill;
            }
        }
        return nullptr;
    }

    std::wstring GetEnemyName(const uint32_t caster_id)
    {
        if (!caster_id) {
            return std::wstring(UNKNOWN_PLAYER_NAME);
        }
        std::wstring name;
        GW::Agents::AsyncGetAgentName(caster_id, name);
        if (name.empty()) {
            return std::wstring(UNKNOWN_PLAYER_NAME);
        }
        const auto sanitized = TextUtils::SanitizePlayerName(name);
        return sanitized.empty() ? std::wstring(UNKNOWN_PLAYER_NAME) : sanitized;
    }

    bool IsDamageFromSameEnemyGroup(const DamageEvent& damage, const uint32_t caster_id)
    {
        if (damage.cause_id == caster_id) {
            return true;
        }

        const auto* caster_agent = GW::Agents::GetAgentByID(caster_id);
        const auto* caster_living = caster_agent ? caster_agent->GetAsAgentLiving() : nullptr;
        const auto* cause_agent = GW::Agents::GetAgentByID(damage.cause_id);
        const auto* cause_living = cause_agent ? cause_agent->GetAsAgentLiving() : nullptr;

        if (caster_living && cause_living && caster_living->player_number != 0 &&
            caster_living->player_number == cause_living->player_number &&
            caster_living->allegiance == GW::Constants::Allegiance::Enemy &&
            cause_living->allegiance == GW::Constants::Allegiance::Enemy) {
            return true;
        }

        if (damage.group_player_number != 0 && caster_living && caster_living->player_number == damage.group_player_number) {
            return true;
        }

        if (damage.group_caster_id != 0) {
            if (auto* group = GetEnemyGroupByCasterId(damage.group_caster_id)) {
                if (group->player_number != 0 && caster_living && caster_living->player_number == group->player_number) {
                    return true;
                }
                if (group->caster_id != 0 && caster_id == group->caster_id) {
                    return true;
                }
            }
        }

        if (caster_living && caster_living->player_number != 0) {
            if (auto* group = GetEnemyGroupByPlayerNumber(caster_living->player_number)) {
                if (damage.group_caster_id != 0 && group->caster_id == damage.group_caster_id) {
                    return true;
                }
            }
        }

        return false;
    }

    bool MatchDamageEventToProjectile(const DamageEvent& damage, const ProjectileImpactEvent& impact)
    {
        if (damage.type == GW::Packet::StoC::GenericValueID::armorignoring) {
            return LogDamageMatchAttempt(L"projectile", damage, impact.projectile_id, impact.impact_timestamp, false);
        }
        if (!IsDamageFromSameEnemyGroup(damage, impact.caster_id) && !impact.is_autoattack) {
            return LogDamageMatchAttempt(L"projectile", damage, impact.projectile_id, impact.impact_timestamp, false);
        }
        const bool result = IsWithinInferWindow(damage.timestamp, impact.impact_timestamp);
        return LogDamageMatchAttempt(L"projectile", damage, impact.projectile_id, impact.impact_timestamp, result);
    }

    bool MatchDamageEventToSkillEnd(const DamageEvent& damage, const SkillEndEvent& skill_end)
    {
        if (!skill_end.is_real_end && !skill_end.is_dot_tick) {
            return LogDamageMatchAttempt(L"skillend", damage, skill_end.caster_id, skill_end.timestamp, false);
        }
        if (!IsDamageFromSameEnemyGroup(damage, skill_end.caster_id)) {
            return LogDamageMatchAttempt(L"skillend", damage, skill_end.caster_id, skill_end.timestamp, false);
        }
        if (IsCancelledSkillEnd(skill_end)) {
            return LogDamageMatchAttempt(L"skillend", damage, skill_end.caster_id, skill_end.timestamp, false);
        }
        const bool result = IsWithinInferWindow(damage.timestamp, skill_end.timestamp);
        return LogDamageMatchAttempt(L"skillend", damage, skill_end.caster_id, skill_end.timestamp, result);
    }

    struct DamageMatchResult {
        const ProjectileImpactEvent* impact = nullptr;
        const SkillEndEvent* skill_end = nullptr;
        uint32_t diff = UINT32_MAX;
    };

    bool IsCancelledSkillEnd(const SkillEndEvent& skill_end)
    {
        for (const auto& cast : recent_skill_cast_events) {
            if (!cast.cancelled || cast.caster_id != skill_end.caster_id || cast.skill_id != skill_end.skill_id) {
                continue;
            }
            if (cast.start_timestamp <= skill_end.timestamp &&
                (cast.finish_timestamp == 0 || skill_end.timestamp <= cast.finish_timestamp + effect_infer_window_ms)) {
                return true;
            }
        }
        return false;
    }

    DamageMatchResult FindBestDamageMatch(const DamageEvent& damage)
    {
        const ProjectileImpactEvent* best_impact = nullptr;
        const SkillEndEvent* best_skill_end = nullptr;
        uint32_t best_impact_diff = UINT32_MAX;
        uint32_t best_skill_end_diff = UINT32_MAX;

        for (const auto& impact_event : recent_projectile_impacts) {
            if (!MatchDamageEventToProjectile(damage, impact_event)) {
                continue;
            }
            const uint32_t diff = damage.timestamp > impact_event.impact_timestamp
                                      ? damage.timestamp - impact_event.impact_timestamp
                                      : impact_event.impact_timestamp - damage.timestamp;
            if (diff < best_impact_diff) {
                best_impact_diff = diff;
                best_impact = &impact_event;
            }
        }
        for (const auto& skill_end_event : recent_skill_end_events) {
            if (!MatchDamageEventToSkillEnd(damage, skill_end_event)) {
                continue;
            }
            const uint32_t diff = damage.timestamp > skill_end_event.timestamp
                                      ? damage.timestamp - skill_end_event.timestamp
                                      : skill_end_event.timestamp - damage.timestamp;
            if (diff < best_skill_end_diff) {
                best_skill_end_diff = diff;
                best_skill_end = &skill_end_event;
            }
        }

        const uint32_t best_diff = std::min(best_impact_diff, best_skill_end_diff);
        if (best_diff <= effect_infer_window_ms) {
            const bool prefer_impact = damage.type != GW::Packet::StoC::GenericValueID::armorignoring &&
                                       damage.amount < 25 && best_impact != nullptr;
            if (prefer_impact || (best_impact && best_impact_diff <= best_skill_end_diff)) {
                return {best_impact, nullptr, best_diff};
            }
            return {nullptr, best_skill_end, best_diff};
        }

        if (const SkillEndEvent* delayed_skill_end = FindBestDelayedSkillEndMatch(damage)) {
            const uint32_t delay = damage.timestamp > delayed_skill_end->timestamp
                                       ? damage.timestamp - delayed_skill_end->timestamp
                                       : delayed_skill_end->timestamp - damage.timestamp;
            return {nullptr, delayed_skill_end, delay};
        }

        return {};
    }

    bool MatchDamageEventToProjectileIgnoringGroup(const DamageEvent& damage, const ProjectileImpactEvent& impact)
    {
        if (damage.type == GW::Packet::StoC::GenericValueID::armorignoring) {
            return false;
        }
        if (impact.target_id != 0 && damage.target_id != 0 && damage.target_id != impact.target_id) {
            return false;
        }
        return true;
    }

    bool MatchDamageEventToSkillEndIgnoringGroup(const DamageEvent& damage, const SkillEndEvent& skill_end)
    {
        if (!skill_end.is_real_end && !skill_end.is_dot_tick) {
            return false;
        }
        if (IsCancelledSkillEnd(skill_end)) {
            return false;
        }
        if (skill_end.target_id != 0 && damage.target_id != 0 && damage.target_id != skill_end.target_id) {
            return false;
        }
        return true;
    }

    const SkillEndEvent* FindBestDelayedSkillEndMatchIgnoringGroup(const DamageEvent& damage)
    {
        const SkillEndEvent* best_skill_end = nullptr;
        uint32_t best_delay = UINT32_MAX;
        for (const auto& skill_end : recent_skill_end_events) {
            for (const auto& delayed_skill : delayed_skill_effects) {
                if (!MatchDamageEventToSkillEndIgnoringGroup(damage, skill_end)) {
                    continue;
                }
                if (skill_end.skill_id != delayed_skill.skill_id) {
                    continue;
                }
                if (damage.type != delayed_skill.packet_type) {
                    continue;
                }
                if (damage.timestamp < skill_end.timestamp) {
                    continue;
                }
                const uint32_t delay = damage.timestamp - skill_end.timestamp;
                if (delay > delayed_skill.max_delay_ms) {
                    continue;
                }
                if (delay < best_delay) {
                    best_delay = delay;
                    best_skill_end = &skill_end;
                }
            }
        }
        return best_skill_end;
    }

    DamageMatchResult FindBestDamageMatchIgnoringGroup(const DamageEvent& damage)
    {
        const ProjectileImpactEvent* best_impact = nullptr;
        const SkillEndEvent* best_skill_end = nullptr;
        uint32_t best_impact_diff = UINT32_MAX;
        uint32_t best_skill_end_diff = UINT32_MAX;

        for (const auto& impact_event : recent_projectile_impacts) {
            if (!MatchDamageEventToProjectileIgnoringGroup(damage, impact_event)) {
                continue;
            }
            const uint32_t diff = damage.timestamp > impact_event.impact_timestamp
                                      ? damage.timestamp - impact_event.impact_timestamp
                                      : impact_event.impact_timestamp - damage.timestamp;
            if (diff < best_impact_diff) {
                best_impact_diff = diff;
                best_impact = &impact_event;
            }
        }
        for (const auto& skill_end_event : recent_skill_end_events) {
            if (!MatchDamageEventToSkillEndIgnoringGroup(damage, skill_end_event)) {
                continue;
            }
            const uint32_t diff = damage.timestamp > skill_end_event.timestamp
                                      ? damage.timestamp - skill_end_event.timestamp
                                      : skill_end_event.timestamp - damage.timestamp;
            if (diff < best_skill_end_diff) {
                best_skill_end_diff = diff;
                best_skill_end = &skill_end_event;
            }
        }

        const uint32_t best_diff = std::min(best_impact_diff, best_skill_end_diff);
        if (best_diff != UINT32_MAX) {
            const bool prefer_impact = damage.type != GW::Packet::StoC::GenericValueID::armorignoring &&
                                       damage.amount < 25 && best_impact != nullptr;
            if (prefer_impact || (best_impact && best_impact_diff <= best_skill_end_diff)) {
                return {best_impact, nullptr, best_diff};
            }
            return {nullptr, best_skill_end, best_diff};
        }
        return {};
    }

    bool IsDelayedSkillMatch(const DamageEvent& damage, const SkillEndEvent& skill_end,
                             const DelayedSkillEffect& delayed_skill)
    {
        if (!skill_end.is_real_end) {
            return false;
        }
        if (skill_end.skill_id != delayed_skill.skill_id) {
            return false;
        }
        if (damage.type != delayed_skill.packet_type) {
            return false;
        }
        if (damage.cause_id != skill_end.caster_id) {
            return false;
        }
        if (skill_end.target_id != 0 && damage.target_id != 0 && damage.target_id != skill_end.target_id) {
            return false;
        }
        if (damage.timestamp < skill_end.timestamp) {
            return false;
        }
        const uint32_t delay = damage.timestamp - skill_end.timestamp;
        return delay <= delayed_skill.max_delay_ms;
    }

    const SkillEndEvent* FindBestDelayedSkillEndMatch(const DamageEvent& damage)
    {
        const SkillEndEvent* best_skill_end = nullptr;
        uint32_t best_delay = UINT32_MAX;
        for (const auto& skill_end : recent_skill_end_events) {
            for (const auto& delayed_skill : delayed_skill_effects) {
                if (!IsDelayedSkillMatch(damage, skill_end, delayed_skill)) {
                    continue;
                }
                const uint32_t delay = damage.timestamp - skill_end.timestamp;
                if (delay < best_delay) {
                    best_delay = delay;
                    best_skill_end = &skill_end;
                }
            }
        }
        return best_skill_end;
    }

    const SkillEndEvent* FindBestDelayedSkillEndMatch(const DamageEvent& damage, const EnemyGroup& group)
    {
        auto GroupMatchesCasterId = [&](const uint32_t caster_id) {
            if (group.player_number != 0) {
                const auto* agent = GW::Agents::GetAgentByID(caster_id);
                if (!agent) {
                    return false;
                }
                const auto* living = agent->GetAsAgentLiving();
                return living && living->player_number == group.player_number;
            }
            return caster_id == group.caster_id;
        };

        const SkillEndEvent* best_skill_end = nullptr;
        uint32_t best_delay = UINT32_MAX;
        for (const auto& skill_end : recent_skill_end_events) {
            if (!GroupMatchesCasterId(skill_end.caster_id)) {
                continue;
            }
            for (const auto& delayed_skill : delayed_skill_effects) {
                if (!skill_end.is_real_end) {
                    continue;
                }
                if (skill_end.skill_id != delayed_skill.skill_id) {
                    continue;
                }
                if (damage.type != delayed_skill.packet_type) {
                    continue;
                }
                if (skill_end.target_id != 0 && damage.target_id != 0 && damage.target_id != skill_end.target_id) {
                    continue;
                }
                if (damage.timestamp < skill_end.timestamp) {
                    continue;
                }
                const uint32_t delay = damage.timestamp - skill_end.timestamp;
                if (delay > delayed_skill.max_delay_ms) {
                    continue;
                }
                if (delay < best_delay) {
                    best_delay = delay;
                    best_skill_end = &skill_end;
                }
            }
        }
        return best_skill_end;
    }

    DamageMatchResult FindBestDamageMatch(const DamageEvent& damage, const EnemyGroup& group)
    {
        auto GroupMatchesCasterId = [&](const uint32_t caster_id) {
            if (group.player_number != 0) {
                const auto* agent = GW::Agents::GetAgentByID(caster_id);
                if (!agent) {
                    return false;
                }
                const auto* living = agent->GetAsAgentLiving();
                return living && living->player_number == group.player_number;
            }
            return caster_id == group.caster_id;
        };

        const ProjectileImpactEvent* best_impact = nullptr;
        const SkillEndEvent* best_skill_end = nullptr;
        uint32_t best_impact_diff = UINT32_MAX;
        uint32_t best_skill_end_diff = UINT32_MAX;

        for (const auto& impact_event : recent_projectile_impacts) {
            if (!GroupMatchesCasterId(impact_event.caster_id)) {
                continue;
            }
            if (!MatchDamageEventToProjectile(damage, impact_event)) {
                continue;
            }
            const uint32_t diff = damage.timestamp > impact_event.impact_timestamp
                                      ? damage.timestamp - impact_event.impact_timestamp
                                      : impact_event.impact_timestamp - damage.timestamp;
            if (diff < best_impact_diff) {
                best_impact_diff = diff;
                best_impact = &impact_event;
            }
        }
        for (const auto& skill_end_event : recent_skill_end_events) {
            if (!GroupMatchesCasterId(skill_end_event.caster_id)) {
                continue;
            }
            if (!MatchDamageEventToSkillEnd(damage, skill_end_event)) {
                continue;
            }
            const uint32_t diff = damage.timestamp > skill_end_event.timestamp
                                      ? damage.timestamp - skill_end_event.timestamp
                                      : skill_end_event.timestamp - damage.timestamp;
            if (diff < best_skill_end_diff) {
                best_skill_end_diff = diff;
                best_skill_end = &skill_end_event;
            }
        }

        const uint32_t best_diff = std::min(best_impact_diff, best_skill_end_diff);
        if (best_diff <= effect_infer_window_ms) {
            const bool prefer_impact = damage.type != GW::Packet::StoC::GenericValueID::armorignoring &&
                                       damage.amount < 25 && best_impact != nullptr;
            if (prefer_impact || (best_impact && best_impact_diff <= best_skill_end_diff)) {
                return {best_impact, nullptr, best_diff};
            }
            return {nullptr, best_skill_end, best_diff};
        }

        if (const SkillEndEvent* delayed_skill_end = FindBestDelayedSkillEndMatch(damage, group)) {
            const uint32_t delay = damage.timestamp > delayed_skill_end->timestamp
                                       ? damage.timestamp - delayed_skill_end->timestamp
                                       : delayed_skill_end->timestamp - damage.timestamp;
            return {nullptr, delayed_skill_end, delay};
        }

        return FindBestDamageMatchIgnoringGroup(damage);
    }

    bool IsDelayedSkillMatch(const DamageEvent& damage, const SkillEndEvent& skill_end,
                             const DelayedSkillEffect& delayed_skill);

    void AddEffectTracker(const uint32_t target_id, const EffectTracker &tracker)
    {
        if (tracker.effect_id == 0 || tracker.skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        auto tracker_copy = tracker;
        tracker_copy.timestamp = GetTimeMs();
        active_effects_by_agent[target_id].push_back(tracker_copy);
    }

    float GetProjectileAirTime(const uint32_t raw_value)
    {
        union {
            uint32_t integer;
            float value;
        } converter;
        converter.integer = raw_value;
        return converter.value;
    }

    uint32_t GetProjectileFlightTimeMs(const GW::Packet::StoC::AgentProjectileLaunched* packet)
    {
        if (!packet) {
            return 0;
        }
        const float air_time = GetProjectileAirTime(packet->unk2);
        if (air_time <= 0.0f) {
            return 0;
        }
        return static_cast<uint32_t>(std::max(0.0f, air_time * 1000.0f));
    }

    float GetProjectileDistance(const uint32_t caster_id, const uint32_t target_id,
                                const GW::Packet::StoC::AgentProjectileLaunched* packet)
    {
        const auto* caster = GW::Agents::GetAgentByID(caster_id);
        if (!caster || !packet) {
            return 0.0f;
        }

        const GW::Vec2f caster_pos = { caster->x, caster->y };
        if (target_id != 0) {
            const auto* target = GW::Agents::GetAgentByID(target_id);
            if (target) {
                return GW::GetDistance(caster_pos, GW::Vec2f{target->x, target->y});
            }
        }

        return GW::GetDistance(caster_pos, packet->destination);
    }

    void AddProjectileTracker(const GW::Packet::StoC::AgentProjectileLaunched* packet)
    {
        if (!packet) {
            return;
        }
        const uint32_t projectile_id = packet->unk4;
        if (projectile_id == 0) {
            return;
        }
        const uint32_t caster_id = packet->agent_id;
        const auto skill_it = last_skill_cast.find(caster_id);
        const uint32_t now = GetTimeMs();
        const uint32_t last_finish_timestamp = (skill_it != last_skill_cast.end() && skill_it->second.finished)
                                                    ? skill_it->second.finish_timestamp
                                                    : 0;
        const bool is_autoattack = (skill_it == last_skill_cast.end() || last_finish_timestamp == 0 || now - last_finish_timestamp > effect_infer_window_ms);

        const auto skill_id = is_autoattack ? GW::Constants::SkillID::No_Skill
                                            : skill_it->second.skill_id;
        const uint32_t cast_timestamp = is_autoattack ? 0 : last_finish_timestamp;
        uint32_t target_id = (skill_it != last_skill_cast.end() ? skill_it->second.target_id : 0);
        if (is_autoattack) {
            const auto* autoattack_action = LookupActiveAutoattackAction(caster_id);
            if (autoattack_action) {
                target_id = autoattack_action->target_id;
            }
        }

        const uint32_t timestamp = GetTimeMs();
        uint32_t flight_time_ms = GetProjectileFlightTimeMs(packet);
        if (flight_time_ms == 0) {
            const float distance = GetProjectileDistance(caster_id, target_id, packet);
            if (distance > 0.0f) {
                constexpr float fallback_projectile_speed = 1600.0f; // units per second
                flight_time_ms = static_cast<uint32_t>(std::max(0.0f, distance / fallback_projectile_speed * 1000.0f));
            }
        }
        const uint32_t impact_timestamp = flight_time_ms > 0 ? timestamp + flight_time_ms : timestamp;

        active_projectiles[projectile_id] = {projectile_id, caster_id, skill_id, target_id, timestamp,
                                             cast_timestamp, flight_time_ms, impact_timestamp, 0, false,
                                             is_autoattack};

        AddProjectileSpawnEvent(active_projectiles[projectile_id]);
        AddProjectileImpactEvent(active_projectiles[projectile_id]);
        const ProjectileImpactEvent impact_event{projectile_id, caster_id, skill_id, target_id, is_autoattack, impact_timestamp};
        ProcessPendingDamageEventsForProjectileImpact(impact_event);
    }

    void RemoveProjectileTrackers(const uint32_t caster_id)
    {
        for (auto it = active_projectiles.begin(); it != active_projectiles.end();) {
            if (it->second.caster_id == caster_id) {
                it = active_projectiles.erase(it);
            } else {
                ++it;
            }
        }
    }

    void MarkEffectTrackersAttributed(const uint32_t cause_id, const GW::Constants::SkillID skill_id,
                                      const uint32_t cast_timestamp)
    {
        if (cast_timestamp == 0) {
            return;
        }
        const uint32_t now = GetTimeMs();
        for (auto &entry : active_effects_by_agent) {
            for (auto &tracker : entry.second) {
                if (!tracker.active || tracker.attributed) {
                    continue;
                }
                if (tracker.cause_agent_id != cause_id || tracker.skill_id != skill_id) {
                    continue;
                }
                if (tracker.cast_timestamp != cast_timestamp) {
                    continue;
                }
                if (now - tracker.timestamp > effect_infer_window_ms) {
                    continue;
                }
                tracker.attributed = true;
            }
        }
    }

    void MarkProjectileTrackersAttributed(const uint32_t cause_id, const GW::Constants::SkillID skill_id,
                                          const uint32_t cast_timestamp)
    {
        if (cast_timestamp == 0) {
            return;
        }
        const uint32_t now = GetTimeMs();
        const uint32_t max_age_ms = 10000;
        for (auto &entry : active_projectiles) {
            auto &tracker = entry.second;
            if (tracker.caster_id != cause_id || tracker.skill_id != skill_id) {
                continue;
            }
            if (tracker.cast_timestamp != cast_timestamp) {
                continue;
            }
            if (now - tracker.timestamp > max_age_ms) {
                continue;
            }
            tracker.attributed = true;
        }
    }

    void RemoveEffectTracker(const uint32_t target_id, const uint32_t effect_id)
    {
        auto it = active_effects_by_agent.find(target_id);
        if (it == active_effects_by_agent.end()) {
            return;
        }
        auto &trackers = it->second;
        trackers.erase(std::remove_if(trackers.begin(), trackers.end(), [=](const EffectTracker &tracker) {
            return tracker.effect_id == effect_id;
        }), trackers.end());
        if (trackers.empty()) {
            active_effects_by_agent.erase(it);
        }
    }

    void AddProjectileImpactEvent(const ProjectileTracker& tracker)
    {
        if (tracker.projectile_id == 0 || tracker.caster_id == 0 || tracker.impact_timestamp == 0) {
            return;
        }
        recent_projectile_impacts.push_back({tracker.projectile_id, tracker.caster_id, tracker.skill_id,
                                            tracker.target_id, tracker.is_autoattack, tracker.impact_timestamp});
        PruneOldTimelineEvents();
    }

    void AddProjectileSpawnEvent(const ProjectileTracker& tracker)
    {
        if (tracker.projectile_id == 0 || tracker.caster_id == 0 || tracker.timestamp == 0) {
            return;
        }
        recent_projectile_timeline_events.push_back({tracker.projectile_id, tracker.caster_id, tracker.skill_id,
                                                    tracker.target_id, tracker.timestamp,
                                                    tracker.impact_timestamp, tracker.is_autoattack});
        PruneOldTimelineEvents();
    }

    void AddSkillCastTimelineEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                                    const uint32_t target_id, const uint32_t start_timestamp,
                                    const bool is_autoattack = false)
    {
        if (caster_id == 0 || (skill_id == GW::Constants::SkillID::No_Skill && !is_autoattack)) {
            return;
        }
        recent_skill_cast_events.push_back({caster_id, skill_id, target_id, start_timestamp, 0, false, false, is_autoattack});
        PruneOldTimelineEvents();
    }

    void FinishSkillCastTimelineEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                                      const uint32_t target_id, const uint32_t finish_timestamp)
    {
        for (auto &event : recent_skill_cast_events) {
            if (!event.finished && !event.cancelled && event.caster_id == caster_id && event.skill_id == skill_id &&
                (event.target_id == target_id || event.target_id == 0 || target_id == 0)) {
                event.finish_timestamp = finish_timestamp;
                event.finished = true;
                break;
            }
        }
        PruneOldTimelineEvents();
    }

    void CancelSkillCastTimelineEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                                      const uint32_t target_id)
    {
        for (auto &event : recent_skill_cast_events) {
            if (!event.finished && !event.cancelled && event.caster_id == caster_id && event.skill_id == skill_id &&
                (event.target_id == target_id || event.target_id == 0 || target_id == 0)) {
                event.cancelled = true;
                break;
            }
        }
        PruneOldTimelineEvents();
    }

    void AddSkillEndEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                          const uint32_t target_id, const uint32_t timestamp,
                          const bool is_real_end = true, const bool is_dot_tick = false)
    {
        if (caster_id == 0) {
            return;
        }
        recent_skill_end_events.push_back({caster_id, skill_id, target_id, timestamp, is_real_end, is_dot_tick});
        PruneOldTimelineEvents();
    }

    std::vector<uint32_t> GetDotTickOffsets(const DotSkillEffect& dot)
    {
        if (dot.duration_ms > 0) {
            const uint32_t interval_ms = 1000;
            const uint32_t tick_count = dot.duration_ms / interval_ms;
            std::vector<uint32_t> offsets;
            offsets.reserve(tick_count);
            for (uint32_t i = 1; i <= tick_count; ++i) {
                offsets.push_back(i * interval_ms);
            }
            return offsets;
        }
        return dot.tick_offsets_ms;
    }

    void AddDotSkillEndEvents(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                              const uint32_t target_id, const uint32_t timestamp)
    {
        if (caster_id == 0 || skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        for (const auto& dot : dot_skill_effects) {
            if (dot.skill_id != skill_id) {
                continue;
            }
            const auto offsets = GetDotTickOffsets(dot);
            for (const auto offset : offsets) {
                AddSkillEndEvent(caster_id, skill_id, target_id, timestamp + offset, false, true);
            }
        }
    }

    void AddUnmatchedDamageEvent(const DamageEvent& damage)
    {
        recent_damage_events.push_back(damage);
        PruneOldTimelineEvents();
    }

    void ExpireOldDamageEvents()
    {
        const uint32_t now = GetTimeMs();
        while (!recent_damage_events.empty() && now - recent_damage_events.front().timestamp > timeline_window_ms) {
            const auto damage = recent_damage_events.front();
            recent_damage_events.erase(recent_damage_events.begin());
            if (!damage.handled) {
                const bool is_healing = damage.value > 0.0f;
                if (auto* party_member = const_cast<PartyMember*>(GetPartyMemberByAgentId(damage.cause_id))) {
                    if (is_healing) {
                        party_member->unattributable_healing += damage.amount;
                    } else {
                        party_member->unattributable_damage += damage.amount;
                    }
                    party_member->unattributable_count++;
                } else if (auto* cause_group = GetEnemyGroupByCasterId(damage.cause_id)) {
                    GW::Packet::StoC::GenericModifier packet{};
                    packet.type = damage.type;
                    packet.value = damage.value;
                    packet.target_id = damage.target_id;
                    packet.cause_id = damage.cause_id;
                    AddEnemyUnattributableValue(cause_group, &packet, is_healing);
                } else if (damage.group_player_number != 0) {
                    if (auto* player_number_group = GetEnemyGroupByPlayerNumber(damage.group_player_number)) {
                        GW::Packet::StoC::GenericModifier packet{};
                        packet.type = damage.type;
                        packet.value = damage.value;
                        packet.target_id = damage.target_id;
                        packet.cause_id = damage.cause_id;
                        AddEnemyUnattributableValue(player_number_group, &packet, is_healing);
                    }
                } else if (damage.group_caster_id != 0) {
                    if (auto* caster_group = GetEnemyGroupByCasterId(damage.group_caster_id)) {
                        GW::Packet::StoC::GenericModifier packet{};
                        packet.type = damage.type;
                        packet.value = damage.value;
                        packet.target_id = damage.target_id;
                        packet.cause_id = damage.cause_id;
                        AddEnemyUnattributableValue(caster_group, &packet, is_healing);
                    }
                }
            }
        }
    }

    bool TryAttributeDamageEventToEnemyGroup(DamageEvent& damage)
    {
        if (damage.handled || !IsDamageReadyForAttribution(damage)) {
            return false;
        }
        auto* group = GetEnemyGroupByDamage(damage);
        if (!group) {
            return false;
        }

        GW::Packet::StoC::GenericModifier packet{};
        packet.type = damage.type;
        packet.value = damage.value;
        packet.target_id = damage.target_id;
        packet.cause_id = damage.cause_id;

        const auto match = FindBestDamageMatch(damage, *group);
        if (match.diff == UINT32_MAX) {
            return false;
        }
        const bool is_healing = packet.value > 0.0f;
        if (match.impact) {
            AttributeDamageEvent(damage, *match.impact, &packet, nullptr);
            if (auto* target_party_member = GetPartyMemberByAgentId(damage.target_id)) {
                AddIncomingSkillValue(target_party_member, match.impact->skill_id, &packet, is_healing);
            }
            damage.handled = true;
            return true;
        }
        if (match.skill_end) {
            AttributeDamageEvent(damage, *match.skill_end, &packet, nullptr);
            if (auto* target_party_member = GetPartyMemberByAgentId(damage.target_id)) {
                AddIncomingSkillValue(target_party_member, match.skill_end->skill_id, &packet, is_healing);
            }
            damage.handled = true;
            return true;
        }
        return false;
    }

    void AttributeDamageEvent(const DamageEvent& damage, const ProjectileImpactEvent& impact,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member)
    {
        (void)damage;
        if (!packet) {
            return;
        }
        const bool is_healing = IsPacketHealing(packet);
        if (impact.is_autoattack) {
            if (party_member) {
                AddAutoattackValue(party_member, packet, is_healing);
            }
            if (auto* group = GetEnemyGroupByCasterId(damage.cause_id)) {
                AddEnemyAutoattackValue(group, packet, is_healing);
            }
        } else {
            if (party_member) {
                AddSkillValue(party_member, impact.skill_id, packet, is_healing, true);
            }
            if (auto* group = GetEnemyGroupByCasterId(damage.cause_id)) {
                AddEnemySkillValue(group, impact.skill_id, packet, is_healing, true);
            }
        }
    }

    void AttributeDamageEvent(const DamageEvent& damage, const SkillEndEvent& skill_end,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member)
    {
        (void)damage;
        if (!packet) {
            return;
        }
        const bool is_healing = IsPacketHealing(packet);
        if (party_member) {
            AddSkillValue(party_member, skill_end.skill_id, packet, is_healing, true);
        }
        if (auto* group = GetEnemyGroupByCasterId(damage.cause_id)) {
            AddEnemySkillValue(group, skill_end.skill_id, packet, is_healing, true);
        }
    }

    void ProcessPendingDamageEventsForProjectileImpact(const ProjectileImpactEvent& impact_event)
    {
        uint32_t pending_damage_count = 0;
        for (const auto& damage : recent_damage_events) {
            if (damage.handled || !IsDamageEvent(damage) || !IsDamageReadyForAttribution(damage)) {
                continue;
            }
            pending_damage_count++;
        }
        if (pending_damage_count == 0) {
            return;
        }
        uint32_t projectile_match_candidate_count = 0;
        uint32_t processed_damage_count = 0;

        for (auto& damage : recent_damage_events) {
            if (damage.handled || !IsDamageEvent(damage) || !IsDamageReadyForAttribution(damage)) {
                continue;
            }
            if (!MatchDamageEventToProjectile(damage, impact_event)) {
                continue;
            }
            projectile_match_candidate_count++;
            const auto match = FindBestDamageMatch(damage);
            if (match.diff == UINT32_MAX) {
                continue;
            }
            if (match.impact != &impact_event) {
                continue;
            }
            bool handled = false;
            GW::Packet::StoC::GenericModifier packet{};
            packet.type = damage.type;
            packet.value = damage.value;
            packet.target_id = damage.target_id;
            packet.cause_id = damage.cause_id;
            const bool is_healing = packet.value > 0.0f;
            if (auto* party_member = GetPartyMemberByAgentId(damage.cause_id)) {
                AttributeDamageEvent(damage, *match.impact, &packet, party_member);
                handled = true;
            }
            else if (auto* group = GetEnemyGroupByDamage(damage)) {
                AttributeDamageEvent(damage, *match.impact, &packet, nullptr);
                if (auto* target_party_member = GetPartyMemberByAgentId(damage.target_id)) {
                    AddIncomingSkillValue(target_party_member, match.impact->skill_id, &packet, is_healing);
                }
                handled = true;
            }
            if (handled) {
                damage.handled = true;
                processed_damage_count++;
            }
        }
    }

    void ProcessPendingDamageEventsForSkillEnd(const SkillEndEvent& skill_end_event)
    {
        uint32_t pending_damage_count = 0;
        for (const auto& damage : recent_damage_events) {
            if (damage.handled || !IsDamageEvent(damage) || !IsDamageReadyForAttribution(damage)) {
                continue;
            }
            pending_damage_count++;
        }
        if (pending_damage_count == 0) {
            return;
        }
        uint32_t skillend_match_candidate_count = 0;
        uint32_t processed_damage_count = 0;

        for (auto& damage : recent_damage_events) {
            if (damage.handled || !IsDamageEvent(damage) || !IsDamageReadyForAttribution(damage)) {
                continue;
            }
            if (!MatchDamageEventToSkillEnd(damage, skill_end_event)) {
                continue;
            }
            skillend_match_candidate_count++;
            const auto match = FindBestDamageMatch(damage);
            if (match.diff == UINT32_MAX) {
                continue;
            }
            if (match.skill_end != &skill_end_event) {
                continue;
            }
            bool handled = false;
            GW::Packet::StoC::GenericModifier packet{};
            packet.type = damage.type;
            packet.value = damage.value;
            packet.target_id = damage.target_id;
            packet.cause_id = damage.cause_id;
            const bool is_healing = packet.value > 0.0f;
            if (auto* party_member = GetPartyMemberByAgentId(damage.cause_id)) {
                AttributeDamageEvent(damage, *match.skill_end, &packet, party_member);
                handled = true;
            }
            else if (auto* group = GetEnemyGroupByDamage(damage)) {
                AttributeDamageEvent(damage, *match.skill_end, &packet, nullptr);
                if (auto* target_party_member = GetPartyMemberByAgentId(damage.target_id)) {
                    AddIncomingSkillValue(target_party_member, match.skill_end->skill_id, &packet, is_healing);
                }
                handled = true;
            }
            if (handled) {
                damage.handled = true;
                processed_damage_count++;
            }
        }
    }

    bool IsDamageFromSameEnemyGroup(const DamageEvent& damage, const uint32_t caster_id);

    uint32_t GetCastFinishTimestamp(const SkillCastState& cast)
    {
        return cast.finish_timestamp != 0 ? cast.finish_timestamp : cast.timestamp;
    }

    uint32_t GetCastFinishTimestamp(const uint32_t caster_id)
    {
        const auto it = last_skill_cast.find(caster_id);
        if (it == last_skill_cast.end()) {
            return 0;
        }
        return GetCastFinishTimestamp(it->second);
    }

    bool MarkCastAttributed(const uint32_t caster_id, const uint32_t cast_timestamp)
    {
        if (cast_timestamp == 0) {
            return false;
        }
        const auto it = last_skill_cast.find(caster_id);
        if (it == last_skill_cast.end()) {
            return false;
        }
        const uint32_t current_finish_timestamp = GetCastFinishTimestamp(it->second);
        const bool matches_current_finish = current_finish_timestamp == cast_timestamp;
        const bool matches_start_before_finish = it->second.finished && it->second.timestamp == cast_timestamp &&
            GetTimeMs() - current_finish_timestamp <= effect_infer_window_ms;
        if (!matches_current_finish && !matches_start_before_finish) {
            return false;
        }
        if (it->second.attributed) {
            return false;
        }
        it->second.attributed = true;
        return true;
    }

    EffectTracker* FindEffectTrackerByCause(const uint32_t target_id, const uint32_t cause_id)
    {
        const auto it = active_effects_by_agent.find(target_id);
        if (it == active_effects_by_agent.end()) {
            return nullptr;
        }
        const uint32_t now = GetTimeMs();
        EffectTracker* fallback = nullptr;
        for (auto &tracker : it->second) {
            if (!tracker.active) {
                continue;
            }
            if (now - tracker.timestamp > effect_infer_window_ms) {
                continue;
            }
            if (tracker.cause_agent_id == cause_id && cause_id != 0) {
                return &tracker;
            }
            fallback = &tracker;
        }
        return (cause_id == 0) ? fallback : nullptr;
    }

    ProjectileTracker* FindProjectileTrackerByCause(const uint32_t cause_id, const uint32_t target_id)
    {
        const uint32_t now = GetTimeMs();
        ProjectileTracker* fallback = nullptr;
        for (auto &entry : active_projectiles) {
            auto &tracker = entry.second;
            if (tracker.caster_id != cause_id || tracker.attributed) {
                continue;
            }
            if (tracker.flight_time_ms > 0) {
                const uint32_t impact_timestamp = tracker.impact_timestamp ? tracker.impact_timestamp : tracker.timestamp;
                if (!IsWithinInferWindow(impact_timestamp, now)) {
                    continue;
                }
            } else if (now - tracker.timestamp > timeline_window_ms) {
                continue;
            }
            if (tracker.target_id == target_id && target_id != 0) {
                return &tracker;
            }
            if (!fallback || tracker.timestamp > fallback->timestamp) {
                fallback = &tracker;
            }
        }
        return fallback;
    }

    uint32_t InferEffectCause(const GW::Constants::SkillID skill_id, const uint32_t target_id)
    {
        uint32_t best_cause = 0;
        uint32_t best_timestamp = 0;
        const uint32_t now = GetTimeMs();
        for (const auto &[caster_id, cast] : last_skill_cast) {
            if (cast.skill_id != skill_id) {
                continue;
            }
            const uint32_t finish_timestamp = GetCastFinishTimestamp(cast);
            if (finish_timestamp == 0 || now - finish_timestamp > effect_infer_window_ms) {
                continue;
            }
            if (cast.target_id != 0 && cast.target_id != target_id) {
                continue;
            }
            if (finish_timestamp > best_timestamp) {
                best_timestamp = finish_timestamp;
                best_cause = cast.caster_id;
            }
        }
        return best_cause;
    }

    void SetTrackedAction(const uint32_t caster_id, const GW::Constants::SkillID skill_id, const uint32_t target_id,
                          const ActionType type)
    {
        if (skill_id == GW::Constants::SkillID::No_Skill && type != ActionType::Autoattack) {
            return;
        }
        auto &actions = tracked_actions[caster_id];
        actions.insert(actions.begin(), {skill_id, target_id, GetTimeMs(), type});
        while (actions.size() > max_tracked_actions) {
            actions.pop_back();
        }
    }

    TrackedAction* LookupTrackedAction(const uint32_t caster_id, const uint32_t target_id)
    {
        const auto it = tracked_actions.find(caster_id);
        if (it == tracked_actions.end() || it->second.empty()) {
            return nullptr;
        }
        auto &actions = it->second;
        const uint32_t now = GetTimeMs();
        while (!actions.empty() && now - actions.back().timestamp > effect_infer_window_ms) {
            actions.pop_back();
        }
        if (actions.empty()) {
            tracked_actions.erase(it);
            return nullptr;
        }
        if (target_id != 0) {
            for (auto &action : actions) {
                if (action.target_id == target_id) {
                    return &action;
                }
            }
        }
        return &actions.front();
    }

    TrackedAction* LookupActiveAutoattackAction(const uint32_t caster_id)
    {
        const auto it = tracked_actions.find(caster_id);
        if (it == tracked_actions.end()) {
            return nullptr;
        }
        for (auto &action : it->second) {
            if (action.type == ActionType::Autoattack) {
                return &action;
            }
        }
        return nullptr;
    }

    bool HasRecentAutoattackProjectile(const uint32_t caster_id)
    {
        const uint32_t now = GetTimeMs();
        for (const auto& entry : active_projectiles) {
            const auto& tracker = entry.second;
            if (tracker.caster_id != caster_id || !tracker.is_autoattack) {
                continue;
            }
            if (tracker.timestamp > now) {
                continue;
            }
            if (now - tracker.timestamp <= timeline_window_ms) {
                return true;
            }
        }
        return false;
    }

    void RecordSkillCast(const uint32_t caster_id, const uint32_t target_id, const GW::Constants::SkillID skill_id)
    {
        if (skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        last_skill_cast[caster_id] = {skill_id, caster_id, target_id, GetTimeMs(), 0, false, false};
    }

    void AddSkillCastTimelineEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                                    const uint32_t target_id, const uint32_t start_timestamp,
                                    const bool is_autoattack);

    void FinishSkillCastTimelineEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                                      const uint32_t target_id, const uint32_t finish_timestamp);

    void CancelSkillCastTimelineEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                                      const uint32_t target_id);

    void AddSkillEndEvent(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                          const uint32_t target_id, const uint32_t timestamp,
                          const bool is_real_end, const bool is_dot_tick);

    void AddDotSkillEndEvents(const uint32_t caster_id, const GW::Constants::SkillID skill_id,
                              const uint32_t target_id, const uint32_t timestamp);

    void AddProjectileImpactEvent(const ProjectileTracker& tracker);
    void AddProjectileSpawnEvent(const ProjectileTracker& tracker);
    void AddUnmatchedDamageEvent(const DamageEvent& damage);

    void ProcessPendingDamageEventsForProjectileImpact(const ProjectileImpactEvent& impact_event);
    void ProcessPendingDamageEventsForSkillEnd(const SkillEndEvent& skill_end_event);

    void AddSkillValue(PartyMember* party_member, const GW::Constants::SkillID skill_id,
                       const GW::Packet::StoC::GenericModifier* packet, const bool is_healing,
                       const bool count_attributed);

    void AddIncomingSkillValue(PartyMember* party_member, const GW::Constants::SkillID skill_id,
                               const GW::Packet::StoC::GenericModifier* packet, const bool is_healing);

    void AddEnemyAutoattackValue(EnemyGroup* group, const GW::Packet::StoC::GenericModifier* packet,
                                 const bool is_healing);

    void AddEnemyUnattributableValue(EnemyGroup* group, const GW::Packet::StoC::GenericModifier* packet,
                                     const bool is_healing);

    void AddEnemySkillValue(EnemyGroup* group, const GW::Constants::SkillID skill_id,
                            const GW::Packet::StoC::GenericModifier* packet, const bool is_healing,
                            const bool count_attributed);

    void AttributeDamageEvent(const DamageEvent& damage, const ProjectileImpactEvent& impact,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member);

    void AttributeDamageEvent(const DamageEvent& damage, const SkillEndEvent& skill_end,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member);

    void AddEffectTracker(const uint32_t target_id, const EffectTracker &tracker);
    void RemoveEffectTracker(const uint32_t target_id, const uint32_t effect_id);

    void AddProjectileTracker(const GW::Packet::StoC::AgentProjectileLaunched* packet);
    void RemoveProjectileTrackers(const uint32_t caster_id);

    bool TryAttributeDamageEventToEnemyGroup(DamageEvent& damage);

    bool MatchDamageEventToProjectileIgnoringGroup(const DamageEvent& damage, const ProjectileImpactEvent& impact);
    bool MatchDamageEventToSkillEndIgnoringGroup(const DamageEvent& damage, const SkillEndEvent& skill_end);
    const SkillEndEvent* FindBestDelayedSkillEndMatchIgnoringGroup(const DamageEvent& damage);
    DamageMatchResult FindBestDamageMatchIgnoringGroup(const DamageEvent& damage);

    uint32_t GetDamagePacketAmount(const GW::Packet::StoC::GenericModifier* packet)
    {
        if (!packet) {
            return 0;
        }
        if (packet->type == GW::Packet::StoC::GenericValueID::damage ||
            packet->type == GW::Packet::StoC::GenericValueID::critical ||
            packet->type == GW::Packet::StoC::GenericValueID::armorignoring ||
            (packet->type == GW::Packet::StoC::GenericValueID::health && packet->value < 0.0f)) {
            return ToSkillValue(packet);
        }
        return 0;
    }

    bool LogDamageMatchAttempt(const wchar_t* match_type, const DamageEvent& damage,
                               const uint32_t event_id, const uint32_t event_timestamp,
                               const bool result)
    {
        (void)match_type;
        (void)damage;
        (void)event_id;
        (void)event_timestamp;
        (void)result;
        return result;
    }

    PartyMember* GetPartyMemberByAgentId(const uint32_t agent_id)
    {
        if (pending_party_members)
            return nullptr;
        const auto found = std::find_if(party_members.begin(), party_members.end(), [agent_id](const auto party_member) {
            return party_member->agent_id == agent_id;
        });
        return found != party_members.end() ? *found : nullptr;
    }

    PartyMember* GetPartyMemberByEncName(const wchar_t* enc_name)
    {
        if (pending_party_members)
            return nullptr;
        const auto found = std::find_if(party_members.begin(), party_members.end(), [enc_name](const auto party_member) {
            return party_member->name_enc == enc_name;
        });
        return found != party_members.end() ? *found : nullptr;
    }

    bool SetPartyMembers()
    {
        if (!GW::PartyMgr::GetIsPartyLoaded()) {
            return false;
        }
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }

        const auto saved_stats = SavePartyMemberStats();

        GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
        if (info == nullptr) {
            return false;
        }

        const uint32_t my_player_id = GW::Agents::GetControlledCharacterId();
        const GW::Skillbar* my_skillbar = GetAgentSkillbar(my_player_id);
        if (!my_skillbar) {
            return false;
        }
        size_t party_idx = 0;

        player_party_member = nullptr;

        std::vector<PartyMember*> valid_party_members;

        auto set_party_member = [&valid_party_members,&party_idx](const uint32_t agent_id) {
            const wchar_t* agent_name = GW::Agents::GetAgentEncName(agent_id);
            if (!agent_name) {
                return static_cast<PartyMember*>(nullptr);
            }
            const auto sanitised = TextUtils::SanitizePlayerName(agent_name);
            auto party_member = GetPartyMemberByEncName(sanitised.c_str());
            if (!party_member) {
                party_member = new PartyMember(sanitised.c_str(), agent_id, party_idx);
                party_members.push_back(party_member);
            }
            valid_party_members.push_back(party_member);
            party_member->party_idx = party_idx;
            party_member->agent_id = agent_id;
            party_idx++;
            return party_member;
        };
        auto set_member_skill = [](PartyMember* party_member, const GW::Constants::SkillID skill_id) {
            auto& existing_skills = party_member->skills;
            if (existing_skills.size() >= max_party_skills) {
                return;
            }
            for (const auto& skill : existing_skills) {
                if (skill.id == skill_id) {
                    return;
                }
            }
            existing_skills.push_back(skill_id);
        };

        for (const GW::PlayerPartyMember& player : info->players) {
            const auto agent_id = GW::Agents::GetAgentIdByLoginNumber(player.login_number);

            auto party_member = set_party_member(agent_id);
            if (!party_member) {
                return false;
            }

            if (agent_id == my_player_id) {
                player_party_member = party_member;
            }
            if (!info->heroes.valid()) {
                continue;
            }
            for (const GW::HeroPartyMember& hero : info->heroes) {
                if (hero.owner_player_id != player.login_number) {
                    continue;
                }

                party_member = set_party_member(hero.agent_id);
                if (!party_member) {
                    return false;
                }
                const GW::Skillbar* skillbar = GetAgentSkillbar(hero.agent_id);
                if (!skillbar) {
                    continue;
                }
                for (const GW::SkillbarSkill& skill : skillbar->skills) {
                    set_member_skill(party_member, skill.skill_id);
                }
            }
        }
        if (info->henchmen.valid()) {
            for (const GW::HenchmanPartyMember& hench : info->henchmen) {
                set_party_member(hench.agent_id);
            }
        }
        ASSERT(player_party_member);

        for (const GW::SkillbarSkill& skill : my_skillbar->skills) {
            set_member_skill(player_party_member, skill.skill_id);
        }

        auto it = party_members.begin();
        while (it != party_members.end()) {
            const auto found = std::find_if(valid_party_members.begin(), valid_party_members.end(), [it](const auto valid) {
                return valid == *it;
            });
            if (found == valid_party_members.end()) {
                party_members.erase(it);
                it = party_members.begin();
            }
            else {
                ++it;
            }
        }

        RestorePartyMemberStats(saved_stats);
        return true;
    }

    std::vector<SavedPartyMemberStats> SavePartyMemberStats()
    {
        std::vector<SavedPartyMemberStats> saved;
        saved.reserve(party_members.size());
        for (const auto* member : party_members) {
            SavedPartyMemberStats stats;
            stats.party_idx = member->party_idx;
            stats.autoattack_count = member->autoattack_count;
            stats.autoattack_damage = member->autoattack_damage;
            stats.autoattack_healing = member->autoattack_healing;
            stats.unattributable_count = member->unattributable_count;
            stats.unattributable_damage = member->unattributable_damage;
            stats.unattributable_healing = member->unattributable_healing;
            stats.skills.reserve(member->skills.size());
            stats.incoming_skills.reserve(member->incoming_skills.size());
            stats.total_skills_used = member->total_skills_used;
            for (const auto& skill : member->skills) {
                stats.skills.push_back({skill.id, skill.count, skill.ended, skill.attributed, skill.damage, skill.healing});
            }
            for (const auto& skill : member->incoming_skills) {
                stats.incoming_skills.push_back({skill.id, skill.count, skill.ended, skill.attributed, skill.damage, skill.healing});
            }
            saved.push_back(std::move(stats));
        }
        return saved;
    }

    void RestorePartyMemberStats(const std::vector<SavedPartyMemberStats>& saved_stats)
    {
        for (auto* member : party_members) {
            const auto found = std::find_if(saved_stats.begin(), saved_stats.end(), [member](const auto& saved) {
                return saved.party_idx == member->party_idx;
            });
            if (found == saved_stats.end()) {
                continue;
            }
            member->total_skills_used = found->total_skills_used;
            member->autoattack_count = found->autoattack_count;
            member->autoattack_damage = found->autoattack_damage;
            member->autoattack_healing = found->autoattack_healing;
            member->unattributable_count = found->unattributable_count;
            member->unattributable_damage = found->unattributable_damage;
            member->unattributable_healing = found->unattributable_healing;
            for (const auto& saved_skill : found->skills) {
                for (auto& skill : member->skills) {
                    if (skill.id == saved_skill.id) {
                        skill.count = saved_skill.count;
                        skill.ended = saved_skill.ended;
                        skill.attributed = saved_skill.attributed;
                        skill.damage = saved_skill.damage;
                        skill.healing = saved_skill.healing;
                        break;
                    }
                }
            }
            for (const auto& saved_skill : found->incoming_skills) {
                for (auto& skill : member->incoming_skills) {
                    if (skill.id == saved_skill.id) {
                        skill.count = saved_skill.count;
                        skill.ended = saved_skill.ended;
                        skill.attributed = saved_skill.attributed;
                        skill.damage = saved_skill.damage;
                        skill.healing = saved_skill.healing;
                        break;
                    }
                }
            }
        }
    }

    void UnsetPartyStatistics()
    {
        for (const auto entry : party_members) {
            delete entry;
        }
        party_members.clear();
        enemy_groups.clear();
        enemy_groups_by_caster_id.clear();
        enemy_groups_by_player_number.clear();

        skill_names.clear();
        tracked_actions.clear();
        active_effects_by_agent.clear();
    }

    void RefreshEnemyGroupName(EnemyGroup& group)
    {
        if (group.name != UNKNOWN_PLAYER_NAME) {
            return;
        }
        const auto new_name = GetEnemyName(group.caster_id);
        if (new_name != UNKNOWN_PLAYER_NAME) {
            group.name = std::move(new_name);
        }
    }

    void DrawEnemyGroupTimeline(EnemyGroup& group)
    {
        const uint32_t now = GetTimeMs();
        const float width = ImGui::GetContentRegionAvail().x;
        const float height = 72.0f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const float left = origin.x + 12.0f;
        const float right = origin.x + width - 12.0f;
        const float top_y = origin.y + 16.0f;
        const float bottom_y = origin.y + 44.0f;
        const ImU32 color_skill_end = IM_COL32(100, 150, 255, 255);
        const ImU32 color_projectile = IM_COL32(100, 255, 150, 255);
        const ImU32 color_link = IM_COL32(220, 220, 220, 180);
        const ImU32 color_axis = IM_COL32(160, 160, 160, 255);

        auto GroupMatchesCasterId = [&](const uint32_t caster_id) {
            if (group.player_number != 0) {
                const auto* agent = GW::Agents::GetAgentByID(caster_id);
                if (!agent) {
                    return false;
                }
                const auto* living = agent->GetAsAgentLiving();
                return living && living->player_number == group.player_number;
            }
            return caster_id == group.caster_id;
        };

        std::vector<const SkillCastTimelineEvent*> cast_events;
        std::vector<const ProjectileTimelineEvent*> projectile_events;
        std::vector<const SkillEndEvent*> skill_ends;
        std::vector<const ProjectileImpactEvent*> impact_events;
        std::vector<const DamageEvent*> damage_events;

        for (const auto& ev : recent_skill_cast_events) {
            if (!GroupMatchesCasterId(ev.caster_id)) {
                continue;
            }
            if (now >= ev.start_timestamp && now - ev.start_timestamp <= timeline_window_ms) {
                cast_events.push_back(&ev);
            }
        }
        for (const auto& ev : recent_projectile_timeline_events) {
            if (!GroupMatchesCasterId(ev.caster_id)) {
                continue;
            }
            if (now >= ev.spawn_timestamp && now - ev.spawn_timestamp <= timeline_window_ms) {
                projectile_events.push_back(&ev);
            }
        }
        for (const auto& ev : recent_skill_end_events) {
            if (!GroupMatchesCasterId(ev.caster_id)) {
                continue;
            }
            if (now >= ev.timestamp && now - ev.timestamp <= timeline_window_ms) {
                skill_ends.push_back(&ev);
            }
        }
        for (const auto& ev : recent_damage_events) {
            if (!GroupMatchesCasterId(ev.cause_id)) {
                continue;
            }
            if (now >= ev.timestamp && now - ev.timestamp <= timeline_window_ms) {
                damage_events.push_back(&ev);
            }
        }
        for (const auto& ev : recent_projectile_impacts) {
            if (!GroupMatchesCasterId(ev.caster_id)) {
                continue;
            }
            if (now >= ev.impact_timestamp && now - ev.impact_timestamp <= timeline_window_ms) {
                impact_events.push_back(&ev);
            }
        }

        std::vector<std::pair<ImVec2, ImVec2>> links;
        for (const DamageEvent* ev : damage_events) {
            if (!ev->handled) {
                continue;
            }
            const ImVec2 damage_pos(TimestampToTimelineX(ev->timestamp, left, right, now), bottom_y);
            const auto match = FindBestDamageMatch(*ev, group);
            if (match.diff == UINT32_MAX) {
                continue;
            }
            if (match.impact) {
                const float x = TimestampToTimelineX(match.impact->impact_timestamp, left, right, now);
                links.emplace_back(ImVec2(x, top_y), damage_pos);
            } else if (match.skill_end) {
                const float x = TimestampToTimelineX(match.skill_end->timestamp, left, right, now);
                links.emplace_back(ImVec2(x, top_y), damage_pos);
            }
        }

        for (const auto& link : links) {
            draw_list->AddLine(link.first, link.second, color_link, 1.0f);
        }

        draw_list->AddLine(ImVec2(left, top_y), ImVec2(right, top_y), color_axis, 1.0f);
        draw_list->AddLine(ImVec2(left, bottom_y), ImVec2(right, bottom_y), color_axis, 1.0f);
        draw_list->AddText(ImVec2(left, origin.y + 2.0f), IM_COL32(220, 220, 220, 255), "End / Impact");
        draw_list->AddText(ImVec2(left, bottom_y + 6.0f), IM_COL32(220, 220, 220, 255), "Damage / Heal");

        for (const SkillCastTimelineEvent* ev : cast_events) {
            const float x_start = TimestampToTimelineX(ev->start_timestamp, left, right, now);
            const ImVec2 start_pos(x_start, top_y - 6.0f);
            draw_list->AddCircleFilled(start_pos, 3.0f, IM_COL32(255, 220, 100, 255));
            if (ev->is_autoattack) {
                draw_list->AddText(ImVec2(x_start - 4.0f, top_y - 18.0f), IM_COL32(255, 255, 255, 255), "A");
            }
            if (ev->finished) {
                const float x_finish = TimestampToTimelineX(ev->finish_timestamp, left, right, now);
                const ImVec2 finish_pos(x_finish, top_y + 6.0f);
                draw_list->AddLine(start_pos, finish_pos, IM_COL32(200, 200, 255, 180), 1.0f);
                draw_list->AddCircleFilled(finish_pos, 4.0f, color_skill_end);
            }
            if (ev->cancelled) {
                const float icon_radius = 12.0f;
                const ImVec2 icon_tl(x_start - icon_radius, top_y - 18.0f);
                const ImVec2 icon_br(x_start + icon_radius, top_y + 2.0f);
                draw_list->AddLine(icon_tl, icon_br, IM_COL32(255, 80, 80, 255), 2.0f);
                draw_list->AddLine(ImVec2(icon_tl.x, icon_br.y), ImVec2(icon_br.x, icon_tl.y), IM_COL32(255, 80, 80, 255), 2.0f);
            }
        }

        const ImU32 color_damage = IM_COL32(255, 100, 100, 255);
        const ImU32 color_heal = IM_COL32(100, 255, 100, 255);
        for (const ProjectileTimelineEvent* ev : projectile_events) {
            const float x_spawn = TimestampToTimelineX(ev->spawn_timestamp, left, right, now);
            const ImVec2 spawn_pos(x_spawn, top_y - 6.0f);
            draw_list->AddCircleFilled(spawn_pos, 3.0f, IM_COL32(120, 220, 255, 255));
            const float x_impact = TimestampToTimelineX(ev->impact_timestamp, left, right, now);
            const ImVec2 impact_pos(x_impact, top_y + 6.0f);
            draw_list->AddLine(spawn_pos, impact_pos, IM_COL32(120, 255, 180, 180), 1.0f);
            draw_list->AddCircleFilled(impact_pos, 4.0f, color_projectile);
            if (ev->is_autoattack) {
                draw_list->AddText(ImVec2(x_spawn - 4.0f, top_y - 18.0f), IM_COL32(255, 255, 255, 255), "A");
            }
        }

        std::vector<int> damage_x_keys;
        damage_x_keys.reserve(damage_events.size());
        for (const DamageEvent* ev : damage_events) {
            const float x = TimestampToTimelineX(ev->timestamp, left, right, now);
            damage_x_keys.push_back(static_cast<int>(std::round(x)));
        }

        for (size_t i = 0; i < damage_events.size(); ++i) {
            const DamageEvent* ev = damage_events[i];
            const int x_key = damage_x_keys[i];
            size_t group_idx = 0;
            for (size_t j = 0; j < i; ++j) {
                if (damage_x_keys[j] == x_key) {
                    group_idx++;
                }
            }
            const float x = static_cast<float>(x_key);
            const int lane = static_cast<int>(group_idx / 2) + 1;
            const float y_offset = ((group_idx % 2) == 0 ? -1.0f : 1.0f) * lane * 10.0f;
            const float y = bottom_y + y_offset;
            const bool is_heal = ev->value > 0.0f;
            const ImU32 color = is_heal ? color_heal : color_damage;
            draw_list->AddCircleFilled(ImVec2(x, y), 5.0f, color);
            char label[16];
            const auto label_amount = ev->amount;
            const int written = snprintf(label, sizeof(label), "%u", label_amount);
            if (written > 0) {
                draw_list->AddText(ImVec2(x + 6.0f, y - 8.0f), color, label);
            }
        }

        for (const SkillEndEvent* ev : skill_ends) {
            const float x = TimestampToTimelineX(ev->timestamp, left, right, now);
            draw_list->AddCircleFilled(ImVec2(x, top_y), 4.0f, color_skill_end);
        }
        for (const ProjectileImpactEvent* ev : impact_events) {
            const float x = TimestampToTimelineX(ev->impact_timestamp, left, right, now);
            draw_list->AddCircleFilled(ImVec2(x, top_y), 4.0f, color_projectile);
        }
    }

    void DrawIncomingDamageTab()
    {
        const float width = ImGui::GetContentRegionAvail().x;
        if (!party_members.empty()) {
            ImGui::Text("Top 10 incoming damage taken per party member");
            ImGui::Spacing();
            if (ImGui::BeginTable("##TakenSummaryTable", 11, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Party Member", ImGuiTableColumnFlags_WidthStretch);
                for (int i = 1; i <= 10; ++i) {
                    char column_name[32];
                    snprintf(column_name, _countof(column_name), "Skill %d", i);
                    ImGui::TableSetupColumn(column_name, ImGuiTableColumnFlags_WidthFixed, 70.0f);
                }
                ImGui::TableHeadersRow();

                const float icon_scale = ImGui::GetFontSize() / 16.0f;
                const ImVec2 icon_size = {24.0f * icon_scale, 24.0f * icon_scale};

                for (auto* member : party_members) {
                    std::vector<const Skill*> sorted_skills;
                    sorted_skills.reserve(member->incoming_skills.size());
                    for (const auto& skill : member->incoming_skills) {
                        if (skill.id == GW::Constants::SkillID::No_Skill) {
                            continue;
                        }
                        sorted_skills.push_back(&skill);
                    }
                    std::sort(sorted_skills.begin(), sorted_skills.end(), [](const Skill* a, const Skill* b) {
                        return a->damage > b->damage;
                    });

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(member->name.string().c_str());
                    for (size_t col = 0; col < 10; ++col) {
                        ImGui::TableNextColumn();
                        if (col < sorted_skills.size()) {
                            const Skill* skill = sorted_skills[col];
                            const auto texture = GetSkillImage(skill->id);
                            if (texture) {
                                ImGui::Image((ImTextureID)texture, icon_size);
                                if (ImGui::IsItemHovered()) {
                                    ImGui::SetTooltip("%ls\n%u", skill->name->wstring().c_str(), skill->damage);
                                }
                                ImGui::SameLine();
                                ImGui::Text("%u", skill->damage);
                            } else {
                                ImGui::Text("%u", skill->damage);
                            }
                        } else {
                            ImGui::TextUnformatted("--");
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
        }

        if (enemy_groups.empty()) {
            ImGui::TextDisabled("No enemy groups tracked yet.");
            return;
        }

        for (size_t group_idx = 0; group_idx < enemy_groups.size(); ++group_idx) {
            auto& group = *enemy_groups[group_idx];
            const auto enemy_name = TextUtils::WStringToString(group.name);
            char header_label[256];
            snprintf(header_label, _countof(header_label), "%s###EnemyGroup%zu", enemy_name.c_str(), group_idx);

            if (!ImGui::CollapsingHeader(header_label, ImGuiTreeNodeFlags_DefaultOpen)) {
                continue;
            }

            const float start_y = ImGui::GetCursorPosY();

            char table_name[32];
            snprintf(table_name, _countof(table_name), "##EnemySkills%zu", group_idx);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (ImGui::BeginTable(table_name, static_cast<int>(group.skills.size()))) {
                const float column_width = width / static_cast<float>(group.skills.size());
                const float scale = ImGui::GetFontSize() / 16.0f;
                const ImVec2 icon_size = {32.f * scale, 32.f * scale};
                for (size_t i = 0; i < group.skills.size(); i++) {
                    char column_name[32];
                    snprintf(column_name, _countof(column_name), "###EnemyCol%zu", i);
                    ImGui::TableSetupColumn(column_name, ImGuiTableColumnFlags_WidthStretch, column_width);
                }
                ImGui::TableNextRow();
                for (const auto& skill : group.skills) {
                    ImGui::TableNextColumn();
                    if (skill.id == GW::Constants::SkillID::No_Skill) {
                        ImGui::TextUnformatted("--");
                        continue;
                    }
                    if (const auto texture = GetSkillImage(skill.id)) {
                        ImGui::Image((ImTextureID)texture, icon_size);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%ls", skill.name->wstring().c_str());
                        }
                    }
                    if (show_abs_values) {
                        ImGui::Text("Start/Fin: %u/%u\nDmg/Heal: %u/%u", skill.count,
                                    skill.ended, skill.damage, skill.healing);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            if (group.autoattack_count > 0) {
                ImGui::Separator();
                ImGui::Text("Autoattack: %u (%u / %u)", group.autoattack_count,
                            group.autoattack_damage, group.autoattack_healing);
            }
            if (group.unattributable_count > 0) {
                ImGui::Text("Unattributable: %u (%u / %u)", group.unattributable_count,
                            group.unattributable_damage, group.unattributable_healing);
            }

            DrawEnemyGroupTimeline(group);

            const float end_y = ImGui::GetCursorPosY();
            const float min_height = 160.0f;
            const float current_height = end_y - start_y;
            if (current_height < min_height) {
                ImGui::Dummy(ImVec2(0.0f, min_height - current_height));
            }
        }
    }

    void GenericModifierCallback(GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet);

    void AddEffectCallback(const GW::HookStatus*, const GW::Packet::StoC::AddEffect* packet);
    void RemoveEffectCallback(const GW::HookStatus*, const GW::Packet::StoC::RemoveEffect* packet);
    void ProjectileLaunchedCallback(const GW::HookStatus*, const GW::Packet::StoC::AgentProjectileLaunched* packet);
    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
                       const uint32_t value, const bool no_target);

    const SkillEndEvent* FindBestDelayedSkillEndMatch(const DamageEvent& damage);

    EnemyGroup* GetEnemyGroupByDamage(const DamageEvent& damage)
    {
        if (damage.cause_id != 0) {
            if (auto* group = GetEnemyGroupByCasterId(damage.cause_id)) {
                return group;
            }
        }
        if (damage.group_caster_id != 0) {
            if (auto* group = GetEnemyGroupByCasterId(damage.group_caster_id)) {
                return group;
            }
        }
        if (damage.group_player_number != 0) {
            if (auto* group = GetEnemyGroupByPlayerNumber(damage.group_player_number)) {
                return group;
            }
        }
        return nullptr;
    }

    const GW::Skillbar* GetAgentSkillbar(const uint32_t agent_id)
    {
        const GW::SkillbarArray* skillbar_array = GW::SkillbarMgr::GetSkillbarArray();

        if (!skillbar_array) {
            return nullptr;
        }

        for (const GW::Skillbar& skillbar : *skillbar_array) {
            if (skillbar.agent_id == agent_id) {
                return &skillbar;
            }
        }

        return nullptr;
    }

    bool TryAttributeDamageEvent(DamageEvent& damage, const GW::Packet::StoC::GenericModifier* packet,
                                 PartyMember* party_member)
    {
        if (damage.handled || !IsDamageReadyForAttribution(damage) || !party_member) {
            return false;
        }

        GW::Packet::StoC::GenericModifier synthetic_packet{};
        if (packet) {
            synthetic_packet = *packet;
        } else {
            synthetic_packet.type = damage.type;
            synthetic_packet.value = damage.value;
            synthetic_packet.target_id = damage.target_id;
            synthetic_packet.cause_id = damage.cause_id;
        }
        const GW::Packet::StoC::GenericModifier* use_packet = packet ? packet : &synthetic_packet;

        const auto match = FindBestDamageMatch(damage);
        if (match.diff == UINT32_MAX) {
            return false;
        }
        if (match.impact) {
            AttributeDamageEvent(damage, *match.impact, use_packet, party_member);
            damage.handled = true;
            return true;
        }
        if (match.skill_end) {
            AttributeDamageEvent(damage, *match.skill_end, use_packet, party_member);
            damage.handled = true;
            return true;
        }
        return false;
    }

    void ProcessDamagePacket(const GW::Packet::StoC::GenericModifier* packet, PartyMember* party_member)
    {
        if (!packet || !party_member) {
            return;
        }
        DamageEvent damage{packet->cause_id, packet->target_id, packet->type, packet->value,
                           ToSkillValue(packet), GetTimeMs(), false, 0, 0};
        if (auto* group = GetEnemyGroupByCasterId(packet->cause_id)) {
            damage.group_player_number = group->player_number;
            damage.group_caster_id = group->caster_id;
        }
        AddUnmatchedDamageEvent(damage);
    }

    void TrackSkillAction(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
                          const uint32_t value)
    {
        const auto skill_id = static_cast<GW::Constants::SkillID>(value);
        if (value_id == GW::Packet::StoC::GenericValueID::attack_started) {
            SetTrackedAction(caster_id, skill_id, target_id, ActionType::Autoattack);
            AddSkillCastTimelineEvent(caster_id, skill_id, target_id, GetTimeMs(), true);
            return;
        }
        if (value_id == GW::Packet::StoC::GenericValueID::melee_attack_finished) {
            if (auto* action = LookupActiveAutoattackAction(caster_id)) {
                const uint32_t finish_timestamp = GetTimeMs();
                FinishSkillCastTimelineEvent(caster_id, action->skill_id, action->target_id, finish_timestamp);
                AddSkillEndEvent(caster_id, action->skill_id, action->target_id, finish_timestamp);
                ProcessPendingDamageEventsForSkillEnd({caster_id, action->skill_id, action->target_id, finish_timestamp});
                const auto* caster_agent = GW::Agents::GetAgentByID(caster_id);
                const auto* caster_living = caster_agent ? caster_agent->GetAsAgentLiving() : nullptr;
                if (caster_living && caster_living->weapon_type == 4) {
                    const uint32_t delayed_end_timestamp = finish_timestamp + 330;
                    AddSkillEndEvent(caster_id, action->skill_id, action->target_id, delayed_end_timestamp, false, true);
                    ProcessPendingDamageEventsForSkillEnd({caster_id, action->skill_id, action->target_id, delayed_end_timestamp});
                }
                auto &actions = tracked_actions[caster_id];
                actions.erase(std::remove_if(actions.begin(), actions.end(), [](const TrackedAction &tracked_action) {
                    return tracked_action.type == ActionType::Autoattack;
                }), actions.end());
            }
            return;
        }
        if (value_id != GW::Packet::StoC::GenericValueID::skill_stopped &&
            value_id != GW::Packet::StoC::GenericValueID::attack_skill_stopped &&
            value_id != GW::Packet::StoC::GenericValueID::interrupted &&
            skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        switch (value_id) {
            case GW::Packet::StoC::GenericValueID::instant_skill_activated:
            case GW::Packet::StoC::GenericValueID::skill_activated:
                RecordSkillCast(caster_id, target_id, skill_id);
                AddSkillCastTimelineEvent(caster_id, skill_id, target_id, GetTimeMs());
                SetTrackedAction(caster_id, skill_id, target_id, ActionType::Skill);
                break;
            case GW::Packet::StoC::GenericValueID::attack_skill_activated:
                RecordSkillCast(caster_id, target_id, skill_id);
                AddSkillCastTimelineEvent(caster_id, skill_id, target_id, GetTimeMs());
                SetTrackedAction(caster_id, skill_id, target_id, ActionType::AttackSkill);
                break;
            case GW::Packet::StoC::GenericValueID::skill_finished:
            case GW::Packet::StoC::GenericValueID::attack_skill_finished:
                // Keep the tracked action alive for delayed modifiers.
                break;
            case GW::Packet::StoC::GenericValueID::skill_stopped:
            case GW::Packet::StoC::GenericValueID::attack_skill_stopped:
            {
                const auto it = last_skill_cast.find(caster_id);
                if (it != last_skill_cast.end() && !it->second.finished) {
                    it->second.cancelled = true;
                    CancelSkillCastTimelineEvent(caster_id, it->second.skill_id, it->second.target_id);
                    it->second.skill_id = GW::Constants::SkillID::No_Skill;
                }
                break;
            }
            case GW::Packet::StoC::GenericValueID::interrupted:
            {
                const auto it = last_skill_cast.find(caster_id);
                if (it != last_skill_cast.end() && !it->second.finished) {
                    it->second.cancelled = true;
                    CancelSkillCastTimelineEvent(caster_id, it->second.skill_id, it->second.target_id);
                    it->second.skill_id = GW::Constants::SkillID::No_Skill;
                }
                break;
            }
            default:
                break;
        }
    }

    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
                       const uint32_t value, const bool no_target)
    {
        uint32_t actual_caster_id = caster_id;
        uint32_t actual_target_id = target_id;
        switch (value_id) {
            case GW::Packet::StoC::GenericValueID::skill_activated:
            case GW::Packet::StoC::GenericValueID::attack_skill_activated:
            case GW::Packet::StoC::GenericValueID::attack_started:
            case GW::Packet::StoC::GenericValueID::melee_attack_finished:
            case GW::Packet::StoC::GenericValueID::skill_finished:
            case GW::Packet::StoC::GenericValueID::attack_skill_finished:
            case GW::Packet::StoC::GenericValueID::skill_stopped:
            case GW::Packet::StoC::GenericValueID::attack_skill_stopped:
            case GW::Packet::StoC::GenericValueID::interrupted:
                if (!no_target) {
                    actual_caster_id = target_id;
                    actual_target_id = caster_id;
                }
                break;
            default:
                break;
        }

        TrackSkillAction(value_id, actual_caster_id, actual_target_id, value);
        auto activated_skill_id = static_cast<GW::Constants::SkillID>(value);

        if (value_id == GW::Packet::StoC::GenericValueID::skill_damage) {
            if (activated_skill_id == GW::Constants::SkillID::No_Skill) {
                return;
            }
            return;
        }

        uint32_t agent_id = actual_caster_id;
        const bool is_start = value_id == GW::Packet::StoC::GenericValueID::instant_skill_activated ||
                              value_id == GW::Packet::StoC::GenericValueID::skill_activated ||
                              value_id == GW::Packet::StoC::GenericValueID::attack_skill_activated;
        const bool is_end = value_id == GW::Packet::StoC::GenericValueID::skill_finished ||
                            value_id == GW::Packet::StoC::GenericValueID::attack_skill_finished;
        if (!is_start && !is_end) {
            return;
        }

        if (is_start && NONE_SKILL == value) {
            return;
        }

        const auto caster_party_member = GetPartyMemberByAgentId(agent_id);
        const auto* caster_agent = GW::Agents::GetAgentByID(agent_id);
        const auto* caster_living = caster_agent ? caster_agent->GetAsAgentLiving() : nullptr;
        const bool caster_is_enemy = caster_living && caster_living->allegiance == GW::Constants::Allegiance::Enemy;
        const bool enemy_caster = !caster_party_member && caster_is_enemy;
        if (!caster_party_member && !caster_is_enemy) {
            return;
        }

        uint32_t skill_target_id = actual_target_id;
        if (is_end) {
            const auto it = last_skill_cast.find(agent_id);
            if (it != last_skill_cast.end() && it->second.cancelled && !it->second.finished) {
                return;
            }
            if (activated_skill_id == GW::Constants::SkillID::No_Skill) {
                if (it != last_skill_cast.end() && it->second.skill_id != GW::Constants::SkillID::No_Skill &&
                    !it->second.finished && !it->second.cancelled) {
                    activated_skill_id = it->second.skill_id;
                    it->second.finished = true;
                    it->second.finish_timestamp = GetTimeMs();
                    if (skill_target_id == 0) {
                        skill_target_id = it->second.target_id;
                    }
                }
            }
        }

        if (activated_skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }

        if (is_start && enemy_caster) {
            RecordSkillCast(agent_id, skill_target_id, activated_skill_id);
        }

        if (is_start) {
            if (value_id == GW::Packet::StoC::GenericValueID::attack_skill_activated) {
                SetTrackedAction(agent_id, activated_skill_id, skill_target_id, ActionType::AttackSkill);
            } else if (value_id == GW::Packet::StoC::GenericValueID::skill_activated ||
                       value_id == GW::Packet::StoC::GenericValueID::instant_skill_activated) {
                SetTrackedAction(agent_id, activated_skill_id, skill_target_id, ActionType::Skill);
            }
            AddSkillCastTimelineEvent(agent_id, activated_skill_id, skill_target_id, GetTimeMs());
            return;
        }

        if (value_id == GW::Packet::StoC::GenericValueID::skill_stopped ||
            value_id == GW::Packet::StoC::GenericValueID::attack_skill_stopped ||
            value_id == GW::Packet::StoC::GenericValueID::interrupted) {
            const auto it = last_skill_cast.find(agent_id);
            if (it != last_skill_cast.end() && !it->second.finished) {
                it->second.cancelled = true;
                CancelSkillCastTimelineEvent(agent_id, it->second.skill_id, it->second.target_id);
                it->second.skill_id = GW::Constants::SkillID::No_Skill;
            }
            return;
        }
    }

}

void EnemyStatisticsWindow::Initialize()
{
    ToolboxWindow::Initialize();

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, [](GW::HookStatus*, GW::Packet::StoC::MapLoaded*) {
        in_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
        enemy_groups.clear();
        enemy_groups_by_caster_id.clear();
        enemy_groups_by_player_number.clear();
        pending_party_members = true;
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [](const GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet) -> void {
            const uint32_t value_id = packet->value_id;
            const uint32_t caster_id = packet->agent_id;
            constexpr uint32_t target_id = 0U;
            const uint32_t value = packet->value;
            constexpr bool no_target = true;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(
        &GenericValueTarget_Entry,
        [](const GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* packet) -> void {
            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            constexpr bool no_target = false;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
        &GenericModifier_Entry,
        [](const GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet) -> void {
            if (!packet) {
                return;
            }

            if (IsDamageOrHealingPacket(packet)) {
                const auto* cause_agent = static_cast<const GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->cause_id));
                const auto* target_agent = static_cast<const GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->target_id));
                if (cause_agent && cause_agent->allegiance == GW::Constants::Allegiance::Enemy) {
                    GetOrAddEnemyGroup(cause_agent->agent_id);
                }
                if (target_agent && target_agent->allegiance == GW::Constants::Allegiance::Enemy) {
                    GetOrAddEnemyGroup(target_agent->agent_id);
                }
            }

            const auto cause_party_member = GetPartyMemberByAgentId(packet->cause_id);
            const auto target_party_member = GetPartyMemberByAgentId(packet->target_id);

            const auto tracked_action = LookupTrackedAction(packet->cause_id, packet->target_id);
            if (tracked_action && tracked_action->type == ActionType::Autoattack) {
                const bool is_healing = packet->value > 0.0f;
                if (packet->type == GW::Packet::StoC::GenericValueID::health ||
                    packet->type == GW::Packet::StoC::GenericValueID::damage ||
                    packet->type == GW::Packet::StoC::GenericValueID::critical ||
                    packet->type == GW::Packet::StoC::GenericValueID::armorignoring) {
                    if (!HasRecentAutoattackProjectile(packet->cause_id)) {
                        AddAutoattackValue(cause_party_member ? cause_party_member : target_party_member, packet, is_healing);
                        if (auto* group = GetEnemyGroupByCasterId(packet->cause_id)) {
                            AddEnemyAutoattackValue(group, packet, is_healing);
                        }
                        return;
                    }
                }
            }

            if (!IsDamageOrHealingPacket(packet)) {
                return;
            }

            ProcessDamagePacket(packet, cause_party_member ? cause_party_member : target_party_member);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentProjectileLaunched>(
        &ProjectileLaunched_Entry,
        [](const GW::HookStatus*, const GW::Packet::StoC::AgentProjectileLaunched* packet) -> void {
            if (packet && packet->agent_id != 0) {
                GetOrAddEnemyGroup(packet->agent_id);
            }
            AddProjectileTracker(packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AddEffect>(
        &AddEffect_Entry,
        [](const GW::HookStatus*, const GW::Packet::StoC::AddEffect* packet) -> void {
            if (!packet) {
                return;
            }
            const auto target_id = packet->agent_id;
            const auto skill_id = static_cast<GW::Constants::SkillID>(packet->skill_id);
            if (skill_id == GW::Constants::SkillID::No_Skill || packet->effect_id == 0) {
                return;
            }
            const auto cause_id = InferEffectCause(skill_id, target_id);
            const auto it = last_skill_cast.find(cause_id);
            const uint32_t cast_timestamp = it != last_skill_cast.end() ? GetCastFinishTimestamp(it->second) : 0;
            AddEffectTracker(target_id, {packet->effect_id, skill_id, cause_id, GetTimeMs(), cast_timestamp, true, false});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::RemoveEffect>(
        &RemoveEffect_Entry,
        [](const GW::HookStatus*, const GW::Packet::StoC::RemoveEffect* packet) -> void {
            if (!packet) {
                return;
            }
            RemoveEffectTracker(packet->agent_id, packet->effect_id);
        });

    UnsetPartyStatistics();
    pending_party_members = true;
}

void EnemyStatisticsWindow::Update(const float)
{
    PruneOldTimelineEvents();

    for (auto& damage : recent_damage_events) {
        if (!IsDamageEvent(damage) || !IsDamageReadyForAttribution(damage)) {
            continue;
        }
        if (auto* party_member = GetPartyMemberByAgentId(damage.cause_id)) {
            TryAttributeDamageEvent(damage, nullptr, party_member);
        } else {
            TryAttributeDamageEventToEnemyGroup(damage);
        }
    }

    if (pending_party_members && SetPartyMembers()) {
        pending_party_members = false;
    }
}

void EnemyStatisticsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    if (!GW::Map::GetIsMapLoaded()) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    if (ImGuiViewport* viewport = ImGui::GetMainViewport()) {
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (!in_explorable) {
            ImGui::TextDisabled("BUILD CHECK: stats update only in explorable areas");
        }
        DrawIncomingDamageTab();
    }
    ImGui::End();
}

void EnemyStatisticsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    //LOAD_BOOL(show_abs_values);
    //LOAD_BOOL(show_perc_values);
}

void EnemyStatisticsWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    //SAVE_BOOL(show_abs_values);
    //SAVE_BOOL(show_perc_values);
}

void EnemyStatisticsWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Show the absolute skill count", &show_abs_values);
    ImGui::SameLine();
    ImGui::Checkbox("Show the percentage skill count", &show_perc_values);
}

void EnemyStatisticsWindow::Terminate()
{
    ToolboxWindow::Terminate();

    GW::StoC::RemoveCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValue>(&GenericValueSelf_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AddEffect>(&AddEffect_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::RemoveEffect>(&RemoveEffect_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentProjectileLaunched>(&ProjectileLaunched_Entry);

    UnsetPartyStatistics();
}
