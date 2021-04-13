#include "stdafx.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/Managers/MapMgr.h>
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
#include <Widgets/ObserverWidget.h>

#include <Logger.h>

#define INI_FILENAME L"observerlog.ini"
#define IniSection "observer"

#define NO_AGENT 0
#define NO_PARTY 0
#define NO_SKILL 0


std::string PadLeft(size_t len, std::string str) {
    const size_t slen = str.length();
    if (slen >= len) {
        return str;
	}
	return str.insert(0, len - slen, ' ');
}

std::string PadRight(size_t len, std::string str) {
    const size_t slen = str.length();
    if (slen >= len) {
        return str;
	}
	return str.insert(slen, len - slen, ' ');
}

void __Log(std::string ctx, int indent, std::string message) {
	const char* _indentchars = "    ";
	std::string _message = PadRight(25, std::string("[") + ctx + "] ");
	int i = 0;
	for (i = 0; i < indent; i += 1) {
		_message.append(_indentchars);
	}
	_message.append(message);
	Log::Log(_message.c_str());
}

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



// //
// // ObservableParty
// //
// void ObserverModule::ObservableParty::SynchroniseParty() {
//     GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
//     if (party_ctx == nullptr) return;
// 	__Log(__FUNCTION__, 0, "Syncronizing party + " + std::to_string(party_id) + ")");
// 
// 	GW::PlayerArray players = GW::Agents::GetPlayerArray();
// 	__Log(__FUNCTION__, 1, "Found (" + std::to_string(players.size()) + ") players");
// 
// 	GW::Array<GW::PartyInfo*> parties = party_ctx->parties;
// 	__Log(__FUNCTION__, 1, "Found (" + std::to_string(parties.size()) + ") parties");
// 
//     GW::PartyInfo* party_info = party_ctx->parties[party_id];
// 	if (party_info == nullptr) {
// 		__Log(__FUNCTION__, 2, "Failed: \"party_info == nullptr\"");
//         return;
// 	}
// 
// 	// load party members:
// 	// 1. players
// 	//	1.1 player heroes
// 	// 2. henchmen
// 	// destroy and re-initialise the member_agent_ids vector;
//     member_agent_ids.clear();
// 	for (const GW::PlayerPartyMember& party_player : party_info->players) {
// 		GW::Player& player = players[party_player.login_number];
// 		member_agent_ids.push_back(player.agent_id);
// 		// ObserverModule::ObservableAgent* observable_player = parent.GetObservableAgentByAgentId(player.agent_id);
// 		auto observable_player = parent.GetObservableAgentByAgentId(player.agent_id);
// 		if (observable_player == nullptr) {
// 			__Log(__FUNCTION__, 2, "Failed: \"observable_player == nullptr\"");
// 			continue;
// 		}
//         __Log(__FUNCTION__, 2, "Syncing player (" + std::to_string(member_agent_ids.size()) + "): " + observable_player->DisplayName());
// 		observable_player->party_id = party_info->party_id;
// 		observable_player->party_index = member_agent_ids.size() - 1;
// 
// 		// No heroes in PvP but keeping this for good measure
// 		for (const GW::HeroPartyMember& hero : party_info->heroes) {
// 			if (hero.owner_player_id == party_player.login_number) {
// 				member_agent_ids.push_back(hero.agent_id);
// 				ObserverModule::ObservableAgent* observable_hero = parent.GetObservableAgentByAgentId(hero.agent_id);
// 				if (observable_hero == nullptr) {
// 					__Log(__FUNCTION__, 2, "Failed: \"observable_hero == nullptr\"");
// 					continue;
// 				}
// 				__Log(__FUNCTION__, 3, "Syncing hero (" + std::to_string(member_agent_ids.size()) + "): " + observable_hero->DisplayName());
// 				observable_hero->party_id = party_info->party_id;
// 				observable_hero->party_index = member_agent_ids.size() - 1;
// 			}
// 		}
// 	}
// 
// 	for (const GW::HenchmanPartyMember& hench : party_info->henchmen) {
// 		member_agent_ids.push_back(hench.agent_id);
// 		ObserverModule::ObservableAgent* observable_hench = parent.GetObservableAgentByAgentId(hench.agent_id);
// 		if (observable_hench == nullptr) {
// 			__Log(__FUNCTION__, 2, "Failed: \"observable_hench == nullptr\"");
// 			continue;
// 		}
// 		__Log(__FUNCTION__, 3, "Syncing henchman (" + std::to_string(member_agent_ids.size()) + "): " + observable_hench->DisplayName());
// 		observable_hench->party_id = party_info->party_id;
// 		observable_hench->party_index = member_agent_ids.size() - 1;
// 	}
// }


//
// Get the Name of the Skill
//
// Copied from SkillListingWindow.cpp
//
const wchar_t* ObserverModule::ObservableSkill::Name() {
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(gw_skill.name, name_enc, 16))
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
    return name_dec;
}


//
// Add a Skill usage by agent_id
//
//
//
void ObserverModule::ObservableSkill::AddUsage(const uint32_t agent_id) {
	// __Log(__FUNCTION__, 0, std::string(" Agent: (") + std::to_string(agent_id) + ")");

    const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    const GW::AgentLiving* agent_living = agent == nullptr ? nullptr : agent->GetAsAgentLiving();
    const bool is_npc = agent_living == nullptr ? false : agent_living->IsNPC();
    const bool is_player = agent_living == nullptr ? false : agent_living->IsPlayer();

	if (agent_living == nullptr) {
        __Log(__FUNCTION__, 0, "WARNING: \"agent_living == nullptr\" {" + DisplayName() + "}");
    }

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


	// add usage by team...
	// if (agent_living != nullptr) {
	// 	//	- by team
	// 	auto it_ubt = usages_by_team.find(agent_living->team_id);
	// 	if (it_ubt == usages_by_team.end()) {
	// 		usages_by_team.insert({agent_living->team_id, 1});
	// 	} else {
	// 		usages_by_team.insert_or_assign(agent_living->team_id, it_ubt->second + 1);
	// 	}
    // }
}

//
// Handle ActionStarted
//
void ObserverModule::ObservableAgent::HandleActionStarted(const TargetAction& action, ActionStartpoint startpoint) {
    if (startpoint != ActionStartpoint::Instant) {
		// destroy previous action
		if (target_action) delete target_action;
		// store next action
		target_action = &action;
    }

    const TargetAction& use_action = action;

	const bool is_attack = use_action.is_attack;
	const bool is_skill = use_action.is_skill;
    // const uint32_t caster_id = use_action->caster_id;
    const uint32_t target_id = use_action.caster_id;
	const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_done.HandleEvent(startpoint);
        LazyGetAttacksDealedAgainst(target_id).HandleEvent(startpoint);
    }

    if (is_skill) {
        total_skills_used.HandleEvent(startpoint);
        LazyGetSkillUsed(skill_id).HandleEvent(startpoint);
        LazyGetSkillUsedAgainst(target_id, skill_id).HandleEvent(startpoint);
    }
}

//
// Handle Received ActionStarted
//
void ObserverModule::ObservableAgent::HandleReceivedActionStarted(const TargetAction& action, ActionStartpoint startpoint) {
    const TargetAction& use_action = action;

	const bool is_attack = use_action.is_attack;
	const bool is_skill = use_action.is_skill;
    const uint32_t caster_id = use_action.caster_id;
    // const uint32_t target_id = use_action->caster_id;
	const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_received.HandleEvent(startpoint);
        LazyGetAttacksReceivedFrom(caster_id).HandleEvent(startpoint);
    }

    if (is_skill) {
        total_skills_received.HandleEvent(startpoint);
        LazyGetSkillReceived(skill_id).HandleEvent(startpoint);
        LazyGetSkillReceievedFrom(caster_id, skill_id).HandleEvent(startpoint);
    }
}


//
// Handle ActionEnded
//
const ObserverModule::ObservableAgent::TargetAction* ObserverModule::ObservableAgent::HandleActionEnded(const ActionEndpoint endpoint) {
    // didn't have an action to finish
    if (target_action == nullptr) return nullptr;

    const TargetAction& use_action = *target_action;

	const bool is_attack = use_action.is_attack;
	const bool is_skill = use_action.is_skill;
    // const uint32_t caster_id = use_action->caster_id;
    const uint32_t target_id = use_action.caster_id;
	const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_done.HandleEvent(endpoint);
        LazyGetAttacksDealedAgainst(target_id).HandleEvent(endpoint);
    }

    if (is_skill) {
        total_skills_used.HandleEvent(endpoint);
        LazyGetSkillUsed(skill_id).HandleEvent(endpoint);
        LazyGetSkillUsedAgainst(target_id, skill_id).HandleEvent(endpoint);
    }

    if (endpoint == ActionEndpoint::Interrupted) {
        interrupted_count += 1;
        if (is_skill) cancelled_count -= 1;
    }

    if (endpoint == ActionEndpoint::Stopped && is_skill) {
        cancelled_count += 1;
    }

    return &use_action;
}


//
// Handle Received ActionEnded
//
void ObserverModule::ObservableAgent::HandleReceivedActionEnded(const TargetAction& action, const ActionEndpoint endpoint) {
    const TargetAction& use_action = action;

	const bool is_attack = use_action.is_attack;
	const bool is_skill = use_action.is_skill;
    const uint32_t caster_id = use_action.caster_id;
    // const uint32_t target_id = use_action.caster_id;
	const uint32_t skill_id = use_action.skill_id;

    if (is_attack) {
        total_attacks_received.HandleEvent(endpoint);
        LazyGetAttacksReceivedFrom(caster_id).HandleEvent(endpoint);
    }

    if (is_skill) {
        total_skills_received.HandleEvent(endpoint);
        LazyGetSkillReceived(skill_id).HandleEvent(endpoint);
        LazyGetSkillReceievedFrom(caster_id, skill_id).HandleEvent(endpoint);
    }
}


//
// Handle Knocked Down
//
void ObserverModule::ObservableAgent::HandleKnockedDown(float duration) {
    knocked_down_count += 1;
    knocked_down_duration += duration;
}


void ObserverModule::Initialize() {
	// ToolboxWidget::Initialize();
	ToolboxWindow::Initialize();

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
            // state 16 = dead
			ObservableAgent* observable_agent = GetObservableAgentByAgentId(packet->agent_id);
            if (observable_agent != nullptr) observable_agent->HandleStateChange(packet->state);
		}
	);

    GW::StoC::RegisterPacketCallback<AgentProjectileLaunched>(
        &AgentProjectileLaunched_Entry, [this](GW::HookStatus* status, AgentProjectileLaunched* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            HandleAgentProjectileLaunched(packet->agent_id, (packet->is_attack == 0) ? false : true);
        }
	);

    // TODO: morale boost
    // TODO: kill

	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
		&GenericModifier_Entry,
		[this](GW::HookStatus* status, GW::Packet::StoC::GenericModifier* packet) -> void {
			UNREFERENCED_PARAMETER(status);
			if (!IsActive()) return;
			if (!observer_session_initialized) InitializeObserverSession();
            const uint32_t value_id = packet->type;
            const uint32_t caster_id = packet->cause_id;
            const uint32_t target_id = packet->target_id;
            const uint32_t i_value = 0;
            const float f_value = packet->value;
            const bool use_float = true;
            const bool no_target = false;
            HandleGenericPacket("GenericModifier", value_id, caster_id, target_id, i_value, f_value, use_float, no_target);
        }
	);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
			UNREFERENCED_PARAMETER(status);
			if (!IsActive()) return;
			if (!observer_session_initialized) InitializeObserverSession();
            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t i_value = packet->value;
            const float f_value = 0;
            const bool use_float = false;
            const bool no_target = false;
            HandleGenericPacket("GenericValueTarget", value_id, caster_id, target_id, i_value, f_value, use_float, no_target);
	});

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValue_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
			UNREFERENCED_PARAMETER(status);
			if (!IsActive()) return;
			if (!observer_session_initialized) InitializeObserverSession();
            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = NO_AGENT;
            const uint32_t i_value = packet->value;
            const float f_value = 0;
            const bool use_float = false;
            const bool no_target = true;
            HandleGenericPacket("GenericValue", value_id, caster_id, target_id, i_value, f_value, use_float, no_target);
	});

	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericFloat>(
		&GenericFloat_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericFloat* packet) -> void {
			UNREFERENCED_PARAMETER(status);
			if (!IsActive()) return;
			if (!observer_session_initialized) InitializeObserverSession();
            const uint32_t value_id = packet->type;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = NO_AGENT;
            const uint32_t i_value = 0;
            const float f_value = packet->value;
            const bool use_float = true;
            const bool no_target = true;
            HandleGenericPacket("GenericFloat", value_id, caster_id, target_id, i_value, f_value, use_float, no_target);
        }
	);

	if (IsActive() && !observer_session_initialized) {
		InitializeObserverSession();
	}
}

void ObserverModule::Terminate() {
    Reset();
	// ToolboxWidget::Terminate();
    ToolboxWindow::Terminate();
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

	__Log(__FUNCTION__, 0, "handling Instance Load Info...");

    const bool was_active = IsActive();

	is_explorable = packet->is_explorable;
    is_observer = packet->is_observer;

    const bool is_active = IsActive();

    if (is_active) {
		__Log(__FUNCTION__, 1, "Loaded observer new mode instance");
        Reset();
        InitializeObserverSession();
    } else {
		// mark false so we initialize next time we load a session
		observer_session_initialized = false;
    }
}


//
// Handle a GenericModifier Packet
//
// Indicates damage/healing received/done
//
void ObserverModule::HandleGenericModifier(GW::HookStatus* status, GW::Packet::StoC::GenericModifier* packet) {
	UNREFERENCED_PARAMETER(status);
	UNREFERENCED_PARAMETER(packet);
}



//
// Handle GenericValueTarget Packet
//
// Indicates an action taken by a "caster" to a "target"
//
// "caster" and "target" may be switched for some Value_id's.
// For example: Value_id of 60 indicates skill activation.
// For skill activation, the "caster" is the actual target,
// and the "target" is the activator.
//
void ObserverModule::HandleGenericValueTarget(GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) {
	UNREFERENCED_PARAMETER(status);
	UNREFERENCED_PARAMETER(packet);
    // HandleGenericPacket()
}

//
// Handle a GenericValue Packet
//
// Indicates a value that has changed for an agent
//
void ObserverModule::HandleGenericValue(GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) {
	UNREFERENCED_PARAMETER(status);
	UNREFERENCED_PARAMETER(packet);
}


//
// Handle a GenericFloat Packet
//
// Indicates a value that has changed for an agent
//
void ObserverModule::HandleGenericFloat(GW::HookStatus* status, GW::Packet::StoC::GenericFloat* packet) {
	UNREFERENCED_PARAMETER(status);
	UNREFERENCED_PARAMETER(packet);
}


//
// Handle a Gneeric Packet
//
void ObserverModule::HandleGenericPacket(const std::string kind, const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const uint32_t i_value, const float f_value, const bool use_float, const bool no_target) {

    switch (value_id) {
		// ignore
        case (uint32_t)ObserverModule::GenericValueId2::add_effect:
        case (uint32_t)ObserverModule::GenericValueId2::remove_effect:
        case (uint32_t)ObserverModule::GenericValueId2::apply_marker:
        case (uint32_t)ObserverModule::GenericValueId2::remove_marker:
        case (uint32_t)ObserverModule::GenericValueId2::effect_on_target:
        case (uint32_t)ObserverModule::GenericValueId2::effect_on_agent:
        case (uint32_t)ObserverModule::GenericValueId2::animation:
        case (uint32_t)ObserverModule::GenericValueId2::animation_special:
        case (uint32_t)ObserverModule::GenericValueId2::animation_loop: break;

        case (uint32_t)ObserverModule::GenericValueId2::health:
			// log
            if (use_float) LogUnexpectedGenericPacket(kind, "health", value_id, caster_id, target_id, f_value);
            else LogUnexpectedGenericPacket(kind, "health", value_id, caster_id, target_id, i_value);
			break;

		// ignore
        case (uint32_t)ObserverModule::GenericValueId2::energygain:
        case (uint32_t)ObserverModule::GenericValueId2::casttime:
        case (uint32_t)ObserverModule::GenericValueId2::disabled: break;

        case (uint32_t)ObserverModule::GenericValueId2::damage:
			// log
            if (use_float) LogGenericPacket(kind, "damage", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "damage", value_id, caster_id, target_id, i_value, false);
			// TODO: handle
            if (use_float) HandleDamageDone(caster_id, target_id, f_value, false);
            else LogUnexpectedGenericPacket(kind, "damage", value_id, caster_id, target_id, i_value);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::critical:
			// log
            if (use_float) LogGenericPacket(kind, "critical", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "critical", value_id, caster_id, target_id, i_value, false);
			// TODO: handle
            if (use_float) HandleDamageDone(caster_id, target_id, f_value, true);
			else LogUnexpectedGenericPacket(kind, "critical", value_id, caster_id, target_id, i_value);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::armorignoring:
			// log
            if (use_float) LogGenericPacket(kind, "armorignoring", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "armorignoring", value_id, caster_id, target_id, i_value, false);
			// TODO: handle
            if (use_float) HandleDamageDone(caster_id, target_id, f_value, false);
            else LogUnexpectedGenericPacket(kind, "armorignoring", value_id, caster_id, target_id, i_value);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_finished:
			// log
            if (use_float) LogGenericPacket(kind, "attack_finished", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "attack_finished", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
                if (use_float) LogUnexpectedGenericPacket(kind, "attack_finished:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "attack_finished:target", value_id, caster_id, target_id, i_value);
            } else {
                if (use_float) LogUnexpectedGenericPacket(kind, "attack_finished:float", value_id, caster_id, target_id, f_value);
                else HandleAttackFinished(caster_id);
            }
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_stopped:
			// log
            if (use_float) LogGenericPacket(kind, "attack_stopped", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "attack_stopped", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
                if (use_float) LogUnexpectedGenericPacket(kind, "attack_stopped:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "attack_stopped:target", value_id, caster_id, target_id, i_value);
            } else {
                if (use_float) LogUnexpectedGenericPacket(kind, "attack_stopped:float", value_id, caster_id, target_id, f_value);
                else HandleAttackStopped(caster_id);
            }
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
			// log
            if (use_float) LogGenericPacket(kind, "attack_started", value_id, _caster_id, _target_id, f_value);
            else LogGenericPacket(kind, "attack_started", value_id, _caster_id, _target_id, i_value, false);
			// handle
            HandleAttackStarted(_caster_id, _target_id);
            break;
        }

        case (uint32_t)ObserverModule::GenericValueId2::hit_by_skill:
			// log
            if (use_float) LogGenericPacket(kind, "hit_by_skill", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "hit_by_skill", value_id, caster_id, target_id, i_value, true);
			// handle
            if (use_float) LogUnexpectedGenericPacket(kind, "hit_by_skill", value_id, caster_id, target_id, f_value);
            else HandleHitBySkill(caster_id, target_id, i_value);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::max_hp_reached:
			// log
            if (use_float) LogGenericPacket(kind, "max_hp_reached", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "max_hp_reached", value_id, caster_id, target_id, i_value, false);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::interrupted:
			// log
            if (use_float) LogGenericPacket(kind, "interrupted", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "interrupted", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
				if (use_float) LogUnexpectedGenericPacket(kind, "interrupted:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "interrupted:target", value_id, caster_id, target_id, i_value);
            } else {
				if (use_float) LogUnexpectedGenericPacket(kind, "interrupted:float", value_id, caster_id, target_id, f_value);
                else HandleInterrupted(caster_id);
            }
            break;

        case (uint32_t)ObserverModule::GenericValueId2::_q_attack_fail:
			// log
            if (use_float) LogGenericPacket(kind, "_q_attack_fail", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "_q_attack_fail", value_id, caster_id, target_id, i_value, false);
            break;

		// ignore
        case (uint32_t)ObserverModule::GenericValueId2::change_health_regen: break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_skill_finsihed:
			// log
            if (use_float) LogGenericPacket(kind, "attack_skill_finished", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "attack_skill_finished", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
				if (use_float) LogUnexpectedGenericPacket(kind, "attack_skill_finished:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "attack_skill_finished:target", value_id, caster_id, target_id, i_value);
            } else {
				if (use_float) LogUnexpectedGenericPacket(kind, "attack_skill_finished:float", value_id, caster_id, target_id, f_value);
                else HandleAttackSkillFinished(caster_id);
            }
            break;

        case (uint32_t)ObserverModule::GenericValueId2::instant_skill_activated:
			// log
            if (use_float) LogGenericPacket(kind, "instant_skill_activated", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "instant_skill_activated", value_id, caster_id, target_id, i_value, true);
			// handle
            if (use_float) LogUnexpectedGenericPacket(kind, "instant_skill_activated:float", value_id, caster_id, target_id, f_value);
			else HandleInstantSkillActivated(caster_id, target_id, i_value);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::attack_skill_stopped:
			// log
            if (use_float) LogGenericPacket(kind, "attack_skill_stopped", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "attack_skill_stopped", value_id, caster_id, target_id, i_value, false);
            // TODO: handle
            if (target_id != NO_AGENT) {
				if (use_float) LogUnexpectedGenericPacket(kind, "attack_skill_stopped:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "attack_skill_stopped:target", value_id, caster_id, target_id, i_value);
            } else {
				if (use_float) LogUnexpectedGenericPacket(kind, "attack_skill_stopped:float", value_id, caster_id, target_id, f_value);
				else HandleAttackSkillStopped(caster_id);
            }
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
			// log
            if (use_float) LogGenericPacket(kind, "attack_skill_activated", value_id, _caster_id, _target_id, f_value);
            else LogGenericPacket(kind, "attack_skill_activated", value_id, _caster_id, _target_id, i_value, true);
			// handle
            if (use_float) LogUnexpectedGenericPacket(kind, "attack_skill_activated", value_id, _caster_id, _target_id, f_value);
			else HandleAttackSkillStarted(_caster_id, _target_id, i_value);
            break;
        }

        case (uint32_t)ObserverModule::GenericValueId2::_q_health_modifier_3:
			// log
            if (use_float) LogGenericPacket(kind, "_q_health_modifier_3", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "_q_health_modifier_3", value_id, caster_id, target_id, i_value, false);
            break;

        case (uint32_t)ObserverModule::GenericValueId2::skill_finished:
			// log
            if (use_float) LogGenericPacket(kind, "skill_finished", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "skill_finished", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
				if (use_float) LogUnexpectedGenericPacket(kind, "skill_finished:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "skill_finished:target", value_id, caster_id, target_id, i_value);
            } else {
				if (use_float) LogUnexpectedGenericPacket(kind, "skill_finished:foat", value_id, caster_id, target_id, f_value);
				else HandleSkillFinished(caster_id);
            }
            break;

        case (uint32_t)ObserverModule::GenericValueId2::skill_stopped:
			// log
            if (use_float) LogGenericPacket(kind, "skill_stopped", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "skill_stopped", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
				if (use_float) LogUnexpectedGenericPacket(kind, "skill_stopped:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "skill_stopped:target", value_id, caster_id, target_id, i_value);
            } else {
				if (use_float) LogUnexpectedGenericPacket(kind, "skill_stopped:float", value_id, caster_id, target_id, f_value);
				else HandleSkillStopped(caster_id);
            }
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
            if (use_float) LogGenericPacket(kind, "skill_activated", value_id, _caster_id, _target_id, f_value);
            else LogGenericPacket(kind, "skill_activated", value_id, _caster_id, _target_id, i_value, true);
			// handle
            if (use_float) LogUnexpectedGenericPacket(kind, "skill_activated", value_id, _caster_id, _target_id, f_value);
			else HandleSkillActivated(_caster_id, _target_id, i_value);
            break;
        }

		// ignore
        case (uint32_t)ObserverModule::GenericValueId2::energy_spent: break;

        case (uint32_t)ObserverModule::GenericValueId2::knocked_down:
			// log
            if (use_float) LogGenericPacket(kind, "knocked_down", value_id, caster_id, target_id, f_value);
            else LogGenericPacket(kind, "knocked_down", value_id, caster_id, target_id, i_value, false);
			// handle
            if (target_id != NO_AGENT) {
				if (use_float) LogUnexpectedGenericPacket(kind, "knocked_down:float:target", value_id, caster_id, target_id, f_value);
                else LogUnexpectedGenericPacket(kind, "knocked_down:target", value_id, caster_id, target_id, i_value);
            } else {
				if (!use_float) LogUnexpectedGenericPacket(kind, "knocked_down:uint32_t", value_id, caster_id, target_id, f_value);
				else HandleKnockedDown(caster_id, static_cast<float>(f_value));
            }
			break;

        default:
            if (use_float) LogUnknownGenericPacket(kind, value_id, caster_id, target_id, f_value);
			else LogUnknownGenericPacket(kind, value_id, caster_id, target_id, i_value);
			break;
    };
}
      

void ObserverModule::LogUnexpectedGenericPacket(const std::string caller, const std::string name, const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const uint32_t value) {
    LogGenericPacket(caller, "! UNEXPECTED !", value_id, caster_id, target_id, value, false);
}

void ObserverModule::LogUnexpectedGenericPacket(const std::string caller, const std::string name, const uint32_t value_id,
	const uint32_t caster_id, const uint32_t target_id, const float value) {
    LogGenericPacket(caller, "! UNEXPECTED !", value_id, caster_id, target_id, value);
}

void ObserverModule::LogUnknownGenericPacket(const std::string caller, const uint32_t value_id, const uint32_t caster_id,
	const uint32_t target_id, const uint32_t value) {
    LogGenericPacket(caller, "UNKNOWN", value_id, caster_id, target_id, value, false);
}

void ObserverModule::LogUnknownGenericPacket(const std::string caller, const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const float value) {
    LogGenericPacket(caller, "UNKNOWN", value_id, caster_id, target_id, value);
}

void ObserverModule::_DoLogGenericPacket(std::string caller, std::string name, const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, std::string value_name) {
    uint32_t game_target_id = GW::Agents::GetTargetId();

    switch (follow_mode) {
        case ObserverModule::FollowMode::None: break;
        case ObserverModule::FollowMode::Target:
            if (!(game_target_id == caster_id || game_target_id == target_id)) return;
            break;
        case ObserverModule::FollowMode::UsedByTarget:
            if (!(game_target_id == caster_id)) return;
            break;
        case ObserverModule::FollowMode::UsedOnTarget:
            if (!(game_target_id == target_id)) return;
            break;
    }

    ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);

	std::string caster_name;
    if (caster == nullptr) caster_name = "(" + std::to_string(caster_id) +")";
    else caster_name = caster->TrimmedDisplayName();

	std::string target_name;
    if (target == nullptr) target_name = "(" + std::to_string(target_id) +")";
    else target_name = target->TrimmedDisplayName();

    std::string value_id_name = PadRight(28, "(" + PadLeft(2, std::to_string(value_id)) + ") " + name);

	__Log(caller, 0, "(" + PadLeft(2, std::to_string(value_id)) + ")" + 
		+ " | "
		+ PadRight(35, caster_name)
		+ " | "
		+ value_id_name
		+ " | "
		+ PadRight(35, value_name)
		+ " | "
		+ PadRight(33, target_name)
    );
}

void ObserverModule::LogGenericPacket(std::string caller, std::string name, const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const uint32_t value, const bool is_skill) {

	std::string value_name;
    if (is_skill) {
        ObserverModule::ObservableSkill* skill = GetObservableSkillBySkillId(value);
        if (skill != nullptr) value_name = skill->DisplayName();
        else value_name = "(" + std::to_string(value) + ")";
    } else {
		value_name = "(" + std::to_string(value) + ")";
    }

    _DoLogGenericPacket(caller, name, value_id, caster_id, target_id, value_name);
}

void ObserverModule::LogGenericPacket(std::string caller, std::string name, const uint32_t value_id, const uint32_t caster_id,
    const uint32_t target_id, const float value) {

	std::string value_name = "(" + std::to_string(value) + ")";
    _DoLogGenericPacket(caller, name, value_id, caster_id, target_id, value_name);
}


void ObserverModule::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
}


//
// Get an ObservableAgent using the agent_id
//
//
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentByAgentId(uint32_t agent_id) {
	// lazy load
	auto it = observable_agents.find(agent_id);
    if (it != observable_agents.end()) {
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


//
// Get an ObservableAgent using the Agent
//
ObserverModule::ObservableAgent* ObserverModule::GetObservableAgentByAgent(const GW::Agent& agent) {
	// lazy load
	auto it = observable_agents.find(agent.agent_id);
    if (it != observable_agents.end()) {
		ObserverModule::ObservableAgent* existing_entity = it->second;
        return existing_entity;
	}
    const GW::AgentLiving* agent_living = agent.GetAsAgentLiving();
    if (agent_living == nullptr) return nullptr;

	ObserverModule::ObservableAgent& observable_agent = CreateObservableAgent(*agent_living);
    return (&observable_agent);
}

//
// Get an ObservableAgent using the AgentLiving
//
//
ObserverModule::ObservableAgent& ObserverModule::GetObservableAgentByAgentLiving(const GW::AgentLiving& agent_living) {
	// lazy load
	auto it = observable_agents.find(agent_living.agent_id);
    if (it != observable_agents.end()) {
		ObserverModule::ObservableAgent* existing_entity = it->second;
        return *existing_entity;
	}

	ObserverModule::ObservableAgent& observable_agent = CreateObservableAgent(agent_living);
    return observable_agent;
}


//
// Get an ObservableParty using the PartyInfo
//
ObserverModule::ObservableParty& ObserverModule::GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info) {
	// lazy load
    if (observed_parties.size() < party_info.party_id) {
        __Log(__FUNCTION__, 0, "Increasing observed_parties size to: (" + std::to_string(party_info.party_id + 1) + ")");
        observed_parties.resize(party_info.party_id + 1, nullptr);
    }
    ObserverModule::ObservableParty* observed_party = observed_parties[party_info.party_id];
    if (observed_party != nullptr) return *observed_party;
    observed_party = new ObserverModule::ObservableParty(*this, party_info);
    observed_parties[party_info.party_id] = observed_party;
    return *observed_party;
}


//
// Get Agent for the ObservableAgent
//
GW::Agent* ObserverModule::ObservableAgent::GetAgent() {
    GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid() || agents.size() > agent_id) return nullptr;
	return agents[agent_id];
}


//
// Get AgentLiving for the ObservableAgent
//
GW::AgentLiving* ObserverModule::ObservableAgent::GetAgentLiving() {
    GW::Agent* agent = GetAgent();
    if (agent == nullptr) return nullptr;
    return agent->GetAsAgentLiving();
}


//
// Create an ObservableAgent from an Agent
//
ObserverModule::ObservableAgent& ObserverModule::CreateObservableAgent(const GW::AgentLiving& agent_living) {
	ObserverModule::ObservableAgent* observable_agent = new ObserverModule::ObservableAgent(*this, agent_living);
    observable_agents.insert({ observable_agent->agent_id, observable_agent });
    // __Log(__FUNCTION__, 0, "Registered agent: " + observable_agent->DisplayName());
    return *observable_agent;
}


//
// Get an ObservableSkill using a SkillID
//
// Lazy loads the Skill
//
ObserverModule::ObservableSkill* ObserverModule::GetObservableSkillBySkillId(uint32_t skill_id) {
    auto existing = observed_skills.find(skill_id);

    if (existing != observed_skills.end()) {
		// found
        return existing->second;
	}

	const GW::Skill& gw_skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    if (gw_skill.skill_id == 0) return nullptr;

	// copied from SkillListingWindow.cpp
    ObserverModule::ObservableSkill* observable_skill = new ObserverModule::ObservableSkill(*this, gw_skill);
	// make sure we initialise the name asynchronously
    observed_skills.insert({ skill_id, observable_skill });
    // __Log(__FUNCTION__, 0, "Registered skill: " + observable_skill->DisplayName());
    return observable_skill;
}


//
// Handle DamageDone
//
void ObserverModule::HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit) {
    ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    ObservableAgent* target = GetObservableAgentByAgentId(target_id);

    const bool is_npc = target == nullptr ? false : target->is_npc;
    const bool is_player = target == nullptr ? false : target->is_player;

    if (is_crit) {
		if (caster != nullptr) {
			caster->total_crits_dealt += 1;
			if (is_npc) caster->total_npc_crits_dealt += 1;
			if (is_player) caster->total_player_crits_dealt += 1;
		}
		if (target != nullptr) {
			target->total_crits_received += 1;
		}

        // can't kill a guy by healing him (hopefully?)
        if (amount_pc >= 0) return;

        GW::AgentLiving* target_living = target->GetAgentLiving();

        // did the target die from their wounds?
        if (caster != nullptr && target_living != nullptr && target_living->IsPlayer()) {
            float after_hp_pc = target_living->hp + amount_pc;
            // if target is really low just assume they died

            // For a target with 10,000hp, (1hp / 1,000,000hp) = 1E-7 (1hp = 1E-5 % of health)
            // if after_hp_pc is under 1E-5 %, then the target has less than (1 of 1,000,000hp)
            // seems pretty safe to assume he's dead
            if (after_hp_pc <= 1E-5) caster->HandleKill();
            // let the server send us a AgentState packet to notify us of the actual death
        }
    }
}

//
// Handle AgentProjectileLaunched
//
// If is_attack, signals that an attack has stopped
//
void ObserverModule::HandleAgentProjectileLaunched(const uint32_t agent_id, const bool is_attack) {
    if (is_attack) HandleAttackFinished(agent_id);
}


//
// Handle Attack Finished
//
void ObserverModule::HandleAttackFinished(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Finished);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Finished);
    }
}

//
// Handle Attack Stopped
//
void ObserverModule::HandleAttackStopped(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Stopped);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Stopped);
    }
}


//
// Handle Attack Started
//
void ObserverModule::HandleAttackStarted(const uint32_t caster_id, const uint32_t target_id) {
	const bool _is_attack = true;
	const bool _is_skill = false;
	const bool _skill_id = NO_SKILL;
	const ObservableAgent::TargetAction* action = new ObservableAgent::TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

	// notify caster
	ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ObservableAgent::ActionStartpoint::Started);
        
	// notify target
	ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
	if (target != nullptr) target->HandleReceivedActionStarted(*action, ObservableAgent::ActionStartpoint::Started);
}


//
// Handle Hit by Skill
//
void ObserverModule::HandleHitBySkill(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
	// not implemented
	ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
	ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
	ObserverModule::ObservableSkill* skill = GetObservableSkillBySkillId(skill_id);
	UNREFERENCED_PARAMETER(caster);
	UNREFERENCED_PARAMETER(target);
	UNREFERENCED_PARAMETER(skill);
}


//
// Handle Interrupted
//
void ObserverModule::HandleInterrupted(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Interrupted);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Interrupted);
    }
}

//
// Handle Attack Skill Finished
//
void ObserverModule::HandleAttackSkillFinished(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Finished);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Finished);
    }
}


//
// Handle Attack Skill Stopped
//
void ObserverModule::HandleAttackSkillStopped(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Stopped);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Stopped);
    }
}



//
// Handle Instant Skill Finished
//
void ObserverModule::HandleInstantSkillActivated(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
	const bool _is_attack = false;
	const bool _is_skill = true;
	const uint32_t _skill_id = skill_id;
	const ObservableAgent::TargetAction* action = new ObservableAgent::TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

	// notify caster
	ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ObservableAgent::ActionStartpoint::Instant);
        
	// notify target
	ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
	if (target != nullptr) target->HandleReceivedActionStarted(*action, ObservableAgent::ActionStartpoint::Instant);
    
    // TODO: move this cleanup better place... (to the ObservableAgent)
    delete action;
}


//
// Attack Skill Activated
//
void ObserverModule::HandleAttackSkillStarted(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id) {
	const bool _is_attack = true;
	const bool _is_skill = true;
	const uint32_t _skill_id = skill_id;
	const ObservableAgent::TargetAction* action = new ObservableAgent::TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

	// notify caster
	ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ObservableAgent::ActionStartpoint::Started);
        
	// notify target
	ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
	if (target != nullptr) target->HandleReceivedActionStarted(*action, ObservableAgent::ActionStartpoint::Started);
}


//
// Attack Skill Finished
//
void ObserverModule::HandleSkillFinished(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Finished);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Finished);
    }
}


//
// Handle Skill Finished
//
void ObserverModule::HandleSkillStopped(const uint32_t agent_id) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);

    if (agent != nullptr) {
        // notify caster
        const ObservableAgent::TargetAction* action = agent->HandleActionEnded(ObservableAgent::ActionEndpoint::Stopped);
        if (action == nullptr) return;

        // notify target
		 ObservableAgent* target = GetObservableAgentByAgentId(action->target_id);
        if (target == nullptr) return;
        target->HandleReceivedActionEnded(*action, ObservableAgent::ActionEndpoint::Stopped);
    }
}


//
// Handle SkillActivated
//
void ObserverModule::HandleSkillActivated(const uint32_t caster_id, const uint32_t target_id, uint32_t skill_id) {
	const bool _is_attack = false;
	const bool _is_skill = true;
	const uint32_t _skill_id = skill_id;
	const ObservableAgent::TargetAction* action = new ObservableAgent::TargetAction(caster_id, target_id, _is_attack, _is_skill, _skill_id);

	// notify caster
	ObserverModule::ObservableAgent* caster = GetObservableAgentByAgentId(caster_id);
    if (caster != nullptr) caster->HandleActionStarted(*action, ObservableAgent::ActionStartpoint::Started);
        
	// notify target
	ObserverModule::ObservableAgent* target = GetObservableAgentByAgentId(target_id);
	if (target != nullptr) target->HandleReceivedActionStarted(*action, ObservableAgent::ActionStartpoint::Started);
}


//
// Handle KnockedDown
//
void ObserverModule::HandleKnockedDown(const uint32_t agent_id, const float duration) {
	ObservableAgent* agent = GetObservableAgentByAgentId(agent_id);
    if (agent != nullptr) agent->HandleKnockedDown(duration);
}


//
// Reset the ObserverWidgets state
//
void ObserverModule::Reset() {
    __Log(__FUNCTION__, 0, "Resetting state");

    size_t i = 0;
     __Log(__FUNCTION__, 1, "Deleting (" + std::to_string(observed_skills.size()) + ") observed_skills");
    for (const auto& it : observed_skills) {
         i += 1;
		if (it.second == nullptr) continue;
        __Log(__FUNCTION__, 2, "Deleting observable_skill (" + std::to_string(i) + "): " + it.second->DisplayName());
		delete it.second;
    }
	observed_skills.clear();

    i = 0;
	 __Log(__FUNCTION__, 1, "Deleting (" + std::to_string(observable_agents.size()) + ") observed_agents");
    for (const auto& it : observable_agents) {
         i += 1;
        if (it.second == nullptr) continue;
		// don't use display name since it throw errors here on termination
        __Log(__FUNCTION__, 2, "Deleting observable_agent (" + std::to_string(i) + "): (" + std::to_string(it.second->agent_id) + ")");
		delete it.second;
    }
	observable_agents.clear();

    // i = 0;
	//  __Log(__FUNCTION__, 1, "Deleting (" + std::to_string(observed_parties.size()) + ") observed_parties");
    // for (const ObserverModule::ObservableParty* observable_party : observed_parties) {
    //      i += 1;
    //     if (observable_party == nullptr) continue;
    //     __Log(__FUNCTION__, 2, "Deleting observable_party (" + std::to_string(i) + "): (" + std::to_string(observable_party->party_id) + ")");
	// 	delete observable_party;
    // }
	// observed_parties.clear();
}


//
// Load all ObservableAgent's on the current map
//
void ObserverModule::InitializeObserverSession() {
	// load parties

	__Log(__FUNCTION__, 0, "Initializing...");

	// load all other agents
    const GW::AgentArray agents = GW::Agents::GetAgentArray();
	if (!agents.valid()) {
		__Log(__FUNCTION__, 1," Failed: \"!agents.valid()\"");
		return;
	}

    // GW::PartyContext* party_ctx = GW::GameContext::instance()->party;
    // if (party_ctx == nullptr) {
	// 	__Log(__FUNCTION__, 1, "Failed: \"partyctx == nullptr\"");
    //     return;
    // }

	size_t i = 0;

	// GW::Array<GW::PartyInfo*> parties = party_ctx->parties;
	// __Log(__FUNCTION__, 1, "Found (" + std::to_string(parties.size()) + ") parties");
	// for (const GW::PartyInfo* party_info : parties) {
	// 	i += 1;
	// 	if (party_info == nullptr) {
	// 		__Log(__FUNCTION__, 2, "Failed (" + std::to_string(i) + "): \"party_info == nullptr\"");
	// 		continue;
	// 	}
	// 	__Log(__FUNCTION__, 2, "Party (" + std::to_string(i) + ") | party_id: (" + std::to_string(party_info->party_id) + ")");
	// 	// load and synchronize the party
    //     ObserverModule::ObservableParty& observable_party = GetObservablePartyByPartyInfo(*party_info);
    //     observable_party.SynchroniseParty();
	// }

	i = 0;
	for (GW::Agent* agent : agents) {
        i += 1;
		// not found?
        if (agent == nullptr) continue;
		// trigger lazy load
		GetObservableAgentByAgentId(agent->agent_id);
	}

	observer_session_initialized = true;
}


void ObserverModule::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);

    // ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    // ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    // if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) return ImGui::End();

	// // float offset = 0.0f;
    // const float text_width = 100.0f * ImGui::GetIO().FontGlobalScale;
	// ImGui::Text("Hello :)");
    // ImGui::End();

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
    float offset = 0.0f;
    // const float tiny_text_width = 50.0f * ImGui::GetIO().FontGlobalScale;
    // const float short_text_width = 80.0f * ImGui::GetIO().FontGlobalScale;
    // const float avail_width = ImGui::GetContentRegionAvail().x;
    // const float long_text_width = 200.0f * ImGui::GetIO().FontGlobalScale;
    // ImGui::Text("#");
    // ImGui::SameLine(offset += tiny_text_width);
    // ImGui::Text("Name");
    // ImGui::SameLine(offset += long_text_width);
    // ImGui::Text("Attr");
    // ImGui::SameLine(offset += tiny_text_width);
    // ImGui::Text("Prof");
    // ImGui::SameLine(offset += tiny_text_width);
    // ImGui::Text("Type");
    // ImGui::Separator();
    // char buf[16] = {};
    // static std::wstring search_term;
    // if (ImGui::InputText("Search", buf, sizeof buf)) {
    //     search_term = GuiUtils::ToLower(GuiUtils::StringToWString(buf));
    // }

	
	if (IsActive()) {
		const GW::Agent* target_agent = GW::Agents::GetTarget();
		const GW::Agent* target_agent_living = target_agent == nullptr ? nullptr : target_agent->GetAsAgentLiving();
		if (target_agent_living != nullptr) {
			last_living_target = target_agent_living->agent_id;
		}
	}
	const uint32_t use_target_id = last_living_target;

    ObserverModule::ObservableAgent* target_observable = GetObservableAgentByAgentId(use_target_id);

	if (use_target_id != 0 && target_observable == nullptr) {
        ImGui::Text("???");
	} else if (target_observable == nullptr) {
        ImGui::Text("No selection");
    } else if (target_observable != nullptr) {
        ImGui::Text(target_observable->DisplayName().c_str());

        const float long_text	= 200.0f * ImGui::GetIO().FontGlobalScale;
        const float medium_text	= 150.0f * ImGui::GetIO().FontGlobalScale;
        const float short_text	= 80.0f * ImGui::GetIO().FontGlobalScale;
        const float mini_text	= 40.0f * ImGui::GetIO().FontGlobalScale;

		// // damage
		// offset = 0;
        // ImGui::Text("damage dealt:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(target_observable->total_damage_dealt).c_str());

		// offset = 0;
        // ImGui::Text("damage taken:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(target_observable->total_damage_received).c_str());

		// offset = 0;
        // ImGui::Text("healing dealt:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(target_observable->total_healing_dealt).c_str());

		// offset = 0;
        // ImGui::Text("healing received:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(target_observable->total_healing_received).c_str());

		offset = 0;
        ImGui::Text("Critical hits:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->total_crits_dealt).c_str());

		offset = 0;
        ImGui::Text("Critical hits received:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->total_crits_received).c_str());

		// offset = 0;
        // ImGui::Text("healing received:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(target_observable->total_healing_received).c_str());

		offset = 0;
        ImGui::Text("knocked down:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->knocked_down_count).c_str());
        ImGui::SameLine(offset += short_text);
        ImGui::Text("times");

		offset = 0;
        ImGui::Text("cancelled:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->cancelled_count).c_str());
        ImGui::SameLine(offset += short_text);
        ImGui::Text("times");

		offset = 0;
        ImGui::Text("interrupted:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->interrupted_count).c_str());
        ImGui::SameLine(offset += short_text);
        ImGui::Text("times");

        // attack stats

        ImGui::Separator();

        offset = 0;
        ImGui::Text("");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Name");
        ImGui::SameLine(offset += long_text);
        ImGui::Text("Atm");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Cnl");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Int");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Fin");
        ImGui::SameLine(offset += mini_text);
        ImGui::Separator();

        offset = 0;
        ImGui::Text("");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Attacks:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_done.started).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_done.stopped).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_done.interrupted).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_done.finished).c_str());

        offset = 0;
        ImGui::Text("");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Skills:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->total_skills_used.started).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_skills_used.stopped).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_skills_used.interrupted).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_skills_used.finished).c_str());

		offset = 0;
        ImGui::Text("");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Attacks taken:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_received.started).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_received.stopped).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_received.interrupted).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_attacks_received.finished).c_str());

		offset = 0;
        ImGui::Text("");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Skills received:");
        ImGui::SameLine(offset += long_text);
        ImGui::Text(std::to_string(target_observable->total_skills_received.started).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_skills_received.stopped).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_skills_received.interrupted).c_str());
        ImGui::SameLine(offset += mini_text);
        ImGui::Text(std::to_string(target_observable->total_skills_received.finished).c_str());

        // skills

        ImGui::Separator();

        offset = 0;
        ImGui::Text("#");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Name");
        ImGui::SameLine(offset += long_text);
        ImGui::Text("Atm");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Cnl");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Int");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Fin");
        ImGui::SameLine(offset += mini_text);
        ImGui::Separator();

        size_t i = 0;
        for (uint32_t skill_id : target_observable->skill_ids_used) {
            i += 1;
            offset = 0;
            // auto usage_it = skills_used_by_agent;
            ObserverModule::ObservableSkill* skill = GetObservableSkillBySkillId(skill_id);

            ImGui::Text((std::to_string(i) + ".").c_str());
			ImGui::SameLine(offset += mini_text);

            if (skill == nullptr) {
				ImGui::Text((std::string("? (") + std::to_string(skill_id) + ")").c_str());
				ImGui::SameLine(offset += long_text);
            } else {
				ImGui::Text(skill->DisplayName().c_str());
				ImGui::SameLine(offset += long_text);
			}

			auto it_usages = target_observable->skills_used.find(skill_id);
            if (it_usages == target_observable->skills_used.end()) {
				ImGui::Text("?");
            } else {
				ImGui::Text(std::to_string(it_usages->second->started).c_str());
				ImGui::SameLine(offset += mini_text);
				ImGui::Text(std::to_string(it_usages->second->stopped).c_str());
				ImGui::SameLine(offset += mini_text);
				ImGui::Text(std::to_string(it_usages->second->interrupted).c_str());
				ImGui::SameLine(offset += mini_text);
				ImGui::Text(std::to_string(it_usages->second->finished).c_str());
			}
		}
    }
    ImGui::End();
}


void ObserverModule::WriteObserverWidget() {
	//
}



void ObserverModule::LoadSettings(CSimpleIni* ini) {
	// ToolboxWidget::LoadSettings(ini);
	ToolboxWindow::LoadSettings(ini);
	// TODO
}


void ObserverModule::SaveSettings(CSimpleIni* ini) {
	// ToolboxWidget::SaveSettings(ini);
	ToolboxWindow::SaveSettings(ini);
	// TODO
	// inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
}


void ObserverModule::DrawSettingInternal() {
	// TODO
}

