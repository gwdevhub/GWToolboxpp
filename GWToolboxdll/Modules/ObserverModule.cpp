#include "stdafx.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Npc.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWToolbox.h>
#include <GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/ObserverModule.h>

#include <Logger.h>

#define INI_FILENAME L"observerlog.ini"
#define IniSection "observer"

#define NO_AGENT 0
#define NO_PARTY 0
#define NO_SKILL 0


// TODO: replace with GWCA
struct AgentProjectileLaunched : GW::Packet::StoC::Packet<AgentProjectileLaunched> {
    uint32_t agent_id;
    GW::Vec2f destination;
    uint32_t unk1;          // word : 0 ?
    uint32_t unk2;          // dword: changes with projectile animation model
    uint32_t unk3;          // dword: value (143) for all martial weapons (?), n values for n skills (?)
    uint32_t unk4;          // dword: 1 ?
    uint32_t is_attack;     // byte
};
const uint32_t GW::Packet::StoC::Packet<AgentProjectileLaunched>::STATIC_HEADER = (0x00A3); // 163



// Synchronise the ObservableParty with its agents (members)
bool ObserverModule::ObservableParty::SynchroniseParty() {
    GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (party_ctx == nullptr) return false;

    const GW::PlayerArray& players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return false;

    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) return false;

    const GW::PartyInfo* party_info = party_ctx->parties[party_id];
    if (party_info == nullptr) return false;
    if (!party_info->players.valid()) return false;

    // note:
    //  We sync agent_ids even if we can't find an Agent object to match.
    //  That's b/c we don't sync parties often, and it doesn't matter if
    //  we can't see the Agent yet. We only care if they're IN the party.

    // load party members:
    // 1. players
    //	1.1 player heroes
    // 2. henchmen
    // destroy and re-initialise the agent_ids;

    for (uint32_t agent_id : agent_ids) {
        // disabuse old party members of their indexes and party_id in-case they're stale (for some reason?)
        // we re-add them back on in a moment
        ObservableAgent* observable_agent = parent.GetObservableAgentByAgentId(agent_id);
        if (observable_agent != nullptr) {
            observable_agent->party_id = NO_PARTY;
            observable_agent->party_index = 0;
        }
    }

    agent_ids.clear();
    size_t party_index = 0;
    for (const GW::PlayerPartyMember& party_player : party_info->players) {
		// notify the player of their party & position
        const GW::Player& player = players[party_player.login_number];
        agent_ids.push_back(player.agent_id);

        ObservableAgent* observable_player = parent.GetObservableAgentByAgentId(player.agent_id);
        if (observable_player != nullptr) {
            // notify the player of their party & position
            observable_player->party_id = party_id;
            observable_player->party_index = party_index;
        }
        party_index += 1;

        // No heroes in PvP but keeping this for consistency in all explorable areas...
        for (const GW::HeroPartyMember& hero : party_info->heroes) {
            if (hero.owner_player_id == party_player.login_number) {
                agent_ids.push_back(hero.agent_id);
                ObserverModule::ObservableAgent* observable_hero = parent.GetObservableAgentByAgentId(hero.agent_id);
                if (observable_hero != nullptr) {
                    // notify the hero of their party & position
                    observable_hero->party_id = party_id;
                    observable_hero->party_index = party_index;
                }
            }
            party_index += 1;
        }
    }

    for (const GW::HenchmanPartyMember& hench : party_info->henchmen) {
        agent_ids.push_back(hench.agent_id);
        ObserverModule::ObservableAgent* observable_hench = parent.GetObservableAgentByAgentId(hench.agent_id);
        if (observable_hench != nullptr) {
            // notify the henchman of their party & position
            observable_hench->party_id = party_id;
            observable_hench->party_index = party_index;
        }
        party_index += 1;
    }

    // success
    return true;
}

// Constructor
ObserverModule::ObservableSkill::ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill) : parent(parent), gw_skill(_gw_skill) {
    skill_id = _gw_skill.skill_id;
    // initialize the name asynchronously here
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(gw_skill.name, name_enc, 16))
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
}


// Add a Skill usage by agent_id
void ObserverModule::ObservableSkill::AddUsage(const uint32_t agent_id) {
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    const GW::AgentLiving* agent_living = agent == nullptr ? nullptr : agent->GetAsAgentLiving();
    const bool is_npc = agent_living == nullptr ? false : agent_living->IsNPC();
    const bool is_player = agent_living == nullptr ? false : agent_living->IsPlayer();

    // increment usages:

    //	- total
    total_usages++;
    if (is_npc) total_npc_usages++;
    if (is_player) total_player_usages++;

    if (agent_id != NO_AGENT) {
        auto it_uba = usages_by_agent.find(agent_id);
        if (it_uba == usages_by_agent.end()) {
            usages_by_agent.insert({agent_id, 1});
        } else {
            usages_by_agent.insert_or_assign(agent_id, it_uba->second + 1);
        }
    }
}

// Handle ActionStarted
void ObserverModule::ObservableAgent::HandleActionStarted(const TargetAction& action, ActionStartpoint startpoint) {
    if (startpoint != ActionStartpoint::Instant) {
        // destroy previous action
        if (current_target_action) delete current_target_action;
        // store next action
        current_target_action = &action;
    }

    const TargetAction& use_action = action;

    ObservableParty* party = nullptr;
    if (party_id != NO_PARTY) party = parent.GetObservablePartyByPartyId(party_id);

    const bool is_attack = use_action.is_attack;
    const bool is_skill = use_action.is_skill;
    // const uint32_t caster_id = use_action->caster_id;
    const uint32_t target_id = use_action.target_id;
    const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_done.HandleEvent(startpoint);
        if (party != nullptr) party->total_attacks_done.HandleEvent(startpoint);
        LazyGetAttacksDealedAgainst(target_id).HandleEvent(startpoint);
    }

    if (is_skill) {
        total_skills_used.HandleEvent(startpoint);
        if (party != nullptr) party->total_skills_used.HandleEvent(startpoint);
        LazyGetSkillUsed(skill_id).HandleEvent(startpoint);
        LazyGetSkillUsedAgainst(target_id, skill_id).HandleEvent(startpoint);
    }
}

// Handle Received ActionStarted
void ObserverModule::ObservableAgent::HandleReceivedActionStarted(const TargetAction& action, ActionStartpoint startpoint) {
    const TargetAction& use_action = action;

    ObservableParty* party = nullptr;
    if (party_id != NO_PARTY) party = parent.GetObservablePartyByPartyId(party_id);

    const bool is_attack = use_action.is_attack;
    const bool is_skill = use_action.is_skill;
    const uint32_t caster_id = use_action.caster_id;
    // const uint32_t target_id = use_action->target_id;
    const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_received.HandleEvent(startpoint);
        if (party != nullptr) party->total_attacks_received.HandleEvent(startpoint);
        LazyGetAttacksReceivedFrom(caster_id).HandleEvent(startpoint);
    }

    if (is_skill) {
        total_skills_received.HandleEvent(startpoint);
        if (party != nullptr) party->total_skills_received.HandleEvent(startpoint);
        LazyGetSkillReceived(skill_id).HandleEvent(startpoint);
        LazyGetSkillReceievedFrom(caster_id, skill_id).HandleEvent(startpoint);
    }
}


// Handle ActionEnded
const ObserverModule::TargetAction* ObserverModule::ObservableAgent::HandleActionEnded(const ActionEndpoint endpoint) {
    // didn't have an action to finish
    if (current_target_action == nullptr) return nullptr;

    const TargetAction& use_action = *current_target_action;

    ObservableParty* party = nullptr;
    if (party_id != NO_PARTY) party = parent.GetObservablePartyByPartyId(party_id);

    const bool is_attack = use_action.is_attack;
    const bool is_skill = use_action.is_skill;
    // const uint32_t caster_id = use_action->caster_id;
    const uint32_t target_id = use_action.target_id;
    const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_done.HandleEvent(endpoint);
        if (party != nullptr) party->total_attacks_done.HandleEvent(endpoint);
        LazyGetAttacksDealedAgainst(target_id).HandleEvent(endpoint);
    }

    if (is_skill) {
        total_skills_used.HandleEvent(endpoint);
        if (party != nullptr) party->total_skills_used.HandleEvent(endpoint);
        LazyGetSkillUsed(skill_id).HandleEvent(endpoint);
        LazyGetSkillUsedAgainst(target_id, skill_id).HandleEvent(endpoint);
    }

    // Interrupted is received after Stopped
    // We have to remove the Stopped count we falsely assumed
    // and increment the interrupted count instead
    if (endpoint == ActionEndpoint::Interrupted) {
        interrupted_count += 1;
        cancelled_count -= 1;
        if (party != nullptr) {
            party->interrupted_count += 1;
            party->cancelled_count += 1;
        }
        if (is_skill) {
            interrupted_skills_count += 1;
            cancelled_skills_count -= 1;
            if (party != nullptr) {
                party->interrupted_skills_count += 1;
                party->cancelled_skills_count += 1;
            }
        }
    }

    if (endpoint == ActionEndpoint::Stopped) {
        cancelled_count += 1;
        if (party != nullptr) {
            party->cancelled_count += 1;
        }
        if (is_skill) {
            cancelled_skills_count += 1;
            if (party != nullptr) {
                party->cancelled_skills_count += 1;
            }
        }
    }

    return &use_action;
}


// Handle Received ActionEnded
void ObserverModule::ObservableAgent::HandleReceivedActionEnded(const TargetAction& action, const ActionEndpoint endpoint) {
    const TargetAction& use_action = action;

    ObservableParty* party = nullptr;
    if (party_id != NO_PARTY) party = parent.GetObservablePartyByPartyId(party_id);

    const bool is_attack = use_action.is_attack;
    const bool is_skill = use_action.is_skill;
    const uint32_t caster_id = use_action.caster_id;
    // const uint32_t target_id = use_action.target_id;
    const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_received.HandleEvent(endpoint);
        if (party != nullptr) party->total_attacks_received.HandleEvent(endpoint);
        LazyGetAttacksReceivedFrom(caster_id).HandleEvent(endpoint);
    }

    if (is_skill) {
        total_skills_received.HandleEvent(endpoint);
        if (party != nullptr) party->total_skills_received.HandleEvent(endpoint);
        LazyGetSkillReceived(skill_id).HandleEvent(endpoint);
        LazyGetSkillReceievedFrom(caster_id, skill_id).HandleEvent(endpoint);
    }
}


// Handle Knocked Down
void ObserverModule::ObservableAgent::HandleKnockedDown(float duration) {
    knocked_down_count += 1;
    knocked_down_duration += duration;

    ObservableParty* party = nullptr;
    if (party_id != NO_PARTY) party = parent.GetObservablePartyByPartyId(party_id);
    if (party != nullptr) {
        party->knocked_down_count += 1;
        party->knocked_down_duration += 1;
    }
}


void ObserverModule::Initialize() {
    ToolboxModule::Initialize();

    is_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    is_observer = GW::Map::GetIsObserving();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
            HandleInstanceLoadInfo(status, packet);
        }
    );

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &AgentState_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::AgentState* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }
            HandleAgentState(packet->agent_id, packet->state);
        }
    );

    GW::StoC::RegisterPacketCallback<AgentProjectileLaunched>(
        &AgentProjectileLaunched_Entry, [this](GW::HookStatus* status, AgentProjectileLaunched* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            HandleAgentProjectileLaunched(packet->agent_id, (packet->is_attack == 0) ? false : true);
        }
    );

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
        &GenericModifier_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericModifier* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->type;
            const uint32_t caster_id = packet->cause_id;
            const uint32_t target_id = packet->target_id;
            const float value = packet->value;
            const bool no_target = false;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
        }
    );

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            const bool no_target = false;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValue_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = NO_AGENT;
            const uint32_t value = packet->value;
            const float f_value = 0;
            const bool no_target = true;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericFloat>(
        &GenericFloat_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericFloat* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }

            const uint32_t value_id = packet->type;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = NO_AGENT;
            const uint32_t i_value = 0;
            const float value = packet->value;
            const bool no_target = true;
            HandleGenericPacket(value_id, caster_id, target_id, value, no_target);
        }
    );

    if (IsActive() && !observer_session_initialized) {
        InitializeObserverSession();
    }
}

void ObserverModule::Terminate() {
    Reset();
    ToolboxModule::Terminate();
}


//
// Handle GenericValueTarget Packet
//
// Reset data if loaded a new observer match
//
// TODO: allow match upload if leaving an obs game
//
void ObserverModule::HandleInstanceLoadInfo(GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo *packet) {
    UNREFERENCED_PARAMETER(status);
    UNREFERENCED_PARAMETER(packet);

    const bool was_active = IsActive();

    is_explorable = packet->is_explorable;
    is_observer = packet->is_observer;

    const bool is_active = IsActive();

    if (is_active) {
        Reset();
        InitializeObserverSession();
    } else {
        // mark false so we initialize next time we load a session
        observer_session_initialized = false;
    }
}


// Handle a Generic Packet (float)
void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const float value, const bool no_target) {
    UNREFERENCED_PARAMETER(no_target);

    switch (value_id) {
        case (uint32_t)ObserverModule::GenericValueId2::damage:
            HandleDamageDone(caster_id, target_id, value, false);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::critical:
            HandleDamageDone(caster_id, target_id, value, true);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::armorignoring:
            HandleDamageDone(caster_id, target_id, value, false);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::knocked_down:
            HandleKnockedDown(caster_id, value);
            break;
    };
}

// Handle a Generic Packet (uint32_t)
void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const uint32_t value, const bool no_target) {

    switch (value_id) {
        case (uint32_t)ObserverModule::GenericValueId2::attack_finished:
            HandleAttackFinished(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_stopped:
            HandleAttackStopped(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_started: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            // handle
            HandleAttackStarted(_caster_id, _target_id);
            break;
        }

        case (uint32_t)ObserverModule::GenericValueId2::interrupted:
            HandleInterrupted(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_skill_finsihed:
            HandleAttackSkillFinished(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::instant_skill_activated:
            HandleInstantSkillActivated(caster_id, target_id, value);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_skill_stopped:
            HandleAttackSkillStopped(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_skill_activated: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            // handle
            HandleAttackSkillStarted(_caster_id, _target_id, value);
            break;
        }

        case (uint32_t)ObserverModule::GenericValueId2::skill_finished:
            HandleSkillFinished(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::skill_stopped:
            HandleSkillStopped(caster_id);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::skill_activated: {
            // swap target and caster for skill_activated
            // log
            uint32_t _caster_id;
            uint32_t _target_id;
            if (no_target) {
                // do nothing... caster is correct in this case
                _caster_id = caster_id;
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            HandleSkillActivated(_caster_id, _target_id, value);
            break;
        }

        case (uint32_t)ObserverModule::GenericValueId2::party_updated:
            HandlePartyUpdated(value);
            break;
    };
}
      

void ObserverModule::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
}


// Get an ObservableAgent using the agent_id
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentByAgentId(uint32_t agent_id) {
    // lazy load
    auto it = observed_agents.find(agent_id);
    if (it != observed_agents.end()) {
        ObserverModule::ObservableAgent* existing_entity = it->second;
        return existing_entity;
    }
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (agent == nullptr) return nullptr;

    const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
    if (agent_living == nullptr) return nullptr;

    ObserverModule::ObservableAgent& observable_agent = CreateObservableAgent(*agent_living);
    return (&observable_agent);
}


// Get an ObservableAgent using the Agent
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentByAgent(const GW::Agent& agent) {
    // lazy load
    auto it = observed_agents.find(agent.agent_id);
    if (it != observed_agents.end()) {
        ObserverModule::ObservableAgent* existing_entity = it->second;
        return existing_entity;
    }
    const GW::AgentLiving* agent_living = agent.GetAsAgentLiving();
    if (agent_living == nullptr) return nullptr;

    ObserverModule::ObservableAgent& observable_agent = CreateObservableAgent(*agent_living);
    return (&observable_agent);
}

// Get an ObservableAgent using the AgentLiving
ObserverModule::ObservableAgent& ObserverModule::GetObservableAgentByAgentLiving(const GW::AgentLiving& agent_living) {
    // lazy load
    auto it = observed_agents.find(agent_living.agent_id);
    if (it != observed_agents.end()) {
        ObserverModule::ObservableAgent* existing_entity = it->second;
        return *existing_entity;
    }

    ObserverModule::ObservableAgent& observable_agent = CreateObservableAgent(agent_living);
    return observable_agent;
}


// Lazy load an ObservableParty using the PartyInfo
ObserverModule::ObservableParty& ObserverModule::GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info) {
    // lazy load
    auto it_observed_party = observed_parties.find(party_info.party_id);
    // found
    if (it_observed_party != observed_parties.end()) return *it_observed_party->second;

    // create
    ObservableParty* observed_party = new ObserverModule::ObservableParty(*this, party_info);
    observed_parties.insert({ observed_party->party_id, observed_party });
    observed_party_ids.push_back(observed_party->party_id);
    SynchroniseParties();
    return *observed_party;
}


// Get Agent for the ObservableAgent
GW::Agent* ObserverModule::ObservableAgent::GetAgent() {
    GW::AgentArray agents = GW::Agents::GetAgentArray();
    // agent may be unloaded at this point... (entered a new area)
    // therefore this isn't even really valid...
    if (!agents.valid() || agent_id >= agents.size()) return nullptr;
    return agents[agent_id];
}


// Get AgentLiving for the ObservableAgent
GW::AgentLiving* ObserverModule::ObservableAgent::GetAgentLiving() {
    GW::Agent* agent = GetAgent();
    if (agent == nullptr) return nullptr;
    return agent->GetAsAgentLiving();
}


// Create an ObservableAgent from an Agent
// Do NOT call this if the Agent already exists
// It will cause a memory leak
ObserverModule::ObservableAgent& ObserverModule::CreateObservableAgent(const GW::AgentLiving& agent_living) {
    ObserverModule::ObservableAgent* observable_agent = new ObserverModule::ObservableAgent(*this, agent_living);
    observed_agents.insert({ observable_agent->agent_id, observable_agent });
    // start initialising the name now since it's asynchronous and might be wanted later
    observable_agent->Name();
    return *observable_agent;
}


// Get an ObservableSkill using a SkillID
// Lazy loads the Skill
ObserverModule::ObservableSkill* ObserverModule::GetObservableSkillBySkillId(uint32_t skill_id) {
    auto existing = observed_skills.find(skill_id);

    // found
    if (existing != observed_skills.end()) return existing->second;

    const GW::Skill& gw_skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    if (gw_skill.skill_id == 0) return nullptr;
    ObservableSkill& skill = CreateObservableSkill(gw_skill);
    return &skill;
}


// Create an ObservableSkill
// Do NOT call this is if the Skill already exists
// It will cause a memory leak
ObserverModule::ObservableSkill& ObserverModule::CreateObservableSkill(const GW::Skill& gw_skill) {
    // copied from SkillListingWindow.cpp
    ObservableSkill* observable_skill = new ObservableSkill(*this, gw_skill);
    observed_skills.insert({ gw_skill.skill_id, observable_skill });
    return *observable_skill;
}


// Handle Agent State
// fired when the server notifies us of an agent state change
// can tell us if the agent has just died
void ObserverModule::HandleAgentState(const uint32_t agent_id, const uint32_t state) {
    // state 16 = dead
    ObservableAgent* observable_agent = GetObservableAgentByAgentId(agent_id);

    if (observable_agent == nullptr) return;

    // 16 = dead
    if (state != 16) return;

    observable_agent->HandleDeath();
    // track player death against the party

    if (observable_agent->party_id != NO_PARTY) {
        ObservableParty* party = GetObservablePartyByPartyId(observable_agent->party_id);
        if (party != nullptr) {
            party->HandleDeath();

            // credit the kill to the last-hitter and their party
            if (observable_agent->last_hit_by != NO_AGENT) {
                ObservableAgent* killer = GetObservableAgentByAgentId(observable_agent->last_hit_by);
                if (killer != nullptr) {
                    killer->HandleKill();
                    uint32_t killer_party_id = killer->party_id;
                    if (killer_party_id != NO_PARTY) {
                        ObservableParty* killer_party = GetObservablePartyByPartyId(killer_party_id);
                        if (killer_party != nullptr) killer_party->HandleKill();
                    }
                }
            }
        }
    }

}


// Handle Damage Done
void ObserverModule::HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit) {
    ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    ObservableAgent* target = GetObservableAgentByAgentId(target_id);

    const bool is_npc = target == nullptr ? false : target->is_npc;
    const bool is_player = target == nullptr ? false : target->is_player;

	// get last hit
	// (last hitter = killer (scores +1 kills))
	if (target != nullptr && caster->party_id != NO_PARTY && (amount_pc < 0)) {
		target->last_hit_by = caster->agent_id;
	}

    if (is_crit) {
        // handle critical hit
        if (caster != nullptr) {
            // regsiter caster crit
            caster->total_crits_dealt += 1;
            if (is_npc) caster->total_npc_crits_dealt += 1;
            if (is_player) caster->total_player_crits_dealt += 1;

            // register party crit
            if (caster->party_id != NO_PARTY) {
                ObservableParty* caster_party = GetObservablePartyByPartyId(caster->party_id);
                if (caster_party != nullptr) {
					caster_party->total_crits_dealt += 1;
					if (is_npc) caster_party->total_npc_crits_dealt += 1;
					if (is_player) caster_party->total_player_crits_dealt += 1;
                }
            }
        }
        if (target != nullptr) {
            target->total_crits_received += 1;
            if (target->party_id != NO_PARTY) {
                ObservableParty* target_party = GetObservablePartyByPartyId(target->party_id);
                if (target_party != nullptr) {
					target_party->total_crits_received += 1;
                }
            }
        }

    }
}

// Create an ObservableParty and cache it
// Do NOT call this if the party already exists
ObserverModule::ObservableParty& ObserverModule::CreateObservableParty(const GW::PartyInfo& party_info) {
    // create
    ObservableParty* party = new ObservableParty(*this, party_info);
    // cache
    observed_parties.insert({ party->party_id, party });
    observed_party_ids.push_back(party->party_id);
    SynchroniseParties();
    return *party;
}

// Get an ObservableParty by PartyId
ObserverModule::ObservableParty* ObserverModule::GetObservablePartyByPartyId(const uint32_t party_id) {
    // try to find
    const auto it_party = observed_parties.find(party_id);

    // found
    if (it_party != observed_parties.end()) return it_party->second;

    // not found
    // try to create

    // get party ctx
    const GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (party_ctx == nullptr) return nullptr;

    // get all parties
    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) return nullptr;

    // check index
    if (party_id >= parties.size()) return nullptr;
    GW::PartyInfo* party_info = parties[party_id];
    if (party_info == nullptr) return nullptr;

    // create
    ObservableParty& observable_party = CreateObservableParty(*party_info);

    return &observable_party;
}


// HandlePartyUpdated
// Fired when the server notifies us of a party update
void ObserverModule::HandlePartyUpdated(const uint32_t party_id) {
    ObservableParty* party = GetObservablePartyByPartyId(party_id);
    party->SynchroniseParty();
}


// Handle AgentProjectileLaunched
// If is_attack, signals that an attack has stopped
void ObserverModule::HandleAgentProjectileLaunched(const uint32_t agent_id, const bool is_attack) {
    if (is_attack) HandleAttackFinished(agent_id);
}


// Handle Attack Finished
void ObserverModule::HandleAttackFinished(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Finished);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Finished);
    }
}

// Handle Attack Stopped
void ObserverModule::HandleAttackStopped(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Stopped);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Stopped);
    }
}


// Handle Attack Started
void ObserverModule::HandleAttackStarted(const uint32_t caster_id, const uint32_t target_id) {
    const bool _is_attack = true;
    const bool _is_skill = false;
    const bool _skill_id = NO_SKILL;
    const TargetAction* action = new TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

    // notify caster
    ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ActionStartpoint::Started);
        
    // notify target
    ObservableAgent* target = GetObservableAgentByAgentId(target_id);
    if (target != nullptr) target->HandleReceivedActionStarted(*action, ActionStartpoint::Started);
}


// Handle Interrupted
void ObserverModule::HandleInterrupted(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Interrupted);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Interrupted);
    }
}

// Handle Attack Skill Finished
void ObserverModule::HandleAttackSkillFinished(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Finished);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Finished);
    }
}


// Handle Attack Skill Stopped
void ObserverModule::HandleAttackSkillStopped(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Stopped);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Stopped);
    }
}



// Handle Instant Skill Finished
void ObserverModule::HandleInstantSkillActivated(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
    const bool _is_attack = false;
    const bool _is_skill = true;
    const uint32_t _skill_id = skill_id;
    const TargetAction* action = new TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

    // notify caster
    ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ActionStartpoint::Instant);
        
    // notify target
    ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
    if (target != nullptr) target->HandleReceivedActionStarted(*action, ActionStartpoint::Instant);
    
    // TODO: move this cleanup better place... (to the ObservableAgent)
    delete action;
}


// Handle Attack Skill Activated
void ObserverModule::HandleAttackSkillStarted(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
    const bool _is_attack = true;
    const bool _is_skill = true;
    const uint32_t _skill_id = skill_id;
    const TargetAction* action = new TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

    // notify caster
    ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ActionStartpoint::Started);
        
    // notify target
    ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
    if (target != nullptr) target->HandleReceivedActionStarted(*action, ActionStartpoint::Started);
}


// Handle Attack Skill Finished
void ObserverModule::HandleSkillFinished(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Finished);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Finished);
    }
}


// Handle Skill Finished
void ObserverModule::HandleSkillStopped(const uint32_t agent_id) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const TargetAction* action = agent->HandleActionEnded(ActionEndpoint::Stopped);
        if (action == nullptr) return;

        // notify target
         ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ActionEndpoint::Stopped);
    }
}


// Handle SkillActivated
void ObserverModule::HandleSkillActivated(const uint32_t caster_id, const uint32_t target_id, uint32_t skill_id) {
    const bool _is_attack = false;
    const bool _is_skill = true;
    const uint32_t _skill_id = skill_id;
    const TargetAction* action = new TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

    // notify caster
    ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ActionStartpoint::Started);
        
    // notify target
    ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
    if (target != nullptr) target->HandleReceivedActionStarted(*action, ActionStartpoint::Started);
}


// Handle Knocked Down
void ObserverModule::HandleKnockedDown(const uint32_t agent_id, const float duration) {
    ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);
    if (agent != nullptr) agent->HandleKnockedDown(duration);
}


// Reset the ObserverWidgets state
void ObserverModule::Reset() {
    // clear skill info
    size_t i = 0;
    for (const auto& it : observed_skills) {
        i += 1;
        ObservableSkill* observed_skill = it.second;
        if (observed_skill == nullptr) continue;
        delete observed_skill;
    }
    observed_skills.clear();

    // clear agent info
    i = 0;
    for (const auto it : observed_agents) {
        i += 1;
        ObservableAgent* observed_agent = it.second;
        if (observed_agent == nullptr) continue;
        // don't use display name since it throw errors here on termination
        delete observed_agent;
    }
    observed_agents.clear();


    // clear party info
    observed_party_ids.clear();
    observed_parties_array.clear();
    i = 0;
    for (const auto& it_observed_party : observed_parties) {
        i += 1;
        ObservableParty* observed_party = it_observed_party.second;
        if (observed_party == nullptr) continue;
        delete observed_party;
    }
    observed_parties.clear();
}


// Load all ObservableAgent's on the current map
// Returns false if failed to initialise. True if successful
bool ObserverModule::InitializeObserverSession() {
    // load parties

    // load all other agents
    const GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid()) return false;

    GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (party_ctx == nullptr) return false;

    GW::Array<GW::PartyInfo*> parties = party_ctx->parties;
    if (!parties.valid()) return false;

    for (const GW::PartyInfo* party_info : parties) {
        if (party_info == nullptr) continue;
        // load and synchronize the party
        ObserverModule::ObservableParty& observable_party = GetObservablePartyByPartyInfo(*party_info);
        bool party_synchronised = observable_party.SynchroniseParty();
        if (!party_synchronised) return false;
    }

    for (GW::Agent* agent : agents) {
        // not found (maybe hasn't loaded in yet)?
        if (agent == nullptr) continue;
        // trigger lazy load
        GetObservableAgentByAgentId(agent->agent_id);
    }

    observer_session_initialized = true;
    return true;
}


void ObserverModule::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    // TODO
}


void ObserverModule::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    // TODO
    // inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
}


void ObserverModule::DrawSettingInternal() {
    // TODO
}

