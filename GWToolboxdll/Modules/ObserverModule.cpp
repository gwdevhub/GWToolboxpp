#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>


#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWToolbox.h>
#include <Utils/GuiUtils.h>

#include <Modules/ObserverModule.h>

#include <Logger.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

namespace {

    ObserverModule::Settings settings;

    GW::HookEntry ChatCmd_HookEntry;

    void CHAT_CMD_FUNC(CmdObserverReset)
    {
        ObserverModule::Instance().Reset();
    }

    bool IsResurrectionSkill(const GW::Constants::SkillID skill_id) {
        const auto s = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        return s && s->IsResurrectionSkill();
    }

} // namespace

namespace ObserverLabel {
    const char* Profession = "Prf";
    const char* Name = "Name";
    const char* PlayerGuildTag = "Tag";
    const char* PlayerGuildRating = "Rtg";
    const char* PlayerGuildRank = "Rnk";
    const char* Kills = "K";
    const char* Deaths = "D";
    const char* KDR = "KDR";
    const char* Attempts = "Atm";
    const char* Integrity = "Dbg";
    const char* Cancels = "C";
    const char* Interrupts = "I";
    const char* Knockdowns = "Kd";
    const char* Finishes = "F";
    const char* AttacksReceivedFromOtherParties = "A-";
    const char* AttacksDealtToOtherParties = "A+";
    const char* CritsReceivedFromOtherParties = "Cr-";
    const char* CritsDealToOtherParties = "Cr+";
    const char* SkillsReceivedFromOtherParties = "Sk-";
    const char* SkillsUsedOnOtherParties = "Sk+";
    const char* SkillsUsed = "Sk";
}; // namespace ObserverLabel


void ObserverModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);

    is_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    is_observer = GW::Map::GetIsObserving();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Entry, [this](const GW::HookStatus* status, const GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
        HandleInstanceLoadInfo(status, packet);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::JumboMessage>(&JumboMessage_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::JumboMessage* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }
        HandleJumboMessage(packet->type, packet->value);
    });

    // Hook for countdown start - this fires twice: once at map load, once when match actually starts
    GW::StoC::RegisterPacketCallback(&CountdownStart_Entry, GAME_SMSG_INSTANCE_COUNTDOWN, [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) -> void {
        if (!IsActive()) {
            return;
        }
        
        const uint32_t instance_time = GW::Map::GetInstanceTime();
        
        if (!first_countdown_seen) {
            // First countdown at map load - reset data for new match
            Reset();
            first_countdown_seen = true;
            if (!InitializeObserverSession(observed_map_id)) {
                return;
            }
        } else {
            // Second countdown - this is the actual match start
            match_start_instance_time = instance_time;
            // Capture the map at match start - use the map from InitializeObserverSession which uses the correct map
            if (map && !match_start_map) {
                const auto map_id = map->map_id;
                const GW::AreaInfo* area_info = GW::Map::GetMapInfo(map_id);
                if (area_info) {
                    match_start_map = new ObservableMap(map_id, *area_info);
                }
            }
        }
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(&AgentState_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::AgentState* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }
        HandleAgentState(packet->agent_id, packet->state);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::AgentAdd* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }
        HandleAgentAdd(packet->agent_id);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentProjectileLaunched>(&AgentProjectileLaunched_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::AgentProjectileLaunched* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }
        HandleAgentProjectileLaunched(packet);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(&GenericModifier_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericModifier* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }

        const uint32_t value_id = packet->type;
        const uint32_t caster_id = packet->cause_id;
        const uint32_t target_id = packet->target_id;
        const float value = packet->value;
        constexpr bool no_target = false;
        HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }

        const uint32_t value_id = packet->Value_id;
        const uint32_t caster_id = packet->caster;
        const uint32_t target_id = packet->target;
        const uint32_t value = packet->value;
        constexpr bool no_target = false;
        HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValue_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }

        const uint32_t value_id = packet->value_id;
        const uint32_t caster_id = packet->agent_id;
        constexpr uint32_t target_id = NO_AGENT;
        const uint32_t value = packet->value;
        constexpr bool no_target = true;
        HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericFloat>(&GenericFloat_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericFloat* packet) -> void {
        if (!IsActive()) {
            return;
        }
        if (!InitializeObserverSession()) {
            return;
        }

        const uint32_t value_id = packet->type;
        const uint32_t caster_id = packet->agent_id;
        constexpr uint32_t target_id = NO_AGENT;
        const float value = packet->value;
        constexpr bool no_target = true;
        HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    if (IsActive() && !observer_session_initialized) {
        InitializeObserverSession();
    }

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"observer:reset", CmdObserverReset);
}

void ObserverModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void ObserverModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void ObserverModule::Terminate()
{
    ToolboxModule::Terminate();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    Reset();

    // TODO: Clear stoc callbacks
}


// Is the Module actively tracking agents?
const bool ObserverModule::IsActive() const
{
    // an observer match is considered an explorable area
    return settings.is_enabled && is_explorable && (settings.enable_in_explorable_areas || is_observer);
}


// Handle InstanceLoadInfo Packet
void ObserverModule::HandleInstanceLoadInfo(const GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* packet)
{
    is_explorable = packet->is_explorable;
    is_observer = packet->is_observer;

    const bool is_active = IsActive();
    
    // Store the observed map ID (this is the actual match map in observer mode, not your physical location)
    observed_map_id = static_cast<GW::Constants::MapID>(packet->map_id);

    if (is_active) {
        // Reset countdown flag for new map
        first_countdown_seen = false;
        // Don't call Reset() here to preserve match data for export after leaving the map
        // Users can explicitly call /observer:reset when they want to start tracking a new match
        InitializeObserverSession(observed_map_id);
    }
    else {
        // mark false so we initialize next time we load a session
        observer_session_initialized = false;
    }
}


// Handle a JumboMessage packet
void ObserverModule::HandleJumboMessage(const uint8_t type, const uint32_t value)
{
    switch (type) {
        case GW::Packet::StoC::JumboMessageType::MORALE_BOOST:
            HandleMoraleBoost(GetObservablePartyById(JumboMessageValueToPartyId(value)));
            break;
        case GW::Packet::StoC::JumboMessageType::CAPTURED_SHRINE:
            HandleShrineCapture(GetObservablePartyById(JumboMessageValueToPartyId(value)));
            break;
        case GW::Packet::StoC::JumboMessageType::CAPTURED_TOWER:
            HandleTowerCapture(GetObservablePartyById(JumboMessageValueToPartyId(value)));
            break;
        case GW::Packet::StoC::JumboMessageType::VICTORY:
        case GW::Packet::StoC::JumboMessageType::FLAWLESS_VICTORY: {
            const uint32_t party_id = JumboMessageValueToPartyId(value);
            HandleVictory(GetObservablePartyById(party_id));
            break;
        }
    }
}


// Handle a GenericPacket of type float
void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id, const float value, const bool)
{
    switch (value_id) {
        case GW::Packet::StoC::GenericValueID::damage:
            HandleDamageDone(caster_id, target_id, value, false);
            break;

        case GW::Packet::StoC::GenericValueID::critical:
            HandleDamageDone(caster_id, target_id, value, true);
            break;

        case GW::Packet::StoC::GenericValueID::armorignoring:
            // Check if it's healing (positive) or damage (negative)
            if (value >= 0) {
                HandleHealingDone(caster_id, target_id, value);
            }
            else {
                HandleDamageDone(caster_id, target_id, value, false);
            }
            break;

        case GW::Packet::StoC::GenericValueID::knocked_down:
            HandleKnockedDown(caster_id, value);
            break;
    }
}

// Handle a Generic Packet of type uint32_t
void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id, const uint32_t value, const bool no_target)
{
    switch (value_id) {
        case GW::Packet::StoC::GenericValueID::melee_attack_finished:
            HandleAttackFinished(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_stopped:
            HandleAttackStopped(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_started: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id; // 0
            }
            else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            // handle
            HandleAttackStarted(_caster_id, _target_id);
            break;
        }

        case GW::Packet::StoC::GenericValueID::interrupted:
            HandleInterrupted(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_skill_finished:
            HandleAttackSkillFinished(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::instant_skill_activated:
            HandleInstantSkillActivated(caster_id, target_id, static_cast<GW::Constants::SkillID>(value));
            break;

        case GW::Packet::StoC::GenericValueID::attack_skill_stopped:
            HandleAttackSkillStopped(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::attack_skill_activated: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id; // 0
            }
            else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            // handle
            HandleAttackSkillStarted(_caster_id, _target_id, static_cast<GW::Constants::SkillID>(value));
            break;
        }

        case GW::Packet::StoC::GenericValueID::skill_finished:
            HandleSkillFinished(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::skill_stopped:
            HandleSkillStopped(caster_id);
            break;

        case GW::Packet::StoC::GenericValueID::skill_activated: {
            // TODO: do location effecs cause entry here?
            // if so, Isle of the Dead, Burning Isle, Isle of Meditation,
            // Frozen Isle, Isle of Weeping Stone, etc... might slow down
            // our application by coming in here 10,000 times
            // TODO: verify whether we need to check for NO_AGENT on caster,
            // or for no living agent...

            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id; // 0
            }
            else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            HandleSkillActivated(_caster_id, _target_id, static_cast<GW::Constants::SkillID>(value));
            break;
        }
    }
}


// Handle AgentState Packet
// Fired when the server notifies us of an agent state change
// Can tell us if the agent has just died or been resurrected
void ObserverModule::HandleAgentState(const uint32_t agent_id, const uint32_t state)
{
    ObservableAgent* observable_agent = GetObservableAgentById(agent_id);
    if (!observable_agent) {
        return;
    }

    ObservableParty* party = GetObservablePartyById(observable_agent->party_id);
    
    // 16 = dead
    const bool is_now_dead = (state == 16);
    
    // Check for resurrection (was dead, now alive)
    if (observable_agent->is_dead && !is_now_dead) {
        const uint32_t instance_time = GW::Map::GetInstanceTime();
        // Use time relative to match start if available, otherwise use instance time
        const uint32_t match_time = match_start_instance_time > 0 ? (instance_time - match_start_instance_time) : instance_time;
        
        // Determine resurrection type based on what caused the resurrection
        ResurrectionType res_type;
        uint32_t resurrector_id = NO_AGENT;
        
        if (observable_agent->last_resurrector != NO_AGENT) {
            // A resurrection skill was just used
            res_type = ResurrectionType::Skill;
            resurrector_id = observable_agent->last_resurrector;
        } 
        else {
            // No skill involved - must be base resurrection
            res_type = ResurrectionType::BaseResurrection;
            resurrector_id = NO_AGENT;
        }
        
        observable_agent->resurrection_events.emplace_back(
            match_time,
            resurrector_id,
            res_type
        );
        observable_agent->is_dead = false;
        observable_agent->last_resurrector = NO_AGENT;
        return; // Don't process as death
    }
    
    // Check for death
    if (!is_now_dead) {
        return; // Not a death event
    }

    // don't credit kills/deaths on parties that are already defeated
    // after a party is defeated all their players die, but we don't
    // count those deaths / kills
    if (match_finished) {
        return;
    }

    if (party && party->is_defeated) {
        return;
    }

    // Record death event with timestamp and coordinates
    const uint32_t instance_time = GW::Map::GetInstanceTime();
    // Use time relative to match start if available, otherwise use instance time
    const uint32_t match_time = match_start_instance_time > 0 ? (instance_time - match_start_instance_time) : instance_time;
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    float pos_x = 0.0f, pos_y = 0.0f;
    if (agent) {
        pos_x = agent->pos.x;
        pos_y = agent->pos.y;
    }
    
    observable_agent->death_events.emplace_back(
        match_time,
        pos_x,
        pos_y,
        observable_agent->last_hit_by,
        static_cast<uint32_t>(observable_agent->last_damage_skill_id),
        observable_agent->is_npc
    );
    
    observable_agent->is_dead = true; // Mark as dead

    // notify the player
    observable_agent->stats.HandleDeath();

    // only grant a kill if the victim belonged to a party
    // (don't count footmen/archer/bodyguard/lord/ghostly kills)
    if (!party) {
        return;
    }
    party->stats.HandleDeath();

    // credit the kill to the last-hitter and their party,
    ObservableAgent* killer = GetObservableAgentById(observable_agent->last_hit_by);
    if (!killer) {
        return;
    }
    killer->stats.HandleKill();
    ObservableParty* killer_party = GetObservablePartyById(killer->party_id);
    if (killer_party) {
        killer_party->stats.HandleKill();
    }
}


// Helper function to get or cache max HP for an agent
// Returns 530 if unable to determine max HP
// GWCA doesn't allow us to retrieve MAX Hp for every player, only currently observed player
// as specified in the Agent.h structure.
// But every damage / heal done to an agent is sent as a percentage of the player max hp.
// it's then necessary to capture this information to calculate the dmg / heal value.
// Get hardcoded max HP for known NPCs based on sanitized name
uint32_t ObserverModule::GetNPCMaxHP(uint32_t agent_id)
{
    ObservableAgent* agent = GetObservableAgentById(agent_id);
    if (!agent || agent->is_player) {
        return 0;
    }

    // Get sanitized name and compare against known NPC names
    const std::string name = agent->SanitizedName();
    
    if (name == "Footman" || name == "Archer" || name == "Knight" || name == "Bodyguard") {
        return 480;
    }
    if (name == "Guild Lord") {
        return 1680;
    }
    
    return 0;
}

// This means every damage and heal calculations are approximations of the real value,
// as the player could have switched gear (defensive set for exemple) and have more or less hp than the current max hp value
// stored in the cache. But I can't find a better way to do it for now.
uint32_t ObserverModule::GetOrCacheMaxHP(const uint32_t agent_id)
{
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (!agent) {
        return 0;
    }

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (!living) {
        return 0;
    }

    // Try to get max_hp directly (only works for currently observed agent in observer mode)
    if (living->max_hp > 0 && living->max_hp < 100000) {
        agent_max_hp_cache[agent_id] = living->max_hp;
        return living->max_hp;
    }

    const auto it = agent_max_hp_cache.find(agent_id);
    if (it != agent_max_hp_cache.end()) {
        return it->second;
    }

    // Check for hardcoded NPC HP values
    const uint32_t npc_hp = GetNPCMaxHP(agent_id);
    if (npc_hp > 0) {
        agent_max_hp_cache[agent_id] = npc_hp;
        return npc_hp;
    }

    // Super abritrary value.
    // TODO: maybe have default hp per profession?
    const uint32_t default_hp = 530;
    agent_max_hp_cache[agent_id] = default_hp;
    return default_hp;
}


// Handle DamageDone (GenericModifier float Packet)
void ObserverModule::HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit)
{
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    ObservableAgent* target = GetObservableAgentById(target_id);

    // get last hit to credit the kill
    if (target && caster && caster->party_id != NO_PARTY && amount_pc < 0) {
        target->last_hit_by = caster->agent_id;
    }

    // Calculate actual damage amount
    // @see GetOrCacheMaxHP to understand why we need to do that.
    uint32_t damage_amount = 0;
    if (amount_pc < 0 && target) {
        const uint32_t max_hp = GetOrCacheMaxHP(target_id);
        damage_amount = static_cast<uint32_t>(std::lround(-amount_pc * max_hp));
    }

    if (damage_amount > 0 && caster && target) {
        ObservableParty* caster_party = GetObservablePartyById(caster->party_id);
        ObservableParty* target_party = GetObservablePartyById(target->party_id);

        GW::Constants::SkillID skill_id = NO_SKILL;
        if (caster->current_target_action && caster->current_target_action->is_skill) {
            skill_id = caster->current_target_action->skill_id;
        }
        
        // Track last damage skill for death tracking
        if (skill_id != NO_SKILL) {
            target->last_damage_skill_id = skill_id;
        }

        // Update caster stats
        caster->stats.total_damage_dealt += damage_amount;
        if (target_party) {
            caster->stats.total_party_damage_dealt += damage_amount;
        }
        caster->stats.LazyGetDamageDealedAgainst(target_id) += damage_amount;

        // Update caster party stats
        if (caster_party) {
            caster_party->stats.total_damage_dealt += damage_amount;
            if (target_party) {
                caster_party->stats.total_party_damage_dealt += damage_amount;
            }
        }

        // Update target stats
        target->stats.total_damage_received += damage_amount;
        if (caster_party) {
            target->stats.total_party_damage_received += damage_amount;
        }
        target->stats.LazyGetDamageReceivedFrom(caster_id) += damage_amount;

        // Update target party stats
        if (target_party) {
            target_party->stats.total_damage_received += damage_amount;
            if (caster_party) {
                target_party->stats.total_party_damage_received += damage_amount;
            }
        }

        // Track damage by skill
        if (skill_id != NO_SKILL) {
            caster->stats.LazyGetDamageBySkill(skill_id) += damage_amount;
            caster->stats.LazyGetDamageBySkillToAgent(target_id, skill_id) += damage_amount;
            target->stats.LazyGetDamageFromSkillFromAgent(caster_id, skill_id) += damage_amount;

            // Also update the skill's ObservedAction if it exists
            auto it_skill = caster->stats.skills_used.find(skill_id);
            if (it_skill != caster->stats.skills_used.end()) {
                it_skill->second->total_damage += damage_amount;
            }
        }
    }

    if (is_crit) {
        ObservableParty* caster_party = nullptr;
        ObservableParty* target_party = nullptr;
        if (caster) {
            caster_party = GetObservablePartyById(caster->party_id);
        }
        if (target) {
            target_party = GetObservablePartyById(target->party_id);
        }

        // notify the caster
        if (caster) {
            caster->stats.total_crits_dealt += 1;
            if (target_party) {
                caster->stats.total_party_crits_dealt += 1;
            }
        }

        // notify the caster_party
        if (caster_party) {
            caster_party->stats.total_crits_dealt += 1;
            if (target_party) {
                caster_party->stats.total_party_crits_dealt += 1;
            }
        }

        // notify the target
        if (target) {
            target->stats.total_crits_received += 1;
            if (caster_party) {
                target->stats.total_party_crits_received += 1;
            }
        }

        // notify the target_party
        if (target_party) {
            target_party->stats.total_crits_received += 1;
            if (caster_party) {
                target_party->stats.total_party_crits_received += 1;
            }
        }
    }
}


// Handle HealingDone (GenericModifier float Packet)
void ObserverModule::HandleHealingDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc)
{
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    ObservableAgent* target = GetObservableAgentById(target_id);

    // Calculate actual healing amount
    // @see GetOrCacheMaxHP to understand why we need to do that.
    uint32_t healing_amount = 0;
    if (amount_pc > 0 && target) {
        const uint32_t max_hp = GetOrCacheMaxHP(target_id);
        healing_amount = static_cast<uint32_t>(std::lround(amount_pc * max_hp));
    }

    // Track healing if we have a valid amount
    if (healing_amount > 0 && caster && target) {
        ObservableParty* caster_party = GetObservablePartyById(caster->party_id);
        ObservableParty* target_party = GetObservablePartyById(target->party_id);

        // Get the skill being used (if any)
        GW::Constants::SkillID skill_id = NO_SKILL;
        if (caster->current_target_action && caster->current_target_action->is_skill) {
            skill_id = caster->current_target_action->skill_id;
        }

        // Update caster stats
        caster->stats.total_healing_dealt += healing_amount;
        if (target_party) {
            caster->stats.total_party_healing_dealt += healing_amount;
        }
        caster->stats.LazyGetHealingDealedTo(target_id) += healing_amount;

        // Update caster party stats
        if (caster_party) {
            caster_party->stats.total_healing_dealt += healing_amount;
            if (target_party) {
                caster_party->stats.total_party_healing_dealt += healing_amount;
            }
        }

        // Update target stats
        target->stats.total_healing_received += healing_amount;
        if (caster_party) {
            target->stats.total_party_healing_received += healing_amount;
        }
        target->stats.LazyGetHealingReceivedFrom(caster_id) += healing_amount;

        // Update target party stats
        if (target_party) {
            target_party->stats.total_healing_received += healing_amount;
            if (caster_party) {
                target_party->stats.total_party_healing_received += healing_amount;
            }
        }

        // Track healing by skill
        if (skill_id != NO_SKILL) {
            caster->stats.LazyGetHealingBySkill(skill_id) += healing_amount;
            caster->stats.LazyGetHealingBySkillToAgent(target_id, skill_id) += healing_amount;
            target->stats.LazyGetHealingFromSkillFromAgent(caster_id, skill_id) += healing_amount;
        }
    }
}


// Handle AgentAdd Packet
// Fired when an Agent is to be loaded into memory
void ObserverModule::HandleAgentAdd(const uint32_t)
{
    // queue update parties
    party_sync_timer = TIMER_INIT();
}


// Handle AgentProjectileLaunched Packet
// can be used to determine when a ranged attack has finished
void ObserverModule::HandleAgentProjectileLaunched(const GW::Packet::StoC::AgentProjectileLaunched* packet)
{
    if (!packet) {
        return;
    }
    ObservableAgent* agent = GetObservableAgentById(packet->agent_id);
    // ensure the projectile was from an attack we're currently undertaking
    if (!agent || !agent->current_target_action || !agent->current_target_action->is_attack) {
        return;
    }
    ReduceAction(agent, ActionStage::Finished);
}


// Handle AttackFinished Packet
void ObserverModule::HandleAttackFinished(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Finished);
}

// Handle AttackStopped Packet
void ObserverModule::HandleAttackStopped(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Stopped);
}


// Handle AttackStarted Packet
void ObserverModule::HandleAttackStarted(const uint32_t caster_id, const uint32_t target_id)
{
    const auto action = new TargetAction(caster_id, target_id, true, false, NO_SKILL);
    if (!ReduceAction(GetObservableAgentById(caster_id), ActionStage::Started, action)) {
        delete action;
    }
}


// Handle Interrupted Packet
void ObserverModule::HandleInterrupted(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Interrupted);
}

// Handle Attack SkillFinished Packet
void ObserverModule::HandleAttackSkillFinished(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Finished);
}


// Handle Attack SkillStopped Packet
void ObserverModule::HandleAttackSkillStopped(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Stopped);
}


// Handle InstantSkillActivated Packet
void ObserverModule::HandleInstantSkillActivated(const uint32_t caster_id, const uint32_t target_id, const GW::Constants::SkillID skill_id)
{
    // assuming there are no instant attack skills...
    const auto action = new TargetAction(caster_id, target_id, false, true, skill_id);
    if (!ReduceAction(GetObservableAgentById(caster_id), ActionStage::Instant, action)) {
        delete action;
    }
}


// Handle AttackSkillActivated Packet
void ObserverModule::HandleAttackSkillStarted(const uint32_t caster_id, const uint32_t target_id, const GW::Constants::SkillID skill_id)
{
    const auto action = new TargetAction(caster_id, target_id, true, true, skill_id);
    if (!ReduceAction(GetObservableAgentById(caster_id), ActionStage::Started, action)) {
        delete action;
    }
}


// Handle AttackSkillFinished Packet
void ObserverModule::HandleSkillFinished(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Finished);
}


// Handle SkillFinished Packet
void ObserverModule::HandleSkillStopped(const uint32_t agent_id)
{
    ReduceAction(GetObservableAgentById(agent_id), ActionStage::Stopped);
}


// Handle SkillActivated Packet
void ObserverModule::HandleSkillActivated(const uint32_t caster_id, const uint32_t target_id, const GW::Constants::SkillID skill_id)
{
    const auto action = new TargetAction(caster_id, target_id, false, true, skill_id);
    if (!ReduceAction(GetObservableAgentById(caster_id), ActionStage::Started, action)) {
        delete action;
    }
}


// Handle KnockedDown Packet
void ObserverModule::HandleKnockedDown(const uint32_t agent_id, const float duration)
{
    // notify the agent
    ObservableAgent* agent = GetObservableAgentById(agent_id);
    if (!agent) {
        return;
    }
    agent->stats.knocked_down_count += 1;
    agent->stats.knocked_down_duration += duration;

    // notify the agents party
    ObservableParty* party = GetObservablePartyById(agent->party_id);
    if (!party) {
        return;
    }
    party->stats.knocked_down_count += 1;
    party->stats.knocked_down_duration += 1;
}


// Convert a JumboMessage value to a party_id
uint32_t ObserverModule::JumboMessageValueToPartyId(const uint32_t value)
{
    // TODO: handle maps with 3 parties where the JumboMessageValue's are different
    switch (value) {
        case GW::Packet::StoC::JumboMessageValue::PARTY_ONE:
            return 1;
        case GW::Packet::StoC::JumboMessageValue::PARTY_TWO:
            return 2;
        default:
            return NO_PARTY;
    }
}


// Fired when a party receives a morale boost
void ObserverModule::HandleMoraleBoost(ObservableParty* boosting_party)
{
    if (!boosting_party) {
        return;
    }
    const uint32_t instance_time = GW::Map::GetInstanceTime();
    // Use time relative to match start if available, otherwise use instance time
    const uint32_t match_time = match_start_instance_time > 0 ? (instance_time - match_start_instance_time) : instance_time;
    boosting_party->morale_boosts.emplace_back(match_time);
}


// Fired when a party captures a shrine
void ObserverModule::HandleShrineCapture(ObservableParty* capturing_party)
{
    if (!capturing_party) {
        return;
    }
    const uint32_t instance_time = GW::Map::GetInstanceTime();
    // Use time relative to match start if available, otherwise use instance time
    const uint32_t match_time = match_start_instance_time > 0 ? (instance_time - match_start_instance_time) : instance_time;
    capturing_party->shrine_captures.emplace_back(match_time);
}


// Fired when a party captures a tower
void ObserverModule::HandleTowerCapture(ObservableParty* capturing_party)
{
    if (!capturing_party) {
        return;
    }
    const uint32_t instance_time = GW::Map::GetInstanceTime();
    // Use time relative to match start if available, otherwise use instance time
    const uint32_t match_time = match_start_instance_time > 0 ? (instance_time - match_start_instance_time) : instance_time;
    capturing_party->tower_captures.emplace_back(match_time);
}


// Fired when a party is victorious
void ObserverModule::HandleVictory(ObservableParty* winning_party)
{
    // TODO: handle draws
    // There is no JumboMessage for a draw so we don't get notified of it...

    // Draws mess up:
    //  - the kills/deaths since everyone dies at the end and we don't know that
    //    the match is over... so we have to count all those as kills/deaths
    //  - we can't set match_finished = true
    //  - we can't get the match duration

    // match has concluded
    // save winner and match duration
    match_finished = true;

    // notify the winning party
    winning_party_id = winning_party->party_id;
    winning_party->is_defeated = false;
    winning_party->is_victorious = true;

    // note the final game duration
    // Calculate from actual match start time if available, otherwise use 60s offset
    const uint32_t instance_time = GW::Map::GetInstanceTime();
    const uint32_t ms = match_start_instance_time > 0 
        ? (instance_time - match_start_instance_time) 
        : (instance_time - 1000 * 60);
    match_duration_ms_total = std::chrono::milliseconds(ms);
    match_duration_ms = std::chrono::milliseconds(ms);
    match_duration_secs = std::chrono::duration_cast<std::chrono::seconds>(match_duration_ms);
    match_duration_ms -= std::chrono::duration_cast<std::chrono::milliseconds>(match_duration_secs);
    match_duration_mins = std::chrono::duration_cast<std::chrono::minutes>(match_duration_secs);
    match_duration_secs -= std::chrono::duration_cast<std::chrono::seconds>(match_duration_mins);

    // notify other parties that they lost
    for (auto& [_, losing_party] : observable_parties) {
        if (losing_party && losing_party->party_id != winning_party->party_id) {
            losing_party->is_defeated = true;
            losing_party->is_victorious = false;
        }
    }
}


bool ObserverModule::ReduceAction(ObservableAgent* caster, const ActionStage stage, TargetAction* new_action)
{
    // if the action ends up owned by the caster, the observermodule is responsible for garbage collecting the action
    // if the action ends up NOT owned by the caster, the caller is responsible for garbage collecting the action
    bool action_ownership_transferred = false;

    if (!caster) {
        return action_ownership_transferred;
    }

    TargetAction* action;

    // If starting a new action, delete the last stored action & store this new action
    if (new_action) {
        ASSERT(stage == ActionStage::Started || stage == ActionStage::Instant);
        // starting a new action

        // "instant" actions do not persist (don't received a "finished" packet) so we don't store them on the agent
        // and they may be activateable while using other skills (e.g. shouts/stances) so we don't clear the current action
        if (stage != ActionStage::Instant) {
            // delete the previous blocking action

            delete caster->current_target_action;

            // store the new blocking action
            caster->current_target_action = new_action;
            action_ownership_transferred = true;
        }
        action = new_action;
    }
    else {
        ASSERT(!(stage == ActionStage::Started || stage == ActionStage::Instant));
        // we are finishing the previous action
        // we have to keep the current_target_action on the caster in-case we receive an "interrupted" packet next
        // after a "stopped" package
        action = caster->current_target_action;
    }

    if (!action) {
        return action_ownership_transferred;
    }

    // if the action was already "finished" in a previous ReduceAction call, there's nothing else to do
    // this is important for skills like Dual Shot, Barrage, etc, where one skill leads to
    // multiple "AttackFinished" packets (via the "AgentProjectileLaunched" packet)
    if (action->was_finished) {
        return action_ownership_transferred;
    }

    if (stage == ActionStage::Stopped) {
        action->was_stopped = true;
    }
    if (stage == ActionStage::Finished) {
        action->was_finished = true;
    }

    ObservableAgent* target = GetObservableAgentById(action->target_id);

    ObservableParty* caster_party = GetObservablePartyById(caster->party_id);
    ObservableParty* target_party = nullptr;
    if (target) {
        target_party = GetObservablePartyById(target->party_id);
    }

    // notify caster & caster_party of interrupt
    //
    // interrupt packet comes after cancelled packet most of the time.
    //
    // Can received a "cancelled" packet after a "finished" packet for attack skills where the target
    // dies during the afterswing of a "finished" attack
    // we don't count that as "stopped/cancelled"
    if (stage == ActionStage::Interrupted) {
        if (caster) {
            if (action->was_stopped) {
                caster->stats.cancelled_count -= 1;
            }
            caster->stats.interrupted_count += 1;
        }
        if (caster_party) {
            if (action->was_stopped) {
                caster_party->stats.cancelled_count -= 1;
            }
            caster_party->stats.interrupted_count += 1;
        }
        if (action->is_skill) {
            if (caster) {
                if (action->was_stopped) {
                    caster->stats.cancelled_skills_count -= 1;
                }
                caster->stats.interrupted_skills_count += 1;
            }
            if (caster_party) {
                if (action->was_stopped) {
                    caster_party->stats.cancelled_skills_count -= 1;
                }
                caster_party->stats.interrupted_skills_count += 1;
            }
        }
    }

    // notify & caster_party caster of cancel
    if (stage == ActionStage::Stopped) {
        if (caster) {
            caster->stats.cancelled_count += 1;
        }
        if (caster_party) {
            caster_party->stats.cancelled_count += 1;
        }
        if (action->is_skill) {
            if (caster) {
                caster->stats.cancelled_skills_count += 1;
            }
            if (caster_party) {
                caster_party->stats.cancelled_skills_count += 1;
            }
        }
    }

    // handle attack
    if (action->is_attack) {
        // update the caster
        if (caster) {
            caster->stats.total_attacks_dealt.Reduce(action, stage);
            if (target) {
                caster->stats.LazyGetAttacksDealedAgainst(target->agent_id).Reduce(action, stage);
            }
            // if the target belonged to a party, the caster just attacked that other party
            if (target_party) {
                caster->stats.total_attacks_dealt_to_other_parties.Reduce(action, stage);
            }
        }

        // update the casters party
        if (caster_party) {
            caster_party->stats.total_attacks_dealt.Reduce(action, stage);
            // if the target belonged to a party, the casters party just attacked that other party
            if (target_party) {
                caster_party->stats.total_attacks_dealt_to_other_parties.Reduce(action, stage);
            }
        }

        // update the target
        if (target) {
            target->stats.total_attacks_received.Reduce(action, stage);
            if (caster) {
                target->stats.LazyGetAttacksReceivedFrom(caster->agent_id).Reduce(action, stage);
            }
            // if the caster belonged to a party, the target was just attacked by that other party
            if (caster_party) {
                target->stats.total_attacks_received_from_other_parties.Reduce(action, stage);
            }
        }

        // update the targets party
        if (target_party) {
            target_party->stats.total_attacks_received.Reduce(action, stage);
            // if the caster belonged to a party, the target_party was just attacked by that other party
            if (caster_party) {
                target_party->stats.total_attacks_received_from_other_parties.Reduce(action, stage);
            }
        }
    }

    // handle skill
    if (action->is_skill) {
        // update skill
        ObservableSkill* skill = GetObservableSkillById(action->skill_id);
        ASSERT(skill != nullptr);

        // Modify the effective `target` and `target_party`, based on the
        // targetting type of the skill to make stats more intuitive.

        // For example if using Heal Burst on yourself, the packet only
        // includes a caster and no target.
        // here we effectively set the
        // target to the caster.
        switch (static_cast<TargetType>(skill->gw_skill.target)) {
            case TargetType::no_target: {
                // don't provide a target
                // e.g. flash enchantments, stances, whirlwind
                break;
            }
            case TargetType::anyone: {
                // Ensure we interpret the target correctly
                // e.g. Mirror of Ice, Stone Sheath
                if (action->target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case TargetType::ally: {
                // Ensure we interpret the target correctly
                // e.g. Healing Burst
                if (action->target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case TargetType::other_ally: {
                // should always have a target
                break;
            }
            case TargetType::enemy: {
                // should always have a target
                break;
            }
        }
        const bool same_team = caster && target && caster->team_id != NO_TEAM && caster->team_id == target->team_id;
        const bool same_party = target_party && caster_party && target_party->party_id == caster_party->party_id;

        // notify the skill
        skill->stats.total_usages.Reduce(action, stage);
        if (target) {
            // target usages
            if (caster == target) {
                skill->stats.total_self_usages.Reduce(action, stage);
            }
            else {
                skill->stats.total_other_usages.Reduce(action, stage);
            }

            // team usages
            if (same_team) {
                skill->stats.total_own_team_usages.Reduce(action, stage);
            }
            else {
                skill->stats.total_other_team_usages.Reduce(action, stage);
            }
        }
        if (target_party) {
            // party usages
            if (caster_party == target_party) {
                skill->stats.total_own_party_usages.Reduce(action, stage);
            }
            else {
                skill->stats.total_other_party_usages.Reduce(action, stage);
            }
        }

        // notify the caster
        if (caster) {
            caster->stats.total_skills_used.Reduce(action, stage);
            caster->stats.LazyGetSkillUsed(action->skill_id).Reduce(action, stage);

            // used against a target?
            if (target) {
                // use against agent
                caster->stats.LazyGetSkillUsedOn(target->agent_id, action->skill_id).Reduce(action, stage);

                // team:
                // same team
                if (same_team) {
                    caster->stats.total_skills_used_on_own_team.Reduce(action, stage);
                }
                // diff team
                else {
                    caster->stats.total_skills_used_on_other_teams.Reduce(action, stage);
                }
            }

            // party:
            if (target_party) {
                // same party
                if (same_party) {
                    caster->stats.total_skills_used_on_own_party.Reduce(action, stage);
                }
                // diff party
                else {
                    caster->stats.total_skills_used_on_other_parties.Reduce(action, stage);
                }
            }
        }

        // notify the caster_party
        if (caster_party) {
            caster_party->stats.total_skills_used.Reduce(action, stage);

            // team
            // same team
            if (same_team) {
                caster_party->stats.total_skills_used_on_own_team.Reduce(action, stage);
            }
            // diff team
            else {
                caster_party->stats.total_skills_used_on_other_teams.Reduce(action, stage);
            }

            // party:
            if (target_party) {
                // same party
                if (same_party) {
                    caster_party->stats.total_skills_used_on_own_party.Reduce(action, stage);
                }
                // diff party
                else {
                    caster_party->stats.total_skills_used_on_other_parties.Reduce(action, stage);
                }
            }
        }

        // notify the target
        if (target) {
            target->stats.total_skills_received.Reduce(action, stage);
            target->stats.LazyGetSkillReceived(action->skill_id).Reduce(action, stage);
            // used from a living caster? (redundant)
            if (caster) {
                // use against agent
                target->stats.LazyGetSkillReceivedFrom(caster->agent_id, action->skill_id).Reduce(action, stage);

                // team
                // same team
                if (same_team) {
                    target->stats.total_skills_received_from_own_team.Reduce(action, stage);
                }
                // diff team
                else {
                    target->stats.total_skills_received_from_other_teams.Reduce(action, stage);
                }
            }

            // party:
            if (caster_party) {
                // same party
                if (same_party) {
                    target->stats.total_skills_received_from_own_party.Reduce(action, stage);
                }
                // diff party
                else {
                    target->stats.total_skills_received_from_other_parties.Reduce(action, stage);
                }
            }
        }

        // notify the target_party
        if (target_party) {
            target_party->stats.total_skills_received.Reduce(action, stage);

            // team:
            // same team
            if (same_team) {
                target_party->stats.total_skills_received_from_own_team.Reduce(action, stage);
            }
            // diff team
            else {
                target_party->stats.total_skills_received_from_other_teams.Reduce(action, stage);
            }

            // party:
            if (caster_party) {
                // same party
                if (same_party) {
                    target_party->stats.total_skills_received_from_own_party.Reduce(action, stage);
                }
                // diff party
                else {
                    target_party->stats.total_skills_received_from_other_parties.Reduce(action, stage);
                }
            }
        }
    }
    
    // Track resurrection attempts for resurrection skills
    // Set resurrector when skill STARTS on a dead target, so it's already marked when AgentState arrives
    if (action->is_skill && target && caster) {
        // Common resurrection skill IDs
        const auto skill_id = action->skill_id;
        const bool is_resurrection_skill = IsResurrectionSkill(skill_id);
        
        if (is_resurrection_skill) {
            if (stage == ActionStage::Started && target->is_dead) {
                // Mark resurrector when skill starts on dead target
                target->last_resurrector = caster->agent_id;
            }
            else if (stage == ActionStage::Stopped || stage == ActionStage::Interrupted) {
                // Clear resurrector if skill was stopped/interrupted
                if (target->last_resurrector == caster->agent_id) {
                    target->last_resurrector = NO_AGENT;
                }
            }
        }
    }

    return action_ownership_transferred;
}


// Module: Reset the Modules state
void ObserverModule::Reset()
{
    if (map) {
        delete map;
        map = nullptr;
    }
    
    if (match_start_map) {
        delete match_start_map;
        match_start_map = nullptr;
    }

    // clear guild info
    observable_guild_ids.clear();
    for (const auto& [_, guild] : observable_guilds) {
        if (guild) {
            delete guild;
        }
    }
    observable_guilds.clear();

    // clear skill info
    observable_skill_ids.clear();
    for (const auto& [_, skill] : observable_skills) {
        if (skill) {
            delete skill;
        }
    }
    observable_skills.clear();

    // clear agent info
    observable_agent_ids.clear();
    for (const auto& [_, agent] : observable_agents) {
        if (agent) {
            delete agent;
        }
    }
    observable_agents.clear();

    // clear party info
    observable_party_ids.clear();
    for (const auto& [_, party] : observable_parties) {
        if (party) {
            delete party;
        }
    }
    observable_parties.clear();

    // clear max HP cache
    agent_max_hp_cache.clear();
}


// Load all known ObservableAgent's on the current map
// Returns false if failed to initialise. True if successful
bool ObserverModule::InitializeObserverSession(GW::Constants::MapID map_id)
{
    if (observer_session_initialized) {
        return true;
    }
    
    // Use provided map_id if valid (from InstanceLoadInfo packet), otherwise fall back to current map
    if (map_id == static_cast<GW::Constants::MapID>(0)) {
        map_id = GW::Map::GetMapID();
    }
    
    // load area info for the observed map
    const GW::AreaInfo* map_info = GW::Map::GetMapInfo(map_id);
    if (!map_info) {
        return false;
    }

    // load parties
    if (!SynchroniseParties()) {
        return false;
    }

    // load all other agents
    const GW::AgentArray* agents = GW::Agents::GetAgentArray();
    if (!agents) {
        return false;
    }

    for (const GW::Agent* agent : *agents) {
        // not found (maybe hasn't loaded in yet)?
        if (!agent) {
            continue;
        }
        // trigger lazy load
        GetObservableAgentById(agent->agent_id);
    }

    // initialise the map

    delete map;
    map = new ObservableMap(map_id, *map_info);

    match_finished = false;
    winning_party_id = NO_PARTY;
    match_start_instance_time = 0;
    match_duration_ms_total = std::chrono::milliseconds(0);
    match_duration_ms = std::chrono::milliseconds(0);
    match_duration_secs = std::chrono::seconds(0);
    match_duration_mins = std::chrono::minutes(0);

    // Clear the max HP cache for new session
    agent_max_hp_cache.clear();

    observer_session_initialized = true;
    return true;
}


// Synchronise known parties in the area
bool ObserverModule::SynchroniseParties()
{
    GW::PartyContext* party_ctx = GW::GetGameContext()->party;
    if (!party_ctx) {
        return false;
    }

    GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) {
        return false;
    }

    for (const GW::PartyInfo* party_info : parties) {
        if (!party_info) {
            continue;
        }
        // load and synchronize the party
        ObservableParty* observable_party = GetObservablePartyByPartyInfo(*party_info);
        if (observable_party) {
            const bool party_synchronised = observable_party->SynchroniseParty();
            if (!party_synchronised) {
                return false;
            }
        }
    }

    // success
    return true;
}


// Draw internal settings
void ObserverModule::DrawSettingsInternal()
{
    ImGui::Text("Enable data collection in Observer Mode.");
    ImGui::Text("Disable if not using this feature to avoid using extra CPU and memory in Observer Mode.");
    ImGui::Checkbox("Enabled", &settings.is_enabled);
    ImGui::Checkbox("Trim henchman names", &settings.trim_hench_names);
    ImGui::Checkbox("Enable in all Explorable Areas (experimental and unsupported)", &settings.enable_in_explorable_areas);
}


void ObserverModule::Update(const float)
{
    if (!IsActive()) {
        party_sync_timer = 0;
        health_snapshot_timer = 0;
        return;
    }
    if (TIMER_DIFF(party_sync_timer) > 1000) {
        SynchroniseParties();
        party_sync_timer = 0;
    }

    // Record health snapshots every 15 seconds for all tracked agents
    if (TIMER_DIFF(health_snapshot_timer) > 15000) {
        const uint32_t instance_time = GW::Map::GetInstanceTime();
        // Use time relative to match start if available, otherwise use instance time
        const uint32_t match_time = match_start_instance_time > 0 ? (instance_time - match_start_instance_time) : instance_time;
        
        // Calculate aggregate party health for each party
        for (const auto& [party_id, party] : observable_parties) {
            if (!party) continue;
            
            float total_hp = 0.0f;
            uint32_t total_max_hp = 0;
            int valid_agents = 0;
            
            // Sum health across all agents in this party
            for (const auto agent_id : party->agent_ids) {
                const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
                if (!agent) continue;
                
                const GW::AgentLiving* living = agent->GetAsAgentLiving();
                if (!living) continue;
                
                const uint32_t max_hp = GetOrCacheMaxHP(agent_id);
                if (max_hp == 0) continue;
                
                total_hp += living->hp * max_hp;
                total_max_hp += max_hp;
                valid_agents++;
            }
            
            // Record aggregate party health snapshot
            if (valid_agents > 0 && total_max_hp > 0) {
                const float hp_percentage = total_hp / total_max_hp;
                const uint32_t hp_value = static_cast<uint32_t>(total_hp);
                
                party->health_snapshots.emplace_back(
                    match_time,
                    hp_percentage,
                    hp_value,
                    total_max_hp
                );
            }
        }
        
        health_snapshot_timer = 0;
    }

    // Opportunistically cache max HP for the currently observed agent
    // This happens passively whenever you observe a player
    const uint32_t observing_id = GW::Agents::GetObservingId();
    if (observing_id != 0) {
        const GW::Agent* observed_agent = GW::Agents::GetAgentByID(observing_id);
        if (observed_agent) {
            const GW::AgentLiving* living = observed_agent->GetAsAgentLiving();
            if (living && living->max_hp > 0 && living->max_hp < 100000) {
                // Cache max HP if we don't have it yet, or if the value has changed
                auto it = agent_max_hp_cache.find(observing_id);
                if (it == agent_max_hp_cache.end() || it->second != living->max_hp) {
                    agent_max_hp_cache[observing_id] = living->max_hp;
                }

                // Cache energy values as well
                if (living->max_energy > 0) {
                    agent_cur_energy_cache[observing_id] = static_cast<uint32_t>(living->energy);
                    agent_max_energy_cache[observing_id] = living->max_energy;
                }
            }
        }
    }
}

// Lazy load an ObservableGuild using a guild_id
ObserverModule::ObservableGuild* ObserverModule::GetObservableGuildById(const uint32_t guild_id)
{
    // shortcircuit for agent_id = 0
    if (guild_id == NO_GUILD) {
        return nullptr;
    }

    // lazy load
    const auto it = observable_guilds.find(guild_id);

    // found
    if (it != observable_guilds.end()) {
        return it->second;
    }

    // create if active
    if (!IsActive()) {
        return nullptr;
    }
    const GW::Guild* guild = GW::GuildMgr::GetGuildInfo(guild_id);
    if (!guild) {
        return nullptr;
    }

    ObservableGuild* observable_guild = CreateObservableGuild(*guild);
    return observable_guild;
}


// Create an ObservableGuild from a GW::Guild and cache it
// Do NOT call this if the Agent already exists, it will cause a memory leak
ObserverModule::ObservableGuild* ObserverModule::CreateObservableGuild(const GW::Guild& guild)
{
    // create
    auto observable_guild = new ObservableGuild(*this, guild);
    // cache
    observable_guilds.insert({observable_guild->guild_id, observable_guild});
    observable_guild_ids.push_back(observable_guild->guild_id);
    std::ranges::sort(observable_guild_ids);
    return observable_guild;
}


// Lazy load an ObservableAgent using an agent_id
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentById(const uint32_t agent_id)
{
    // shortcircuit for agent_id = 0
    if (agent_id == NO_AGENT) {
        return nullptr;
    }

    // lazy load
    const auto it = observable_agents.find(agent_id);

    // found
    if (it != observable_agents.end()) {
        return it->second;
    }

    // create if active
    if (!IsActive()) {
        return nullptr;
    }
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (!agent) {
        return nullptr;
    }

    const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
    if (!agent_living) {
        return nullptr;
    }

    ObservableAgent* observable_agent = CreateObservableAgent(*agent_living);
    return observable_agent;
}


// Create an ObservableAgent from a GW::AgentLiving and cache it
// Do NOT call this if the Agent already exists, it will cause a memory leak
ObserverModule::ObservableAgent* ObserverModule::CreateObservableAgent(const GW::AgentLiving& agent_living)
{
    // create
    // ensure the guild is loaded...
    GetObservableGuildById(agent_living.tags->guild_id);
    auto observable_agent = new ObservableAgent(*this, agent_living);
    // cache
    observable_agents.insert({observable_agent->agent_id, observable_agent});
    observable_agent_ids.push_back(observable_agent->agent_id);
    std::ranges::sort(observable_agent_ids);
    return observable_agent;
}


// Lazy load an ObservableSkill using a skill_id
ObserverModule::ObservableSkill* ObserverModule::GetObservableSkillById(const GW::Constants::SkillID skill_id)
{
    // short circuit for skill_id = 0
    if (skill_id == NO_SKILL) {
        return nullptr;
    }

    // find
    const auto it_existing = observable_skills.find(skill_id);

    // found
    if (it_existing != observable_skills.end()) {
        return it_existing->second;
    }

    // create if active
    if (!IsActive()) {
        return nullptr;
    }
    const GW::Skill* gw_skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    if (!gw_skill) {
        return nullptr;
    }
    ObservableSkill* skill = CreateObservableSkill(*gw_skill);
    return skill;
}


// Create an ObservableSkill from a GW::Skill and cache it
// Do NOT call this is if the Skill already exists, It will cause a memory leak
ObserverModule::ObservableSkill* ObserverModule::CreateObservableSkill(const GW::Skill& gw_skill)
{
    // create
    auto observable_skill = new ObservableSkill(*this, gw_skill);
    // cache
    observable_skills.insert({gw_skill.skill_id, observable_skill});
    observable_skill_ids.push_back(observable_skill->skill_id);
    std::ranges::sort(observable_skill_ids);
    return observable_skill;
}


// Lazy load an ObservableParty using a PartyInfo
ObserverModule::ObservableParty* ObserverModule::GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info)
{
    // lazy load
    const auto it_observable_party = observable_parties.find(party_info.party_id);
    // found
    if (it_observable_party != observable_parties.end()) {
        return it_observable_party->second;
    }

    // create if active
    if (!IsActive()) {
        return nullptr;
    }
    ObservableParty* observable_party = this->CreateObservableParty(party_info);
    return observable_party;
}


// Lazy load an ObservableParty using a party_id
ObserverModule::ObservableParty* ObserverModule::GetObservablePartyById(const uint32_t party_id)
{
    // shortcircuit for party_id = 0
    if (party_id == NO_PARTY) {
        return nullptr;
    }

    // try to find
    const auto it_party = observable_parties.find(party_id);

    // found
    if (it_party != observable_parties.end()) {
        return it_party->second;
    }

    // create if active
    if (!IsActive()) {
        return nullptr;
    }
    const GW::PartyContext* party_ctx = GW::GetGameContext()->party;
    if (!party_ctx) {
        return nullptr;
    }

    // get all parties
    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) {
        return nullptr;
    }

    // check index
    if (party_id >= parties.size()) {
        return nullptr;
    }
    const GW::PartyInfo* party_info = parties[party_id];
    if (!party_info) {
        return nullptr;
    }

    // create
    ObservableParty* observable_party = CreateObservableParty(*party_info);

    return observable_party;
}


// Create an ObservableParty and cache it
// Do NOT call this if the party already exists, will cause memory leak
ObserverModule::ObservableParty* ObserverModule::CreateObservableParty(const GW::PartyInfo& party_info)
{
    // create
    auto observable_party = new ObservableParty(*this, party_info);
    // cache
    observable_parties.insert({observable_party->party_id, observable_party});
    observable_party_ids.push_back(observable_party->party_id);
    std::ranges::sort(observable_party_ids);
    return observable_party;
}


// change state based on an actions stage
void ObserverModule::ObservedAction::Reduce(const TargetAction* action, const ActionStage stage)
{
    if (!action) {
        return;
    }

    switch (stage) {
        case ActionStage::Instant:
            started += 1;
            finished += 1;
            break;
        case ActionStage::Started:
            started += 1;
            break;
        case ActionStage::Stopped:
            // nothing to do if the action was already finished
            // we can get "cancelled" packet after a "finished" packet if for example; we're using
            // an attack skill, the attack skill completes, we begin the afterswing, and the target dies
            // then the afterswing is cancelled, even though the action completed
            if (!action->was_finished) {
                stopped += 1;
            }
            break;
        case ActionStage::Interrupted:
            // nothing to do if the action was already finished
            if (!action->was_finished) {
                // were we prepended by a fake "cancelled" packet?
                if (action->was_stopped) {
                    stopped -= 1;
                }
                interrupted += 1;
            }
            break;
        case ActionStage::Finished:
            finished += 1;
            break;
    }

    // re-calculate integrity
    integrity = started - finished - stopped - interrupted;
}


// fired when the Agent dies
void ObserverModule::SharedStats::HandleDeath()
{
    deaths += 1;
    // recalculate kdr
    kdr_pc = static_cast<float>(kills) / deaths;
    // get kdr string
    std::stringstream str;
    str << std::fixed << std::setprecision(2) << kdr_pc;
    kdr_str = str.str();
}


// fired when the agent scores a kill
void ObserverModule::SharedStats::HandleKill()
{
    kills += 1;
    // recalculate kdr
    if (deaths < 1) {
        kdr_pc = static_cast<float>(kills);
    }
    else {
        kdr_pc = static_cast<float>(kills) / deaths;
    }
    // get kdr string
    std::stringstream str;
    str << std::fixed << std::setprecision(2) << kdr_pc;
    kdr_str = str.str();
}


ObserverModule::ObservableAgentStats::~ObservableAgentStats()
{
    // attacks received (by agent)
    for (const auto& [_, o_atk] : attacks_received_from_agents) {
        if (o_atk) {
            delete o_atk;
        }
    }
    attacks_received_from_agents.clear();

    // attacks dealed (by agent)
    for (const auto& [_, o_atk] : attacks_dealt_to_agents) {
        if (o_atk) {
            delete o_atk;
        }
    }
    attacks_dealt_to_agents.clear();

    // skills used
    skill_ids_used.clear();
    for (const auto& [_, o_skill] : skills_used) {
        if (o_skill) {
            delete o_skill;
        }
    }
    skills_used.clear();

    // skills received
    skill_ids_received.clear();
    for (const auto& [_, o_skill] : skills_received) {
        if (o_skill) {
            delete o_skill;
        }
    }
    skills_received.clear();

    // skill received (by agent)
    for (auto& [_, skill_ids] : skill_ids_received_from_agents) {
        skill_ids.clear();
    }
    skill_ids_received_from_agents.clear();
    for (auto& [_, agent_skills] : skills_received_from_agents) {
        for (const auto [__, o_skill] : agent_skills) {
            if (o_skill) {
                delete o_skill;
            }
        }
        agent_skills.clear();
    }
    skills_received_from_agents.clear();

    // skill used (by agent)
    for (auto& [_, skill_ids] : skill_ids_used_on_agents) {
        skill_ids.clear();
    }
    skill_ids_used_on_agents.clear();
    for (auto& [_, agent_skills] : skills_used_on_agents) {
        for (const auto [__, o_skill] : agent_skills) {
            if (o_skill) {
                delete o_skill;
            }
        }
        agent_skills.clear();
    }
    skills_used_on_agents.clear();

    // damage tracking cleanup
    damage_dealt_to_agents.clear();
    damage_received_from_agents.clear();
    damage_by_skill.clear();

    for (auto& [_, skill_ids] : skill_ids_damage_to_agents) {
        skill_ids.clear();
    }
    skill_ids_damage_to_agents.clear();
    for (auto& [_, skill_damage] : damage_by_skill_to_agents) {
        skill_damage.clear();
    }
    damage_by_skill_to_agents.clear();

    for (auto& [_, skill_ids] : skill_ids_damage_from_agents) {
        skill_ids.clear();
    }
    skill_ids_damage_from_agents.clear();
    for (auto& [_, skill_damage] : damage_from_skill_from_agents) {
        skill_damage.clear();
    }
    damage_from_skill_from_agents.clear();

    // healing tracking cleanup
    healing_dealt_to_agents.clear();
    healing_received_from_agents.clear();
    healing_by_skill.clear();

    for (auto& [_, skill_ids] : skill_ids_healing_to_agents) {
        skill_ids.clear();
    }
    skill_ids_healing_to_agents.clear();
    for (auto& [_, skill_healing] : healing_by_skill_to_agents) {
        skill_healing.clear();
    }
    healing_by_skill_to_agents.clear();

    for (auto& [_, skill_ids] : skill_ids_healing_from_agents) {
        skill_ids.clear();
    }
    skill_ids_healing_from_agents.clear();
    for (auto& [_, skill_healing] : healing_from_skill_from_agents) {
        skill_healing.clear();
    }
    healing_from_skill_from_agents.clear();
}


// Get attacks dealed against this agent, by a caster_agent_id
// Lazy initialises the caster_agent_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetAttacksDealedAgainst(const uint32_t target_agent_id)
{
    const auto it = attacks_dealt_to_agents.find(target_agent_id);
    if (it == attacks_dealt_to_agents.end()) {
        // receiver not registered
        auto observed_action = new ObservedAction();
        attacks_dealt_to_agents.insert({target_agent_id, observed_action});
        return *observed_action;
    }
    // receiver is already reigstered
    return *it->second;
}


// Get attacks dealed against this agent, by a caster_agent_id
// Lazy initialises the caster_agent_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetAttacksReceivedFrom(const uint32_t caster_agent_id)
{
    const auto it = attacks_received_from_agents.find(caster_agent_id);
    if (it == attacks_received_from_agents.end()) {
        // attacker not registered
        auto observed_action = new ObservedAction();
        attacks_received_from_agents.insert({caster_agent_id, observed_action});
        return *observed_action;
    }
    // attacker is already reigstered
    return *it->second;
}


// Get skills used by this agent
// Lazy initialises the skill_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetSkillUsed(const GW::Constants::SkillID skill_id)
{
    const auto it_skill = skills_used.find(skill_id);
    if (it_skill == skills_used.end()) {
        // skill not registered
        skill_ids_used.push_back(skill_id);
        // re-sort skills
        std::ranges::sort(skill_ids_used);
        auto observed_skill = new ObservedSkill(skill_id);
        skills_used.insert({skill_id, observed_skill});
        return *observed_skill;
    }
    // skill already registered
    return *it_skill->second;
}


// Get skills used received by this agent
// Lazy initialises the skill_id
ObserverModule::ObservedAction& ObserverModule::ObservableAgentStats::LazyGetSkillReceived(const GW::Constants::SkillID skill_id)
{
    const auto it_skill = skills_received.find(skill_id);
    if (it_skill == skills_received.end()) {
        // skill not registered
        skill_ids_received.push_back(skill_id);
        // re-sort skills
        std::ranges::sort(skill_ids_received);
        auto observed_skill = new ObservedSkill(skill_id);
        skills_received.insert({skill_id, observed_skill});
        return *observed_skill;
    }
    // skill already registered
    return *it_skill->second;
}


// Get a skill received by this agent, from another agent
// Lazy initialises the skill_id and caster_agent_id
ObserverModule::ObservedSkill& ObserverModule::ObservableAgentStats::LazyGetSkillReceivedFrom(const uint32_t caster_agent_id, const GW::Constants::SkillID skill_id)
{
    const auto it_caster = skills_received_from_agents.find(caster_agent_id);
    if (it_caster == skills_received_from_agents.end()) {
        // receiver and his skills are not registered with this agent
        std::vector received_skill_ids = {skill_id};
        skill_ids_received_from_agents.insert({caster_agent_id, received_skill_ids});
        auto observed_skill = new ObservedSkill(skill_id);
        std::unordered_map<GW::Constants::SkillID, ObservedSkill*> received_skills = {{skill_id, observed_skill}};
        skills_received_from_agents.insert({caster_agent_id, received_skills});
        return *observed_skill;
    }
    // receiver is registered with this agent
    std::unordered_map<GW::Constants::SkillID, ObservedSkill*>& used_by_caster = it_caster->second;
    const auto it_observed_skill = used_by_caster.find(skill_id);
    // does receiver have the skill registered from/against us?
    if (it_observed_skill == used_by_caster.end()) {
        // caster hasn't registered this skill with this agent
        // add & re-sort skill_ids by the caster
        std::vector<GW::Constants::SkillID>& skills_ids_received_by_agent_vec = skill_ids_received_from_agents.find(caster_agent_id)->second;
        skills_ids_received_by_agent_vec.push_back(skill_id);
        // re-sort
        std::ranges::sort(skills_ids_received_by_agent_vec);
        // add the observed skill for the caster
        auto observed_skill = new ObservedSkill(skill_id);
        used_by_caster.insert({skill_id, observed_skill});
        return *observed_skill;
    }
    // receivers already registered this skill
    return *it_observed_skill->second;
}


// Get a skill received by this agent, from another agent
// Lazy initialises the skill_id and caster_agent_id
ObserverModule::ObservedSkill& ObserverModule::ObservableAgentStats::LazyGetSkillUsedOn(const uint32_t target_agent_id, const GW::Constants::SkillID skill_id)
{
    const auto it_target = skills_used_on_agents.find(target_agent_id);
    if (it_target == skills_used_on_agents.end()) {
        // receiver and his skills are not registered with this agent
        std::vector used_skill_ids = {skill_id};
        skill_ids_used_on_agents.insert({target_agent_id, used_skill_ids});
        auto observed_skill = new ObservedSkill(skill_id);
        std::unordered_map<GW::Constants::SkillID, ObservedSkill*> used_skills = {{skill_id, observed_skill}};
        skills_used_on_agents.insert({target_agent_id, used_skills});
        return *observed_skill;
    }
    std::unordered_map<GW::Constants::SkillID, ObservedSkill*>& used_on_target = it_target->second;
    // receiver is registered with this agent
    const auto it_observed_skill = used_on_target.find(skill_id);
    // does receiver have the skill registered from/against us?
    if (it_observed_skill == used_on_target.end()) {
        // target hasn't registered this skill with this agent
        // add & re-sort skill_ids by the caster
        std::vector<GW::Constants::SkillID>& skills_ids_used_on_agent_vec = skill_ids_used_on_agents.find(target_agent_id)->second;
        skills_ids_used_on_agent_vec.push_back(skill_id);
        // re-sort
        std::ranges::sort(skills_ids_used_on_agent_vec);
        // add the observed skill for the caster
        auto observed_skill = new ObservedSkill(skill_id);
        used_on_target.insert({skill_id, observed_skill});
        return *observed_skill;
    }
    // receivers already registered this skill
    return *it_observed_skill->second;
}


// Get damage dealt to a target agent
// Lazy initialises the target_agent_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetDamageDealedAgainst(const uint32_t target_agent_id)
{
    const auto it = damage_dealt_to_agents.find(target_agent_id);
    if (it == damage_dealt_to_agents.end()) {
        damage_dealt_to_agents.insert({target_agent_id, 0});
        return damage_dealt_to_agents[target_agent_id];
    }
    return it->second;
}


// Get damage received from a caster agent
// Lazy initialises the caster_agent_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetDamageReceivedFrom(const uint32_t caster_agent_id)
{
    const auto it = damage_received_from_agents.find(caster_agent_id);
    if (it == damage_received_from_agents.end()) {
        damage_received_from_agents.insert({caster_agent_id, 0});
        return damage_received_from_agents[caster_agent_id];
    }
    return it->second;
}


// Get damage dealt by a skill
// Lazy initialises the skill_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetDamageBySkill(const GW::Constants::SkillID skill_id)
{
    const auto it = damage_by_skill.find(skill_id);
    if (it == damage_by_skill.end()) {
        damage_by_skill.insert({skill_id, 0});
        return damage_by_skill[skill_id];
    }
    return it->second;
}


// Get damage dealt by a skill to a specific agent
// Lazy initialises the target_agent_id and skill_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetDamageBySkillToAgent(const uint32_t target_agent_id, const GW::Constants::SkillID skill_id)
{
    const auto it_target = damage_by_skill_to_agents.find(target_agent_id);
    if (it_target == damage_by_skill_to_agents.end()) {
        // target and skills are not registered
        std::vector skill_ids = {skill_id};
        skill_ids_damage_to_agents.insert({target_agent_id, skill_ids});
        std::unordered_map<GW::Constants::SkillID, uint32_t> skill_damage = {{skill_id, 0}};
        damage_by_skill_to_agents.insert({target_agent_id, skill_damage});
        return damage_by_skill_to_agents[target_agent_id][skill_id];
    }
    // target is registered
    std::unordered_map<GW::Constants::SkillID, uint32_t>& damage_to_target = it_target->second;
    const auto it_skill = damage_to_target.find(skill_id);
    if (it_skill == damage_to_target.end()) {
        // skill not registered for this target
        std::vector<GW::Constants::SkillID>& skill_ids_vec = skill_ids_damage_to_agents.find(target_agent_id)->second;
        skill_ids_vec.push_back(skill_id);
        std::ranges::sort(skill_ids_vec);
        damage_to_target.insert({skill_id, 0});
        return damage_to_target[skill_id];
    }
    return it_skill->second;
}


// Get damage received from a skill from a specific agent
// Lazy initialises the caster_agent_id and skill_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetDamageFromSkillFromAgent(const uint32_t caster_agent_id, const GW::Constants::SkillID skill_id)
{
    const auto it_caster = damage_from_skill_from_agents.find(caster_agent_id);
    if (it_caster == damage_from_skill_from_agents.end()) {
        // caster and skills are not registered
        std::vector skill_ids = {skill_id};
        skill_ids_damage_from_agents.insert({caster_agent_id, skill_ids});
        std::unordered_map<GW::Constants::SkillID, uint32_t> skill_damage = {{skill_id, 0}};
        damage_from_skill_from_agents.insert({caster_agent_id, skill_damage});
        return damage_from_skill_from_agents[caster_agent_id][skill_id];
    }
    // caster is registered
    std::unordered_map<GW::Constants::SkillID, uint32_t>& damage_from_caster = it_caster->second;
    const auto it_skill = damage_from_caster.find(skill_id);
    if (it_skill == damage_from_caster.end()) {
        // skill not registered for this caster
        std::vector<GW::Constants::SkillID>& skill_ids_vec = skill_ids_damage_from_agents.find(caster_agent_id)->second;
        skill_ids_vec.push_back(skill_id);
        std::ranges::sort(skill_ids_vec);
        damage_from_caster.insert({skill_id, 0});
        return damage_from_caster[skill_id];
    }
    return it_skill->second;
}


// Get healing dealt to a target agent
// Lazy initialises the target_agent_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetHealingDealedTo(const uint32_t target_agent_id)
{
    const auto it = healing_dealt_to_agents.find(target_agent_id);
    if (it == healing_dealt_to_agents.end()) {
        healing_dealt_to_agents.insert({target_agent_id, 0});
        return healing_dealt_to_agents[target_agent_id];
    }
    return it->second;
}


// Get healing received from a caster agent
// Lazy initialises the caster_agent_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetHealingReceivedFrom(const uint32_t caster_agent_id)
{
    const auto it = healing_received_from_agents.find(caster_agent_id);
    if (it == healing_received_from_agents.end()) {
        healing_received_from_agents.insert({caster_agent_id, 0});
        return healing_received_from_agents[caster_agent_id];
    }
    return it->second;
}


// Get healing dealt by a skill
// Lazy initialises the skill_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetHealingBySkill(const GW::Constants::SkillID skill_id)
{
    const auto it = healing_by_skill.find(skill_id);
    if (it == healing_by_skill.end()) {
        healing_by_skill.insert({skill_id, 0});
        return healing_by_skill[skill_id];
    }
    return it->second;
}


// Get healing dealt by a skill to a specific agent
// Lazy initialises the target_agent_id and skill_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetHealingBySkillToAgent(const uint32_t target_agent_id, const GW::Constants::SkillID skill_id)
{
    const auto it_target = healing_by_skill_to_agents.find(target_agent_id);
    if (it_target == healing_by_skill_to_agents.end()) {
        // target and skills are not registered
        std::vector skill_ids = {skill_id};
        skill_ids_healing_to_agents.insert({target_agent_id, skill_ids});
        std::unordered_map<GW::Constants::SkillID, uint32_t> skill_healing = {{skill_id, 0}};
        healing_by_skill_to_agents.insert({target_agent_id, skill_healing});
        return healing_by_skill_to_agents[target_agent_id][skill_id];
    }
    // target is registered
    std::unordered_map<GW::Constants::SkillID, uint32_t>& healing_to_target = it_target->second;
    const auto it_skill = healing_to_target.find(skill_id);
    if (it_skill == healing_to_target.end()) {
        // skill not registered for this target
        std::vector<GW::Constants::SkillID>& skill_ids_vec = skill_ids_healing_to_agents.find(target_agent_id)->second;
        skill_ids_vec.push_back(skill_id);
        std::ranges::sort(skill_ids_vec);
        healing_to_target.insert({skill_id, 0});
        return healing_to_target[skill_id];
    }
    return it_skill->second;
}


// Get healing received from a skill from a specific agent
// Lazy initialises the caster_agent_id and skill_id
uint32_t& ObserverModule::ObservableAgentStats::LazyGetHealingFromSkillFromAgent(const uint32_t caster_agent_id, const GW::Constants::SkillID skill_id)
{
    const auto it_caster = healing_from_skill_from_agents.find(caster_agent_id);
    if (it_caster == healing_from_skill_from_agents.end()) {
        // caster and skills are not registered
        std::vector skill_ids = {skill_id};
        skill_ids_healing_from_agents.insert({caster_agent_id, skill_ids});
        std::unordered_map<GW::Constants::SkillID, uint32_t> skill_healing = {{skill_id, 0}};
        healing_from_skill_from_agents.insert({caster_agent_id, skill_healing});
        return healing_from_skill_from_agents[caster_agent_id][skill_id];
    }
    // caster is registered
    std::unordered_map<GW::Constants::SkillID, uint32_t>& healing_from_caster = it_caster->second;
    const auto it_skill = healing_from_caster.find(skill_id);
    if (it_skill == healing_from_caster.end()) {
        // skill not registered for this caster
        std::vector<GW::Constants::SkillID>& skill_ids_vec = skill_ids_healing_from_agents.find(caster_agent_id)->second;
        skill_ids_vec.push_back(skill_id);
        std::ranges::sort(skill_ids_vec);
        healing_from_caster.insert({skill_id, 0});
        return healing_from_caster[skill_id];
    }
    return it_skill->second;
}


// Constructor
ObserverModule::ObservableParty::ObservableParty(ObserverModule& parent, const GW::PartyInfo& info) : party_id(info.party_id), parent(parent) {}


// Destructor
ObserverModule::ObservableParty::~ObservableParty()
{
    agent_ids.clear();
}


// Synchronise the ObservableParty with its agents/members
// Does not load Party Allies (others) (Pets, Guild Lord, Bodyguard, ...)
bool ObserverModule::ObservableParty::SynchroniseParty()
{
    GW::PartyContext* party_ctx = GW::GetPartyContext();
    GW::PlayerArray* players = party_ctx ? GW::Agents::GetPlayerArray() : nullptr;
    if (!players) {
        return false;
    }

    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) {
        return false;
    }

    const GW::PartyInfo* party_info = party_ctx->parties[party_id];
    if (!party_info) {
        return false;
    }
    if (!party_info->players.valid()) {
        return false;
    }

    // load party members:
    // 1. players
    //  1.1 player heroes
    // 2. henchmen

    size_t party_index = 0;

    // ensure agent_ids size
    const size_t party_size = party_info->players.size() + party_info->heroes.size() + party_info->henchmen.size();
    if (party_size > agent_ids.size()) {
        // agent_ids is too small
        // add empty agent_ids
        agent_ids.resize(party_size, NO_AGENT);
    }
    else if (party_size < agent_ids.size()) {
        // agent_ids is too large
        // clear stale agent_ids
        for (size_t i = party_size; i < agent_ids.size(); i += 1) {
            // clear old party member
            ObservableAgent* observable_agent_prev = parent.GetObservableAgentById(agent_ids[i]);
            if (observable_agent_prev) {
                observable_agent_prev->party_id = NO_PARTY;
                observable_agent_prev->party_index = 0;
            }
        }
        agent_ids.resize(party_size, NO_AGENT);
    }

    // fill agent_ids and notify the agents
    for (const GW::PlayerPartyMember& party_player : party_info->players) {
        // notify the player of their party & position
        const GW::Player& player = players->at(party_player.login_number);
        if (player.agent_id != 0) {
            // if agent_id is 0, the agent either hasn't loaded or has disconnected
            // if the agent has simply disconnected we keep them from agent_ids
            // by avoiding this code block
            if (agent_ids[party_index] != player.agent_id) {
                // clear old party member
                ObservableAgent* observable_player_prev = parent.GetObservableAgentById(agent_ids[party_index]);
                if (observable_player_prev) {
                    observable_player_prev->party_id = NO_PARTY;
                    observable_player_prev->party_index = 0;
                }
            }
            // add new party member
            agent_ids[party_index] = player.agent_id;
            ObservableAgent* observable_player = parent.GetObservableAgentById(player.agent_id);
            if (observable_player) {
                // notify the player of their party & position
                observable_player->party_id = party_id;
                observable_player->party_index = party_index;
            }
        }
        party_index += 1;

        // No heroes in PvP but keeping this for consistency in all explorable areas...
        for (const GW::HeroPartyMember& hero : party_info->heroes) {
            if (hero.owner_player_id == party_player.login_number) {
                if (hero.agent_id != 0) {
                    // out of scope/compass for some reason if agent_id = 0;
                    // just leave the previous entry in our party
                    if (agent_ids[party_index] != hero.agent_id) {
                        // clear old party member
                        ObservableAgent* observable_hero_prev = parent.GetObservableAgentById(agent_ids[party_index]);
                        if (observable_hero_prev) {
                            observable_hero_prev->party_id = NO_PARTY;
                            observable_hero_prev->party_index = 0;
                        }
                    }
                    // add new party member
                    agent_ids[party_index] = hero.agent_id;
                    ObservableAgent* observable_hero = parent.GetObservableAgentById(hero.agent_id);
                    if (observable_hero) {
                        // notify the hero of their party & position
                        observable_hero->party_id = party_id;
                        observable_hero->party_index = party_index;
                    }
                }
            }
            party_index += 1;
        }
    }

    for (const GW::HenchmanPartyMember& hench : party_info->henchmen) {
        if (hench.agent_id != 0) {
            // out of scope/compass for some reason if agent_id = 0;
            // just leave the previous entry in our party
            if (agent_ids[party_index] != hench.agent_id) {
                // clear old party member
                ObservableAgent* observable_hench_prev = parent.GetObservableAgentById(agent_ids[party_index]);
                if (observable_hench_prev) {
                    observable_hench_prev->party_id = NO_PARTY;
                    observable_hench_prev->party_index = 0;
                }
            }
            // add new party member
            agent_ids[party_index] = hench.agent_id;
            ObservableAgent* observable_hench = parent.GetObservableAgentById(hench.agent_id);
            if (observable_hench) {
                // notify the henchman of their party & position
                observable_hench->party_id = party_id;
                observable_hench->party_index = party_index;
            }
        }
        party_index += 1;
    }

    // infer teams name from first players guild
    // TODO: retrieve this information from memory instead of inferring it
    // note: this won't be accurate in HA where the teams name isn't simply
    // player 0's guild
    guild_id = NO_GUILD;
    name = "";
    display_name = "";
    rank = NO_RANK;
    rank_str = "";
    rating = NO_RATING;
    if (agent_ids.size() > 0) {
        ObservableAgent* agent0 = parent.GetObservableAgentById(agent_ids[0]);
        if (agent0) {
            const ObservableGuild* guild = parent.GetObservableGuildById(agent0->guild_id);
            if (guild) {
                guild_id = guild->guild_id;
                name = guild->name;
                rank = guild->rank;
                rank_str = guild->rank == NO_RANK ? "N/A" : std::to_string(guild->rank);
                rating = guild->rating;
                display_name = guild->name + " [" + guild->tag + "]";
            }
            else {
                name = agent0->SanitizedName() + "'s team";
                display_name = agent0->SanitizedName() + "'s team";
            }
        }
    }

    // success
    return true;
}


// Constructor
ObserverModule::ObservableSkill::ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill) : parent(parent), gw_skill(_gw_skill)
{
    skill_id = _gw_skill.skill_id;
    // initialize the name asynchronously here
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(gw_skill.name, name_enc, 16)) {
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
    }
}


// Name of the skill
const std::string ObserverModule::ObservableSkill::Name()
{
    // cached?
    if (_name.length() > 0) {
        return _name;
    }
    std::string name = TextUtils::WStringToString(DecName());
    // not ready to cache
    if (name.length() == 0) {
        return name;
    }
    // ready to cache
    _name = name;
    return _name;
}


// Name + skill_id of the Skill
std::string ObserverModule::ObservableSkill::DebugName()
{
    using namespace std::string_literals;
    return "("s + std::to_string(static_cast<uint32_t>(skill_id)) + "s \"" + TextUtils::WStringToString(DecName()) + "\"";
}


// Constructor
ObserverModule::ObservableGuild::ObservableGuild(ObserverModule& parent, const GW::Guild& guild)
    : parent(parent), guild_id(guild.index), key(guild.key), name(TextUtils::WStringToString(guild.name)), tag(TextUtils::WStringToString(guild.tag)), wrapped_tag("[" + tag + "]"), rank(guild.rank), rating(guild.rating), faction(guild.faction),
      faction_point(guild.faction_point), qualifier_point(guild.qualifier_point), cape_trim(guild.cape.cape_trim)
{
    //
}


// Constructor
ObserverModule::ObservableAgent::ObservableAgent(ObserverModule& parent, const GW::AgentLiving& agent_living)
    : parent(parent), agent_id(agent_living.agent_id), login_number(agent_living.login_number), state(agent_living.model_state), guild_id(static_cast<uint32_t>(agent_living.tags->guild_id)), team_id(agent_living.team_id),
      primary(static_cast<GW::Constants::Profession>(agent_living.primary)), secondary(static_cast<GW::Constants::Profession>(agent_living.secondary)), is_player(agent_living.IsPlayer()), is_npc(agent_living.IsNPC())
{
    // async initialise the agents name now because we probably want it later
    GW::UI::AsyncDecodeStr(GW::Agents::GetAgentEncName(&agent_living), &_raw_name_w);

    if (primary != GW::Constants::Profession::None) {
        std::string prof = ToolboxUtils::GetProfessionAcronym(primary)->string();
        if (secondary != GW::Constants::Profession::None) {
            const std::string s_prof = ToolboxUtils::GetProfessionAcronym(secondary)->string();
            prof = prof + "/" + s_prof;
        }
        profession = prof;
    }
};


// Destructor
ObserverModule::ObservableAgent::~ObservableAgent()
{
    delete current_target_action;
}


// Name of the Agent to display on HUD
std::string ObserverModule::ObservableAgent::DisplayName()
{
    const bool is_initialised = _display_name.length() > 0;
    // additional name modification settings can go here...
    const bool cache_busted = settings.trim_hench_names != trim_hench_name;
    if (is_initialised && !cache_busted) {
        return _display_name;
    }

    // generate and cache display_name
    std::string next_display_name = RawName();

    // remove hench name
    if (settings.trim_hench_names) {
        const size_t begin = next_display_name.find("[");
        const size_t end = next_display_name.find_first_of("]");
        if (std::string::npos != begin && std::string::npos != end && begin <= end) {
            next_display_name.erase(begin, end - begin + 1);
        }
    }

    // trim whitespace
    const size_t w_first = next_display_name.find_first_not_of(' ');
    const size_t w_last = next_display_name.find_last_not_of(' ');
    if (w_first != std::string::npos) {
        next_display_name = next_display_name.substr(w_first, w_last + 1);
    }

    trim_hench_name = settings.trim_hench_names;
    _display_name = next_display_name;
    return _display_name;
}


// Sanitized Name of the Agent (as std::string)
std::string ObserverModule::ObservableAgent::SanitizedName()
{
    // has been cached
    if (_sanitized_name.length() > 0) {
        return _sanitized_name;
    }
    const std::wstring sanitized_name_w = SanitizedNameW();
    // can't be cached yet
    if (sanitized_name_w.length() == 0) {
        return "";
    }
    // can now be cached
    _sanitized_name = TextUtils::WStringToString(sanitized_name_w);
    return _sanitized_name;
}


// Sanitized Name of the Agent (as std::wstring)
std::wstring ObserverModule::ObservableAgent::SanitizedNameW()
{
    // has been cached
    if (_sanitized_name_w.length() > 0) {
        return _sanitized_name_w;
    }
    const std::wstring raw_name_w = RawNameW();
    // can't be cached yet
    if (raw_name_w.length() == 0) {
        return L"";
    }
    // can now be cached
    _sanitized_name_w = TextUtils::SanitizePlayerName(raw_name_w);
    return _sanitized_name_w;
}


// Name of the Agent (as std::string)
std::string ObserverModule::ObservableAgent::RawName()
{
    // has been cached
    if (_raw_name.length() > 0) {
        return _raw_name;
    }
    const std::wstring raw_name_w = RawNameW();
    // can't be cached yet
    if (raw_name_w.length() == 0) {
        return "";
    }
    // can now be cached
    _raw_name = TextUtils::WStringToString(raw_name_w);
    return _raw_name;
}


// Name of the Agent (un-edited as wstring)
std::wstring ObserverModule::ObservableAgent::RawNameW()
{
    // rely on the constructor initialising the name...
    return _raw_name_w;
}


// Name + agent_id of the Agent
std::string ObserverModule::ObservableAgent::DebugName()
{
    std::string debug_name = "(" + std::to_string(agent_id) + ") " + "\"" + RawName() + "\"";
    return debug_name;
}


// Constructor
ObserverModule::ObservableMap::ObservableMap(const GW::Constants::MapID map_id, const GW::AreaInfo& area_info)
    : map_id(map_id), campaign(area_info.campaign), continent(area_info.continent), region(area_info.region), type(area_info.type), flags(area_info.flags), name_id(area_info.name_id), description_id(area_info.description_id)
{
    // async initialise the name
    if (GW::UI::UInt32ToEncStr(area_info.name_id, name_enc, 8)) {
        GW::UI::AsyncDecodeStr(name_enc, &name_w);
    }

    // async initialise the description
    if (GW::UI::UInt32ToEncStr(area_info.description_id, description_enc, 8)) {
        GW::UI::AsyncDecodeStr(description_enc, &description_w);
    }
}

// Cache & return name
std::string ObserverModule::ObservableMap::Name()
{
    if (name.length() > 0) {
        return name;
    }
    name = TextUtils::WStringToString(name_w);
    return name;
}

// Cache & return description
std::string ObserverModule::ObservableMap::Description()
{
    if (description.length() > 0) {
        return description;
    }
    description = TextUtils::WStringToString(description_w);
    return description;
}
