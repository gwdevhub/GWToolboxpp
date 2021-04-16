#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxWidget.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Party.h>

#include <Timer.h>

#define NO_AGENT 0
#define NO_PARTY 0

namespace ObserverLabel {
    extern const char* Name;
    extern const char* Kills;
    extern const char* Deaths;
    extern const char* KDR;
    extern const char* Cancels;
    extern const char* Interrupts;
    extern const char* Knockdowns;
    extern const char* Finishes;
    extern const char* AttacksReceivedFromOtherParties;
    extern const char* AttacksDealtToOtherParties;
    extern const char* CritsReceivedFromOtherParties;
    extern const char* CritsDealToOtherParties;
    extern const char* SkillsReceivedFromOtherParties;
    extern const char* SkillsUsedOnOtherParties;
}; // namespace ObserverLabels


class ObserverModule : public ToolboxModule {
public:
    // An action between a caster and target
    // Where an action can be a skill and/or attack
    struct TargetAction {
        TargetAction(const uint32_t caster_id,
            const uint32_t target_id,
            const bool is_attack,
            const bool is_skill,
            const uint32_t skill_id
        )
            : caster_id(caster_id)
            , target_id(target_id)
            , is_attack(is_attack)
            , is_skill(is_skill)
            , skill_id(skill_id)
        {}
        const uint32_t caster_id;
        const uint32_t target_id;
        const bool is_attack;
        const bool is_skill;
        const uint32_t skill_id;
    };

    enum class ActionStage {
        // start
        Started,

        // both
        Instant,

        // finish
        Stopped,
        Finished,
        Interrupted // "Interrupted" is received after "Stopped"
    };

    // an agents statistics for an action (attack)
    class ObservedAction {
    public:
        size_t started = 0;
        size_t stopped = 0;
        size_t finished = 0;
        size_t interrupted = 0;

        void Reduce(const ActionStage stage) {
            switch (stage) {
                case ActionStage::Instant:
                    started += 1;
                    finished += 1;
                    break;
                case ActionStage::Started:
                    started += 1;
                    break;
                case ActionStage::Stopped:
                    stopped += 1;
                    break;
                case ActionStage::Interrupted:
                    stopped -= 1;
                    interrupted += 1;
                    break;
                case ActionStage::Finished:
                    finished += 1;
                    break;
            }
        }
    };

    // an agents statistics for an action (skill)
    struct ObservedSkill : ObservedAction {
    public:
        ObservedSkill(const uint32_t skill_id) : skill_id(skill_id) {}
        const uint32_t skill_id;
    };


    // Shared stats for an Agent or Team
    class SharedStats {
    public:
        // ****
        // misc
        // ****

        size_t total_crits_received = 0;
        size_t total_crits_dealt = 0;

        size_t total_party_crits_received = 0;
        size_t total_party_crits_dealt = 0;

        size_t knocked_down_count = 0;
        size_t interrupted_count = 0;
        size_t interrupted_skills_count = 0;
        size_t cancelled_count = 0;
        size_t cancelled_skills_count = 0;
        float knocked_down_duration = 0;

        // deaths: from AgentState packets
        size_t deaths = 0;
        // kills: player kills only, not npc kills
        //        guessed from damage packets
        size_t kills = 0;
        float kdr_pc = 0;
        std::string kdr_str = "0.00";

        // *******
        // attacks
        // *******

        // attacks:total
        // -------------
        ObservedAction total_attacks_done = ObservedAction();
        ObservedAction total_attacks_received = ObservedAction();

        // attacks:party
        // -------------
        // attacks done on other parties (not inc. npcs)
        ObservedAction total_attacks_done_on_other_party = ObservedAction();
        // attacks received from other parties (not inc. npcs)
        ObservedAction total_attacks_received_by_other_party = ObservedAction();

        // ******
        // skills
        // ******

        // ------------
        // skills:total
        // ------------

        // skills used on anyone
        ObservedAction total_skills_used = ObservedAction();
        // skills received from anyone
        ObservedAction total_skills_received = ObservedAction();

        // ------------
        // skills:party
        // ------------

        // skills used on your own party (not inc. npcs)
        ObservedAction total_skills_used_on_own_party = ObservedAction();
        // skills used on other parties (not inc. npcs)
        ObservedAction total_skills_used_on_other_party = ObservedAction();

        // skills received from your own party (not inc. npcs)
        ObservedAction total_skills_received_by_own_party = ObservedAction();
        // skills received from other parties (not inc. npcs)
        ObservedAction total_skills_received_by_other_party = ObservedAction();

        // -----------
        // skills:team
        // -----------

        // skills used on your own team (inc. npcs)
        ObservedAction total_skills_used_on_own_team = ObservedAction();
        // skills used on other team (inc. npcs)
        ObservedAction total_skills_used_on_other_team = ObservedAction();

        // skills received from your own team (inc. npcs)
        ObservedAction total_skills_received_by_own_team = ObservedAction();
        // skills received from other team (inc. npcs)
        ObservedAction total_skills_received_by_other_team = ObservedAction();


        // fired when the agent dies
        void HandleDeath() {
            deaths += 1;
            // recalculate kdr
            kdr_pc = (float) kills / deaths;
            // get kdr string
            std::stringstream str;
            str << std::fixed << std::setprecision(2) << kdr_pc;
            kdr_str = str.str();
        }

        // fired when the agent scores a kill
        void HandleKill() {
            kills += 1;
            // recalculate kdr
            if (deaths < 1)
                kdr_pc = static_cast<float>(kills);
            else
                kdr_pc = static_cast<float>(kills) / deaths;
            // get kdr string
            std::stringstream str;
            str << std::fixed << std::setprecision(2) << kdr_pc;
            kdr_str = str.str();
        }
    };

    // Stats for Agents
    class ObservableAgentStats : public SharedStats {
    public:
        ~ObservableAgentStats()
        {
            // attacks received (by agent)
            for (const auto& [_, o_atk] : attacks_received_by_agent) if (o_atk) delete o_atk;
            attacks_received_by_agent.clear();

            // attacks dealed (by agent)
            for (const auto& [_, o_atk] : attacks_done_by_agent) if (o_atk) delete o_atk;
            attacks_done_by_agent.clear();

            // skills used
            skill_ids_used.clear();
            for (const auto& [_, o_skill] : skills_used) if (o_skill) delete o_skill;
            skills_used.clear();

            // skills received
            skill_ids_received.clear();
            for (const auto& [_, o_skill] : skills_received) if (o_skill) delete o_skill;
            skills_received.clear();

            // skill received (by agent)
            for (auto& [_, skill_ids] : skills_ids_received_by_agent) skill_ids.clear();
            skills_ids_received_by_agent.clear();
            for (auto& [_, agent_skills] : skills_received_by_agent) {
                for (const auto [__, o_skill] : agent_skills) if (o_skill) delete o_skill;
                agent_skills.clear();
            }
            skills_received_by_agent.clear();

            // skill used (by agent)
            for (auto& [_, skill_ids] : skills_ids_used_on_agent) skill_ids.clear();
            for (auto& [_, agent_skills] : skills_used_on_agent) {
                for (const auto [__, o_skill] : agent_skills) if (o_skill) delete o_skill;
                agent_skills.clear();
            }
            skills_ids_used_on_agent.clear();
        }

        // map of agent_id -> ObservedAction
        std::unordered_map<uint32_t, ObservedAction*> attacks_received_by_agent = {};
        // getter helper
        ObservedAction& LazyGetAttacksReceivedFrom(const uint32_t attacker_agent_id) {
            auto it = attacks_received_by_agent.find(attacker_agent_id);
            if (it == attacks_received_by_agent.end()) {
                // attacker not registered
                ObservedAction* observed_action = new ObservedAction();
                attacks_received_by_agent.insert({attacker_agent_id, observed_action});
                return *observed_action;
            } else {
                // attacker is already reigstered
                return *it->second;
            }
        }

        // map of agent_id -> ObservedAction
        std::unordered_map<uint32_t, ObservedAction*> attacks_done_by_agent = {};
        // getter helper
        ObservedAction& LazyGetAttacksDealedAgainst(const uint32_t receiver_agent_id) {
            auto it = attacks_done_by_agent.find(receiver_agent_id);
            if (it == attacks_done_by_agent.end()) {
                // receiver not registered
                ObservedAction* observed_action = new ObservedAction();
                attacks_done_by_agent.insert({receiver_agent_id, observed_action});
                return *observed_action;
            } else {
                // receiver is already reigstered
                return *it->second;
            }
        }

        // ******
        // skills
        // ******

        // map of skill_id -> count of times received
        std::unordered_map<uint32_t, ObservedSkill*> skills_used = {};
        // skill ids used by agent (ordered by usage)
        std::vector<uint32_t> skill_ids_used = {};
        // getter helper
        ObservedAction& LazyGetSkillUsed(const uint32_t skill_id) {
            auto it_skill = skills_used.find(skill_id);
            if (it_skill == skills_used.end()) {
                // skill not registered
                skill_ids_used.push_back(skill_id);
                // re-sort skills
                std::sort(skill_ids_used.begin(), skill_ids_used.end());
                ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                skills_used.insert({skill_id, observed_skill});
                return *observed_skill;
            } else {
                // skill already registered
                return *it_skill->second;
            }
        }

        // map of skill_id -> count of times used
        std::unordered_map<uint32_t, ObservedSkill*> skills_received = {};
        // skill ids used by agent (ordered by receipt)
        std::vector<uint32_t> skill_ids_received = {};
        // getter helper
        ObservedAction& LazyGetSkillReceived(const uint32_t skill_id) {
            auto it_skill = skills_received.find(skill_id);
            if (it_skill == skills_received.end()) {
                // skill not registered
                skill_ids_received.push_back(skill_id);
                // re-sort skills
                std::sort(skill_ids_received.begin(), skill_ids_received.end());
                ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                skills_received.insert({skill_id, observed_skill});
                return *observed_skill;
            } else {
                // skill already registered
                return *it_skill->second;
            }
        }

        // ***************
        // skills by agent
        // ***************

        // map of agent_id -> skill_id -> count of times received
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, ObservedSkill*>> skills_received_by_agent = {};
        // map of agent_id -> vector<skill_ids>
        std::unordered_map<uint32_t, std::vector<uint32_t>> skills_ids_received_by_agent = {};
        // getter helper
        // note: registers the skill_id on access
        ObservedSkill& LazyGetSkillReceievedFrom(const uint32_t caster_agent_id, const uint32_t skill_id) {
            auto it_caster = skills_received_by_agent.find(caster_agent_id);
            if (it_caster == skills_received_by_agent.end()) {
                // receiver and his skills are not registered with this agent
                std::vector<uint32_t> received_skill_ids = {skill_id};
                skills_ids_received_by_agent.insert({ caster_agent_id, received_skill_ids });
                ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                std::unordered_map<uint32_t, ObservedSkill*> received_skills = {{skill_id, observed_skill}};
                skills_received_by_agent.insert({caster_agent_id, received_skills});
                return *observed_skill;
            } else {
                // receiver is registered with this agent
                std::unordered_map<uint32_t, ObservedSkill*>& used_by_caster = it_caster->second;
                auto it_observed_skill = used_by_caster.find(skill_id);
                // does receiver have the skill registered from/against us?
                if (it_observed_skill == used_by_caster.end()) {
                    // caster hasn't registered this skill with this agent
                    // add & re-sort skill_ids by the caster
                    std::vector<uint32_t>& skills_ids_received_by_agent_vec = skills_ids_received_by_agent.find(caster_agent_id)->second;
                    skills_ids_received_by_agent_vec.push_back(skill_id);
                    // re-sort
                    std::sort(skills_ids_received_by_agent_vec.begin(), skills_ids_received_by_agent_vec.end());
                    // add the observed skill for the caster
                    ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                    used_by_caster.insert({skill_id, observed_skill});
                    return *observed_skill;
                } else {
                    // receivers already registered this skill
                    return *(it_observed_skill->second);
                }
            }
        }


        // map of agent_id -> skill_id -> count of times used
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, ObservedSkill*>> skills_used_on_agent = {};
        // map of agent_id -> vector<skill_ids>
        std::unordered_map<uint32_t, std::vector<uint32_t>> skills_ids_used_on_agent = {};
        // getter helper
        // note: registers the skill_id on access
        ObservedSkill& LazyGetSkillUsedAgainst(const uint32_t target_agent_id, const uint32_t skill_id) {
            auto it_target = skills_used_on_agent.find(target_agent_id);
            if (it_target == skills_used_on_agent.end()) {
                // receiver and his skills are not registered with this agent
                std::vector<uint32_t> used_skill_ids = {skill_id};
                skills_ids_used_on_agent.insert({ target_agent_id, used_skill_ids });
                ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                std::unordered_map<uint32_t, ObservedSkill*> used_skills = {{skill_id, observed_skill}};
                skills_used_on_agent.insert({target_agent_id, used_skills});
                return *observed_skill;
            } else {
                std::unordered_map<uint32_t, ObservedSkill*>& used_on_target = it_target->second;
                // receiver is registered with this agent
                auto it_observed_skill = used_on_target.find(skill_id);
                // does receiver have the skill registered from/against us?
                if (it_observed_skill == used_on_target.end()) {
                    // target hasn't registered this skill with this agent
                    // add & re-sort skill_ids by the caster
                    std::vector<uint32_t>& skills_ids_used_on_agent_vec = skills_ids_used_on_agent.find(target_agent_id)->second;
                    skills_ids_used_on_agent_vec.push_back(skill_id);
                    // re-sort
                    std::sort(skills_ids_used_on_agent_vec.begin(), skills_ids_used_on_agent_vec.end());
                    // add the observed skill for the caster
                    ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                    used_on_target.insert({skill_id, observed_skill});
                    return *observed_skill;
                } else {
                    // receivers already registered this skill
                    return *(it_observed_skill->second);
                }
            }
        }
    };

    // Stats for Parties
    class ObservablePartyStats : public SharedStats {
    public:
        //
    };

    // Represents an agent whose stats are tracked
    // Includes players AND npc's
    class ObservableAgent {
    public:
        ObservableAgent(ObserverModule& parent, const GW::AgentLiving& agent_living)
            : parent(parent)
            , state(agent_living.model_state)
            , agent_id(agent_living.agent_id)
            , team_id(agent_living.team_id)
            , primary((GW::Constants::Profession) agent_living.primary)
            , secondary((GW::Constants::Profession) agent_living.secondary)
            , is_npc(agent_living.IsNPC())
            , is_player(agent_living.IsPlayer())
            , login_number(agent_living.login_number)
        {
            // async initialise the agents name now because we probably want it later
            GW::Agents::AsyncGetAgentName(&agent_living, raw_name);

			if (primary != GW::Constants::Profession::None) {
                std::string prof = GW::Constants::GetProfessionAcronym(primary);
                if (secondary != GW::Constants::Profession::None) {
                    std::string s_prof = GW::Constants::GetProfessionAcronym(secondary);
					prof = prof + "/" + s_prof;
                }
                profession = prof;
            }
        };

        ~ObservableAgent() {
            delete current_target_action;
        }

        std::string profession = "";

        ObserverModule& parent;
        uint32_t agent_id;
        uint32_t login_number;
        uint32_t state = state;

        uint32_t team_id;
        // initialise to no party id
        // let the party set the party_id
        uint32_t party_id = NO_PARTY;
        uint32_t party_index = 0;

        GW::Constants::Profession primary;
        GW::Constants::Profession secondary;

        // latest action (attack/skill) the agent was undertaking
        const TargetAction* current_target_action = nullptr;

        // last_hit_by tells us who killed the player if they die
        // MUST be a party_member (e.g. not a footman)
        // Not literally who is responsible for the kill, but may help
        // account for degen / npc steals / such.
        // It's a for fun stat so don't take it too serious
        uint32_t last_hit_by = NO_AGENT;

        // TODO: last_hit_at to limit the kill window

        bool is_player;
        bool is_npc;

        // stats:
        ObservableAgentStats stats = ObservableAgentStats();
    public:
        // Name of the Agent
        std::string Name();

        // Name of the Agent
        std::string RawName() {
			// has been cached
			if (cached_raw_name.length() > 0) return cached_raw_name;
			// hasn't been cached yet
            if (raw_name == L"") return "";
            // can now be cached
			cached_raw_name = GuiUtils::WStringToString(raw_name);
            return cached_raw_name;
        }

        // Name + agent_id of the Agent
        std::string DebugName() {
            std::string display_name = "(" + std::to_string(agent_id) + ") " + "\"" + Name() + "\"";
            return display_name;
        }

    private:
        // name: Inially unknown for NPC's.
        //       Asynchronously initialized in constructor.
        std::wstring raw_name = L"";
        std::string cached_raw_name = "";
        std::string trimmed_name = "";
    };


    // Represents a skill in an ObserverMode
    // Usage is tracked
    class ObservableSkill {
    public:
        ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill);

        ~ObservableSkill() {
            //
        }

        uint32_t skill_id;
        ObserverModule& parent;
        const GW::Skill& gw_skill;

        const wchar_t* DecName() { return name_dec; }
        const bool HasExhaustion() { return gw_skill.special & 0x1; }
        const bool IsMaintained() { return gw_skill.duration0 == 0x20000;  }
        const bool IsPvE() { return (gw_skill.special & 0x80000) != 0;  }
        const bool IsElite() { return (gw_skill.special & 0x4) != 0; }

        ObservedAction total_usages = ObservedAction();
        ObservedAction total_party_caster_usages = ObservedAction();
        ObservedAction total_party_target_usages = ObservedAction();

        const std::string Name() {
            // the sanitize fn occasionally throws fatal errors for some reason
            // return GuiUtils::WStringToString(GuiUtils::SanitizePlayerName(Name()));
            return GuiUtils::WStringToString(DecName());
        }

        const std::string DebugName() {
            return std::string("(") + std::to_string(skill_id) + ") \"" + GuiUtils::WStringToString(DecName()) + "\"";
        }

    private:
        wchar_t name_enc[64] = { 0 };
        wchar_t name_dec[256] = { 0 };
    };


    class ObservableParty {
    public:
        ObservableParty(ObserverModule& parent, const GW::PartyInfo& info)
            : parent(parent)
            , party_id(info.party_id) {}
        ~ObservableParty() {
            agent_ids.clear();
        }

        uint32_t party_id;
        ObserverModule& parent;

        ObservablePartyStats stats = ObservablePartyStats();

        bool SynchroniseParty();

        // agent_ids representing the players
        std::vector<uint32_t> agent_ids = {};

        std::string DebugName() {
            std::string display_name = "(" + std::to_string(party_id) + ")";
            return display_name;
        }
    };

    // TODO: push this to GWCA
    // applies to skill.target
    enum class TargetType {
        no_target = 0,
        anyone = 1,
        ally = 3,
        other_ally = 4,
        enemy = 5,
    };

    // TODO: replace with GWCA GenericValueId
    // https://github.com/GameRevision/GWLP-R/wiki/GenericValues
    enum class GenericValueId2
    {
        add_effect                  = GW::Packet::StoC::P156_Type::add_effect,
        remove_effect				= GW::Packet::StoC::P156_Type::remove_effect,
        apply_marker				= GW::Packet::StoC::P156_Type::apply_marker,
        remove_marker				= GW::Packet::StoC::P156_Type::remove_marker,
        damage				        = GW::Packet::StoC::P156_Type::damage,
        critical				    = GW::Packet::StoC::P156_Type::critical,
        effect_on_target			= GW::Packet::StoC::P156_Type::effect_on_target,
        effect_on_agent				= GW::Packet::StoC::P156_Type::effect_on_agent,
        animation				    = GW::Packet::StoC::P156_Type::animation,
        animation_special			= GW::Packet::StoC::P156_Type::animation_special,
        animation_loop				= GW::Packet::StoC::P156_Type::animation_loop,
        health				        = GW::Packet::StoC::P156_Type::health,
        energygain				    = GW::Packet::StoC::P156_Type::energygain,
        armorignoring				= GW::Packet::StoC::P156_Type::armorignoring,
        casttime				    = GW::Packet::StoC::P156_Type::casttime,
        attack_finished				= 1,
        attack_stopped				= 3,
        attack_started				= 4,
        disabled				    = 8,
        hit_by_skill                = 10,
        max_hp_reached              = 32,
        interrupted                 = 35,
        _q_attack_fail              = 38,
        change_health_regen         = 44,
        attack_skill_finsihed       = 46,
        instant_skill_activated	    = 48,
        attack_skill_stopped        = 49,
        attack_skill_activated	    = 50,
        _q_health_modifier_3        = 56,
        skill_finished				= 58,
        skill_stopped				= 59,
        skill_activated				= 60,
        energy_spent			    = 62,
        knocked_down                = 63,


        party_updated               = 65
    };


    ObserverModule() {}
    ~ObserverModule() {
        Reset();
    };

public:
    static ObserverModule& Instance() {
        static ObserverModule instance;
        return instance;
    }

    // lets the module run it in outposts and explorable areas
    void ToggleEnableExplorableAreas() {
        enable_explorable_areas = !enable_explorable_areas;
        if (enable_explorable_areas) {
            Log::Info("[ObserverMode]: Observer Module may run in Explorable Areas");
            if (IsActive()) InitializeObserverSession();
        }
        else Log::Info("[ObserverMode]: Observer Module will only run in Observer Mode.");
    }

    // Is the Module actively tracking agents?
    const bool IsActive() {
        // an observer match is considered an explorable area
        return is_enabled && is_explorable && (enable_explorable_areas || is_observer);
    }

    bool is_enabled = false;

    const char* Name() const override { return "Observer Module"; }
    const char* Icon() const override { return ICON_FA_EYE; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    bool InitializeObserverSession();
    void Reset();

    ObservableAgent* GetObservableAgentById(const uint32_t agent_id);
    ObservableAgent& CreateObservableAgent(const GW::AgentLiving& agent_living);

    ObservableSkill* GetObservableSkillById(const uint32_t skill_id);
    ObservableSkill& CreateObservableSkill(const GW::Skill& gw_skill);

    ObservableParty& GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info);
    ObservableParty* GetObservablePartyById(const uint32_t party_id);
    ObservableParty& CreateObservableParty(const GW::PartyInfo& party_info);

    const std::vector<uint32_t> GetObservablePartyIds() { return observable_party_ids; }
    const std::vector<ObservableParty*> GetObservablePartyArray() { return observable_parties_array; }

private:
    clock_t party_sync_timer = 0;

    // can be force enabled in non-explorable explorable areas
    bool enable_explorable_areas = false;

    bool observer_session_initialized = false;
    bool is_observer = false;
    bool is_explorable = false;

    // ***************
    // packet handlers
    // ***************

    void HandleInstanceLoadInfo(GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo *packet);
    void HandleAgentProjectileLaunched(const uint32_t agent_id, const bool is_attack);

    // ****************
    // generic handlers
    // ****************

    void HandleInterrupted(const uint32_t agent_id);
    void HandleKnockedDown(const uint32_t agent_id, const float duration);
    void HandleAgentState(const uint32_t agent_id, const uint32_t state);
    void HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit);
    void HandleAgentAdd(const uint32_t agent_id);

    void HandleAttackFinished(const uint32_t agent_id);
    void HandleAttackStopped(const uint32_t agent_id);
    void HandleAttackStarted(const uint32_t caster_id, const uint32_t target_id);

    void HandleInstantSkillActivated(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id);

    void HandleAttackSkillFinished(const uint32_t agent_id);
    void HandleAttackSkillStopped(const uint32_t agent_id);
    void HandleAttackSkillStarted(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id);

    void HandleSkillFinished(const uint32_t agent_id);
    void HandleSkillStopped(const uint32_t agent_id);
    void HandleSkillActivated(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id);

    void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id,
        const uint32_t target_id, const float value, const bool no_target);

    void ObserverModule::HandleGenericPacket(const uint32_t value_id, const uint32_t caster_id,
            const uint32_t target_id, const uint32_t value, const bool no_target);

    // action handlers
    void ReduceAction(ObservableAgent* caster, const TargetAction& action, ActionStage stage);

    // lazy loaded observed agents
    std::unordered_map<uint32_t, ObservableAgent*> observable_agents = {};

    // lazy loaded observed skills
    std::unordered_map<uint32_t, ObservableSkill*> observable_skills = {};

    // lazy loaded observed parties
    std::unordered_map<uint32_t, ObservableParty*> observable_parties = {};
    std::vector<uint32_t> observable_party_ids = {};
    std::vector<ObservableParty*> observable_parties_array = {};

    bool SynchroniseParties();

    // reorder the observable parties
    // cache array of party pointers
    // TODO: is an ordered map smarter than unordered+vector in this case because of infrequent IO to the map?
    void ReorderAndCacheParties() {
        observable_parties_array.clear();
        std::sort(observable_party_ids.begin(), observable_party_ids.end());
        for (uint32_t party_id : observable_party_ids) {
            // disabuse any players of their party_id's so we can reset them
            auto it_party = observable_parties.find(party_id);
            if (it_party != observable_parties.end()) {
                ObservableParty* party = it_party->second;
                observable_parties_array.push_back(party);
            }
        }
    }

    // hooks
    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry AgentState_Entry;
    GW::HookEntry AgentAdd_Entry;
    GW::HookEntry AgentProjectileLaunched_Entry;
    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericValue_Entry;
    GW::HookEntry GenericFloat_Entry;
};

