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
#define NO_TEAM 0

namespace ObserverLabel {
    const char* Name = "Name";
    const char* Kills = "K";
    const char* Deaths = "D";
    const char* KDR = "KDR";
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
}; // namespace ObserverLabels


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



void ObserverModule::Initialize()
{
    ToolboxModule::Initialize();

    is_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    is_observer = GW::Map::GetIsObserving();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
            HandleInstanceLoadInfo(status, packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &AgentState_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::AgentState* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!IsActive()) return;
            if (!observer_session_initialized) {
                if (!InitializeObserverSession()) return;
            }
            HandleAgentState(packet->agent_id, packet->state);
        });

	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(
		&AgentAdd_Entry,
			[this](GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) -> void {
			UNREFERENCED_PARAMETER(status);
			if (!IsActive()) return;
			if (!observer_session_initialized) {
				if (!InitializeObserverSession()) return;
			}
			HandleAgentAdd(packet->agent_id);
		});

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
// TODO: match upload into gw memorial
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
                _target_id = target_id;     // 0
            } else {
                // caster and target are swapped
                _caster_id = target_id;
                _target_id = caster_id;
            }
            HandleSkillActivated(_caster_id, _target_id, value);
            break;
        }
    };
}



// Packet: Handle Agent State
// fired when the server notifies us of an agent state change
// can tell us if the agent has just died
void ObserverModule::HandleAgentState(const uint32_t agent_id, const uint32_t state) {
    // state 16 = dead
    ObservableAgent* observable_agent = GetObservableAgentById(agent_id);

    if (!observable_agent) return;

    // 16 = dead
    if (state != 16) return;

    observable_agent->stats.HandleDeath();
    // track player death against the party

    if (observable_agent->party_id != NO_PARTY) {
        ObservableParty* party = GetObservablePartyById(observable_agent->party_id);
        if (party) {
            party->stats.HandleDeath();

            // credit the kill to the last-hitter and their party
            if (observable_agent->last_hit_by != NO_AGENT) {
                ObservableAgent* killer = GetObservableAgentById(observable_agent->last_hit_by);
                if (killer) {
                    killer->stats.HandleKill();
                    uint32_t killer_party_id = killer->party_id;
                    if (killer_party_id != NO_PARTY) {
                        ObservableParty* killer_party = GetObservablePartyById(killer_party_id);
                        if (killer_party) killer_party->stats.HandleKill();
                    }
                }
            }
        }
    }

}


// Packet: Handle Damage Done
void ObserverModule::HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    ObservableAgent* target = GetObservableAgentById(target_id);

	// get last hit
	// (last hitter = killer (scores +1 kills))
	if (target && caster->party_id != NO_PARTY && (amount_pc < 0)) {
		target->last_hit_by = caster->agent_id;
	}

    if (is_crit) {
        ObservableParty* caster_party = nullptr;
        ObservableParty* target_party = nullptr;
        if (caster) caster_party = GetObservablePartyById(caster->party_id);
        if (target) target_party = GetObservablePartyById(target->party_id);

        // caster dealt
		if (caster) {
			caster->stats.total_crits_dealt += 1;
			if (target_party) caster->stats.total_party_crits_dealt += 1;
		}

        // caster party dealt
		if (caster_party) {
			caster_party->stats.total_crits_dealt += 1;
			if (target_party) caster_party->stats.total_party_crits_dealt += 1;
		}

        // target received
		if (target) {
			target->stats.total_crits_received += 1;
			if (caster_party) target->stats.total_party_crits_received += 1;
		}

		// target party received
		if (target_party) {
			target_party->stats.total_crits_received += 1;
			if (caster_party) target_party->stats.total_party_crits_received += 1;
		}

    }
}


// Packet: Handle AgentAdd
// Fired when an agent is to be loaded into memory
void ObserverModule::HandleAgentAdd(const uint32_t agent_id) {
    // update parties
    UNREFERENCED_PARAMETER(agent_id);
    party_sync_timer = TIMER_INIT();
}


// Packet: Handle AgentProjectileLaunched
// If is_attack, signals that an attack has stopped
void ObserverModule::HandleAgentProjectileLaunched(const uint32_t agent_id, const bool is_attack) {
    if (!is_attack) return;
	ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster) return;
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}


// Packet: Handle Attack Finished
void ObserverModule::HandleAttackFinished(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster) return;
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}

// Packet: Handle Attack Stopped
void ObserverModule::HandleAttackStopped(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster) return;
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Stopped);
}


// Packet: Handle Attack Started
void ObserverModule::HandleAttackStarted(const uint32_t caster_id, const uint32_t target_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    if (!caster) return;
    const TargetAction* action = new TargetAction(caster_id, target_id, true, false, NO_SKILL);
    ReduceAction(caster, *action, ActionStage::Started);
}


// Packet: Handle Interrupted
void ObserverModule::HandleInterrupted(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Interrupted);
}

// Packet: Handle Attack Skill Finished
void ObserverModule::HandleAttackSkillFinished(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}


// Packet: Handle Attack Skill Stopped
void ObserverModule::HandleAttackSkillStopped(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Stopped);
}



// Packet: Handle Instant Skill Finished
void ObserverModule::HandleInstantSkillActivated(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    // assuming there are no instant attack skills...
    const TargetAction* action = new TargetAction(caster_id, target_id, false, true, skill_id);
	ReduceAction(caster, *action, ActionStage::Instant);
    // TODO: move this cleanup better place... (to the ObservableAgent)
    // We can only delete here because we know ReduceAction won't own the action since it's instant...
    delete action;
}


// Packet: Handle Attack Skill Activated
void ObserverModule::HandleAttackSkillStarted(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    const TargetAction* action = new TargetAction(caster_id, target_id, true, true, skill_id);
	ReduceAction(caster, *action, ActionStage::Started);
}


// Packet: Handle Attack Skill Finished
void ObserverModule::HandleSkillFinished(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Finished);
}


// Packet: Handle Skill Finished
void ObserverModule::HandleSkillStopped(const uint32_t agent_id) {
    ObservableAgent* caster = GetObservableAgentById(agent_id);
    if (!caster->current_target_action) return;
	ReduceAction(caster, *caster->current_target_action, ActionStage::Stopped);
}


// Packet: Handle SkillActivated
void ObserverModule::HandleSkillActivated(const uint32_t caster_id, const uint32_t target_id, uint32_t skill_id) {
    ObservableAgent* caster = GetObservableAgentById(caster_id);
    const TargetAction* action = new TargetAction(caster_id, target_id, false, true, skill_id);
	ReduceAction(caster, *action, ActionStage::Started);
}


// Packet: Handle Knocked Down
void ObserverModule::HandleKnockedDown(const uint32_t agent_id, const float duration) {
    ObservableAgent* agent = GetObservableAgentById(agent_id);
    if (!agent) return;
    agent->stats.knocked_down_count += 1;
    agent->stats.knocked_down_duration += duration;
    ObservableParty* party = GetObservablePartyById(agent->party_id);
    if (!party) return;
	party->stats.knocked_down_count += 1;
	party->stats.knocked_down_duration += 1;
}


// Reduce Action
// Update the state of the module based on an Action & Stage
void ObserverModule::ReduceAction(ObservableAgent* caster, const TargetAction& action, ActionStage stage) {
    if (!caster) return;
    ObserverModule::ObservableAgent* target = GetObservableAgentById(action.target_id);

    // If starting a new action, delete the last stored action & store this new action
    if (stage == ActionStage::Started) {
        // destroy previous action
        if (caster->current_target_action) delete caster->current_target_action;
        // store next action
        caster->current_target_action = &action;
    }

    ObservableParty* caster_party = GetObservablePartyById(caster->party_id);
    ObservableParty* target_party = nullptr;
    if (target) target_party = GetObservablePartyById(target->party_id);

    bool same_team = caster && target && (caster->team_id != NO_TEAM) && (caster->team_id == target->team_id);
    bool same_party = target_party && caster_party && (target_party->party_id == caster_party->party_id);
    // const bool no_target = action.target_id == NO_TARGET;

    // update interrupts
    // @note: interrupt message comes after cancelled.
    // if we get interrupted, we've falsely assumed a cancel and must remove it
    // @note: KD's on actions also count as "cancelled", but I don't have a way
    // to track that properly at the moment
	if (stage == ActionStage::Interrupted) {
        if (caster) {
            if (caster->stats.cancelled_count > 0) caster->stats.cancelled_count -= 1;
            caster->stats.interrupted_count += 1;
        }
        if (caster_party) {
            if (caster_party->stats.cancelled_count > 0) caster_party->stats.cancelled_count -= 1;
            caster_party->stats.interrupted_count += 1;
        }
        if (action.is_skill) {
            if (caster) {
				if (caster->stats.cancelled_skills_count > 0) caster->stats.cancelled_skills_count -= 1;
                caster->stats.interrupted_skills_count += 1;
            }
            if (caster_party) {
				if (caster_party->stats.cancelled_skills_count > 0) caster_party->stats.cancelled_skills_count -= 1;
                caster_party->stats.interrupted_skills_count += 1;
            }
        }
    }

    // update cancels
	if (stage == ActionStage::Stopped) {
        if (caster) caster->stats.cancelled_count += 1;
        if (caster_party) caster_party->stats.cancelled_count += 1;
        if (action.is_skill) {
			if (caster) caster->stats.cancelled_skills_count += 1;
			if (caster_party) caster_party->stats.cancelled_skills_count += 1;
        }
    }

    // handle attack
    if (action.is_attack) {
		// update the caster
        if (caster) {
			caster->stats.total_attacks_done.Reduce(stage);
            if (target) caster->stats.LazyGetAttacksDealedAgainst(target->agent_id).Reduce(stage);
            // if the target belonged to a party, the caster just attacked that other party
			if (target_party) caster->stats.total_attacks_done_on_other_party.Reduce(stage);

        }

        // update the casters party
        if (caster_party) {
            caster_party->stats.total_attacks_done.Reduce(stage);
            // if the target belonged to a party, the casters party just attacked that other party
            if (target_party) caster_party->stats.total_attacks_done_on_other_party.Reduce(stage);
        }

        // update the target
        if (target) {
            target->stats.total_attacks_received.Reduce(stage);
            if (caster) target->stats.LazyGetAttacksReceivedFrom(caster->agent_id).Reduce(stage);
            // if the caster belonged to a party, the target was just attacked by that other party
            if (caster_party) target->stats.total_attacks_received_by_other_party.Reduce(stage);
        }

        // update the targets party
        if (target_party) {
            target_party->stats.total_attacks_received.Reduce(stage);
            // if the caster belonged to a party, the target_party was just attacked by that other party
            if (caster_party) target_party->stats.total_attacks_received_by_other_party.Reduce(stage);
        }
    }

    // handle skill
    if (action.is_skill) {
        // update skill
        ObservableSkill* skill = GetObservableSkillById(action.skill_id);
        if (!skill) {
			// no skill??! skill needs to be added to GWCA!
            Log::Error((std::string("Unknown skill_id: (")
                + std::to_string(action.skill_id)
				+ "). Please contact a developer or create a GitHub issue https://github.com/HasKha/GWToolboxpp/issues/ with this message." ).c_str());
            return;
        }

        // Modify the effective `target` and `target_party`, based on the
        // targetting type of the skill to make stats more intuitive.

        // For example if using Heal Burst on yourself, the packet only
        // includes a caster and no target.
        // here we effectively set the
        // target to the caster.
        uint32_t target_type = skill->gw_skill.target;
        switch (target_type) {
            case (uint32_t) TargetType::no_target: {
                // For stats purposes, consider as "used on self"
                // but not "used on party"
                // e.g. Sprint, Whirlwind
                if (action.target_id == NO_AGENT) {
                    target = caster;
                    target_party = nullptr;
                }
                break;
            }
            case (uint32_t) TargetType::anyone: {
                // Ensure we interpret the target correctly
                // e.g. Mirror of Ice, Stone Sheath
                if (action.target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case (uint32_t) TargetType::ally: {
                // Ensure we interpret the target correctly
                // e.g. Healing Burst
                if (action.target_id == NO_AGENT) {
                    target = caster;
                    target_party = caster_party;
                }
                break;
            }
            case (uint32_t) TargetType::other_ally: {
                // should always have a target
                break;
            }
            case (uint32_t) TargetType::enemy: {
                // should always have a target
                break;
            }
        }
		same_team = caster && target && (caster->team_id != NO_TEAM) && (caster->team_id == target->team_id);
		same_party = target_party && caster_party && (target_party->party_id == caster_party->party_id);

		skill->total_usages.Reduce(stage);
		if (target_party) skill->total_party_target_usages.Reduce(stage);

        // update the caster
        if (caster) {
			caster->stats.total_skills_used.Reduce(stage);
			caster->stats.LazyGetSkillUsed(action.skill_id).Reduce(stage);
            // handle caster use against target

            if (target) {
                // use against agent
				caster->stats.LazyGetSkillUsedAgainst(target->agent_id, action.skill_id).Reduce(stage);

                // team
                // same team
                if (same_team) caster->stats.total_skills_used_on_own_team.Reduce(stage);
                // diff team
                else caster->stats.total_skills_used_on_other_team.Reduce(stage);

                // party:
                if (target_party) {
                    // same party
                    if (same_party) caster->stats.total_skills_used_on_own_party.Reduce(stage);
                    // diff party
                    else caster->stats.total_skills_used_on_other_party.Reduce(stage);
                }
            }
        }

        // update the casters party
        if (caster_party) {
            caster_party->stats.total_skills_used.Reduce(stage);

            // team
            // same team
            if (same_team) caster_party->stats.total_skills_used_on_own_team.Reduce(stage);
            // diff team
            else caster_party->stats.total_skills_used_on_other_team.Reduce(stage);

            // party
            if (target_party) {
				// same party
				if (same_party) caster_party->stats.total_skills_used_on_own_party.Reduce(stage);
				// diff party
				else caster_party->stats.total_skills_used_on_other_party.Reduce(stage);
            }
        }

        // update the target
        if (target) {
			target->stats.total_skills_received.Reduce(stage);
			target->stats.LazyGetSkillReceived(action.skill_id).Reduce(stage);
            // handle caster use against target
            if (caster) {
                // use against agent
                target->stats.LazyGetSkillReceievedFrom(caster->agent_id, action.skill_id).Reduce(stage);

                // team
                // same team
                if (same_team) target->stats.total_skills_received_by_own_team.Reduce(stage);
                // diff team
                else target->stats.total_skills_received_by_other_team.Reduce(stage);

                // party:
                if (caster_party) {
                    // same party
                    if (same_party) target->stats.total_skills_received_by_own_party.Reduce(stage);
                    // diff party
                    else target->stats.total_skills_received_by_other_party.Reduce(stage);
                }
            }
        }

        // update the targets party
        if (target_party) {
            target_party->stats.total_skills_received.Reduce(stage);

            // team
            // same team
            if (same_team) target_party->stats.total_skills_received_by_own_team.Reduce(stage);
            // diff team
            else target_party->stats.total_skills_received_by_other_team.Reduce(stage);

            // party
            if (caster_party) {
				// same party
				if (same_party) target_party->stats.total_skills_received_by_own_party.Reduce(stage);
				// diff party
				else target_party->stats.total_skills_received_by_other_party.Reduce(stage);
            }
        }
    }
}

// Module: Reset the Modules state
void ObserverModule::Reset() {
    // clear skill info
    for (const auto& [_, skill] : observable_skills) if (skill) delete skill;
    observable_skills.clear();

    // clear agent info
    for (const auto& [_, agent] : observable_agents) if (agent) delete agent;
    observable_agents.clear();

    // clear party info
    observable_party_ids.clear();
    observable_parties_array.clear();
    for (const auto& [_, party] : observable_parties) if (party) delete party;
    observable_parties.clear();
}


// Module: Load all ObservableAgent's on the current map
// Returns false if failed to initialise. True if successful
bool ObserverModule::InitializeObserverSession() {
    // load parties
    if (!SynchroniseParties()) return false;

    // load all other agents
    const GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid()) return false;

    for (GW::Agent* agent : agents) {
        // not found (maybe hasn't loaded in yet)?
        if (!agent) continue;
        // trigger lazy load
        GetObservableAgentById(agent->agent_id);
    }

    observer_session_initialized = true;
    return true;
}

// Module: Synchronise parties in the area
bool ObserverModule::SynchroniseParties() {
    GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (!party_ctx) return false;

    GW::Array<GW::PartyInfo*> parties = party_ctx->parties;
    if (!parties.valid()) return false;

    for (const GW::PartyInfo* party_info : parties) {
        if (!party_info) continue;
        // load and synchronize the party
        ObserverModule::ObservableParty& observable_party = GetObservablePartyByPartyInfo(*party_info);
        bool party_synchronised = observable_party.SynchroniseParty();
        if (!party_synchronised) return false;
    }

    // success
    return true;
}


void ObserverModule::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    // No settings yet
}


void ObserverModule::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    // No settings yet
}


void ObserverModule::DrawSettingInternal() {
    // No settings yet
}


void ObserverModule::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
	if (party_sync_timer == 0) return;
	if (!IsActive()) {
        party_sync_timer = 0;
        return;
	};
    if (TIMER_DIFF(party_sync_timer) > 1000) {
        SynchroniseParties();
        party_sync_timer = 0;
    }
 }


// Module: Lazy load an ObservableAgent using the agent_id
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentById(const uint32_t agent_id) {
    // shortcircuit for agent_id = 0
    if (agent_id == NO_AGENT) return nullptr;

    // lazy load
    auto it = observable_agents.find(agent_id);

    // found
    if (it != observable_agents.end()) return it->second;

    // try create
    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (!agent) return nullptr;

    const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
    if (!agent_living) return nullptr;

    ObserverModule::ObservableAgent& observable_agent = CreateObservableAgent(*agent_living);
    return (&observable_agent);
}



// Module: Create an ObservableAgent from an Agent
// Do NOT call this if the Agent already exists, it will cause a memory leak
ObserverModule::ObservableAgent& ObserverModule::CreateObservableAgent(const GW::AgentLiving& agent_living) {
    ObserverModule::ObservableAgent* observable_agent = new ObserverModule::ObservableAgent(*this, agent_living);
    observable_agents.insert({ observable_agent->agent_id, observable_agent });
    return *observable_agent;
}


// Skill: Create an ObservableSkill
// Do NOT call this is if the Skill already exists, It will cause a memory leak
ObserverModule::ObservableSkill& ObserverModule::CreateObservableSkill(const GW::Skill& gw_skill) {
    ObservableSkill* observable_skill = new ObservableSkill(*this, gw_skill);
    observable_skills.insert({ gw_skill.skill_id, observable_skill });
    return *observable_skill;
}


// Skill: Get an ObservableSkill using a SkillID
// Lazy loads the Skill
ObserverModule::ObservableSkill* ObserverModule::GetObservableSkillById(uint32_t skill_id) {
    // short circuit for skill_id = 0
    if (skill_id == NO_SKILL) return nullptr;

    // find
    auto it_existing = observable_skills.find(skill_id);

    // found
    if (it_existing != observable_skills.end()) return it_existing->second;

    const GW::Skill& gw_skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    if (gw_skill.skill_id == 0) return nullptr;
    ObservableSkill& skill = CreateObservableSkill(gw_skill);
    return &skill;
}


// Create an ObservableParty and cache it
// Do NOT call this if the party already exists, will cause memory leak
ObserverModule::ObservableParty& ObserverModule::CreateObservableParty(const GW::PartyInfo& party_info) {
    // create
    ObservableParty* party = new ObservableParty(*this, party_info);
    // cache
    observable_parties.insert({ party->party_id, party });
    observable_party_ids.push_back(party->party_id);
    ReorderAndCacheParties();
    return *party;
}


// Module: Get an ObservableParty by PartyId
ObserverModule::ObservableParty* ObserverModule::GetObservablePartyById(const uint32_t party_id) {
    // shortcircuit for party_id = 0
    if (party_id == NO_PARTY) return nullptr;

    // try to find
    const auto it_party = observable_parties.find(party_id);

    // found
    if (it_party != observable_parties.end()) return it_party->second;

    // not found
    // try to create

    // get party ctx
    const GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (!party_ctx) return nullptr;

    // get all parties
    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) return nullptr;

    // check index
    if (party_id >= parties.size()) return nullptr;
    GW::PartyInfo* party_info = parties[party_id];
    if (!party_info) return nullptr;

    // create
    ObservableParty& observable_party = CreateObservableParty(*party_info);

    return &observable_party;
}


// Module: Lazy load an ObservableParty using the PartyInfo
ObserverModule::ObservableParty& ObserverModule::GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info) {
    // lazy load
    auto it_observed_party = observable_parties.find(party_info.party_id);
    // found
    if (it_observed_party != observable_parties.end()) return *it_observed_party->second;

    // create
    ObservableParty* observed_party = new ObserverModule::ObservableParty(*this, party_info);
    observable_parties.insert({ observed_party->party_id, observed_party });
    observable_party_ids.push_back(observed_party->party_id);
    ReorderAndCacheParties();
    return *observed_party;
}


// Party: Synchronise the ObservableParty with its agents (members)
bool ObserverModule::ObservableParty::SynchroniseParty() {
    GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    if (!party_ctx) return false;

    const GW::PlayerArray& players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return false;

    const GW::Array<GW::PartyInfo*>& parties = party_ctx->parties;
    if (!parties.valid()) return false;

    const GW::PartyInfo* party_info = party_ctx->parties[party_id];
    if (!party_info) return false;
    if (!party_info->players.valid()) return false;

    // load party members:
    // 1. players
    //	1.1 player heroes
    // 2. henchmen
    // destroy and re-initialise the agent_ids;

    for (uint32_t agent_id : agent_ids) {
        // disabuse old party members of their indexes and party_id
        // to re-add them back on in a moment
        ObservableAgent* observable_agent = parent.GetObservableAgentById(agent_id);
        if (observable_agent) {
            // clear observable_agents party info
            observable_agent->party_id = NO_PARTY;
            observable_agent->party_index = 0;
        }
    }

    agent_ids.clear();
    size_t party_index = 1;
    for (const GW::PlayerPartyMember& party_player : party_info->players) {
		// notify the player of their party & position
        const GW::Player& player = players[party_player.login_number];
        agent_ids.push_back(player.agent_id);

        ObservableAgent* observable_player = parent.GetObservableAgentById(player.agent_id);
        if (observable_player) {
            // notify the player of their party & position
            observable_player->party_id = party_id;
            observable_player->party_index = party_index;
        }
        party_index += 1;

        // No heroes in PvP but keeping this for consistency in all explorable areas...
        for (const GW::HeroPartyMember& hero : party_info->heroes) {
            if (hero.owner_player_id == party_player.login_number) {
                agent_ids.push_back(hero.agent_id);
                ObserverModule::ObservableAgent* observable_hero = parent.GetObservableAgentById(hero.agent_id);
                if (observable_hero) {
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
        ObserverModule::ObservableAgent* observable_hench = parent.GetObservableAgentById(hench.agent_id);
        if (observable_hench) {
            // notify the henchman of their party & position
            observable_hench->party_id = party_id;
            observable_hench->party_index = party_index;
        }
        party_index += 1;
    }

    // success
    return true;
}


// Skill: ObservableSkill Constructor
ObserverModule::ObservableSkill::ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill) : parent(parent), gw_skill(_gw_skill) {
    skill_id = _gw_skill.skill_id;
    // initialize the name asynchronously here
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(gw_skill.name, name_enc, 16))
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
}

// Name of the Agent (modified a little)
std::string ObserverModule::ObservableAgent::Name() {
	// has been cached
	if (trimmed_name.length() > 0) return trimmed_name;
	// hasn't been cached yet
	if (raw_name == L"") return "";
	// can now be cached
	std::string _trimmed_name = GuiUtils::WStringToString(raw_name);

    if (profession.length() > 0) _trimmed_name = profession + " " + _trimmed_name;

    // strip ending [hench name] from henchmen
	size_t begin = _trimmed_name.find(" [");
	size_t end = _trimmed_name.find_first_of("]");
	if (std::string::npos != begin && std::string::npos != end && begin <= end) {
		_trimmed_name.erase(begin, end-begin + 1);
	}

    // // strip ending (player number) from player
	// begin = _trimmed_name.find(" (");
	// end = _trimmed_name.find_first_of(")");
	// if (std::string::npos != begin && std::string::npos != end && begin <= end) {
	// 	_trimmed_name.erase(begin, end-begin + 1);
	// }

	trimmed_name = _trimmed_name;
	return trimmed_name;
}
