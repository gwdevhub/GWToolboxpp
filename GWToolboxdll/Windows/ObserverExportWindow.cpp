#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ObserverModule.h>

#include <Windows/ObserverExportWindow.h>

#define NO_AGENT 0

void ObserverExportWindow::Initialize() {
    ToolboxWindow::Initialize();
}

// Convert to JSON (Version 0.1)
nlohmann::json ObserverExportWindow::ToJSON_V_0_1() {
    nlohmann::json json;
    ObserverModule& observer_module = ObserverModule::Instance();
    const std::vector<uint32_t>& party_ids = observer_module.GetObservablePartyIds();

    auto shared_stats_to_json = [](const ObserverModule::SharedStats& stats) -> nlohmann::json {
        nlohmann::json shared_json;
        shared_json["total_crits_received"]         = stats.total_crits_received;
        shared_json["total_crits_dealt"]            = stats.total_crits_dealt;
        shared_json["total_party_crits_received"]   = stats.total_party_crits_received;
        shared_json["total_party_crits_dealt"]      = stats.total_party_crits_dealt;
        shared_json["knocked_down_count"]           = stats.knocked_down_count;
        shared_json["interrupted_count"]            = stats.interrupted_count;
        shared_json["interrupted_skills_count"]     = stats.interrupted_skills_count;
        shared_json["cancelled_count"]              = stats.cancelled_count;
        shared_json["cancelled_skills_count"]       = stats.cancelled_skills_count;
        shared_json["knocked_down_duration"]        = stats.knocked_down_duration;
        shared_json["deaths"]                       = stats.deaths;
        shared_json["kills"]                        = stats.kills;
        shared_json["kdr_str"]                      = stats.kdr_str;
        return shared_json;
    };

    for (const uint32_t party_id : party_ids) {
        // parties
        const ObserverModule::ObservableParty* party = observer_module.GetObservablePartyById(party_id);
        if (!party) {
            json["parties"].push_back(nlohmann::json::value_t::null);
            continue;
        }
        json["parties"].push_back([&]() -> nlohmann::json {
            // parties -> party
            nlohmann::json json_party;
            json_party["party_id"] = party->party_id;
            json_party["stats"] = shared_stats_to_json(party->stats);
            for (uint32_t agent_id : party->agent_ids) {
                // parties -> party -> agents
                ObserverModule::ObservableAgent* agent = observer_module.GetObservableAgentById(agent_id);
                if (!agent) {
                    json_party["members"].push_back(nlohmann::json::value_t::null);
                    continue;
                }
                json_party["members"].push_back([&]() -> nlohmann::json {
                    // parties -> party -> agents -> agent
                    nlohmann::json json_agent;
                    json_agent["display_name"]    = agent->DisplayName();
                    json_agent["raw_name"]        = agent->RawName();
                    json_agent["debug_name"]      = agent->DebugName();
                    json_agent["sanitized_name"]  = agent->SanitizedName();
                    json_agent["party_id"]        = agent->party_id;
                    json_agent["party_index"]     = agent->party_index;
                    json_agent["primary"]         = agent->primary;
                    json_agent["secondary"]       = agent->secondary;
                    json_agent["profession"]      = agent->profession;
                    json_agent["stats"] = shared_stats_to_json(agent->stats);
                    for (auto skill_id : agent->stats.skill_ids_used) {
                        // parties -> party -> agents -> agent -> skills
                        ObserverModule::ObservableSkill* skill = ObserverModule::Instance().GetObservableSkillById(skill_id);
                        if (!skill) {
                            json["skills"].push_back(nlohmann::json::value_t::null);
                            continue;
                        }
                        json["skills"].push_back([&]() -> nlohmann::json {
                            // parties -> party -> agents -> agent -> skills -> skill
                            nlohmann::json json_skill;
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
nlohmann::json ObserverExportWindow::ToJSON_V_1_0() {
    nlohmann::json json;
    ObserverModule& om = ObserverModule::Instance();

    // name is built by iterating over the parties
    bool name_prepend_vs = false;
    std::string name = "";

    json["match_finished"]              = om.match_finished;
    json["winning_party_id"]            = om.winning_party_id;
    json["match_duration_ms_total"]     = om.match_duration_ms_total.count();
    json["match_duration_ms"]           = om.match_duration_ms.count();
    json["match_duration_secs"]         = om.match_duration_secs.count();
    json["match_duration_mins"]         = om.match_duration_mins.count();

    ObserverModule::ObservableMap* map = om.GetMap();
    json["map"] = nlohmann::json::value_t::null;
    if (map) {
        json["map"]                     = {};
        json["map"]["name"]             = map->Name();
        json["map"]["description"]      = map->Description();
        json["map"]["is_pvp"]           = map->GetIsPvP();
        json["map"]["is_guild_hall"]    = map->GetIsGuildHall();
        json["map"]["campaign"]         = map->campaign;
        json["map"]["continent"]        = map->continent;
        json["map"]["region"]           = map->region;
        json["map"]["type"]             = map->type;
        json["map"]["flags"]            = map->flags;
        json["map"]["name_id"]          = map->name_id;
        json["map"]["description_id"]   = map->description_id;
    }


    auto action_to_json = [](const ObserverModule::ObservedAction& action) -> nlohmann::json {
        nlohmann::json action_json;
        action_json["started"]      = action.started;
        action_json["stopped"]      = action.stopped;
        action_json["interrupted"]  = action.interrupted;
        action_json["finished"]     = action.finished;
        action_json["integrity"]    = action.integrity;
        return action_json;
    };

    auto shared_stats_to_json = [&action_to_json](const ObserverModule::SharedStats& stats) -> nlohmann::json {
        nlohmann::json stats_json;
        stats_json["total_crits_received"]          = stats.total_crits_received;
        stats_json["total_crits_dealt"]             = stats.total_crits_dealt;
        stats_json["total_party_crits_received"]    = stats.total_party_crits_received;
        stats_json["total_party_crits_dealt"]       = stats.total_party_crits_dealt;
        stats_json["knocked_down_count"]            = stats.knocked_down_count;
        stats_json["interrupted_count"]             = stats.interrupted_count;
        stats_json["interrupted_skills_count"]      = stats.interrupted_skills_count;
        stats_json["cancelled_count"]               = stats.cancelled_count;
        stats_json["cancelled_skills_count"]        = stats.cancelled_skills_count;
        stats_json["knocked_down_duration"]         = stats.knocked_down_duration;
        stats_json["deaths"]                        = stats.deaths;
        stats_json["kills"]                         = stats.kills;
        stats_json["kdr_str"]                       = stats.kdr_str;
        stats_json["total_attacks_dealt"]                       = action_to_json(stats.total_attacks_dealt);
        stats_json["total_attacks_received"]                    = action_to_json(stats.total_attacks_received);
        stats_json["total_attacks_dealt_to_other_parties"]      = action_to_json(stats.total_attacks_dealt_to_other_parties);
        stats_json["total_attacks_received_from_other_parties"] = action_to_json(stats.total_attacks_received_from_other_parties);
        stats_json["total_skills_used"]                         = action_to_json(stats.total_skills_used);
        stats_json["total_skills_received"]                     = action_to_json(stats.total_skills_received);
        stats_json["total_skills_used_on_own_party"]            = action_to_json(stats.total_skills_used_on_own_party);
        stats_json["total_skills_used_on_other_parties"]        = action_to_json(stats.total_skills_used_on_other_parties);
        stats_json["total_skills_received_from_own_party"]      = action_to_json(stats.total_skills_received_from_own_party);
        stats_json["total_skills_received_from_other_parties"]  = action_to_json(stats.total_skills_received_from_other_parties);
        stats_json["total_skills_used_on_own_team"]             = action_to_json(stats.total_skills_used_on_own_team);
        stats_json["total_skills_used_on_other_teams"]          = action_to_json(stats.total_skills_used_on_other_teams);
        stats_json["total_skills_received_from_own_team"]       = action_to_json(stats.total_skills_received_from_own_team);
        stats_json["total_skills_received_from_other_teams"]    = action_to_json(stats.total_skills_received_from_other_teams);
        return stats_json;
    };

    const std::vector<uint32_t>& guild_ids = om.GetObservableGuildIds();
    const std::vector<uint32_t>& agent_ids = om.GetObservableAgentIds();
    const std::vector<uint32_t>& party_ids = om.GetObservablePartyIds();
    const std::vector<GW::Constants::SkillID>& skill_ids = om.GetObservableSkillIds();

    // guilds
    json["guilds"]["ids"] = guild_ids;
    json["guilds"]["by_id"] = {};
    for (uint32_t guild_id : guild_ids) {
        std::string guild_id_s = std::to_string(guild_id);
        ObserverModule::ObservableGuild* guild = om.GetObservableGuildById(guild_id);
        if (!guild) {
            json["guilds"]["by_id"][guild_id_s] = nlohmann::json::value_t::null;
            continue;
        }
        json["guilds"]["by_id"][guild_id_s]["guild_id"]           = guild->guild_id;
        json["guilds"]["by_id"][guild_id_s]["key"]                = guild->key.k;
        json["guilds"]["by_id"][guild_id_s]["name"]               = guild->name;
        json["guilds"]["by_id"][guild_id_s]["tag"]                = guild->tag;
        json["guilds"]["by_id"][guild_id_s]["wrapped_tag"]        = guild->wrapped_tag;
        json["guilds"]["by_id"][guild_id_s]["rank"]               = guild->rank;
        json["guilds"]["by_id"][guild_id_s]["rating"]             = guild->rating;
        json["guilds"]["by_id"][guild_id_s]["faction"]            = guild->faction;
        json["guilds"]["by_id"][guild_id_s]["faction_point"]      = guild->faction_point;
        json["guilds"]["by_id"][guild_id_s]["qualifier_point"]    = guild->qualifier_point;
        json["guilds"]["by_id"][guild_id_s]["cape_trim"]          = guild->cape_trim;
    }

    // skills
    json["skills"]["ids"] = skill_ids;
    json["skills"]["by_id"] = {};
    for (auto skill_id : skill_ids) {
        std::string skill_id_s = std::to_string((uint32_t)skill_id);
        ObserverModule::ObservableSkill* skill = om.GetObservableSkillById(skill_id);
        if (!skill) {
            json["skills"]["by_id"][skill_id_s] = nlohmann::json::value_t::null;
            continue;
        }
        json["skills"]["by_id"][skill_id_s]["skill_id"]         = skill->gw_skill.skill_id;
        json["skills"]["by_id"][skill_id_s]["name"]             = skill->Name();
        json["skills"]["by_id"][skill_id_s]["stats"]["total_usages"]                = action_to_json(skill->stats.total_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_self_usages"]           = action_to_json(skill->stats.total_self_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_other_usages"]          = action_to_json(skill->stats.total_other_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_own_party_usages"]      = action_to_json(skill->stats.total_own_party_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_other_party_usages"]    = action_to_json(skill->stats.total_other_party_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_own_team_usages"]       = action_to_json(skill->stats.total_own_team_usages);
        json["skills"]["by_id"][skill_id_s]["stats"]["total_other_team_usages"]     = action_to_json(skill->stats.total_other_team_usages);
        json["skills"]["by_id"][skill_id_s]["campaign"]         = skill->gw_skill.campaign;
        json["skills"]["by_id"][skill_id_s]["type"]             = skill->gw_skill.type;
        json["skills"]["by_id"][skill_id_s]["sepcial"]          = skill->gw_skill.special;
        json["skills"]["by_id"][skill_id_s]["activation"]       = skill->gw_skill.activation;
        json["skills"]["by_id"][skill_id_s]["combo_req"]        = skill->gw_skill.combo_req;
        json["skills"]["by_id"][skill_id_s]["effect1"]          = skill->gw_skill.effect1;
        json["skills"]["by_id"][skill_id_s]["condition"]        = skill->gw_skill.condition;
        json["skills"]["by_id"][skill_id_s]["effect2"]          = skill->gw_skill.effect2;
        json["skills"]["by_id"][skill_id_s]["weapon_req"]       = skill->gw_skill.weapon_req;
        json["skills"]["by_id"][skill_id_s]["profession"]       = skill->gw_skill.profession;
        json["skills"]["by_id"][skill_id_s]["attribute"]        = skill->gw_skill.attribute;
        json["skills"]["by_id"][skill_id_s]["skill_id_pvp"]     = skill->gw_skill.skill_id_pvp;
        json["skills"]["by_id"][skill_id_s]["combo"]            = skill->gw_skill.combo;
        json["skills"]["by_id"][skill_id_s]["target"]           = skill->gw_skill.target;
        json["skills"]["by_id"][skill_id_s]["skill_equip_type"] = skill->gw_skill.skill_equip_type;
        json["skills"]["by_id"][skill_id_s]["energy_cost"]      = skill->gw_skill.energy_cost;
        json["skills"]["by_id"][skill_id_s]["health_cost"]      = skill->gw_skill.health_cost;
        json["skills"]["by_id"][skill_id_s]["adrenaline"]       = skill->gw_skill.adrenaline;
        json["skills"]["by_id"][skill_id_s]["activation"]       = skill->gw_skill.activation;
        json["skills"]["by_id"][skill_id_s]["aftercast"]        = skill->gw_skill.aftercast;
        json["skills"]["by_id"][skill_id_s]["duration0"]        = skill->gw_skill.duration0;
        json["skills"]["by_id"][skill_id_s]["duration15"]       = skill->gw_skill.duration15;
        json["skills"]["by_id"][skill_id_s]["recharge"]         = skill->gw_skill.recharge;
        json["skills"]["by_id"][skill_id_s]["scale0"]           = skill->gw_skill.scale0;
        json["skills"]["by_id"][skill_id_s]["scale15"]          = skill->gw_skill.scale15;
        json["skills"]["by_id"][skill_id_s]["bonusScale0"]      = skill->gw_skill.bonusScale0;
        json["skills"]["by_id"][skill_id_s]["bonusScale15"]     = skill->gw_skill.bonusScale15;
        json["skills"]["by_id"][skill_id_s]["aoe_range"]        = skill->gw_skill.aoe_range;
        json["skills"]["by_id"][skill_id_s]["const_effect"]     = skill->gw_skill.const_effect;
        json["skills"]["by_id"][skill_id_s]["icon_file_id"]     = skill->gw_skill.icon_file_id;
    }

    // parties
    json["parties"]["ids"] = party_ids;
    json["parties"]["by_id"] = {};
    for (uint32_t party_id : party_ids) {
        std::string party_id_s = std::to_string(party_id);
        ObserverModule::ObservableParty* party = om.GetObservablePartyById(party_id);
        if (!party) {
            json["parties"]["by_id"][party_id_s] = nlohmann::json::value_t::null;
            continue;
        }
        if (name_prepend_vs) name.append(" vs ");
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
    }

    // agents
    json["agents"]["ids"] = agent_ids;
    json["agents"]["by_id"] = {};
    for (uint32_t agent_id : agent_ids) {
        std::string agent_id_s = std::to_string(agent_id);
        ObserverModule::ObservableAgent* agent = om.GetObservableAgentById(agent_id);
        if (!agent) {
            json["agents"]["by_id"][agent_id_s] = nlohmann::json::value_t::null;
            continue;
        }
        json["agents"]["by_id"][agent_id_s]["agent_id"]       = agent->agent_id;
        json["agents"]["by_id"][agent_id_s]["display_name"]   = agent->DisplayName();
        json["agents"]["by_id"][agent_id_s]["raw_name"]       = agent->RawName();
        json["agents"]["by_id"][agent_id_s]["debug_name"]     = agent->DebugName();
        json["agents"]["by_id"][agent_id_s]["sanitized_name"] = agent->SanitizedName();
        json["agents"]["by_id"][agent_id_s]["party_id"]       = agent->party_id;
        json["agents"]["by_id"][agent_id_s]["party_index"]    = agent->party_index;
        json["agents"]["by_id"][agent_id_s]["primary"]        = agent->primary;
        json["agents"]["by_id"][agent_id_s]["secondary"]      = agent->secondary;
        json["agents"]["by_id"][agent_id_s]["profession"]     = agent->profession;
        json["agents"]["by_id"][agent_id_s]["guild_id"]       = agent->guild_id;
        json["agents"]["by_id"][agent_id_s]["stats"]          = shared_stats_to_json(agent->stats);

        // attacks

        // attacks dealt (by agent)
        for (auto& [target_id, action] : agent->stats.attacks_dealt_to_agents) {
            std::string target_id_s = std::to_string(target_id);
            if (!action) {
                json["agents"]["by_id"][agent_id_s]["stats"]["attacks_dealt_to_agents"][target_id_s] = nlohmann::json::value_t::null;
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["attacks_dealt_to_agents"][target_id_s] = action_to_json(*action);
        }

        // attacks received (by agent)
        for (auto& [caster_id, action] : agent->stats.attacks_received_from_agents) {
            std::string caster_id_s = std::to_string(caster_id);
            if (!action) {
                json["agents"]["by_id"][agent_id_s]["stats"]["attacks_received_from_agents"][caster_id_s] = nlohmann::json::value_t::null;
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["attacks_received_from_agents"][caster_id_s] = action_to_json(*action);
        }

        // skills

        // skills used
        json["agents"]["by_id"][agent_id_s]["stats"]["skill_ids_used"] = agent->stats.skill_ids_used;
        for (auto skill_id : agent->stats.skill_ids_used) {
            std::string skill_id_s = std::to_string((uint32_t)skill_id);
            const auto it_skill = agent->stats.skills_used.find(skill_id);
            if (it_skill == agent->stats.skills_used.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_used"][skill_id_s] = nlohmann::json::value_t::null;
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_used"][skill_id_s] = action_to_json(*it_skill->second);
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_used"][skill_id_s]["skill_id"] = it_skill->second->skill_id;
        }

        // skills received
        json["agents"]["by_id"][agent_id_s]["stats"]["skill_ids_received"] = agent->stats.skill_ids_received;
        for (auto skill_id : agent->stats.skill_ids_received) {
            std::string skill_id_s = std::to_string((uint32_t)skill_id);
            const auto it_skill = agent->stats.skills_received.find(skill_id);
            if (it_skill == agent->stats.skills_received.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_received"][skill_id_s] = nlohmann::json::value_t::null;
                continue;
            }
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_received"][skill_id_s]             = action_to_json(*it_skill->second);
            json["agents"]["by_id"][agent_id_s]["stats"]["skills_received"][skill_id_s]["skill_id"] = it_skill->second->skill_id;
        }

        // skills used (by agent)
        for (auto& [target_id, agent_skill_ids] : agent->stats.skill_ids_used_on_agents) {
            std::string target_id_s = std::to_string(target_id);
            auto it_target = agent->stats.skills_used_on_agents.find(target_id);
            if (it_target == agent->stats.skills_used_on_agents.end()) {
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_used_on_agents"][target_id_s] = nlohmann::json::value_t::null;
                continue;
            }
            for (auto skill_id : agent_skill_ids) {
                std::string skill_id_s = std::to_string((uint32_t)skill_id);
                auto it_skill = it_target->second.find(skill_id);
                if (it_skill == it_target->second.end()) {
                    json["agents"]["by_id"][agent_id_s]["stats"]["skills_used_on_agents"][target_id_s][skill_id_s] = nlohmann::json::value_t::null;
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
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_received_from_agents"][caster_id_s] = nlohmann::json::value_t::null;
                continue;
            }
            for (auto skill_id : agent_skill_ids) {
                std::string skill_id_s = std::to_string((uint32_t)skill_id);
                auto it_skill = it_target->second.find(skill_id);
                if (it_skill == it_target->second.end()) {
                    json["agents"]["by_id"][agent_id_s]["stats"]["skills_received_from_agents"][caster_id_s][skill_id_s] = nlohmann::json::value_t::null;
                    continue;
                }
                json["agents"]["by_id"][agent_id_s]["stats"]["skills_received_from_agents"][caster_id_s][skill_id_s] = action_to_json(*it_skill->second);
            }
        }
    }

    json["name"] = name;

    return json;
}

std::string ObserverExportWindow::PadLeft(std::string input, uint8_t count, char c) {
    input.insert(input.begin(), count - input.size(), c);
    return input;
}


// Export as JSON
void ObserverExportWindow::ExportToJSON(Version version) {
    nlohmann::json json;
    std::string filename;
    SYSTEMTIME time;
    GetLocalTime(&time);
    std::string year = std::to_string(time.wYear);
    std::string month = std::to_string(time.wMonth);
    std::string day = std::to_string(time.wDay);
    std::string hour = std::to_string(time.wHour);
    std::string minute = std::to_string(time.wMinute);
    std::string second = std::to_string(time.wSecond);
    std::string export_time = PadLeft(year, 4, '0')
        + "-"
        + PadLeft(month, 2, '0')
        + "-"
        + PadLeft(day, 2, '0')
        + "T"
        + PadLeft(hour, 2, '0')
        + "-"
        + PadLeft(minute, 2, '0')
        + "-"
        + PadLeft(second, 2, '0');

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
            std::string name = json["name"].dump();
            // remove quotation marks (come in from json.dump())
            std::erase(name, '"');
            // replace spaces with _
            std::ranges::transform(name, name.begin(), [](unsigned char c){
                return static_cast<unsigned char>(c == ' ' ? '_' : c);
            });
            // replace non-alphanumeric with "x" to make simply FS safe, but also show something is missing
            name = std::regex_replace(name, std::regex("[^A-Za-z0-9.-_]/g"), "x");
            filename = export_time + "_" + name + ".json";
            json["filename"] = filename;
            break;
        }
        default: {
            break;
        }
    }

    Resources::EnsureFolderExists(Resources::GetPath(L"observer"));
    auto file_location = Resources::GetPath(L"observer\\" + GuiUtils::StringToWString(filename));
    if (std::filesystem::exists(file_location)) {
        std::filesystem::remove(file_location);
    }

    std::ofstream out(file_location);
    out << json.dump();
    out.close();
    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    const std::wstring message = file_location.wstring();

    size_t max_len = _countof(file_location_wc) - 1;

    for (size_t i = 0; i < message.length(); i++) {
        // Break on the end of the message
        if (!message[i])
            break;
        // Double escape backsashes
        if (message[i] == '\\')
            file_location_wc[msg_len++] = message[i];
        if (msg_len >= max_len)
            break;
        file_location_wc[msg_len++] = message[i];
    }
    file_location_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(chat_message, _countof(chat_message), L"Match exported to <a=1>\x200C%s</a>", file_location_wc);
    GW::Chat::WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}


// Draw the window
void ObserverExportWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    ImGui::Text("Export Observer matches to JSON");

    if (ImGui::Button("Export to JSON (Version 0.1)"))
        ExportToJSON(Version::V_0_1);

    if (ImGui::Button("Export to JSON (Version 1.0)"))
        ExportToJSON(Version::V_1_0);

    ImGui::End();
}

// Load settings
void ObserverExportWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
}


// Save settings
void ObserverExportWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
}

// Draw settings
void ObserverExportWindow::DrawSettingInternal() {
    // No internal settings yet
}

