#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ObserverModule.h>

#include <Windows/ObserverExportWindow.h>
#include <Utils/TextUtils.h>

#include <RestClient.h>

void ObserverExportWindow::Initialize()
{
    ToolboxWindow::Initialize();
}

// Convert to JSON (Version 0.1)
glz::generic ObserverExportWindow::ToJSON_V_0_1()
{
    glz::generic json;
    ObserverModule& observer_module = ObserverModule::Instance();
    const std::vector<uint32_t>& party_ids = observer_module.GetObservablePartyIds();

    auto shared_stats_to_json = [](const ObserverModule::SharedStats& stats) {
        glz::generic shared_json;
        shared_json["total_crits_received"] = stats.total_crits_received;
        shared_json["total_crits_dealt"] = stats.total_crits_dealt;
        shared_json["total_party_crits_received"] = stats.total_party_crits_received;
        shared_json["total_party_crits_dealt"] = stats.total_party_crits_dealt;
        shared_json["knocked_down_count"] = stats.knocked_down_count;
        shared_json["interrupted_count"] = stats.interrupted_count;
        shared_json["interrupted_skills_count"] = stats.interrupted_skills_count;
        shared_json["cancelled_count"] = stats.cancelled_count;
        shared_json["cancelled_skills_count"] = stats.cancelled_skills_count;
        shared_json["knocked_down_duration"] = stats.knocked_down_duration;
        shared_json["deaths"] = stats.deaths;
        shared_json["kills"] = stats.kills;
        shared_json["kdr_str"] = stats.kdr_str;
        return shared_json;
    };

    json["parties"] = glz::generic::array_t{};
    json["skills"] = glz::generic::array_t{};

    for (const uint32_t party_id : party_ids) {
        // parties
        const ObserverModule::ObservableParty* party = observer_module.GetObservablePartyById(party_id);
        if (!party) {
            json["parties"].get_array().push_back(glz::generic{nullptr});
            continue;
        }
        json["parties"].get_array().push_back([&] {
            // parties -> party
            glz::generic json_party;
            json_party["party_id"] = party->party_id;
            json_party["stats"] = shared_stats_to_json(party->stats);
            
            // Party aggregate health snapshots (recorded every 15 seconds)
            json_party["health_snapshots"] = glz::generic::array_t{};
            for (const auto& snapshot : party->health_snapshots) {
                glz::generic snapshot_json;
                snapshot_json["timestamp_ms"] = snapshot.timestamp_ms;
                snapshot_json["hp_percentage"] = snapshot.hp_percentage;
                snapshot_json["hp_value"] = snapshot.hp_value;
                snapshot_json["max_hp"] = snapshot.max_hp;
                json_party["health_snapshots"].get_array().push_back(snapshot_json);
            }

            // Shrine captures (captured shrines by this party)
            json_party["shrine_captures"] = glz::generic::array_t{};
            for (const auto& shrine_capture : party->shrine_captures) {
                glz::generic shrine_json;
                shrine_json["timestamp_ms"] = shrine_capture.timestamp_ms;
                json_party["shrine_captures"].get_array().push_back(shrine_json);
            }

            // Tower captures (captured towers/flags by this party)
            json_party["tower_captures"] = glz::generic::array_t{};
            for (const auto& tower_capture : party->tower_captures) {
                glz::generic tower_json;
                tower_json["timestamp_ms"] = tower_capture.timestamp_ms;
                json_party["tower_captures"].get_array().push_back(tower_json);
            }

            json_party["members"] = glz::generic::array_t{};
            for (const uint32_t agent_id : party->agent_ids) {
                // parties -> party -> agents
                ObserverModule::ObservableAgent* agent = observer_module.GetObservableAgentById(agent_id);
                if (!agent) {
                    json_party["members"].get_array().push_back(glz::generic{nullptr});
                    continue;
                }
                json_party["members"].get_array().push_back([&] {
                    // parties -> party -> agents -> agent
                    glz::generic json_agent;
                    json_agent["display_name"] = agent->DisplayName();
                    json_agent["raw_name"] = agent->RawName();
                    json_agent["debug_name"] = agent->DebugName();
                    json_agent["sanitized_name"] = agent->SanitizedName();
                    json_agent["party_id"] = agent->party_id;
                    json_agent["party_index"] = agent->party_index;
                    json_agent["primary"] = agent->primary;
                    json_agent["secondary"] = agent->secondary;
                    json_agent["profession"] = agent->profession;
                    json_agent["stats"] = shared_stats_to_json(agent->stats);

                    // Death events (all deaths for this agent)
                    json_agent["death_events"] = glz::generic::array_t{};
                    for (const auto& death : agent->death_events) {
                        glz::generic death_json;
                        death_json["timestamp_ms"] = death.timestamp_ms;
                        death_json["position_x"] = death.position_x;
                        death_json["position_y"] = death.position_y;
                        death_json["killer_agent_id"] = death.killer_agent_id;
                        death_json["killing_skill_id"] = death.killing_skill_id;
                        death_json["is_npc"] = death.is_npc;
                        json_agent["death_events"].get_array().push_back(death_json);
                    }

                    // Resurrection events (all resurrections for this agent)
                    json_agent["resurrection_events"] = glz::generic::array_t{};
                    for (const auto& rez : agent->resurrection_events) {
                        glz::generic rez_json;
                        rez_json["timestamp_ms"] = rez.timestamp_ms;
                        rez_json["resurrector_agent_id"] = rez.resurrector_agent_id;

                        // Add resurrection type
                        const char* res_type_str = "unknown";
                        switch (rez.resurrection_type) {
                            case ObserverModule::ResurrectionType::Skill: res_type_str = "skill"; break;
                            case ObserverModule::ResurrectionType::BaseResurrection: res_type_str = "base_resurrection"; break;
                            default: res_type_str = "unknown"; break;
                        }
                        rez_json["resurrection_type"] = res_type_str;

                        json_agent["resurrection_events"].get_array().push_back(rez_json);
                    }

                    for (const auto skill_id : agent->stats.skill_ids_used) {
                        // parties -> party -> agents -> agent -> skills
                        ObserverModule::ObservableSkill* skill = ObserverModule::Instance().GetObservableSkillById(skill_id);
                        if (!skill) {
                            json["skills"].get_array().push_back(glz::generic{nullptr});
                            continue;
                        }
                        json["skills"].get_array().push_back([&] {
                            // parties -> party -> agents -> agent -> skills -> skill
                            glz::generic json_skill;
                            json_skill["name"] = skill->Name();
                            return json_skill;
                        }());
                    }
                    return json_agent;
                }());
                return json;
            }
            return json_party;
        }());
    }

    return json;
}

// Convert to JSON (Version 1.0)
glz::generic ObserverExportWindow::ToJSON_V_1_0()
{
    glz::generic json;
    ObserverModule& om = ObserverModule::Instance();

    // name is built by iterating over the parties
    bool name_prepend_vs = false;
    std::string name = "";

    json["match_finished"] = om.match_finished;
    json["winning_party_id"] = om.winning_party_id;
    json["match_duration_ms_total"] = om.match_duration_ms_total.count();
    json["match_duration_ms"] = om.match_duration_ms.count();
    json["match_duration_secs"] = om.match_duration_secs.count();
    json["match_duration_mins"] = om.match_duration_mins.count();
    json["match_type"] = Instance().match_type;
    json["match_date"] = Instance().match_date;
    json["mat_round"] = Instance().mat_round;

    // Use the map from when the match started (if available), otherwise fall back to current map
    ObserverModule::ObservableMap* map = om.match_start_map ? om.match_start_map : om.GetMap();
    
    json["map"] = glz::generic{nullptr};
    if (map) {
        json["map"] = glz::generic::object_t{};
        json["map"]["map_id"] = static_cast<uint32_t>(map->map_id);
        json["map"]["name"] = map->Name();
        json["map"]["description"] = map->Description();
        json["map"]["is_pvp"] = map->GetIsPvP();
        json["map"]["is_guild_hall"] = map->GetIsGuildHall();
        json["map"]["campaign"] = map->campaign;
        json["map"]["continent"] = map->continent;
        json["map"]["region"] = map->region;
        json["map"]["type"] = map->type;
        json["map"]["flags"] = map->flags;
        json["map"]["name_id"] = map->name_id;
        json["map"]["description_id"] = map->description_id;
    }

    auto action_to_json = [](const ObserverModule::ObservedAction& action) {
        glz::generic action_json;
        action_json["started"] = action.started;
        action_json["stopped"] = action.stopped;
        action_json["interrupted"] = action.interrupted;
        action_json["finished"] = action.finished;
        action_json["integrity"] = action.integrity;
        return action_json;
    };

    auto shared_stats_to_json = [&action_to_json](const ObserverModule::SharedStats& stats) {
        glz::generic stats_json;
        stats_json["total_crits_received"] = stats.total_crits_received;
        stats_json["total_crits_dealt"] = stats.total_crits_dealt;
        stats_json["total_party_crits_received"] = stats.total_party_crits_received;
        stats_json["total_party_crits_dealt"] = stats.total_party_crits_dealt;
        stats_json["knocked_down_count"] = stats.knocked_down_count;
        stats_json["interrupted_count"] = stats.interrupted_count;
        stats_json["interrupted_skills_count"] = stats.interrupted_skills_count;
        stats_json["cancelled_count"] = stats.cancelled_count;
        stats_json["cancelled_skills_count"] = stats.cancelled_skills_count;
        stats_json["knocked_down_duration"] = stats.knocked_down_duration;
        stats_json["deaths"] = stats.deaths;
        stats_json["kills"] = stats.kills;
        stats_json["kdr_str"] = stats.kdr_str;
        stats_json["total_attacks_dealt"] = action_to_json(stats.total_attacks_dealt);
        stats_json["total_attacks_received"] = action_to_json(stats.total_attacks_received);
        stats_json["total_attacks_dealt_to_other_parties"] = action_to_json(stats.total_attacks_dealt_to_other_parties);
        stats_json["total_attacks_received_from_other_parties"] = action_to_json(stats.total_attacks_received_from_other_parties);
        stats_json["total_skills_used"] = action_to_json(stats.total_skills_used);
        stats_json["total_skills_received"] = action_to_json(stats.total_skills_received);
        stats_json["total_skills_used_on_own_party"] = action_to_json(stats.total_skills_used_on_own_party);
        stats_json["total_skills_used_on_other_parties"] = action_to_json(stats.total_skills_used_on_other_parties);
        stats_json["total_skills_received_from_own_party"] = action_to_json(stats.total_skills_received_from_own_party);
        stats_json["total_skills_received_from_other_parties"] = action_to_json(stats.total_skills_received_from_other_parties);
        stats_json["total_skills_used_on_own_team"] = action_to_json(stats.total_skills_used_on_own_team);
        stats_json["total_skills_used_on_other_teams"] = action_to_json(stats.total_skills_used_on_other_teams);
        stats_json["total_skills_received_from_own_team"] = action_to_json(stats.total_skills_received_from_own_team);
        stats_json["total_skills_received_from_other_teams"] = action_to_json(stats.total_skills_received_from_other_teams);
        stats_json["total_damage_dealt"] = stats.total_damage_dealt;
        stats_json["total_damage_received"] = stats.total_damage_received;
        stats_json["total_party_damage_dealt"] = stats.total_party_damage_dealt;
        stats_json["total_party_damage_received"] = stats.total_party_damage_received;
        stats_json["total_healing_dealt"] = stats.total_healing_dealt;
        stats_json["total_healing_received"] = stats.total_healing_received;
        stats_json["total_party_healing_dealt"] = stats.total_party_healing_dealt;
        stats_json["total_party_healing_received"] = stats.total_party_healing_received;
        return stats_json;
    };

    const std::vector<uint32_t>& guild_ids = om.GetObservableGuildIds();
    const std::vector<uint32_t>& agent_ids = om.GetObservableAgentIds();
    const std::vector<uint32_t>& party_ids = om.GetObservablePartyIds();
    const std::vector<GW::Constants::SkillID>& skill_ids = om.GetObservableSkillIds();

    // EXPERIMENT (glaze 7.6): broader STL support — try direct vector and {}
    json["guilds"]["ids"] = guild_ids;
    json["guilds"]["by_id"] = glz::generic::object_t{};
    for (uint32_t guild_id : guild_ids) {
        std::string guild_id_s = std::to_string(guild_id);
        ObserverModule::ObservableGuild* guild = om.GetObservableGuildById(guild_id);
        if (!guild) {
            json["guilds"]["by_id"][guild_id_s] = glz::generic{nullptr};
            continue;
        }
        json["guilds"]["by_id"][guild_id_s]["guild_id"] = guild->guild_id;
        json["guilds"]["by_id"][guild_id_s]["key"] = guild->key.k;
        json["guilds"]["by_id"][guild_id_s]["name"] = guild->name;
        json["guilds"]["by_id"][guild_id_s]["tag"] = guild->tag;
        json["guilds"]["by_id"][guild_id_s]["wrapped_tag"] = guild->wrapped_tag;
        json["guilds"]["by_id"][guild_id_s]["rank"] = guild->rank;
        json["guilds"]["by_id"][guild_id_s]["rating"] = guild->rating;
        json["guilds"]["by_id"][guild_id_s]["faction"] = guild->faction;
        json["guilds"]["by_id"][guild_id_s]["faction_point"] = guild->faction_point;
        json["guilds"]["by_id"][guild_id_s]["qualifier_point"] = guild->qualifier_point;
        json["guilds"]["by_id"][guild_id_s]["cape_trim"] = guild->cape_trim;
    }

    // skills
    json["skills"]["ids"] = skill_ids;
    json["skills"]["by_id"] = glz::generic::object_t{};
    for (auto skill_id : skill_ids) {
        std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
        ObserverModule::ObservableSkill* skill = om.GetObservableSkillById(skill_id);
        if (!skill) {
            json["skills"]["by_id"][skill_id_s] = glz::generic{nullptr};
            continue;
        }
        json["skills"]["by_id"][skill_id_s]["skill_id"] = skill->gw_skill.skill_id;
        json["skills"]["by_id"][skill_id_s]["name"] = skill->Name();
        json["skills"]["by_id"][skill_id_s]["stats"]["total_usages"] = action_to_json(skill->stats.total_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_self_usages"] = action_to_json(skill->stats.total_self_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_other_usages"] = action_to_json(skill->stats.total_other_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_own_party_usages"] = action_to_json(skill->stats.total_own_party_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_other_party_usages"] = action_to_json(skill->stats.total_other_party_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_own_team_usages"] = action_to_json(skill->stats.total_own_team_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_other_team_usages"] = action_to_json(skill->stats.total_other_team_usages);
        json["skills"]["by_id"][skill_id_s]["campaign"] = skill->gw_skill.campaign;
        json["skills"]["by_id"][skill_id_s]["type"] = skill->gw_skill.type;
        json["skills"]["by_id"][skill_id_s]["sepcial"] = skill->gw_skill.special;
        json["skills"]["by_id"][skill_id_s]["activation"] = skill->gw_skill.activation;
        json["skills"]["by_id"][skill_id_s]["combo_req"] = skill->gw_skill.combo_req;
        json["skills"]["by_id"][skill_id_s]["effect1"] = skill->gw_skill.effect1;
        json["skills"]["by_id"][skill_id_s]["condition"] = skill->gw_skill.condition;
        json["skills"]["by_id"][skill_id_s]["effect2"] = skill->gw_skill.effect2;
        json["skills"]["by_id"][skill_id_s]["weapon_req"] = skill->gw_skill.weapon_req;
        json["skills"]["by_id"][skill_id_s]["profession"] = skill->gw_skill.profession;
        json["skills"]["by_id"][skill_id_s]["attribute"] = skill->gw_skill.attribute;
        json["skills"]["by_id"][skill_id_s]["skill_id_pvp"] = skill->gw_skill.skill_id_pvp;
        json["skills"]["by_id"][skill_id_s]["combo"] = skill->gw_skill.combo;
        json["skills"]["by_id"][skill_id_s]["target"] = skill->gw_skill.target;
        json["skills"]["by_id"][skill_id_s]["skill_equip_type"] = skill->gw_skill.skill_equip_type;
        json["skills"]["by_id"][skill_id_s]["energy_cost"] = skill->gw_skill.energy_cost;
        json["skills"]["by_id"][skill_id_s]["health_cost"] = skill->gw_skill.health_cost;
        json["skills"]["by_id"][skill_id_s]["adrenaline"] = skill->gw_skill.adrenaline;
        json["skills"]["by_id"][skill_id_s]["activation"] = skill->gw_skill.activation;
        json["skills"]["by_id"][skill_id_s]["aftercast"] = skill->gw_skill.aftercast;
        json["skills"]["by_id"][skill_id_s]["duration0"] = skill->gw_skill.duration0;
        json["skills"]["by_id"][skill_id_s]["duration15"] = skill->gw_skill.duration15;
        json["skills"]["by_id"][skill_id_s]["recharge"] = skill->gw_skill.recharge;
        json["skills"]["by_id"][skill_id_s]["scale0"] = skill->gw_skill.scale0;
        json["skills"]["by_id"][skill_id_s]["scale15"] = skill->gw_skill.scale15;
        json["skills"]["by_id"][skill_id_s]["bonusScale0"] = skill->gw_skill.bonusScale0;
        json["skills"]["by_id"][skill_id_s]["bonusScale15"] = skill->gw_skill.bonusScale15;
        json["skills"]["by_id"][skill_id_s]["aoe_range"] = skill->gw_skill.aoe_range;
        json["skills"]["by_id"][skill_id_s]["const_effect"] = skill->gw_skill.const_effect;
        json["skills"]["by_id"][skill_id_s]["icon_file_id"] = skill->gw_skill.icon_file_id;
    }

    // parties
    json["parties"]["ids"] = party_ids;
    json["parties"]["by_id"] = glz::generic::object_t{};
    for (uint32_t party_id : party_ids) {
        std::string party_id_s = std::to_string(party_id);
        ObserverModule::ObservableParty* party = om.GetObservablePartyById(party_id);
        if (!party) {
            json["parties"]["by_id"][party_id_s] = glz::generic{nullptr};
            continue;
        }
        if (name_prepend_vs) {
            name.append(" vs ");
        }
        name.append(party->display_name);
        name_prepend_vs = true;
        json["parties"]["by_id"][party_id_s]["party_id"] = party->party_id;
        json["parties"]["by_id"][party_id_s]["name"] = party->name;
        json["parties"]["by_id"][party_id_s]["display_name"] = party->display_name;
        json["parties"]["by_id"][party_id_s]["is_victorious"] = party->is_victorious;
        json["parties"]["by_id"][party_id_s]["is_defeated"] = party->is_defeated;
        json["parties"]["by_id"][party_id_s]["guild_id"] = party->guild_id;
        json["parties"]["by_id"][party_id_s]["agent_ids"] = party->agent_ids;
        json["parties"]["by_id"][party_id_s]["rank"] = party->rank;
        json["parties"]["by_id"][party_id_s]["rank_str"] = party->rank_str;
        json["parties"]["by_id"][party_id_s]["rating"] = party->rating;
        json["parties"]["by_id"][party_id_s]["stats"] = shared_stats_to_json(party->stats);
        
        // Morale boost events (with timestamps)
        json["parties"]["by_id"][party_id_s]["morale_boosts"] = glz::generic::array_t{};
        for (const auto& morale_boost : party->morale_boosts) {
            glz::generic morale_json;
            morale_json["timestamp_ms"] = morale_boost.timestamp_ms;
            json["parties"]["by_id"][party_id_s]["morale_boosts"].get_array().push_back(morale_json);
        }
        
        // Shrine capture events (with timestamps)
        json["parties"]["by_id"][party_id_s]["shrine_captures"] = glz::generic::array_t{};
        for (const auto& shrine_capture : party->shrine_captures) {
            glz::generic shrine_json;
            shrine_json["timestamp_ms"] = shrine_capture.timestamp_ms;
            json["parties"]["by_id"][party_id_s]["shrine_captures"].get_array().push_back(shrine_json);
        }
        
        // Tower capture events (with timestamps)
        json["parties"]["by_id"][party_id_s]["tower_captures"] = glz::generic::array_t{};
        for (const auto& tower_capture : party->tower_captures) {
            glz::generic tower_json;
            tower_json["timestamp_ms"] = tower_capture.timestamp_ms;
            json["parties"]["by_id"][party_id_s]["tower_captures"].get_array().push_back(tower_json);
        }
        
        // Party aggregate health snapshots (recorded every 15 seconds)
        json["parties"]["by_id"][party_id_s]["health_snapshots"] = glz::generic::array_t{};
        for (const auto& snapshot : party->health_snapshots) {
            glz::generic snapshot_json;
            snapshot_json["timestamp_ms"] = snapshot.timestamp_ms;
            snapshot_json["hp_percentage"] = snapshot.hp_percentage;
            snapshot_json["hp_value"] = snapshot.hp_value;
            snapshot_json["max_hp"] = snapshot.max_hp;
            json["parties"]["by_id"][party_id_s]["health_snapshots"].get_array().push_back(snapshot_json);
        }
    }

    // agents
    json["agents"]["ids"] = agent_ids;
    json["agents"]["by_id"] = glz::generic::object_t{};
    for (uint32_t agent_id : agent_ids) {
        std::string agent_id_s = std::to_string(agent_id);
        ObserverModule::ObservableAgent* agent = om.GetObservableAgentById(agent_id);
        if (!agent) {
            json["agents"]["by_id"][agent_id_s] = glz::generic{nullptr};
            continue;
        }
        json["agents"]["by_id"][agent_id_s]["agent_id"] = agent->agent_id;
        json["agents"]["by_id"][agent_id_s]["display_name"] = agent->DisplayName();
        json["agents"]["by_id"][agent_id_s]["raw_name"] = agent->RawName();
        json["agents"]["by_id"][agent_id_s]["debug_name"] = agent->DebugName();
        json["agents"]["by_id"][agent_id_s]["sanitized_name"] = agent->SanitizedName();
        json["agents"]["by_id"][agent_id_s]["party_id"] = agent->party_id;
        json["agents"]["by_id"][agent_id_s]["party_index"] = agent->party_index;
        json["agents"]["by_id"][agent_id_s]["primary"] = agent->primary;
        json["agents"]["by_id"][agent_id_s]["secondary"] = agent->secondary;
        json["agents"]["by_id"][agent_id_s]["profession"] = agent->profession;
        json["agents"]["by_id"][agent_id_s]["guild_id"] = agent->guild_id;
        json["agents"]["by_id"][agent_id_s]["stats"] = shared_stats_to_json(agent->stats);

        // attacks

        // attacks dealt (by agent)
        for (auto& [target_id, action] : agent->stats.attacks_dealt_to_agents) {
            std::string target_id_s = std::to_string(target_id);
            if (!action) {
                json["agents"]["by_id"][agent_id_s]["stats"]["attacks_dealt_to_agents"][target_id_s] = glz::generic{nullptr};
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["attacks_dealt_to_agents"][target_id_s] = action_to_json(*action);
        }

        // attacks received (by agent)
        for (auto& [caster_id, action] : agent->stats.attacks_received_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            if (!action) {
                json["agents"]["by_id"][agent_id_s]["stats"]["attacks_received_from_agents"][caster_id_s] = glz::generic{nullptr};
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["attacks_received_from_agents"][caster_id_s] = action_to_json(*action);
        }

        // skills

        // skills used
        json["agents"]["by_id"][agent_id_s]["stats"]["skill_ids_used"] = agent->stats.skill_ids_used;
        for (auto skill_id : agent->stats.skill_ids_used) {
            std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
            const auto it_skill = agent->stats.skills_used.find(skill_id);
            if (it_skill == agent->stats.skills_used.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_used"][skill_id_s] = glz::generic{nullptr};
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_used"][skill_id_s] = action_to_json(*it_skill->second);
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_used"][skill_id_s]["skill_id"] = it_skill->second->skill_id;
        }

        // skills received
        json["agents"]["by_id"][agent_id_s]["stats"]["skill_ids_received"] = agent->stats.skill_ids_received;
        for (auto skill_id : agent->stats.skill_ids_received) {
            std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
            const auto it_skill = agent->stats.skills_received.find(skill_id);
            if (it_skill == agent->stats.skills_received.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_received"][skill_id_s] = glz::generic{nullptr};
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_received"][skill_id_s] = action_to_json(*it_skill->second);
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_received"][skill_id_s]["skill_id"] = it_skill->second->skill_id;
        }

        // skills used (by agent)
        for (auto& [target_id, agent_skill_ids] : agent->stats.skill_ids_used_on_agents) {
            std::string target_id_s = std::to_string(target_id);
            auto it_target = agent->stats.skills_used_on_agents.find(target_id);
            if (it_target == agent->stats.skills_used_on_agents.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_used_on_agents"][target_id_s] = glz::generic{nullptr};
                continue;
            }
            for (auto skill_id : agent_skill_ids) {
                std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
                auto it_skill = it_target->second.find(skill_id);
                if (it_skill == it_target->second.end()) {
                    json["agents"]["by_id"][agent_id_s]["stats"]["skills_used_on_agents"][target_id_s][skill_id_s] = glz::generic{nullptr};
                    continue;
                }
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_used_on_agents"][target_id_s][skill_id_s] = action_to_json(*it_skill->second);
            }
        }

        // skills received (by agent)
        for (auto& [caster_id, agent_skill_ids] : agent->stats.skill_ids_received_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            auto it_target = agent->stats.skills_received_from_agents.find(caster_id);
            if (it_target == agent->stats.skills_received_from_agents.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_received_from_agents"][caster_id_s] = glz::generic{nullptr};
                continue;
            }
            for (auto skill_id : agent_skill_ids) {
                std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
                auto it_skill = it_target->second.find(skill_id);
                if (it_skill == it_target->second.end()) {
                    json["agents"]["by_id"][agent_id_s]["stats"]["skills_received_from_agents"][caster_id_s][skill_id_s] = glz::generic{nullptr};
                    continue;
                }
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_received_from_agents"][caster_id_s][skill_id_s] = action_to_json(*it_skill->second);
            }
        }

        // damage dealt (by agent)
        for (auto& [target_id, damage] : agent->stats.damage_dealt_to_agents) {
            std::string target_id_s = std::to_string(target_id);
            json["agents"]["by_id"][agent_id_s]["stats"]["damage_dealt_to_agents"][target_id_s] = damage;
        }

        // damage received (by agent)
        for (auto& [caster_id, damage] : agent->stats.damage_received_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            json["agents"]["by_id"][agent_id_s]["stats"]["damage_received_from_agents"][caster_id_s] = damage;
        }

        // healing dealt (by agent)
        for (auto& [target_id, healing] : agent->stats.healing_dealt_to_agents) {
            std::string target_id_s = std::to_string(target_id);
            json["agents"]["by_id"][agent_id_s]["stats"]["healing_dealt_to_agents"][target_id_s] = healing;
        }

        // healing received (by agent)
        for (auto& [caster_id, healing] : agent->stats.healing_received_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            json["agents"]["by_id"][agent_id_s]["stats"]["healing_received_from_agents"][caster_id_s] = healing;
        }

        // damage by skill
        for (auto& [skill_id, damage] : agent->stats.damage_by_skill) {
            std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
            json["agents"]["by_id"][agent_id_s]["stats"]["damage_by_skill"][skill_id_s] = damage;
        }

        // healing by skill
        for (auto& [skill_id, healing] : agent->stats.healing_by_skill) {
            std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
            json["agents"]["by_id"][agent_id_s]["stats"]["healing_by_skill"][skill_id_s] = healing;
        }

        // damage by skill to agents
        for (auto& [target_id, skill_damage_map] : agent->stats.damage_by_skill_to_agents) {
            std::string target_id_s = std::to_string(target_id);
            for (auto& [skill_id, damage] : skill_damage_map) {
                std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
                json["agents"]["by_id"][agent_id_s]["stats"]["damage_by_skill_to_agents"][target_id_s][skill_id_s] = damage;
            }
        }

        // damage from skill from agents
        for (auto& [caster_id, skill_damage_map] : agent->stats.damage_from_skill_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            for (auto& [skill_id, damage] : skill_damage_map) {
                std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
                json["agents"]["by_id"][agent_id_s]["stats"]["damage_from_skill_from_agents"][caster_id_s][skill_id_s] = damage;
            }
        }

        // healing by skill to agents
        for (auto& [target_id, skill_healing_map] : agent->stats.healing_by_skill_to_agents) {
            std::string target_id_s = std::to_string(target_id);
            for (auto& [skill_id, healing] : skill_healing_map) {
                std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
                json["agents"]["by_id"][agent_id_s]["stats"]["healing_by_skill_to_agents"][target_id_s][skill_id_s] = healing;
            }
        }

        // healing from skill from agents
        for (auto& [caster_id, skill_healing_map] : agent->stats.healing_from_skill_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            for (auto& [skill_id, healing] : skill_healing_map) {
                std::string skill_id_s = std::to_string(std::to_underlying(skill_id));
                json["agents"]["by_id"][agent_id_s]["stats"]["healing_from_skill_from_agents"][caster_id_s][skill_id_s] = healing;
            }
        }

        // Death events (all deaths for this agent)
        json["agents"]["by_id"][agent_id_s]["death_events"] = glz::generic::array_t{};
        for (const auto& death : agent->death_events) {
            glz::generic death_json;
            death_json["timestamp_ms"] = death.timestamp_ms;
            death_json["position_x"] = death.position_x;
            death_json["position_y"] = death.position_y;
            death_json["killer_agent_id"] = death.killer_agent_id;
            death_json["killing_skill_id"] = death.killing_skill_id;
            death_json["is_npc"] = death.is_npc;
            json["agents"]["by_id"][agent_id_s]["death_events"].get_array().push_back(death_json);
        }
        
        // Resurrection events (all resurrections for this agent)
        json["agents"]["by_id"][agent_id_s]["resurrection_events"] = glz::generic::array_t{};
        for (const auto& rez : agent->resurrection_events) {
            glz::generic rez_json;
            rez_json["timestamp_ms"] = rez.timestamp_ms;
            rez_json["resurrector_agent_id"] = rez.resurrector_agent_id;
            
            // Add resurrection type
            const char* res_type_str = "unknown";
            switch (rez.resurrection_type) {
                case ObserverModule::ResurrectionType::Skill: res_type_str = "skill"; break;
                case ObserverModule::ResurrectionType::BaseResurrection: res_type_str = "base_resurrection"; break;
                default: res_type_str = "unknown"; break;
            }
            rez_json["resurrection_type"] = res_type_str;
            
            json["agents"]["by_id"][agent_id_s]["resurrection_events"].get_array().push_back(rez_json);
        }
    }

    json["name"] = name;

    return json;
}

std::string ObserverExportWindow::PadLeft(std::string input, const uint8_t count, const char c)
{
    input.insert(input.begin(), count - input.size(), c);
    return input;
}


// Export to GWRank.com
void ObserverExportWindow::ExportToGWRank()
{
    ObserverModule& observer_module = ObserverModule::Instance();
    
    // Check if match is finished
    if (!observer_module.match_finished) {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, L"<c=#FF0000>Match is not finished yet. Cannot export to GWRank.com.</c>");
        return;
    }
    
    // Check if API key is configured
    if (Instance().gwrank_api_key.empty()) {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, L"<c=#FF0000>API key not configured. Please set it in the Observer Export settings.</c>");
        return;
    }
    
    // Generate JSON v1.0
    glz::generic json = ToJSON_V_1_0();
    json["verson"] = "1.0";
    std::string json_str = glz::write<glz::opts{.prettify = true}>(json).value_or(std::string{});
    
    // Build a multipart/form-data body containing a single "json_file" part.
    const std::string boundary = "----GWToolboxBoundary7d8e6f4c2a1b";
    std::string body;
    body.append("--");
    body.append(boundary);
    body.append("\r\n");
    body.append("Content-Disposition: form-data; name=\"json_file\"; filename=\"match.json\"\r\n");
    body.append("Content-Type: application/json\r\n\r\n");
    body.append(json_str);
    body.append("\r\n--");
    body.append(boundary);
    body.append("--\r\n");

    RestClient client;
    client.SetUrl(Instance().gwrank_endpoint.c_str());
    client.SetMethod(HttpMethod::Post);
    const std::string content_type = "multipart/form-data; boundary=" + boundary;
    client.SetHeader("Content-Type", content_type.c_str());
    const std::string auth_header_value = "Bearer " + Instance().gwrank_api_key;
    client.SetHeader("Authorization", auth_header_value.c_str());
    client.SetPostContent(body, ContentFlag::ByRef);
    client.SetTimeoutSec(30);
    client.SetVerifyPeer(false);
    client.SetVerifyHost(false);

    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, L"<c=#00FF00>Uploading match data to GWRank.com...</c>");
    client.Execute();

    const int status_code = client.GetStatusCode();
    if (client.GetStatus() != ResponseStatus::Completed) {
        wchar_t error_msg[512];
        swprintf(error_msg, 512, L"<c=#FF0000>Failed to upload: %S</c>", client.GetStatusStr());
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, error_msg);
    }
    else if (status_code >= 200 && status_code < 300) {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, L"<c=#00FF00>Successfully uploaded match to GWRank.com!</c>");
    }
    else {
        const std::string& response_data = client.GetContent();
        wchar_t error_msg[256];
        swprintf(error_msg, 256, L"<c=#FF0000>Upload failed with HTTP status %d. Response: %S</c>",
                 status_code, response_data.substr(0, 100).c_str());
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, error_msg);
    }
}


// Export as JSON
void ObserverExportWindow::ExportToJSON(Version version)
{
    glz::generic json;
    std::string filename;
    SYSTEMTIME time;
    GetLocalTime(&time);
    std::string export_time = std::format("{:04}-{:02}-{:02}T{:02}-{:02}-{:02}", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

    switch (version) {
        case Version::V_0_1: {
            json = ToJSON_V_0_1();
            json["verson"] = "0.1";
            json["exported_at_local"] = export_time;
            filename = export_time + "_observer.json";
            json["filename"] = filename;
            break;
        }
        case Version::V_1_0: {
            json = ToJSON_V_1_0();
            json["verson"] = "1.0";
            json["exported_at_local"] = export_time;
            std::string name = glz::write_json(json["name"]).value_or(std::string{});
            // remove quotation marks (come in from json.dump())
            std::erase(name, '"');
            // replace spaces with _
            std::ranges::transform(name, name.begin(), [](const unsigned char c) {
                return static_cast<unsigned char>(c == ' ' ? '_' : c);
            });
            filename = TextUtils::SanitiseFilename(name) + ".json";
            json["filename"] = filename;
            break;
        }
        default: {
            break;
        }
    }

    Resources::EnsureFolderExists(Resources::GetPath(L"observer"));
    auto file_location = Resources::GetPath(L"observer\\" + TextUtils::StringToWString(filename));
    if (exists(file_location)) {
        std::filesystem::remove(file_location);
    }

    std::ofstream out(file_location);
    out << glz::write_json(json).value_or(std::string{});
    out.close();
    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    const std::wstring message = file_location.wstring();

    size_t max_len = _countof(file_location_wc) - 1;

    for (wchar_t i : message) {
        // Break on the end of the message
        if (!i) {
            break;
        }
        // Double escape backsashes
        if (i == '\\') {
            file_location_wc[msg_len++] = i;
        }
        if (msg_len >= max_len) {
            break;
        }
        file_location_wc[msg_len++] = i;
    }
    file_location_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(chat_message, _countof(chat_message), L"Match exported to <a=1>\x200C%s</a>", file_location_wc);
    WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}


// Draw the window
void ObserverExportWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    ImGui::Text("Match Information");
    ImGui::Spacing();
    
    // Match Type dropdown
    const char* match_types[] = { "AT A", "AT B", "AT C", "MAT", "Ladder", "Scrim" };
    static int current_match_type = -1;
    
    // Find current selection index based on stored match_type
    if (current_match_type == -1 && !Instance().match_type.empty()) {
        for (int i = 0; i < 6; i++) {
            if (Instance().match_type == match_types[i]) {
                current_match_type = i;
                break;
            }
        }
    }
    
    if (ImGui::BeginCombo("Match Type", current_match_type >= 0 ? match_types[current_match_type] : "Select...")) {
        for (int i = 0; i < 6; i++) {
            const bool is_selected = (current_match_type == i);
            if (ImGui::Selectable(match_types[i], is_selected)) {
                current_match_type = i;
                Instance().match_type = match_types[i];
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    // MAT Round dropdown (only shown if MAT is selected)
    if (current_match_type == 3) { // MAT is at index 3
        const char* mat_rounds[] = { "Qualification Stage", "Playoff", "Quarterfinals", "Semi-finals", "Finals" };
        static int current_mat_round = -1;
        
        // Find current selection index based on stored mat_round
        if (current_mat_round == -1 && !Instance().mat_round.empty()) {
            for (int i = 0; i < 5; i++) {
                if (Instance().mat_round == mat_rounds[i]) {
                    current_mat_round = i;
                    break;
                }
            }
        }
        
        if (ImGui::BeginCombo("MAT Round", current_mat_round >= 0 ? mat_rounds[current_mat_round] : "Select...")) {
            for (int i = 0; i < 5; i++) {
                const bool is_selected = (current_mat_round == i);
                if (ImGui::Selectable(mat_rounds[i], is_selected)) {
                    current_mat_round = i;
                    Instance().mat_round = mat_rounds[i];
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    
    // Match Date input
    char date_buf[64];
    strncpy_s(date_buf, Instance().match_date.c_str(), 63);
    if (ImGui::InputText("Match Date", date_buf, 64)) {
        Instance().match_date = date_buf;
    }
    ImGui::ShowHelp("Format: YYYY-MM-DD (e.g., 2026-02-22)");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Export Observer matches to JSON");

    if (ImGui::Button("Export to JSON (Version 0.1)")) {
        ExportToJSON(Version::V_0_1);
    }

    if (ImGui::Button("Export to JSON (Version 1.0)")) {
        ExportToJSON(Version::V_1_0);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Upload to GWRank.com");
    
    ObserverModule& observer_module = ObserverModule::Instance();
    bool can_export = observer_module.match_finished && !Instance().gwrank_api_key.empty();
    
    if (!observer_module.match_finished) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Match not finished");
    } else if (Instance().gwrank_api_key.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "API key not configured");
    }
    
    if (!can_export) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("Export to GWRank.com")) {
        ExportToGWRank();
    }
    
    if (!can_export) {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

// Load settings
void ObserverExportWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    
    LOAD_STRING(gwrank_api_key);
    LOAD_STRING(gwrank_endpoint);
    LOAD_STRING(match_type);
    LOAD_STRING(match_date);
    LOAD_STRING(mat_round);
    
    if (gwrank_endpoint.empty()) {
        gwrank_endpoint = "https://gwrank.com/api/v1/matches";
    }
    
    // Prefill match_date with current date if empty
    if (match_date.empty()) {
        SYSTEMTIME time;
        GetLocalTime(&time);
        match_date = std::format("{:04}-{:02}-{:02}", time.wYear, time.wMonth, time.wDay);
    }
}


// Save settings
void ObserverExportWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    SAVE_STRING(gwrank_api_key);
    SAVE_STRING(gwrank_endpoint);
    SAVE_STRING(match_type);
    SAVE_STRING(match_date);
    SAVE_STRING(mat_round);
}

// Draw settings
void ObserverExportWindow::DrawSettingsInternal()
{
    ImGui::TextUnformatted("GWRank.com API Integration");
    ImGui::Text("Configure API credentials for exporting matches to GWRank.com");
    ImGui::Spacing();
    
    char api_key_buf[256];
    char endpoint_buf[256];
    strncpy_s(api_key_buf, Instance().gwrank_api_key.c_str(), 255);
    strncpy_s(endpoint_buf, Instance().gwrank_endpoint.c_str(), 255);
    
    if (ImGui::InputText("API Key", api_key_buf, 256, ImGuiInputTextFlags_Password)) {
        Instance().gwrank_api_key = api_key_buf;
    }
    ImGui::ShowHelp("Enter your GWRank.com API key for authentication");
    
    if (ImGui::InputText("API Endpoint", endpoint_buf, 256)) {
        Instance().gwrank_endpoint = endpoint_buf;
    }
    ImGui::ShowHelp("URL for the GWRank.com API endpoint (default: https://gwrank.com/api/v1/matches)");
}
