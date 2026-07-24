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
#include <Windows/PartyStatisticsWindow.h>
#include <Utils/TextUtils.h>

namespace GW {
    namespace Agents {
        void AsyncGetAgentName(uint32_t agent_id, std::wstring& out);
        void AsyncGetAgentName(const Agent* agent, std::wstring& out);
    }
}

/*************************/
/* Static Helper Methods */
/*************************/

namespace {
    GW::HookEntry ChatCmd_HookEntry;

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
            skill_names[skill_id] = std::make_unique<GuiUtils::EncString>(skill_data ? skill_data->name : 0);
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
        uint32_t actual_damage = 0;
        uint32_t actual_healing = 0;
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
        uint32_t autoattack_count = 0;
        uint32_t autoattack_damage = 0;
        uint32_t autoattack_actual_damage = 0;
        uint32_t autoattack_healing = 0;
        uint32_t autoattack_actual_healing = 0;
        uint32_t unattributable_count = 0;
        uint32_t unattributable_damage = 0;
        uint32_t unattributable_healing = 0;
        uint32_t unattributable_actual_damage = 0;
        uint32_t unattributable_actual_healing = 0;
        uint32_t actual_damage_total = 0;
        uint32_t combat_duration_ms_total = 0;
        uint32_t combat_segment_start_timestamp = 0;
        uint32_t last_damage_timestamp = 0;
        uint32_t damage_taken_armor_ignoring = 0;
        uint32_t damage_taken_non_armor_ignoring = 0;
    };

    struct SavedSkillStats {
        GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
        uint32_t count = 0;
        uint32_t ended = 0;
        uint32_t attributed = 0;
        uint32_t damage = 0;
        uint32_t healing = 0;
        uint32_t actual_damage = 0;
        uint32_t actual_healing = 0;
    };

    struct SavedPartyMemberStats {
        uint32_t party_idx = 0;
        uint32_t total_skills_used = 0;
        uint32_t autoattack_count = 0;
        uint32_t autoattack_damage = 0;
        uint32_t autoattack_actual_damage = 0;
        uint32_t autoattack_healing = 0;
        uint32_t autoattack_actual_healing = 0;
        uint32_t unattributable_count = 0;
        uint32_t unattributable_damage = 0;
        uint32_t unattributable_healing = 0;
        uint32_t unattributable_actual_damage = 0;
        uint32_t unattributable_actual_healing = 0;
        uint32_t actual_damage_total = 0;
        uint32_t combat_duration_ms_total = 0;
        uint32_t combat_segment_start_timestamp = 0;
        uint32_t last_damage_timestamp = 0;
        uint32_t damage_taken_armor_ignoring = 0;
        uint32_t damage_taken_non_armor_ignoring = 0;
        std::vector<SavedSkillStats> skills;
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

    /* Chat messaging */
    clock_t send_timer = 0;
    std::queue<std::wstring> chat_queue;

    /* Callbacks */
    GW::HookEntry MapLoaded_Entry;
    GW::HookEntry GenericValueSelf_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry AddEffect_Entry;
    GW::HookEntry RemoveEffect_Entry;
    GW::HookEntry ProjectileLaunched_Entry;

    /* Window settings */
    bool show_abs_values = true;
    bool show_perc_values = true;
    bool print_by_click = true;

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
        uint32_t actual_amount = 0;
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
        {GW::Constants::SkillID::Chain_Lightning, 0, 0, {500, 1000}},
        {GW::Constants::SkillID::Chaos_Storm, 0, 10000, {}},
        {GW::Constants::SkillID::Churning_Earth, 0, 5000, {}},
        {GW::Constants::SkillID::Eruption, 0, 5000, {}},
        {GW::Constants::SkillID::Fire_Storm, 0, 10000, {}},
        {GW::Constants::SkillID::Glimmering_Mark, 0, 10000, {}},
        {GW::Constants::SkillID::Illusion_of_Pain, 0, 8000, {}},
        {GW::Constants::SkillID::Kirins_Wrath, 0, 5000, {}},
        {GW::Constants::SkillID::Lava_Font, 0, 5000, {}},
        {GW::Constants::SkillID::Maelstrom, 0, 10000, {}},
        {GW::Constants::SkillID::Meteor, 0, 0, {500}},
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

    struct BattleSegment {
        uint32_t start_timestamp = 0;
        uint32_t end_timestamp = 0;
        uint32_t actual_damage = 0;
        bool open = true;
    };

    std::vector<BattleSegment> recent_battle_segments;
    uint32_t total_burn_enemy_ms = 0;
    uint32_t current_burn_enemy_count = 0;
    uint32_t current_burn_last_update_timestamp = 0;

    uint32_t party_combat_duration_ms_total = 0;
    uint32_t party_combat_segment_start_timestamp = 0;
    uint32_t party_last_damage_timestamp = 0;

    constexpr uint32_t effect_infer_window_ms = 300;
    constexpr uint32_t damage_attribution_delay_ms = 1000;
    constexpr uint32_t timeline_window_ms = 20000;
    constexpr uint32_t battle_gap_ms = 5000;
    constexpr size_t max_tracked_actions = 8;
    constexpr size_t max_party_skills = 8;

    int debug_skill_logs = 0;
    int debug_timeline_logs = 0;
    constexpr int max_debug_skill_logs = 30;
    constexpr int max_debug_timeline_logs = 200;

    uint32_t GetTimeMs()
    {
        return GetTickCount();
    }

    uint32_t ToSkillValue(const GW::Packet::StoC::GenericModifier* packet);
    uint32_t GetActualDamageAmount(const GW::Packet::StoC::GenericModifier* packet);
    void AddPartyMemberCombatDamage(PartyMember* party_member, const uint32_t actual_amount, const uint32_t timestamp);
    uint32_t GetPartyMemberDPS(const PartyMember& party_member, const uint32_t now);
    uint32_t CountBurningEnemies();
    void SetCurrentBurnEnemyCount(const uint32_t count, const uint32_t now);
    void UpdateBurnEnemyDuration(const uint32_t now);
    void UpdatePartyCombatDuration(const uint32_t timestamp);
    uint32_t GetBurnDPS(const uint32_t now);
    bool HasSkill(const PartyMember& party_member, const GW::Constants::SkillID skill_id);
    void UpdateBattleSegments(const DamageEvent& damage);
    void PruneOldBattleSegments();
    PartyMember* GetPartyMemberByAgentId(const uint32_t agent_id);
    PartyMember* GetPartyMemberByPetAgentId(const uint32_t agent_id);
    void AddAutoattackValue(PartyMember* party_member, const GW::Packet::StoC::GenericModifier* packet, const bool is_healing);
    void AddSkillValue(PartyMember* party_member, const GW::Constants::SkillID skill_id,
                       const GW::Packet::StoC::GenericModifier* packet, const bool is_healing,
                       const bool count_attributed);
    void AddUnattributableValue(PartyMember* party_member, const GW::Packet::StoC::GenericModifier* packet,
                                const bool is_healing);
    void AddEnemyAutoattackValue(EnemyGroup* group, const GW::Packet::StoC::GenericModifier* packet,
                                 const bool is_healing);
    void AddEnemyUnattributableValue(EnemyGroup* group, const GW::Packet::StoC::GenericModifier* packet,
                                     const bool is_healing);
    void AddEnemySkillValue(EnemyGroup* group, const GW::Constants::SkillID skill_id,
                            const GW::Packet::StoC::GenericModifier* packet, const bool is_healing,
                            const bool count_attributed);
    EnemyGroup* GetEnemyGroupByPlayerNumber(const uint32_t player_number);
    EnemyGroup* GetEnemyGroupByCasterId(const uint32_t caster_id);
    EnemyGroup* GetOrAddEnemyGroup(const uint32_t caster_id);
    void CacheEnemyGroup(EnemyGroup* group);
    Skill* GetOrAddEnemySkill(const uint32_t caster_id, const GW::Constants::SkillID skill_id);
    bool IsDamageEvent(const DamageEvent& damage);
    bool IsDamageReadyForAttribution(const DamageEvent& damage);
    void ExpireOldDamageEvents();
    bool TryAttributeDamageEvent(DamageEvent& damage, const GW::Packet::StoC::GenericModifier* packet,
                                 PartyMember* party_member);
    EnemyGroup* GetEnemyGroupByDamage(const DamageEvent& damage);
    bool TryAttributeDamageEventToEnemyGroup(DamageEvent& damage);
    void AttributeDamageEvent(const DamageEvent& damage, const ProjectileImpactEvent& impact,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member);
    void AttributeDamageEvent(const DamageEvent& damage, const SkillEndEvent& skill_end,
                              const GW::Packet::StoC::GenericModifier* packet,
                              PartyMember* party_member);
    bool IsDelayedSkillMatch(const DamageEvent& damage, const SkillEndEvent& skill_end,
                             const DelayedSkillEffect& delayed_skill);
    const SkillEndEvent* FindBestDelayedSkillEndMatch(const DamageEvent& damage);
    bool LogDamageMatchAttempt(const wchar_t* match_type, const DamageEvent& damage,
                               const uint32_t event_id, const uint32_t event_timestamp,
                               const bool result);

    bool IsWithinInferWindow(const uint32_t event_timestamp, const uint32_t packet_timestamp)
    {
        const uint32_t diff = event_timestamp > packet_timestamp
                                  ? event_timestamp - packet_timestamp
                                  : packet_timestamp - event_timestamp;
        return diff <= effect_infer_window_ms;
    }

    bool IsDamageReadyForAttribution(const DamageEvent& damage)
    {
        const uint32_t now = GetTimeMs();
        return now >= damage.timestamp && now - damage.timestamp >= damage_attribution_delay_ms;
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
        PruneOldBattleSegments();
    }

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
/*
        if (const SkillEndEvent* delayed_skill_end = FindBestDelayedSkillEndMatchIgnoringGroup(damage)) {
            const uint32_t delay = damage.timestamp > delayed_skill_end->timestamp
                                       ? damage.timestamp - delayed_skill_end->timestamp
                                       : delayed_skill_end->timestamp - damage.timestamp;
            return {nullptr, delayed_skill_end, delay};
        }
*/
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

    void AddUnmatchedDamageEvent(const DamageEvent& damage)
    {
        recent_damage_events.push_back(damage);
        UpdateBattleSegments(damage);
        PruneOldTimelineEvents();
    }

    void UpdateBattleSegments(const DamageEvent& damage)
    {
        if (damage.value >= 0.0f || damage.actual_amount == 0) {
            return;
        }
        const uint32_t now = damage.timestamp;
        if (recent_battle_segments.empty()) {
            recent_battle_segments.push_back({damage.timestamp, damage.timestamp, damage.actual_amount, true});
            return;
        }
        auto& segment = recent_battle_segments.back();
        if (!segment.open || now - segment.end_timestamp > battle_gap_ms) {
            if (segment.open) {
                segment.open = false;
            }
            recent_battle_segments.push_back({now, now, damage.actual_amount, true});
            return;
        }
        segment.end_timestamp = now;
        segment.actual_damage += damage.actual_amount;
    }

    void PruneOldBattleSegments()
    {
        const uint32_t now = GetTimeMs();
        while (!recent_battle_segments.empty()) {
            const auto& segment = recent_battle_segments.front();
            const bool segment_ended = !segment.open || (now - segment.end_timestamp > battle_gap_ms);
            if (!segment_ended) {
                break;
            }
            if (segment.end_timestamp + timeline_window_ms < now) {
                recent_battle_segments.erase(recent_battle_segments.begin());
                continue;
            }
            break;
        }
    }

    void ExpireOldDamageEvents()
    {
        const uint32_t now = GetTimeMs();
        while (!recent_damage_events.empty() && now - recent_damage_events.front().timestamp > timeline_window_ms) {
            const auto damage = recent_damage_events.front();
            recent_damage_events.erase(recent_damage_events.begin());
            if (!damage.handled) {
                const bool is_healing = damage.value > 0.0f;
                GW::Packet::StoC::GenericModifier packet{};
                packet.type = damage.type;
                packet.value = damage.value;
                packet.target_id = damage.target_id;
                packet.cause_id = damage.cause_id;

                auto* party_member = const_cast<PartyMember*>(GetPartyMemberByAgentId(damage.cause_id));
                if (!party_member) {
                    party_member = const_cast<PartyMember*>(GetPartyMemberByPetAgentId(damage.cause_id));
                }
                if (party_member) {
                    AddUnattributableValue(party_member, &packet, is_healing);
                } else if (auto* group = GetEnemyGroupByCasterId(damage.cause_id)) {
                    AddEnemyUnattributableValue(group, &packet, is_healing);
                } else if (damage.group_player_number != 0) {
                    if (auto* enemy_group = GetEnemyGroupByPlayerNumber(damage.group_player_number)) {
                        AddEnemyUnattributableValue(enemy_group, &packet, is_healing);
                    }
                } else if (damage.group_caster_id != 0) {
                    if (auto* enemy_group = GetEnemyGroupByCasterId(damage.group_caster_id)) {
                        AddEnemyUnattributableValue(enemy_group, &packet, is_healing);
                    }
                }
            }
        }
    }

    auto IsPacketHealing = [](const GW::Packet::StoC::GenericModifier* packet) {
        return packet->value > 0.0f;
    };

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
            } else {
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
        } else {
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
            Log::LogW(L"PartyStats [timeline] projectile impact %u has no pending damage events", impact_event.projectile_id);
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
                Log::LogW(L"PartyStats [timeline] no match for damage cause=%u target=%u type=%u amount=%u ts=%u group_caster_id=%u group_player_number=%u impact=%u",
                          damage.cause_id, damage.target_id, damage.type, damage.amount, damage.timestamp,
                          damage.group_caster_id, damage.group_player_number, impact_event.projectile_id);
                continue;
            }
            if (match.impact != &impact_event) {
                Log::LogW(L"PartyStats [timeline] match found but not this impact for damage cause=%u target=%u type=%u amount=%u ts=%u impact=%u matched_impact=%u group_caster_id=%u group_player_number=%u",
                          damage.cause_id, damage.target_id, damage.type, damage.amount, damage.timestamp,
                          impact_event.projectile_id,
                          match.impact ? match.impact->projectile_id : 0,
                          damage.group_caster_id, damage.group_player_number);
                continue;
            }
            bool handled = false;
            GW::Packet::StoC::GenericModifier packet{};
            packet.type = damage.type;
            packet.value = damage.value;
            packet.target_id = damage.target_id;
            packet.cause_id = damage.cause_id;
            if (auto* party_member = GetPartyMemberByAgentId(damage.cause_id)) {
                AttributeDamageEvent(damage, *match.impact, &packet, party_member);
                handled = true;
            }
            else if (auto* group = GetEnemyGroupByDamage(damage)) {
                AttributeDamageEvent(damage, *match.impact, &packet, nullptr);
                handled = true;
            }
            else {
                Log::LogW(L"PartyStats [timeline] matched impact but no party/group to attribute for damage cause=%u target=%u type=%u amount=%u ts=%u group_caster_id=%u group_player_number=%u impact=%u",
                          damage.cause_id, damage.target_id, damage.type, damage.amount, damage.timestamp,
                          damage.group_caster_id, damage.group_player_number, impact_event.projectile_id);
            }
            if (handled) {
                damage.handled = true;
                processed_damage_count++;
            }
        }
        if (projectile_match_candidate_count == 0) {
            Log::LogW(L"PartyStats [timeline] projectile impact %u found %u pending damage events but none passed projectile matching", impact_event.projectile_id, pending_damage_count);
        } else if (processed_damage_count == 0) {
            Log::LogW(L"PartyStats [timeline] projectile impact %u had %u candidate damages but none attributed", impact_event.projectile_id, projectile_match_candidate_count);
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
            Log::LogW(L"PartyStats [timeline] skill_end %u has no pending damage events", static_cast<uint32_t>(skill_end_event.skill_id));
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
                Log::LogW(L"PartyStats [timeline] no match for damage cause=%u target=%u type=%u amount=%u ts=%u group_caster_id=%u group_player_number=%u skill_end=%u",
                          damage.cause_id, damage.target_id, damage.type, damage.amount, damage.timestamp,
                          damage.group_caster_id, damage.group_player_number,
                          static_cast<uint32_t>(skill_end_event.skill_id));
                continue;
            }
            if (match.skill_end != &skill_end_event) {
                Log::LogW(L"PartyStats [timeline] match found but not this skill_end for damage cause=%u target=%u type=%u amount=%u ts=%u skill_end=%u matched_skill_end=%u group_caster_id=%u group_player_number=%u",
                          damage.cause_id, damage.target_id, damage.type, damage.amount, damage.timestamp,
                          static_cast<uint32_t>(skill_end_event.skill_id),
                          match.skill_end ? static_cast<uint32_t>(match.skill_end->skill_id) : 0,
                          damage.group_caster_id, damage.group_player_number);
                continue;
            }
            bool handled = false;
            GW::Packet::StoC::GenericModifier packet{};
            packet.type = damage.type;
            packet.value = damage.value;
            packet.target_id = damage.target_id;
            packet.cause_id = damage.cause_id;
            if (auto* party_member = GetPartyMemberByAgentId(damage.cause_id)) {
                AttributeDamageEvent(damage, *match.skill_end, &packet, party_member);
                handled = true;
            }
            else if (auto* group = GetEnemyGroupByDamage(damage)) {
                AttributeDamageEvent(damage, *match.skill_end, &packet, nullptr);
                handled = true;
            }
            else {
                Log::LogW(L"PartyStats [timeline] matched skill_end but no party/group to attribute for damage cause=%u target=%u type=%u amount=%u ts=%u group_caster_id=%u group_player_number=%u skill_end=%u",
                          damage.cause_id, damage.target_id, damage.type, damage.amount, damage.timestamp,
                          damage.group_caster_id, damage.group_player_number,
                          static_cast<uint32_t>(skill_end_event.skill_id));
            }
            if (handled) {
                damage.handled = true;
                processed_damage_count++;
            }
        }
        if (skillend_match_candidate_count == 0) {
            Log::LogW(L"PartyStats [timeline] skill_end %u found %u pending damage events but none passed skill_end matching", static_cast<uint32_t>(skill_end_event.skill_id), pending_damage_count);
        } else if (processed_damage_count == 0) {
            Log::LogW(L"PartyStats [timeline] skill_end %u had %u candidate damages but none attributed", static_cast<uint32_t>(skill_end_event.skill_id), skillend_match_candidate_count);
        }
    }

    bool IsDamageEvent(const DamageEvent& damage)
    {
        return damage.type == GW::Packet::StoC::GenericValueID::health ||
               damage.type == GW::Packet::StoC::GenericValueID::damage ||
               damage.type == GW::Packet::StoC::GenericValueID::critical ||
               damage.type == GW::Packet::StoC::GenericValueID::armorignoring;
    }

    void ProcessDamagePacket(const GW::Packet::StoC::GenericModifier* packet, PartyMember* party_member)
    {
        if (!packet || !party_member) {
            return;
        }
        if (packet->cause_id != 0 && packet->cause_id == packet->target_id && packet->value < 0.0f &&
            (packet->type == GW::Packet::StoC::GenericValueID::damage ||
             packet->type == GW::Packet::StoC::GenericValueID::critical ||
             packet->type == GW::Packet::StoC::GenericValueID::armorignoring)) {
            return;
        }
        const uint32_t potential_amount = ToSkillValue(packet);
        DamageEvent damage{packet->cause_id, packet->target_id, packet->type, packet->value,
                           potential_amount, GetActualDamageAmount(packet), GetTimeMs(), false, 0, 0};
        if (auto* group = GetEnemyGroupByCasterId(packet->cause_id)) {
            damage.group_player_number = group->player_number;
            damage.group_caster_id = group->caster_id;
        }
        AddUnmatchedDamageEvent(damage);
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
        if (match.impact) {
            AttributeDamageEvent(damage, *match.impact, &packet, nullptr);
            damage.handled = true;
            return true;
        }
        if (match.skill_end) {
            AttributeDamageEvent(damage, *match.skill_end, &packet, nullptr);
            damage.handled = true;
            return true;
        }
        return false;
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

    void RecordSkillCast(const uint32_t caster_id, const uint32_t target_id, const GW::Constants::SkillID skill_id)
    {
        if (skill_id == GW::Constants::SkillID::No_Skill) {
            return;
        }
        last_skill_cast[caster_id] = {skill_id, caster_id, target_id, GetTimeMs(), 0, false, false};
    }

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

    PartyMember* GetPartyMemberByPartyIdx(const uint32_t party_idx)
    {
        if (pending_party_members)
            return nullptr;
        const auto found = std::find_if(party_members.begin(), party_members.end(), [party_idx](const auto party_member) {
            return party_member->party_idx == party_idx;
        });
        return found != party_members.end() ? *found : nullptr;
    }

    PartyMember* GetPartyMemberByAgentId(const uint32_t agent_id)
    {
        if (pending_party_members)
            return nullptr;
        // NB: This function is called on the game thread whenever a skill is used. Would it be much performance difference to keep a std::map for this?
        const auto found = std::find_if(party_members.begin(), party_members.end(), [agent_id](const auto party_member) {
            return party_member->agent_id == agent_id;
        });
        return found != party_members.end() ? *found : nullptr;
    }
    PartyMember* GetPartyMemberByEncName(const wchar_t* enc_name)
    {
        /* 
        @Cleanup: 
         - What happens when 3 players each bring the same hero ? "Ebon Vanguard Mesmer" x 2
         - GW sometimes sends the enc_name packet after the NPC is created; would this affect henchmen?
         - Players can have names that match heros or henchmen in some edge cases
         - Obfuscator could be a problem

         Instead maybe hook into the PartyPlayerAdd / PartyAllyAdd / PartyHeroAdd packets, then identify by player owner / hero id instead of just using name
         */

        if (pending_party_members)
            return nullptr;
        const auto found = std::find_if(party_members.begin(), party_members.end(), [enc_name](const auto party_member) {
            return party_member->name_enc == enc_name;
        });
        return found != party_members.end() ? *found : nullptr;
    }

    Skill* GetOrAddSkill(PartyMember* party_member, const GW::Constants::SkillID skill_id)
    {
        if (skill_id == GW::Constants::SkillID::No_Skill) {
            return nullptr;
        }
        for (auto& skill : party_member->skills) {
            if (skill.id == skill_id) {
                return &skill;
            }
        }
        // Not in the party member's skill bar (e.g. pet, spirit, minion skill).
        // Add it so the damage can still be tracked and contributed to DPS.
        if (party_member->skills.size() >= max_party_skills) {
            return nullptr;
        }
        party_member->skills.emplace_back(skill_id);
        return &party_member->skills.back();
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

        if (auto* group = GetEnemyGroupByCasterId(caster_id)) {
            if (player_number != 0 && group->player_number == 0) {
                group->player_number = player_number;
                CacheEnemyGroup(group);
            }
            return group;
        }

        if (player_number != 0) {
            if (auto* group = GetEnemyGroupByPlayerNumber(player_number)) {
                if (group->caster_id == 0 || group->caster_id == caster_id) {
                    if (group->caster_id == 0) {
                        group->caster_id = caster_id;
                    }
                    CacheEnemyGroup(group);
                    return group;
                }
            }
            for (auto& group_ptr : enemy_groups) {
                auto* group = group_ptr.get();
                if (group->player_number == player_number && (group->caster_id == 0 || group->caster_id == caster_id)) {
                    if (group->caster_id == 0) {
                        group->caster_id = caster_id;
                    }
                    CacheEnemyGroup(group);
                    if (player_number != 0 && group->player_number == 0) {
                        group->player_number = player_number;
                        CacheEnemyGroup(group);
                    }
                    return group;
                }
            }
        }

        EnemyGroup group;
        group.caster_id = caster_id;
        group.player_number = player_number;
        group.name = GetEnemyName(caster_id);
        group.skills.reserve(8);
        for (int i = 0; i < 8; ++i) {
            group.skills.emplace_back(GW::Constants::SkillID::No_Skill);
        }
        enemy_groups.emplace_back(std::make_unique<EnemyGroup>(std::move(group)));
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

    uint32_t GetActualDamageAmount(const GW::Packet::StoC::GenericModifier* packet)
    {
        if (!packet) {
            return 0;
        }
        const auto agent = GW::Agents::GetAgentByID(packet->target_id);
        const auto target = agent ? agent->GetAsAgentLiving() : nullptr;
        const uint32_t potential = ToSkillValue(packet);
        if (!target) {
            return potential;
        }

        uint32_t max_hp = PartyDamage::GetMaxHp(target->player_number);
        if (max_hp == 0 && target->max_hp > 0 && target->max_hp < 100000) {
            max_hp = target->max_hp;
        }
        if (max_hp == 0) {
            const uint32_t approx_hp = target->level * 20 + 100;
            max_hp = approx_hp;
        }

        const uint32_t current_hp = static_cast<uint32_t>(std::lround(target->hp * static_cast<float>(max_hp)));
        if (packet->value > 0.0f) {
            const uint32_t missing_hp = current_hp >= max_hp ? 0u : max_hp - current_hp;
            return std::min(potential, missing_hp);
        }
        return std::min(potential, current_hp);
    }

    void LogSkillModifierDebug(const GW::Packet::StoC::GenericModifier* packet, const GW::Constants::SkillID skill_id,
                               const wchar_t* status)
    {
        (void)packet;
        (void)skill_id;
        (void)status;
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
        const uint32_t actual_amount = GetActualDamageAmount(packet);
        if (is_healing) {
            skill->healing += amount;
            skill->actual_healing += actual_amount;
        } else {
            skill->damage += amount;
            skill->actual_damage += actual_amount;
        }
        if (count_attributed) {
            skill->attributed++;
        }
    }

    void AddAutoattackValue(PartyMember* party_member, const GW::Packet::StoC::GenericModifier* packet,
                            const bool is_healing)
    {
        if (!party_member || !packet) {
            return;
        }
        const uint32_t amount = ToSkillValue(packet);
        const uint32_t actual_amount = GetActualDamageAmount(packet);
        if (is_healing) {
            party_member->autoattack_healing += amount;
            party_member->autoattack_actual_healing += actual_amount;
        } else {
            party_member->autoattack_damage += amount;
            party_member->autoattack_actual_damage += actual_amount;
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
        const uint32_t actual_amount = GetActualDamageAmount(packet);
        if (is_healing) {
            party_member->unattributable_healing += amount;
            party_member->unattributable_actual_healing += actual_amount;
        } else {
            party_member->unattributable_damage += amount;
            party_member->unattributable_actual_damage += actual_amount;
        }
        party_member->unattributable_count++;
    }

    void AddPartyMemberDamageTaken(PartyMember* party_member, const GW::Packet::StoC::GenericModifier* packet)
    {
        if (!party_member || !packet) {
            return;
        }
        if (packet->value >= 0.0f) {
            return;
        }
        const uint32_t actual_amount = GetActualDamageAmount(packet);
        if (actual_amount == 0) {
            return;
        }
        if (packet->type == GW::Packet::StoC::GenericValueID::armorignoring) {
            party_member->damage_taken_armor_ignoring += actual_amount;
        } else {
            party_member->damage_taken_non_armor_ignoring += actual_amount;
        }
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

    void UpdatePartyCombatDuration(const uint32_t timestamp)
    {
        if (party_last_damage_timestamp != 0 && timestamp - party_last_damage_timestamp <= battle_gap_ms) {
            party_last_damage_timestamp = timestamp;
        } else {
            if (party_combat_segment_start_timestamp != 0 && party_last_damage_timestamp != 0 &&
                party_last_damage_timestamp > party_combat_segment_start_timestamp) {
                const uint32_t segment_duration = party_last_damage_timestamp - party_combat_segment_start_timestamp;
                party_combat_duration_ms_total += segment_duration;
            }
            party_combat_segment_start_timestamp = timestamp;
            party_last_damage_timestamp = timestamp;
        }
    }

    void AddPartyMemberCombatDamage(PartyMember* party_member, const uint32_t actual_amount, const uint32_t timestamp)
    {
        if (!party_member || actual_amount == 0 || timestamp == 0) {
            return;
        }

        if (party_member->last_damage_timestamp != 0 && timestamp - party_member->last_damage_timestamp <= battle_gap_ms) {
            // Continue the current combat segment.
            party_member->last_damage_timestamp = timestamp;
        } else {
            // Close the previous segment if it exists.
            if (party_member->combat_segment_start_timestamp != 0 && party_member->last_damage_timestamp != 0 &&
                party_member->last_damage_timestamp > party_member->combat_segment_start_timestamp) {
                const uint32_t segment_duration = party_member->last_damage_timestamp - party_member->combat_segment_start_timestamp;
                party_member->combat_duration_ms_total += segment_duration;
            }
            party_member->combat_segment_start_timestamp = timestamp;
            party_member->last_damage_timestamp = timestamp;
        }

        party_member->actual_damage_total += actual_amount;
        UpdatePartyCombatDuration(timestamp);
    }

    uint32_t GetPartyMemberDPS(const PartyMember& party_member, const uint32_t now)
    {
        uint32_t total_duration = party_member.combat_duration_ms_total;
        if (party_member.combat_segment_start_timestamp != 0 && party_member.last_damage_timestamp != 0 &&
            now - party_member.last_damage_timestamp <= battle_gap_ms) {
            total_duration += now - party_member.combat_segment_start_timestamp;
        }
        if (total_duration == 0) {
            return 0;
        }
        const float seconds = static_cast<float>(total_duration) / 1000.0f;
        return static_cast<uint32_t>(std::round(static_cast<float>(party_member.actual_damage_total) / seconds));
    }

    uint32_t CountBurningEnemies()
    {
        uint32_t count = 0;
        auto agents = GW::Agents::GetAgentArray();
        if (!agents) {
            return 0;
        }
        for (GW::Agent* agent : *agents) {
            if (!agent) {
                continue;
            }
            GW::AgentLiving* living = agent->GetAsAgentLiving();
            if (!living || living->allegiance != GW::Constants::Allegiance::Enemy) {
                continue;
            }
            for (auto link = living->visible_effects.Get(); link != nullptr; link = link->NextLink()) {
                auto node = link->Next();
                if (!node) {
                    break;
                }
                if (node->id == GW::Constants::EffectID::burning && !node->has_ended) {
                    count++;
                    break;
                }
            }
        }
        return count;
    }

    void UpdateBurnEnemyDuration(const uint32_t now)
    {
        if (current_burn_last_update_timestamp == 0 || now <= current_burn_last_update_timestamp) {
            current_burn_last_update_timestamp = now;
            return;
        }
        const uint32_t delta = now - current_burn_last_update_timestamp;
        total_burn_enemy_ms += current_burn_enemy_count * delta;
        current_burn_last_update_timestamp = now;
    }

    void SetCurrentBurnEnemyCount(const uint32_t count, const uint32_t now)
    {
        UpdateBurnEnemyDuration(now);
        current_burn_enemy_count = count;
    }

    uint32_t GetBurnDPS(const uint32_t now)
    {
        if (party_combat_duration_ms_total == 0) {
            return 0;
        }
        uint32_t party_duration = party_combat_duration_ms_total;
        if (party_combat_segment_start_timestamp != 0 && party_last_damage_timestamp != 0 &&
            now - party_last_damage_timestamp <= battle_gap_ms) {
            party_duration += now - party_combat_segment_start_timestamp;
        }
        if (party_duration == 0) {
            return 0;
        }
        uint32_t burn_ms = total_burn_enemy_ms;
        if (current_burn_last_update_timestamp != 0 && now > current_burn_last_update_timestamp) {
            burn_ms += current_burn_enemy_count * (now - current_burn_last_update_timestamp);
        }
        return static_cast<uint32_t>(std::round(static_cast<float>(burn_ms) * 14.0f / static_cast<float>(party_duration)));
    }

    uint32_t GetPartyMemberActualHealing(const PartyMember& party_member)
    {
        uint32_t total_healing = party_member.autoattack_actual_healing + party_member.unattributable_actual_healing;
        for (const auto& skill : party_member.skills) {
            total_healing += skill.actual_healing;
        }
        return total_healing;
    }

    bool HasSkill(const PartyMember& party_member, const GW::Constants::SkillID skill_id)
    {
        for (const auto& skill : party_member.skills) {
            if (skill.id == skill_id) {
                return true;
            }
        }
        return false;
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

    int GetSkillString(const std::wstring& agent_name, const std::wstring& skill_name,
                       const uint32_t started, const uint32_t ended, const uint32_t attributed,
                       const uint32_t damage, const uint32_t healing, wchar_t* out, const size_t len)
    {
        const auto written = swprintf(out, len,
                                      L"%s used %s started:%u ended:%u attributed:%u damage:%u healing:%u",
                                      agent_name.c_str(), skill_name.c_str(), started, ended, attributed,
                                      damage, healing);
        ASSERT(written != -1);
        return written;
    }

    void WritePlayerStatisticsSingleSkill(PartyMember* party_member, const uint32_t skill_idx)
    {
        if (!(party_member && skill_idx < party_member->skills.size())) {
            return;
        }

        const Skill& skill = party_member->skills[skill_idx];

        wchar_t chat_str[256];
        GetSkillString(party_member->name.wstring(), skill.name->wstring(), skill.count, skill.ended,
                      skill.attributed, skill.damage, skill.healing, chat_str, _countof(chat_str));
        chat_queue.push(chat_str);
    }

    void WritePlayerStatisticsAllSkills(PartyMember* party_member)
    {
        if (!party_member) {
            return;
        }
        const size_t visible_skills = std::min(party_member->skills.size(), max_party_skills);
        for (size_t i = 0; i < visible_skills; i++) {
            const auto& skill = party_member->skills[i];
            if (0U == skill.count) {
                continue;
            }
            WritePlayerStatisticsSingleSkill(party_member, i);
        }
        if (party_member->autoattack_count > 0) {
            wchar_t chat_str[256];
            const wchar_t* autoattack_name = L"Autoattack";
            const auto written = swprintf(chat_str, _countof(chat_str),
                                          party_member->autoattack_count == 1
                                              ? L"%s used %s %u time."
                                              : L"%s used %s %u times.",
                                          party_member->name.wstring().c_str(), autoattack_name,
                                          party_member->autoattack_count);
            if (written != -1) {
                chat_queue.push(chat_str);
            }
        }
        if (party_member->unattributable_count > 0) {
            wchar_t chat_str[256];
            const wchar_t* unknown_name = L"Unattributable";
            const auto written = swprintf(chat_str, _countof(chat_str),
                                          party_member->unattributable_count == 1
                                              ? L"%s had %s %u time."
                                              : L"%s had %s %u times.",
                                          party_member->name.wstring().c_str(), unknown_name,
                                          party_member->unattributable_count);
            if (written != -1) {
                chat_queue.push(chat_str);
            }
        }
    }

    /***********************/
    /* Draw Helper Methods */
    /***********************/

    float TimestampToTimelineX(const uint32_t timestamp, const float left, const float right, const uint32_t now)
    {
        if (timestamp >= now) {
            return right;
        }
        const float t = std::clamp((now - timestamp) / static_cast<float>(timeline_window_ms), 0.0f, 1.0f);
        return right - t * (right - left);
    }

    void DrawPartyMemberTimeline(PartyMember& party_member, const bool incoming = false)
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
        const ImU32 color_damage = IM_COL32(255, 100, 100, 255);
        const ImU32 color_heal = IM_COL32(100, 255, 100, 255);
        const ImU32 color_link = IM_COL32(220, 220, 220, 180);
        const ImU32 color_axis = IM_COL32(160, 160, 160, 255);

        draw_list->AddLine(ImVec2(left, top_y), ImVec2(right, top_y), color_axis, 1.0f);
        draw_list->AddLine(ImVec2(left, bottom_y), ImVec2(right, bottom_y), color_axis, 1.0f);
        draw_list->AddText(ImVec2(left, origin.y + 2.0f), IM_COL32(220, 220, 220, 255), "End / Impact");
        draw_list->AddText(ImVec2(left, bottom_y + 6.0f), IM_COL32(220, 220, 220, 255), "Damage / Heal");

        std::vector<const SkillCastTimelineEvent*> cast_events;
        std::vector<const ProjectileTimelineEvent*> projectile_events;
        std::vector<const SkillEndEvent*> skill_ends;
        std::vector<const ProjectileImpactEvent*> impact_events;
        std::vector<const DamageEvent*> damage_events;

        for (const auto& ev : recent_skill_cast_events) {
            if ((incoming ? ev.target_id == party_member.agent_id : ev.caster_id == party_member.agent_id) && now >= ev.start_timestamp && now - ev.start_timestamp <= timeline_window_ms) {
                if (incoming && GetPartyMemberByAgentId(ev.caster_id)) {
                    continue;
                }
                cast_events.push_back(&ev);
            }
        }
        for (const auto& ev : recent_projectile_timeline_events) {
            if ((incoming ? ev.target_id == party_member.agent_id : ev.caster_id == party_member.agent_id) && now >= ev.spawn_timestamp && now - ev.spawn_timestamp <= timeline_window_ms) {
                if (incoming && GetPartyMemberByAgentId(ev.caster_id)) {
                    continue;
                }
                projectile_events.push_back(&ev);
            }
        }
        for (const auto& ev : recent_skill_end_events) {
            if ((incoming ? ev.target_id == party_member.agent_id : ev.caster_id == party_member.agent_id) && now >= ev.timestamp && now - ev.timestamp <= timeline_window_ms) {
                if (incoming && GetPartyMemberByAgentId(ev.caster_id)) {
                    continue;
                }
                skill_ends.push_back(&ev);
            }
        }
        for (const auto& ev : recent_projectile_impacts) {
            if ((incoming ? ev.target_id == party_member.agent_id : ev.caster_id == party_member.agent_id) && now >= ev.impact_timestamp && now - ev.impact_timestamp <= timeline_window_ms) {
                if (incoming && GetPartyMemberByAgentId(ev.caster_id)) {
                    continue;
                }
                impact_events.push_back(&ev);
            }
        }
        for (const auto& ev : recent_damage_events) {
            if ((incoming ? ev.target_id == party_member.agent_id : ev.cause_id == party_member.agent_id) && now >= ev.timestamp && now - ev.timestamp <= timeline_window_ms) {
                if (ev.value > 0.0f) {
                    continue;
                }
                if (incoming && GetPartyMemberByAgentId(ev.cause_id)) {
                    continue;
                }
                damage_events.push_back(&ev);
            }
        }

        std::vector<const BattleSegment*> visible_segments;
        visible_segments.reserve(recent_battle_segments.size());
        for (const auto& segment : recent_battle_segments) {
            if (segment.end_timestamp >= now - timeline_window_ms || segment.start_timestamp >= now - timeline_window_ms) {
                visible_segments.push_back(&segment);
            }
        }

        const ImU32 color_battle_line = IM_COL32(255, 60, 60, 220);
        for (const auto* segment : visible_segments) {
            const bool segment_ended = !segment->open || (now - segment->end_timestamp > battle_gap_ms);
            const float start_x = TimestampToTimelineX(segment->start_timestamp, left, right, now);
            draw_list->AddLine(ImVec2(start_x, top_y), ImVec2(start_x, bottom_y), color_battle_line, 2.0f);
            if (segment_ended) {
                const float end_x = TimestampToTimelineX(segment->end_timestamp, left, right, now);
                draw_list->AddLine(ImVec2(end_x, top_y), ImVec2(end_x, bottom_y), color_battle_line, 2.0f);
            }
        }

        std::vector<std::pair<ImVec2, ImVec2>> links;
        for (const DamageEvent* damage : damage_events) {
            if (!damage->handled) {
                continue;
            }
            const ImVec2 damage_pos(TimestampToTimelineX(damage->timestamp, left, right, now), bottom_y);
            const auto match = FindBestDamageMatch(*damage);
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

        for (const SkillCastTimelineEvent* ev : cast_events) {
            const float x_start = TimestampToTimelineX(ev->start_timestamp, left, right, now);
            const ImVec2 start_pos(x_start, top_y - 6.0f);
            draw_list->AddCircleFilled(start_pos, 3.0f, IM_COL32(255, 220, 100, 255));
            if (ev->is_autoattack) {
                draw_list->AddText(ImVec2(x_start - 4.0f, top_y - 18.0f), IM_COL32(255, 255, 255, 255), "A");
            } else if (const auto texture = GetSkillImage(ev->skill_id)) {
                draw_list->AddImage((ImTextureID)texture, ImVec2(x_start - 10.0f, top_y - 18.0f),
                                    ImVec2(x_start + 10.0f, top_y + 2.0f));
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
            } else if (const auto texture = GetSkillImage(ev->skill_id)) {
                draw_list->AddImage((ImTextureID)texture, ImVec2(x_spawn - 10.0f, top_y - 18.0f),
                                    ImVec2(x_spawn + 10.0f, top_y + 2.0f));
            }
        }
        auto HasFinishedCastAtTimestamp = [&](const uint32_t timestamp) {
            for (const SkillCastTimelineEvent* ev : cast_events) {
                if (ev->finished && ev->finish_timestamp == timestamp) {
                    return true;
                }
            }
            return false;
        };
        auto HasProjectileImpactAtTimestamp = [&](const uint32_t timestamp) {
            for (const ProjectileTimelineEvent* ev : projectile_events) {
                if (ev->impact_timestamp == timestamp) {
                    return true;
                }
            }
            return false;
        };

        for (const SkillEndEvent* skill_end : skill_ends) {
            if (HasFinishedCastAtTimestamp(skill_end->timestamp)) {
                continue;
            }
            const float x = TimestampToTimelineX(skill_end->timestamp, left, right, now);
            draw_list->AddCircleFilled(ImVec2(x, top_y), 4.0f, color_skill_end);
        }
        for (const ProjectileImpactEvent* impact : impact_events) {
            if (HasProjectileImpactAtTimestamp(impact->impact_timestamp)) {
                continue;
            }
            const float x = TimestampToTimelineX(impact->impact_timestamp, left, right, now);
            draw_list->AddCircleFilled(ImVec2(x, top_y), 4.0f, color_projectile);
        }

        std::vector<int> damage_x_keys;
        damage_x_keys.reserve(damage_events.size());
        for (const DamageEvent* damage : damage_events) {
            const float x = TimestampToTimelineX(damage->timestamp, left, right, now);
            damage_x_keys.push_back(static_cast<int>(std::round(x)));
        }

        for (size_t i = 0; i < damage_events.size(); ++i) {
            const DamageEvent* damage = damage_events[i];
            const int x_key = damage_x_keys[i];
            size_t group_idx = 0;
            for (size_t j = 0; j < i; ++j) {
                if (damage_x_keys[j] == x_key) {
                    ++group_idx;
                }
            }
            const float x = static_cast<float>(x_key);
            const int lane = static_cast<int>(group_idx / 2) + 1;
            const float y_offset = ((group_idx % 2) == 0 ? -1.0f : 1.0f) * lane * 10.0f;
            const float y = bottom_y + y_offset;
            const bool is_heal = damage->value > 0.0f;
            const ImU32 color = is_heal ? color_heal : color_damage;
            draw_list->AddCircleFilled(ImVec2(x, y), 5.0f, color);
            char label[32];
            const uint32_t label_amount = damage->amount;
            const uint32_t actual_amount = damage->actual_amount;
            int written;
            if (actual_amount == label_amount) {
                written = snprintf(label, sizeof(label), "%u", label_amount);
            } else {
                written = snprintf(label, sizeof(label), "%u(%u)", label_amount, actual_amount);
            }
            if (written > 0) {
                draw_list->AddText(ImVec2(x + 6.0f, y - 8.0f), color, label);
            }
        }
    }

    void DrawPartyMember(PartyMember& party_member)
    {
        char header_label[256];
        snprintf(header_label, _countof(header_label), "%s###%u", party_member.name.string().c_str(), party_member.party_idx);

        if (ImGui::CollapsingHeader(header_label, ImGuiTreeNodeFlags_DefaultOpen)) {
            const float start_y = ImGui::GetCursorPosY();

            char table_name[16];
            snprintf(table_name, _countof(table_name), "###Table%d", party_member.party_idx);

            const float width = ImGui::GetContentRegionAvail().x;
            if (party_member.skills.empty()) {
                return;
            }
            const size_t visible_skills = std::min(party_member.skills.size(), max_party_skills);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (ImGui::BeginTable(table_name, static_cast<int>(visible_skills))) {
                const float column_width = width / static_cast<float>(visible_skills);
                const float scale = ImGui::GetFontSize() / 16.0f;
                const ImVec2 icon_size = {32.f * scale, 32.f * scale};
                for (size_t i = 0; i < visible_skills; i++) {
                    char column_name[32];
                    snprintf(column_name, _countof(table_name), "###%sColumn%d", table_name, i);
                    ImGui::TableSetupColumn(column_name, ImGuiTableColumnFlags_WidthStretch, column_width);
                }
                ImGui::TableNextRow();
                for (size_t i = 0; i < visible_skills; i++) {
                    const Skill& skill = party_member.skills[i];
                    if (skill.id == GW::Constants::SkillID::No_Skill) {
                        continue; // Skip empty skill slots (for heroes and yourself)
                    }
                    ImGui::TableNextColumn();
                    if (const auto texture = GetSkillImage(skill.id)) {
                        ImGui::Image((ImTextureID)texture, icon_size);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(skill.name->string().c_str());
                        }
                    }
                    if (show_abs_values) {
                        char damage_text[32];
                        if (skill.actual_damage == skill.damage) {
                            snprintf(damage_text, sizeof(damage_text), "%u", skill.damage);
                        } else {
                            snprintf(damage_text, sizeof(damage_text), "%u(%u)", skill.damage, skill.actual_damage);
                        }
                        char healing_text[32];
                        if (skill.actual_healing == skill.healing) {
                            snprintf(healing_text, sizeof(healing_text), "%u", skill.healing);
                        } else {
                            snprintf(healing_text, sizeof(healing_text), "%u(%u)", skill.healing, skill.actual_healing);
                        }
                        ImGui::Text("Start/Fin: %u/%u\nDmg: %s\nTotal Heal: %s", skill.count, skill.ended,
                                    damage_text, healing_text);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            if (party_member.autoattack_actual_damage == party_member.autoattack_damage) {
                ImGui::Text("Autoattack: %u (%u / %u)", party_member.autoattack_count,
                            party_member.autoattack_damage, party_member.autoattack_healing);
            } else {
                ImGui::Text("Autoattack: %u (%u(%u) / %u)", party_member.autoattack_count,
                            party_member.autoattack_damage, party_member.autoattack_actual_damage,
                            party_member.autoattack_healing);
            }
            if (party_member.unattributable_count == 0) {
                ImGui::Text("Unattributable: 0 (0 / 0)");
            } else if (party_member.unattributable_actual_damage == party_member.unattributable_damage &&
                       party_member.unattributable_actual_healing == party_member.unattributable_healing) {
                ImGui::Text("Unattributable: %u (%u / %u)", party_member.unattributable_count,
                            party_member.unattributable_damage, party_member.unattributable_healing);
            } else {
                ImGui::Text("Unattributable: %u (%u(%u) / %u(%u))", party_member.unattributable_count,
                            party_member.unattributable_damage, party_member.unattributable_actual_damage,
                            party_member.unattributable_healing, party_member.unattributable_actual_healing);
            }
            const uint32_t dps = GetPartyMemberDPS(party_member, GetTimeMs());
            const uint32_t total_healing = GetPartyMemberActualHealing(party_member);
            ImGui::Text("DPS: %u   Heal: %u   Taken: %u vs %u", dps, total_healing,
                        party_member.damage_taken_non_armor_ignoring, party_member.damage_taken_armor_ignoring);
            if (HasSkill(party_member, GW::Constants::SkillID::Mind_Burn) || HasSkill(party_member, GW::Constants::SkillID::Searing_Flames)) {
                const uint32_t burn_dps = GetBurnDPS(GetTimeMs());
                if (burn_dps > 0) {
                    ImGui::Text("Burn DPS: %u", burn_dps);
                }
            }
            DrawPartyMemberTimeline(party_member, false);
            ImGui::PopStyleVar();

            const float end_y = ImGui::GetCursorPosY();
            const float min_height = 160.0f;
            const float current_height = end_y - start_y;
            if (current_height < min_height) {
                ImGui::Dummy(ImVec2(0.0f, min_height - current_height));
            }
            if (print_by_click) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
                char button_name[32];
                snprintf(button_name, _countof(button_name), "###WriteStatistics%d", party_member.party_idx);
                const float height = ImGui::GetCursorPosY() - start_y;
                ImGui::SetCursorPosY(start_y);
                if (ImGui::Button(button_name, ImVec2(width, height)) && ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                    WritePlayerStatisticsAllSkills(&party_member);
                }
                ImGui::PopStyleVar();
            }
        }
    }

    /********************/
    /* Set Data Methods */
    /********************/

    void UnsetPartyStatistics()
    {
        for (const auto entry : party_members) {
            delete entry;
        }
        party_members.clear();
        enemy_groups.clear();
        enemy_groups_by_caster_id.clear();
        enemy_groups_by_player_number.clear();
        recent_battle_segments.clear();

        skill_names.clear();
        tracked_actions.clear();
        active_effects_by_agent.clear();
        total_burn_enemy_ms = 0;
        current_burn_enemy_count = 0;
        current_burn_last_update_timestamp = 0;
        party_combat_duration_ms_total = 0;
        party_combat_segment_start_timestamp = 0;
        party_last_damage_timestamp = 0;
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
            stats.autoattack_actual_damage = member->autoattack_actual_damage;
            stats.autoattack_healing = member->autoattack_healing;
            stats.autoattack_actual_healing = member->autoattack_actual_healing;
            stats.unattributable_count = member->unattributable_count;
            stats.unattributable_damage = member->unattributable_damage;
            stats.unattributable_healing = member->unattributable_healing;
            stats.unattributable_actual_damage = member->unattributable_actual_damage;
            stats.unattributable_actual_healing = member->unattributable_actual_healing;
            stats.actual_damage_total = member->actual_damage_total;
            stats.combat_duration_ms_total = member->combat_duration_ms_total;
            stats.combat_segment_start_timestamp = member->combat_segment_start_timestamp;
            stats.last_damage_timestamp = member->last_damage_timestamp;
            stats.damage_taken_armor_ignoring = member->damage_taken_armor_ignoring;
            stats.damage_taken_non_armor_ignoring = member->damage_taken_non_armor_ignoring;
            stats.skills.reserve(member->skills.size());
            stats.total_skills_used = member->total_skills_used;
            for (const auto& skill : member->skills) {
                stats.skills.push_back({skill.id, skill.count, skill.ended, skill.attributed, skill.damage, skill.healing, skill.actual_damage, skill.actual_healing});
            }
            saved.push_back(std::move(stats));
        }
        return saved;
    }

    void RestorePartyMemberStats(const std::vector<SavedPartyMemberStats>& saved_stats)
    {
        if (saved_stats.empty()) {
            return;
        }

        for (auto* party_member : party_members) {
            const auto found = std::find_if(saved_stats.begin(), saved_stats.end(), [party_member](const auto& saved) {
                return saved.party_idx == party_member->party_idx;
            });
            if (found == saved_stats.end()) {
                continue;
            }

            party_member->autoattack_count = found->autoattack_count;
            party_member->autoattack_damage = found->autoattack_damage;
            party_member->autoattack_actual_damage = found->autoattack_actual_damage;
            party_member->autoattack_healing = found->autoattack_healing;
            party_member->autoattack_actual_healing = found->autoattack_actual_healing;
            party_member->actual_damage_total = found->actual_damage_total;
            party_member->combat_duration_ms_total = found->combat_duration_ms_total;
            party_member->combat_segment_start_timestamp = found->combat_segment_start_timestamp;
            party_member->last_damage_timestamp = found->last_damage_timestamp;
            party_member->damage_taken_armor_ignoring = found->damage_taken_armor_ignoring;
            party_member->damage_taken_non_armor_ignoring = found->damage_taken_non_armor_ignoring;
            party_member->total_skills_used = found->total_skills_used;
            party_member->unattributable_count = found->unattributable_count;
            party_member->unattributable_damage = found->unattributable_damage;
            party_member->unattributable_healing = found->unattributable_healing;
            party_member->unattributable_actual_damage = found->unattributable_actual_damage;
            party_member->unattributable_actual_healing = found->unattributable_actual_healing;

            const size_t restore_count = std::min(party_member->skills.size(), found->skills.size());
            for (size_t skill_idx = 0; skill_idx < restore_count; ++skill_idx) {
                party_member->skills[skill_idx].count = found->skills[skill_idx].count;
                party_member->skills[skill_idx].ended = found->skills[skill_idx].ended;
                party_member->skills[skill_idx].attributed = found->skills[skill_idx].attributed;
                party_member->skills[skill_idx].damage = found->skills[skill_idx].damage;
                party_member->skills[skill_idx].healing = found->skills[skill_idx].healing;
                party_member->skills[skill_idx].actual_damage = found->skills[skill_idx].actual_damage;
                party_member->skills[skill_idx].actual_healing = found->skills[skill_idx].actual_healing;
            }
        }
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
                return static_cast<PartyMember*>(nullptr); // Can fail if game hasn't got all the goodies yet
            }
            // NB: Sanitising removes [henchman type] and player numbers
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
                /* Skillbar for other players and henchmen is unknown in outpost init with No_Skill */

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

        // Add player skills
        for (const GW::SkillbarSkill& skill : my_skillbar->skills) {
            set_member_skill(player_party_member, skill.skill_id);
        }

        // Clear out any party members that are no longer in the party.
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

    /************************/
    /* Chat Command Methods */
    /************************/

    void WritePlayerStatistics(const uint32_t player_idx = -1, const uint32_t skill_idx = -1)
    {
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) {
            return;
        }

        /* all skills for self player */
        if (static_cast<size_t>(-1) == player_idx) {
            WritePlayerStatisticsAllSkills(player_party_member);
            /* single skill for some player */
        }
        else if (std::numeric_limits<uint32_t>::max() != skill_idx) {
            WritePlayerStatisticsSingleSkill(GetPartyMemberByPartyIdx(player_idx), skill_idx);
            /* all skills for some player */
        }
        else {
            WritePlayerStatisticsAllSkills(GetPartyMemberByPartyIdx(player_idx));
        }
    }

    void CHAT_CMD_FUNC(CmdSkillStatistics)
    {
        /* command: /skillstats */
        /* will write the stats of the self player */
        if (argc < 2) {
            WritePlayerStatistics();
            return;
        }

        const std::wstring arg1 = TextUtils::ToLower(argv[1]);

        if (argc == 2) {
            /* command: /skillstats reset */
            if (arg1 == L"reset") {
                UnsetPartyStatistics();
                pending_party_members = true;
            }
            /* command: /skllstats playerNum */
            else {
                uint32_t player_number = 0;
                if (TextUtils::ParseUInt(argv[1], &player_number) && player_number > 0 &&
                    player_number <= party_members.size()) {
                    --player_number; // List will start at index zero
                    WritePlayerStatistics(player_number);
                }
                else {
                    Log::Error("Invalid player number '%ls', please use an integer value of 1 to %u", argv[1],
                               party_members.size() + 1);
                }
            }

            return;
        }

        /* command: /skillstats playerNum skillNum */
        if (argc >= 3) {
            uint32_t player_number = 0;
            if (TextUtils::ParseUInt(argv[1], &player_number) && player_number > 0 &&
                player_number <= party_members.size()) {
                --player_number;
                uint32_t skill_number = 0;
                if (TextUtils::ParseUInt(argv[2], &skill_number) && skill_number > 0 && skill_number < 9) {
                    --skill_number;
                    WritePlayerStatistics(player_number, skill_number);
                }
                else {
                    Log::Error("Invalid skill number '%ls', please use an integer value of 1 to 8", argv[2]);
                }
            }
            else {
                Log::Error("Invalid player number '%ls', please use an integer value of 1 to %u", argv[1],
                           party_members.size());
            }
        }
    }

    /********************/
    /* Callback Methods */
    /********************/

    void MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded*)
    {
        // Preserve party statistics across zoning, but reset enemy group timelines for the new map.
        in_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
        enemy_groups.clear();
        enemy_groups_by_caster_id.clear();
        enemy_groups_by_player_number.clear();
        pending_party_members = true;
    }

    PartyMember* GetPartyMemberByPetAgentId(const uint32_t agent_id)
    {
        if (agent_id == 0) return nullptr;
        const auto* agent = static_cast<const GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!(agent && agent->GetIsLivingType())) return nullptr;
        // Check if this agent is owned by a party member (e.g. pet, spirit, minion)
        if (agent->owner == 0) return nullptr;
        return GetPartyMemberByAgentId(agent->owner);
    }

    void GenericModifierCallback(GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet)
    {
        if (!packet) {
            return;
        }

        if (packet->cause_id != 0 && packet->cause_id == packet->target_id && packet->value < 0.0f &&
            (packet->type == GW::Packet::StoC::GenericValueID::damage ||
             packet->type == GW::Packet::StoC::GenericValueID::critical ||
             packet->type == GW::Packet::StoC::GenericValueID::armorignoring)) {
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

        auto cause_party_member = GetPartyMemberByAgentId(packet->cause_id);
        if (!cause_party_member) {
            cause_party_member = GetPartyMemberByPetAgentId(packet->cause_id);
        }
        const auto target_party_member = GetPartyMemberByAgentId(packet->target_id);

        if (target_party_member && packet->value < 0.0f) {
            AddPartyMemberDamageTaken(target_party_member, packet);
        }

        // Centralised combat damage tracking: all actual damage to enemies feeds
        // into actual_damage_total (and thus DPS) via this single call, using the
        // actual (non-overkill) damage amount computed from the target's current HP.
        const uint32_t actual_amount = GetActualDamageAmount(packet);
        if (cause_party_member && packet->value < 0.0f && actual_amount > 0) {
            AddPartyMemberCombatDamage(cause_party_member, actual_amount, GetTimeMs());
        }

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

        //if (!cause_party_member && !target_party_member) {
        //    return;
        //}

        if (!IsDamageOrHealingPacket(packet)) {
            return;
        }

        ProcessDamagePacket(packet, cause_party_member ? cause_party_member : target_party_member);
    }

    void AddEffectCallback(const GW::HookStatus*, const GW::Packet::StoC::AddEffect* packet)
    {
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
        SetCurrentBurnEnemyCount(CountBurningEnemies(), GetTimeMs());
    }

    void RemoveEffectCallback(const GW::HookStatus*, const GW::Packet::StoC::RemoveEffect* packet)
    {
        if (!packet) {
            return;
        }
        RemoveEffectTracker(packet->agent_id, packet->effect_id);
        SetCurrentBurnEnemyCount(CountBurningEnemies(), GetTimeMs());
    }

    void ProjectileLaunchedCallback(const GW::HookStatus*, const GW::Packet::StoC::AgentProjectileLaunched* packet)
    {
        if (packet && packet->agent_id != 0) {
            GetOrAddEnemyGroup(packet->agent_id);
        }
        AddProjectileTracker(packet);
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
        const auto target_party_member = GetPartyMemberByAgentId(actual_target_id);
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

        PartyMember* skill_party_member = caster_party_member ? caster_party_member : target_party_member;
        Skill* found_skill = nullptr;
        EnemyGroup* enemy_group = nullptr;
        if (enemy_caster) {
            enemy_group = GetOrAddEnemyGroup(agent_id);
            found_skill = GetOrAddEnemySkill(agent_id, activated_skill_id);
        } else {
            found_skill = GetOrAddSkill(skill_party_member, activated_skill_id);
        }
        if (!found_skill) {
            return;
        }
        if (is_start) {
            if (!enemy_caster) {
                skill_party_member->total_skills_used++;
            }
            found_skill->count++;
            if (value_id == GW::Packet::StoC::GenericValueID::instant_skill_activated) {
                const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(activated_skill_id);
                if (skill_data && !skill_data->target) {
                    found_skill->ended++;
                    auto it = last_skill_cast.find(agent_id);
                    if (it != last_skill_cast.end()) {
                        it->second.finished = true;
                        it->second.finish_timestamp = it->second.timestamp;
                    }
                }
            }
        } else if (is_end) {
            found_skill->ended++;
            const uint32_t finish_timestamp = GetTimeMs();
            FinishSkillCastTimelineEvent(agent_id, activated_skill_id, skill_target_id, finish_timestamp);
            AddSkillEndEvent(agent_id, activated_skill_id, skill_target_id, finish_timestamp);
            AddDotSkillEndEvents(agent_id, activated_skill_id, skill_target_id, finish_timestamp);
            ProcessPendingDamageEventsForSkillEnd({agent_id, activated_skill_id, skill_target_id, finish_timestamp});
        }
    }
} // namespace


/**********************/
/* Overridden Methods */
/**********************/

void PartyStatisticsWindow::Initialize()
{
    ToolboxWindow::Initialize();

    send_timer = TIMER_INIT();

    GW::Chat::CreateCommand(&ChatCmd_HookEntry,L"skillstats", CmdSkillStatistics);

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, &MapLoadedCallback);

    /* Skill on self or party player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet) -> void {
            const uint32_t value_id = packet->value_id;
            const uint32_t caster_id = packet->agent_id;
            constexpr uint32_t target_id = 0U;
            const uint32_t value = packet->value;
            constexpr bool no_target = true;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    /* Skill on enemy player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(
        &GenericValueTarget_Entry,
        [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* packet) -> void {
            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            constexpr bool no_target = false;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
        &GenericModifier_Entry,
        [this](const GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet) -> void {
            GenericModifierCallback(nullptr, packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentProjectileLaunched>(
        &ProjectileLaunched_Entry,
        [this](const GW::HookStatus* status, const GW::Packet::StoC::AgentProjectileLaunched* packet) -> void {
            ProjectileLaunchedCallback(status, packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AddEffect>(
        &AddEffect_Entry,
        [this](const GW::HookStatus* status, const GW::Packet::StoC::AddEffect* packet) -> void {
            AddEffectCallback(status, packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::RemoveEffect>(
        &RemoveEffect_Entry,
        [this](const GW::HookStatus* status, const GW::Packet::StoC::RemoveEffect* packet) -> void {
            RemoveEffectCallback(status, packet);
        });

    UnsetPartyStatistics();
    pending_party_members = true;
}

void PartyStatisticsWindow::Update(const float)
{
    PruneOldTimelineEvents();

    for (auto& damage : recent_damage_events) {
        if (!IsDamageEvent(damage) || !IsDamageReadyForAttribution(damage)) {
            continue;
        }
        auto* party_member = GetPartyMemberByAgentId(damage.cause_id);
        if (!party_member) {
            party_member = GetPartyMemberByPetAgentId(damage.cause_id);
        }
        if (party_member) {
            TryAttributeDamageEvent(damage, nullptr, party_member);
        } else {
            TryAttributeDamageEventToEnemyGroup(damage);
        }
    }

    if (pending_party_members && SetPartyMembers()) {
        pending_party_members = false;
    }

    if (constexpr auto time_diff_threshold = 600.0f; !chat_queue.empty() && TIMER_DIFF(send_timer) > time_diff_threshold) {
        send_timer = TIMER_INIT();
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) {
            return;
        }
        if (!GW::Agents::GetControlledCharacter()) {
            return;
        }

        GW::Chat::SendChat('#', chat_queue.front().c_str());
        chat_queue.pop();
    }
}

void PartyStatisticsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    if (!GW::Map::GetIsMapLoaded()) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (ImGuiViewport* viewport = ImGui::GetMainViewport()) {
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        for (const auto party_member : party_members) {
            DrawPartyMember(*party_member);
        }
    }
    ImGui::End();
}

void PartyStatisticsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    //LOAD_BOOL(show_abs_values);
    //LOAD_BOOL(show_perc_values);
    //LOAD_BOOL(print_by_click);
}

void PartyStatisticsWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    //SAVE_BOOL(show_abs_values);
    //SAVE_BOOL(show_perc_values);
    //SAVE_BOOL(print_by_click);
}

void PartyStatisticsWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Show the absolute skill count", &show_abs_values);
    ImGui::SameLine();
    ImGui::Checkbox("Show the percentage skill count", &show_perc_values);
    ImGui::SameLine();
    ImGui::Checkbox("Print skill statistics by Ctrl+LeftClick", &print_by_click);
}

void PartyStatisticsWindow::Terminate()
{
    ToolboxWindow::Terminate();

    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);

    GW::StoC::RemoveCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValue>(&GenericValueSelf_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AddEffect>(&AddEffect_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::RemoveEffect>(&RemoveEffect_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentProjectileLaunched>(&ProjectileLaunched_Entry);

    UnsetPartyStatistics();
}
