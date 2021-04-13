#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>
// #include <GWCA/GameEntities/Skill.h>

#include <Color.h>
#include <Defines.h>
#include <Timer.h>
#include <ToolboxWidget.h>
#include <ToolboxWindow.h>


// class ObserverModule : public ToolboxWidget {
class ObserverModule : public ToolboxWindow {
    //
    // Represents a character in an observer mode match
    //
    // Includes players AND npc's
    //
    class ObservableAgent {
    public:
		// for attacking or casting
		struct TargetAction {
			TargetAction(const uint32_t caster_id, const uint32_t target_id, const bool is_attack, const bool is_skill, const uint32_t skill_id)
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

		enum class ActionStartpoint {
            Started,
			Instant,
		};

		enum class ActionEndpoint {
			Stopped,
			Finished,
			Interrupted // "Interrupted" is received after "Stopped"
		};

        class ObservedAction {
        public:
			size_t started = 0;
			size_t stopped = 0;
			size_t finished = 0;
			size_t interrupted = 0;

			void HandleEvent(const ActionStartpoint startpoint) {
				switch (startpoint) {
					case ActionStartpoint::Instant:
						started += 1;
						finished += 1;
						break;
					case ActionStartpoint::Started:
						started += 1;
						break;
				}
			}

			void HandleEvent(const ActionEndpoint endpoint) {
				switch (endpoint) {
					case ActionEndpoint::Stopped:
						stopped += 1;
						break;
					case ActionEndpoint::Interrupted:
						stopped -= 1;
						interrupted += 1;
						break;
					case ActionEndpoint::Finished:
						finished += 1;
						break;
				}
			}
        };

		struct ObservedSkill : ObservedAction {
        public:
			ObservedSkill(const uint32_t skill_id) : skill_id(skill_id) {}
			const uint32_t skill_id;
		};


        ObservableAgent(ObserverModule& parent, const GW::AgentLiving& agent_living): parent(parent) {
            state = agent_living.model_state;
			agent_id = agent_living.agent_id;
			team_id = agent_living.team_id;
			GW::Agents::AsyncGetAgentName(&agent_living, name);
			primary = agent_living.primary;
			secondary = agent_living.secondary;
			is_npc = agent_living.IsNPC();
			is_player = agent_living.IsPlayer();
            login_number = agent_living.login_number;
        };
        ~ObservableAgent() {
            delete target_action;
            attacks_received_by_agent.clear();
            attacks_dealed_by_agent.clear();
            skill_ids_used.clear();
            skills_used.clear();
            skill_ids_received.clear();
            skills_received.clear();
            skills_received_by_agent.clear();
            skills_used_by_agent.clear();
        }

        ObserverModule& parent;
        uint32_t agent_id;
        uint32_t team_id;
        uint32_t login_number;
        uint32_t state = state;

        // kills only apply to players, not npcs
        size_t deaths = 0;
        size_t kills = 0;
        float kdr_pc = 0;
        std::string kdr_str = 0;

        // size_t party_index = 0;
        // size_t party_id = 0;

        bool is_player;
        bool is_npc;

        std::wstring name = L"???";             // initially unknown
        uint32_t primary = (uint32_t) GW::Constants::Profession::None;
        uint32_t secondary = (uint32_t) GW::Constants::Profession::None;

        // latest action the agent was undertaking
        const TargetAction* target_action = nullptr;

        // ******
        // totals
        // ******

        ObservedAction total_attacks_done = ObservedAction();
        ObservedAction total_attacks_received = ObservedAction();

        ObservedAction total_skills_used = ObservedAction();
        ObservedAction total_skills_received = ObservedAction();

        size_t total_crits_received = 0;
        size_t total_crits_dealt = 0;

        size_t total_npc_crits_received = 0;
        size_t total_npc_crits_dealt = 0;

        size_t total_player_crits_received = 0;
        size_t total_player_crits_dealt = 0;

        size_t knocked_down_count = 0;
        size_t interrupted_count = 0;
        size_t cancelled_count = 0;
        float knocked_down_duration = 0;


        // ******
        // attacks by agent
        // ******

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
        std::unordered_map<uint32_t, ObservedAction*> attacks_dealed_by_agent = {};
        // getter helper
        ObservedAction& LazyGetAttacksDealedAgainst(const uint32_t receiver_agent_id) {
            auto it = attacks_dealed_by_agent.find(receiver_agent_id);
            if (it == attacks_dealed_by_agent.end()) {
                // receiver not registered
                ObservedAction* observed_action = new ObservedAction();
                attacks_dealed_by_agent.insert({receiver_agent_id, observed_action});
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
            auto it_receiver = skills_received_by_agent.find(caster_agent_id);
            if (it_receiver == skills_received_by_agent.end()) {
				// receiver and his skills are not registered with this agent
                std::vector<uint32_t> received_skill_ids = {skill_id};
                skills_ids_received_by_agent.insert({ caster_agent_id, received_skill_ids });
				ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                std::unordered_map<uint32_t, ObservedSkill*> received_skills = {{skill_id, observed_skill}};
                skills_received_by_agent.insert({caster_agent_id, received_skills});
                return *observed_skill;
            } else {
                // receiver is registered with this agent
                auto it_observed_skill = it_receiver->second.find(skill_id);
                // does receiver have the skill registered from/against us?
                if (it_observed_skill == it_receiver->second.end()) {
                    // receivers hasn't registered this skill with us
					ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                    skills_ids_received_by_agent.find(caster_agent_id)->second.push_back(skill_id);
                    it_receiver->second.insert({skill_id, observed_skill});
                    return *observed_skill;
                } else {
                    // receivers already registered this skill
                    return *(it_observed_skill->second);
                }
            }
        }


        // map of agent_id -> skill_id -> count of times used
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, ObservedSkill*>> skills_used_by_agent = {};
        // map of agent_id -> vector<skill_ids>
        std::unordered_map<uint32_t, std::vector<uint32_t>> skills_ids_used_by_agent = {};
        // getter helper
        // note: registers the skill_id on access
        ObservedSkill& LazyGetSkillUsedAgainst(const uint32_t target_agent_id, const uint32_t skill_id) {
            auto it_receiver = skills_used_by_agent.find(target_agent_id);
            if (it_receiver == skills_used_by_agent.end()) {
                // receiver and his skills are not registered with this agent
                std::vector<uint32_t> used_skill_ids = {skill_id};
                skills_ids_used_by_agent.insert({ target_agent_id, used_skill_ids });
				ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                std::unordered_map<uint32_t, ObservedSkill*> used_skills = {{skill_id, observed_skill}};
                skills_used_by_agent.insert({target_agent_id, used_skills});
                return *observed_skill;
            } else {
                // receiver is registered with this agent
                auto it_observed_skill = it_receiver->second.find(skill_id);
                // does receiver have the skill registered from/against us?
                if (it_observed_skill == it_receiver->second.end()) {
                    // receivers hasn't registered this skill with us
					ObservedSkill* observed_skill = new ObservedSkill(skill_id);
                    skills_ids_used_by_agent.find(target_agent_id)->second.push_back(skill_id);
                    it_receiver->second.insert({skill_id, observed_skill});
                    return *observed_skill;
                } else {
                    // receivers already registered this skill
                    return *(it_observed_skill->second);
                }
            }
        }

        // *******
        // methods
        // *******

        void HandleActionStarted(const TargetAction& action, ActionStartpoint startpoint);
        void HandleReceivedActionStarted(const TargetAction& action, ActionStartpoint startpoint);

        const TargetAction* HandleActionEnded(const ActionEndpoint endpoint);
        void HandleReceivedActionEnded(const TargetAction& action, const ActionEndpoint endpoint);

        void HandleKnockedDown(float duration);

        void HandleStateChange(const uint32_t _state) {
            state = _state;
            if (_state == 16) {
                // dead
                deaths += 1;
            }
        }

        void HandleDeath() {
            deaths += 1;
            // recalculate kdr
            kdr_pc = (float) kills / deaths;
            // get kdr string
            std::stringstream str;
            str << std::fixed << std::setprecision(2) << kdr_pc;
            kdr_str = str.str();
        }

        void HandleKill() {
            kills += 1;
            // recalculate kdr
            if (deaths == 1)
                kdr_pc = kills;
            else
                kdr_pc = (float) kills / deaths;
            // get kdr string
            std::stringstream str;
            str << std::fixed << std::setprecision(2) << kdr_pc;
            kdr_str = str.str();
        }

        GW::Agent* GetAgent();
        GW::AgentLiving* GetAgentLiving();

        std::string SafeName() {
            // the sanitize fn occasionally throws fatal errors for some reason
            // std::string safe_name = GuiUtils::WStringToString(GuiUtils::SanitizePlayerName(name));
            std::string safe_name = GuiUtils::WStringToString(name);
		    return safe_name;
        }

        std::string DisplayName() {
            std::string display_name = "(" + std::to_string(agent_id) + ") " + "\"" + SafeName() + "\"";
		    return display_name;
        }

        std::string TrimmedDisplayName() {
            // trim hench names appended like [Illusion Henchmen]
            std::string display_name = "(" + std::to_string(agent_id) + ") " + "\"" + SafeName() + "\"";
			size_t begin = display_name.find(" [");
			size_t end = display_name.find_first_of("]");
			if (std::string::npos != begin && std::string::npos != end && begin <= end) {
				display_name.erase(begin, end-begin + 1);
			}
		    return display_name;
        }
    };


    //
    // Represents a skill in an observer mode match
    //
    class ObservableSkill {
    public:
        ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill) : parent(parent), gw_skill(_gw_skill) {
            skill_id = _gw_skill.skill_id;
        }

        ~ObservableSkill() {
            // usages_by_team.clear();
            usages_by_agent.clear();
        }

        ObserverModule& parent;
        uint32_t skill_id;
		const wchar_t* Name();
        const GW::Skill& gw_skill;
        const bool HasExhaustion() { return gw_skill.special & 0x1; }
        const bool IsMaintained() { return gw_skill.duration0 == 0x20000;  }
        const bool IsPvE() { return (gw_skill.special & 0x80000) != 0;  }
        const bool IsElite() { return (gw_skill.special & 0x4) != 0; }

        size_t total_usages = 0;
        size_t total_npc_usages = 0;
        size_t total_player_usages = 0;

        // std::unordered_map<uint32_t, size_t> usages_by_team = {};
        std::unordered_map<uint32_t, size_t> usages_by_agent = {};

        void AddUsage(const uint32_t agent_id);

		const std::string SafeName() {
            // the sanitize fn occasionally throws fatal errors for some reason
			// return GuiUtils::WStringToString(GuiUtils::SanitizePlayerName(Name()));
			return GuiUtils::WStringToString(Name());
		}

		const std::string DisplayName() {
			return std::string("(") + std::to_string(skill_id) + ") \"" + GuiUtils::WStringToString(Name()) + "\"";
		}

    private:
        wchar_t name_enc[64] = { 0 };
        wchar_t name_dec[256] = { 0 };
    };


    class ObservableParty {
    public:
        ObservableParty(ObserverModule& parent, const GW::PartyInfo& info) : parent(parent) {
            party_id = info.party_id;
        }
        ~ObservableParty() {
            //
        }

        uint32_t party_id;
        ObserverModule& parent;
        uint32_t max_hp_current = 0;
        uint32_t max_hp_initial = 0;

        void SynchroniseParty();

        size_t team_id = 0;

        // agent_ids representing the players
        std::vector<uint32_t> member_agent_ids = {};

        // general totals
        size_t total_healing_received = 0;
        size_t total_healing_dealt = 0;

        size_t total_damage_received = 0;
        size_t total_damage_dealt = 0;

        // npc totals
        size_t total_npc_healing_received = 0;
        size_t total_npc_healing_dealt = 0;

        size_t total_npc_damage_received = 0;
        size_t total_npc_damage_dealt = 0;

        // player totals
        size_t total_player_healing_received = 0;
        size_t total_player_healing_dealt = 0;

        size_t total_player_damage_received = 0;
        size_t total_player_damage_dealt = 0;

        // misc
        size_t knocked_down_count = 0;
        size_t interrupted_count = 0;

        void HandleDamageDealt(const ObservableAgent& to, int value);
        void HandleDamageReceived(const ObservableAgent& from, int value);

        std::string DisplayName() {
            std::string display_name = "(" + std::to_string(party_id) + ")";
		    return display_name;
        }
    };

    // struct AgentInfo {
    //     GW::Agent* core;
    //     GW::AgentLiving* living;
    //     ObservableAgent* observed;
    // };



public:
    static ObserverModule& Instance() {
        static ObserverModule instance;
        return instance;
    }
    ObserverModule() {
        // is_resizable = false;
    }
    ~ObserverModule() {
        //
    };

    // const AgentInfo& GetAgentInfo(const uint32_t* agent_id);
    // const AgentInfo& GetAgentInfo(const uint32_t agent_id);


    // https://github.com/GameRevision/GWLP-R/wiki/GenericValues
	enum class GenericValueId2
    {
        // not useful:
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


        //
        // melee attack finished?
        attack_finished				= 1,                                                //  GenericValue  |                    |                 |
        // melee attack stopped?
        attack_stopped				= 3,                                                //  GenericValue  |                    |                 |

        // (melee or ranged) attack started
        attack_started				= 4,                                                //  Genericvalue  |                    |                 |

        // value = 0 | 1 for can/can't cast. because of aftercast / knockdown / ...
        // (self only)
        disabled				    = 8,



        // useful:

        // name                     | value                                              |  GenericValue  | GenericValueTarget | GenericModifier | notes
        // ------------------------ | -------------------------------------------------  | -------------- | ------------------ | --------------- | ------

        // skill damage
        hit_by_skill                = 10,

        // ? regenned_full_health        = 32,
        max_hp_reached              = 32,


        // knocked down
        interrupted                 = 35,

        _q_attack_fail              = 38,

        // 
        change_health_regen         = 44,

        // MELEE attack skill finished
        attack_skill_finsihed       = 46,
        
        instant_skill_activated	    = 48,                                               //  GenericValue  | ????? coward ????? |                 | non-blocking skills (stances/shouts/...) todo: test for GenericValueTarget

        attack_skill_stopped        = 49,

        // xxxxxxxxxxx 49: attack interrupted?

        // attack_skill_cast
        attack_skill_activated	    = 50,                                               //                | GenericValueTarget |                 | todo: test different attack skills


        _q_health_modifier_3        = 56,

        // _q_fight_stance2?
        skill_finished				= 58,                                               //  GenericValue  |                    |                 |

        // /interrupt_skill?
        skill_stopped				= 59,                                               //  GenericValue  |                    |                 |
        skill_activated				= 60,                                               //  GenericValue  | GenericValueTarget |                 | blocking skills

        energy_spent			    = 62,                                               //  GenericValue  | GenericValueTarget |                 | blocking skills

        knocked_down                = 63,


		//   1:	GenericValue		| finished attack					| agent  = protagonist		|							| value = ?						|
		//   3:	GenericValue		| stop attack						| agent  = protagonist		|							| value = ?						|
		//   4:	GenericValueTarget	| start attack						| caster = protagonist		| target = antagonist		| ?								|
		// | 8:	GenericValue		| disabled 						    | agent	 = protagonist		|							| (can cast, value = 0)			|
		// | 						| (kd, aftercast, casting already)	|							|							| (cannot casting, value = 1)	|
		//  10:	GenericValue		| hit by skill						|?agent  = protagonist?		|							| value = skill_id				|
		//  20: "effect_on_target":
		//	20: GenericValueTarget	| effect							| caster = protagonist		| target = antagonist		| values:						|
		//							|									|							|							| 110: interrupt				|

        //
		//  32: GenericValue		| inturrupting?						| agent  = ?				|							| value = 0
        //                          | reached full health?              |
        //                          | remove health regeneration?       |
		//  35: GenericValue		| interrupted 						| agent  = protagonist	    |							| value = 0                      | follows 59 - cancelled

		//  49: GenericValue		| interrupted? (after getting leech signet on attack skill)

        //  44: GenericFloat        | change health regeneration        | agent  = protagonist      |                           | value = %of max_hp regen
		//  46: GenericValue		| Finish attack skill               | agent  = protagonist		|							|								|
		//  48:	GenericValue		| activate insta skill stnc/sht/atk | agent  = protagonist		|							| value = skill_id				|

		//  49: GenericValue		| interrupted? (after getting leech signet on attack skill)
		//  50: GenericValueTarget	| use attack skill					| caster = antagonist		| target = protagonist		| value = ?						|

		//  58: interrupted?																									  value = 0
		//  58: GenericValue		| finish casting					| agent  = protagonist		|							| value = ?						| includes protectors defense
		//  59: GenericValue		| stopped casting (cancel/rupt)     | agent  = protagonist		|							| value = ?						|
		// |60: GenericValueTarget	| use (non attack) skill (other)	| caster = antagonist		| target = protagonist		| value = ?						|
		// |60: GenericValue		| use skill (self)					| agent  = protagonist		|							| value = skill_id				| includes protectors defense

        //  62: GenericFloat        | energy spent                      | agent  = protagonist      |                           | value = % max energy gained
        //  63: GenericFloat        | knocked down                      | agent  = protagonist      |                           | value = knockdown duration    |


        //  65: GenericValue ? (in map range?) value_id: agent_id to add
    };


    const char* Name() const override { return "Observer"; }
    // const char* Name() const { return "Observer"; }
    const char* Icon() const override { return ICON_FA_BARS; }
    // const char* Icon() const { return ICON_FA_BARS; }

    void Initialize() override;
    // void Initialize();
    void Terminate() override;
    // void Terminate();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    void WriteObserverWidget();
    // void WriteDamageOf(size_t index, uint32_t rank = 0); // party index from 0 to 12

    void InitializeObserverSession();
    void Reset();

    ObservableAgent* GetObservableAgentByAgentId(uint32_t agent_id);
    ObservableAgent* GetObservableAgentByAgent(const GW::Agent& agent);
    ObservableAgent& GetObservableAgentByAgentLiving(const GW::AgentLiving& agent_living);
	ObservableAgent& CreateObservableAgent(const GW::AgentLiving& agent_living);
    ObservableSkill* GetObservableSkillBySkillId(uint32_t skill_id);

    ObservableParty& GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info);

     // lets you run it in outposts and explorable areas
    void ToggleForceEnabled() {
        force_enabled = !force_enabled;
        if (force_enabled) {
			Log::Info("Forcing ObserverModule enabled");
        } else {
			Log::Info("Stopped forcing ObserverModule enabled");
        }
    }

    enum class FollowMode {
        None,
        Target,
        UsedByTarget,
        UsedOnTarget,
    };

    // restricts logging to only the currently selected target
    void ToggleFollowMode() {
        switch (follow_mode) {
            case FollowMode::None:
                follow_mode = FollowMode::Target;
                Log::Info("[FollowMode]: Switching to \"Target\"");
                break;
            case FollowMode::Target:
                follow_mode = FollowMode::UsedByTarget;
                Log::Info("[FollowMode]: Switching to \"UsedByTarget\"");
                break;
            case FollowMode::UsedByTarget:
                follow_mode = FollowMode::UsedOnTarget;
                Log::Info("[FollowMode]: Switching to \"UsedOnTarget\"");
                break;
            case FollowMode::UsedOnTarget:
                follow_mode = FollowMode::None;
                Log::Info("[FollowMode]: off");
                break;
        
        }
    }

    bool force_enabled = false;


private:
    // Expect some combinations of packets to come in sequence.
    // We use this to infer things like whether a skill was cancelled,
    // or knocked down / interrupted

    uint32_t packet_number = 0;

    FollowMode follow_mode = FollowMode::None;
    bool observer_session_initialized = false;
    bool is_observer = false;
    bool is_explorable = false;
    uint32_t last_living_target = 0;

    bool IsActive() {
        return is_explorable && (force_enabled || is_observer);
    }

    // packet handlers

    void HandleInstanceLoadInfo(GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo *packet);
    // void HandleAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState *packet);

    void HandleGenericModifier(GW::HookStatus* status, GW::Packet::StoC::GenericModifier *packet);
    void HandleGenericValueTarget(GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget *packet);
    void HandleGenericValue(GW::HookStatus* status, GW::Packet::StoC::GenericValue *packet);
    void HandleGenericFloat(GW::HookStatus* status, GW::Packet::StoC::GenericFloat *packet);

    // generic handlers

    void HandleHitBySkill(const uint32_t caster_id, const uint32_t target_id, const uint32_t skill_id);
    void HandleInterrupted(const uint32_t agent_id);
    void HandleKnockedDown(const uint32_t agent_id, const float duration);
    void HandleDamageDone(const uint32_t caster_id, const uint32_t target_id, const float amount_pc, const bool is_crit);

    void HandleAgentProjectileLaunched(const uint32_t agent_id, const bool is_attack);
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


	void LogUnexpectedGenericPacket(const std::string caller, const std::string name, const uint32_t value_id,
        const uint32_t caster_id, const uint32_t target_id, const uint32_t value);

    void LogUnexpectedGenericPacket(const std::string caller, const std::string name, const uint32_t value_id,
        const uint32_t caster_id, const uint32_t target_id, const float value);

    void LogUnknownGenericPacket(const std::string caller, const uint32_t value_id, const uint32_t caster_id,
        const uint32_t target_id, const uint32_t value);

    void LogUnknownGenericPacket(const std::string caller, const uint32_t value_id, const uint32_t caster_id,
        const uint32_t target_id, const float value);

    void LogGenericPacket(const std::string caller, const std::string name, const uint32_t value_id,
        const uint32_t caster_id, const uint32_t target_id, const uint32_t value, const bool is_skill);

    void LogGenericPacket(const std::string caller, const std::string name, const uint32_t value_id,
        const uint32_t caster_id, const uint32_t target_id, const float value);

    void _DoLogGenericPacket(const std::string kind, std::string name, const uint32_t value_id,
        const uint32_t caster_id, const uint32_t target_id, std::string value_name);

    void HandleGenericPacket(const std::string kind,  const uint32_t value_id, const uint32_t caster_id,
        const uint32_t target_id, const uint32_t i_value, const float f_value, const bool is_float, const bool no_target);

      

    // void HandleSkillActivate(GW::HookStatus* status, GW::Packet::StoC::SkillActivate *packet);
    // void HandleAgentName(GW::HookStatus* status, GW::Packet::StoC::AgentName *packet);
    // void HandlePlayerJoinInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance *packet);
    // void HandlePlayerLeaveInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerLeaveInstance *packet);

    // lazy loaded observed agents
    std::unordered_map<uint32_t, ObservableAgent*> observable_agents = {};

    // lazy loaded observed skills
    std::unordered_map<uint32_t, ObservableSkill*> observed_skills = {};

    // lazy loaded observed parties
    std::vector<ObservableParty*> observed_parties = {};

    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry AgentState_Entry;
    GW::HookEntry AgentProjectileLaunched_Entry;

    // GW::HookEntry AgentHealth_Entry;

    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericValue_Entry;
    GW::HookEntry GenericFloat_Entry;
};

