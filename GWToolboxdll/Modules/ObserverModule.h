#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <GWCA/GameEntities/Agent.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>

constexpr auto NO_SKILL = static_cast<GW::Constants::SkillID>(0);
constexpr auto NO_AGENT = 0;
constexpr auto NO_TEAM = 0;
constexpr auto NO_PARTY = 0;
constexpr auto NO_GUILD = 0;
constexpr auto NO_RANK = 0;
constexpr auto NO_RATING = 0;

namespace ObserverLabel {
    extern const char* Profession;
    extern const char* Name;
    extern const char* PlayerGuildTag;
    extern const char* PlayerGuildRating;
    extern const char* PlayerGuildRank;
    extern const char* Kills;
    extern const char* Deaths;
    extern const char* KDR;
    extern const char* Attempts;
    extern const char* Integrity;
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
    extern const char* SkillsUsed;
}; // namespace ObserverLabels


class ObserverModule : public ToolboxModule {
public:
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

    struct DeathEvent {
        uint32_t timestamp_ms;      // Match time when death occurred
        float position_x;           // X coordinate of death
        float position_y;           // Y coordinate of death
        uint32_t killer_agent_id;   // Agent ID of killer (if known)
        uint32_t killing_skill_id;  // Skill ID that caused death (NO_SKILL if unknown)
        bool is_npc;                // Whether the deceased was an NPC
        
        DeathEvent(uint32_t ts, float x, float y, uint32_t killer, uint32_t skill, bool npc)
            : timestamp_ms(ts), position_x(x), position_y(y), killer_agent_id(killer), killing_skill_id(skill), is_npc(npc) {}
    };

    struct MoraleBoostEvent {
        uint32_t timestamp_ms;      // Match time when morale boost occurred
        
        MoraleBoostEvent(uint32_t ts)
            : timestamp_ms(ts) {}
    };

    // Capture event tracking (for shrines and towers)
    struct CaptureEvent {
        uint32_t timestamp_ms;      // Match time when captured
        
        CaptureEvent(uint32_t ts)
            : timestamp_ms(ts) {}
    };

    // Resurrection event tracking
    enum class ResurrectionType {
        Unknown,        // Unknown source
        Skill,          // Player cast resurrection skill
        BaseResurrection // Automatic base resurrection (every 2 minutes)
    };
    
    struct ResurrectionEvent {
        uint32_t timestamp_ms;      // Match time when resurrected
        uint32_t resurrector_agent_id; // Agent ID of who resurrected (NO_AGENT if unknown)
        ResurrectionType resurrection_type; // Type of resurrection
        
        ResurrectionEvent(uint32_t ts, uint32_t resurrector, ResurrectionType type)
            : timestamp_ms(ts), resurrector_agent_id(resurrector), resurrection_type(type) {}
    };

    // Health snapshot for graphing
    struct HealthSnapshot {
        uint32_t timestamp_ms;      // Match time of snapshot
        float hp_percentage;        // HP as percentage (0.0 - 1.0)
        uint32_t hp_value;          // Actual HP value
        uint32_t max_hp;            // Max HP at time of snapshot
        
        HealthSnapshot(uint32_t ts, float hp_pct, uint32_t hp, uint32_t max)
            : timestamp_ms(ts), hp_percentage(hp_pct), hp_value(hp), max_hp(max) {}
    };

    // An action between a caster and target
    // Where an action can be a skill and/or attack
    struct TargetAction {
        TargetAction(const uint32_t caster_id,
                     const uint32_t target_id,
                     const bool is_attack,
                     const bool is_skill,
                     const GW::Constants::SkillID skill_id
        )
            : caster_id(caster_id)
            , target_id(target_id)
            , is_attack(is_attack)
            , is_skill(is_skill)
            , skill_id(skill_id) { }

        const uint32_t caster_id;
        const uint32_t target_id;
        const bool is_attack;
        const bool is_skill;
        const GW::Constants::SkillID skill_id;

        bool was_stopped = false;
        bool was_finished = false;
    };

    // an agents statistics for an action (attack)
    class ObservedAction {
    public:
        size_t started = 0;
        size_t stopped = 0;
        size_t finished = 0;
        size_t interrupted = 0;

        int integrity = 0;

        // damage tracking
        uint32_t total_damage = 0;

        void Reduce(const TargetAction* action, ActionStage stage);
    };

    // an agents statistics for an action (skill)
    struct ObservedSkill : ObservedAction {
        ObservedSkill(const GW::Constants::SkillID skill_id)
            : skill_id(skill_id) { }

        const GW::Constants::SkillID skill_id;
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

        // damage tracking
        uint32_t total_damage_dealt = 0;
        uint32_t total_damage_received = 0;
        uint32_t total_party_damage_dealt = 0;
        uint32_t total_party_damage_received = 0;

        // healing tracking
        uint32_t total_healing_dealt = 0;
        uint32_t total_healing_received = 0;
        uint32_t total_party_healing_dealt = 0;
        uint32_t total_party_healing_received = 0;

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

        // attacks

        ObservedAction total_attacks_dealt;
        ObservedAction total_attacks_received;

        // attacks done on other parties (not inc. npcs)
        ObservedAction total_attacks_dealt_to_other_parties;
        // attacks received from other parties (not inc. npcs)
        ObservedAction total_attacks_received_from_other_parties;

        // skills

        // skills used on anyone
        ObservedAction total_skills_used;
        // skills received from anyone
        ObservedAction total_skills_received;

        // skills used on your own party (not inc. npcs)
        ObservedAction total_skills_used_on_own_party;
        // skills used on other parties (not inc. npcs)
        ObservedAction total_skills_used_on_other_parties;

        // skills received from your own party (not inc. npcs)
        ObservedAction total_skills_received_from_own_party;
        // skills received from other parties (not inc. npcs)
        ObservedAction total_skills_received_from_other_parties;

        // skills used on your own team (inc. npcs)
        ObservedAction total_skills_used_on_own_team;
        // skills used on other team (inc. npcs)
        ObservedAction total_skills_used_on_other_teams;

        // skills received from your own team (inc. npcs)
        ObservedAction total_skills_received_from_own_team;
        // skills received from other team (inc. npcs)
        ObservedAction total_skills_received_from_other_teams;

        void HandleDeath();

        void HandleKill();
    };

    // Stats for Agents
    class ObservableAgentStats : public SharedStats {
    public:
        ~ObservableAgentStats();

        // map of agent_id -> ObservedAction
        std::unordered_map<uint32_t, ObservedAction*> attacks_dealt_to_agents = {};
        ObservedAction& LazyGetAttacksDealedAgainst(uint32_t target_agent_id);

        // map of agent_id -> ObservedAction
        std::unordered_map<uint32_t, ObservedAction*> attacks_received_from_agents = {};
        ObservedAction& LazyGetAttacksReceivedFrom(uint32_t attacker_agent_id);

        // skills

        // map of skill_id -> count of times received
        std::unordered_map<GW::Constants::SkillID, ObservedSkill*> skills_used = {};
        std::vector<GW::Constants::SkillID> skill_ids_used = {};
        ObservedAction& LazyGetSkillUsed(GW::Constants::SkillID skill_id);

        // map of skill_id -> count of times used
        std::unordered_map<GW::Constants::SkillID, ObservedSkill*> skills_received = {};
        std::vector<GW::Constants::SkillID> skill_ids_received = {};
        ObservedAction& LazyGetSkillReceived(GW::Constants::SkillID skill_id);

        // skills by agent

        // map of agent_id -> skill_id -> count of times received
        std::unordered_map<uint32_t, std::unordered_map<GW::Constants::SkillID, ObservedSkill*>> skills_received_from_agents = {};
        std::unordered_map<uint32_t, std::vector<GW::Constants::SkillID>> skill_ids_received_from_agents = {};
        ObservedSkill& LazyGetSkillReceivedFrom(uint32_t caster_agent_id, GW::Constants::SkillID skill_id);

        // map of agent_id -> skill_id -> count of times used
        std::unordered_map<uint32_t, std::unordered_map<GW::Constants::SkillID, ObservedSkill*>> skills_used_on_agents = {};
        std::unordered_map<uint32_t, std::vector<GW::Constants::SkillID>> skill_ids_used_on_agents = {};
        ObservedSkill& LazyGetSkillUsedOn(uint32_t target_agent_id, GW::Constants::SkillID skill_id);

        // damage tracking

        // map of agent_id -> damage dealt to that agent
        std::unordered_map<uint32_t, uint32_t> damage_dealt_to_agents = {};
        uint32_t& LazyGetDamageDealedAgainst(uint32_t target_agent_id);

        // map of agent_id -> damage received from that agent
        std::unordered_map<uint32_t, uint32_t> damage_received_from_agents = {};
        uint32_t& LazyGetDamageReceivedFrom(uint32_t caster_agent_id);

        // map of skill_id -> damage dealt by that skill
        std::unordered_map<GW::Constants::SkillID, uint32_t> damage_by_skill = {};
        uint32_t& LazyGetDamageBySkill(GW::Constants::SkillID skill_id);

        // map of agent_id -> skill_id -> damage dealt by skill to agent
        std::unordered_map<uint32_t, std::unordered_map<GW::Constants::SkillID, uint32_t>> damage_by_skill_to_agents = {};
        std::unordered_map<uint32_t, std::vector<GW::Constants::SkillID>> skill_ids_damage_to_agents = {};
        uint32_t& LazyGetDamageBySkillToAgent(uint32_t target_agent_id, GW::Constants::SkillID skill_id);

        // map of agent_id -> skill_id -> damage received from skill from agent
        std::unordered_map<uint32_t, std::unordered_map<GW::Constants::SkillID, uint32_t>> damage_from_skill_from_agents = {};
        std::unordered_map<uint32_t, std::vector<GW::Constants::SkillID>> skill_ids_damage_from_agents = {};
        uint32_t& LazyGetDamageFromSkillFromAgent(uint32_t caster_agent_id, GW::Constants::SkillID skill_id);

        // healing tracking (same structure as damage)

        // map of agent_id -> healing dealt to that agent
        std::unordered_map<uint32_t, uint32_t> healing_dealt_to_agents = {};
        uint32_t& LazyGetHealingDealedTo(uint32_t target_agent_id);

        // map of agent_id -> healing received from that agent
        std::unordered_map<uint32_t, uint32_t> healing_received_from_agents = {};
        uint32_t& LazyGetHealingReceivedFrom(uint32_t caster_agent_id);

        // map of skill_id -> healing dealt by that skill
        std::unordered_map<GW::Constants::SkillID, uint32_t> healing_by_skill = {};
        uint32_t& LazyGetHealingBySkill(GW::Constants::SkillID skill_id);

        // map of agent_id -> skill_id -> healing dealt by skill to agent
        std::unordered_map<uint32_t, std::unordered_map<GW::Constants::SkillID, uint32_t>> healing_by_skill_to_agents = {};
        std::unordered_map<uint32_t, std::vector<GW::Constants::SkillID>> skill_ids_healing_to_agents = {};
        uint32_t& LazyGetHealingBySkillToAgent(uint32_t target_agent_id, GW::Constants::SkillID skill_id);

        // map of agent_id -> skill_id -> healing received from skill from agent
        std::unordered_map<uint32_t, std::unordered_map<GW::Constants::SkillID, uint32_t>> healing_from_skill_from_agents = {};
        std::unordered_map<uint32_t, std::vector<GW::Constants::SkillID>> skill_ids_healing_from_agents = {};
        uint32_t& LazyGetHealingFromSkillFromAgent(uint32_t caster_agent_id, GW::Constants::SkillID skill_id);
    };

    // Stats for Parties
    class ObservablePartyStats : public SharedStats {
    public:
        //
    };

    class ObservableSkillStats {
    public:
        ObservedAction total_usages;

        ObservedAction total_self_usages;
        ObservedAction total_other_usages;

        ObservedAction total_own_party_usages;
        ObservedAction total_own_team_usages;

        ObservedAction total_other_party_usages;
        ObservedAction total_other_team_usages;
    };


    // Represents an agent whose stats are tracked
    // Includes players AND npc's
    class ObservableParty;

    class ObservableAgent {
    public:
        ObservableAgent(ObserverModule& parent, const GW::AgentLiving& agent_living);
        ~ObservableAgent();

        std::string profession = "";

        ObserverModule& parent;
        uint32_t agent_id;
        uint32_t login_number;
        uint32_t state = state;

        uint32_t guild_id;
        uint32_t team_id;
        // initialise to no party id
        // let the party set the party_id
        uint32_t party_id = NO_PARTY;
        uint32_t party_index = 0;

        GW::Constants::Profession primary;
        GW::Constants::Profession secondary;

        // latest action (attack/skill) the agent was undertaking
        TargetAction* current_target_action = nullptr;

        uint32_t last_hit_by = NO_AGENT;
        
        // Track the skill that last damaged this agent (for death tracking)
        GW::Constants::SkillID last_damage_skill_id = NO_SKILL;

        bool is_dead = false;
        
        uint32_t last_resurrector = NO_AGENT;

        bool is_player;
        bool is_npc;

        ObservableAgentStats stats;;

        std::vector<DeathEvent> death_events;
        
        std::vector<ResurrectionEvent> resurrection_events;

        // name fns with excessive caching & lazy loading
        std::string DisplayName();
        std::string RawName();
        std::wstring RawNameW();
        std::string DebugName();
        std::string SanitizedName();
        std::wstring SanitizedNameW();
        ObservableParty* GetParty() const;
        GW::Constants::SkillID GetCurrentSkill() const;

    private:
        bool trim_hench_name = false;

        std::string _display_name = "";

        // raw_name is inially unknown for NPC's
        // raw_name is asynchronously initialized
        std::wstring _raw_name_w = L"";
        std::string _raw_name = "";

        std::wstring _sanitized_name_w = L"";
        std::string _sanitized_name = "";
    };

    class ObservableGuild {
    public:
        ObservableGuild(ObserverModule& parent, const GW::Guild& guild);

        ObserverModule& parent;
        uint32_t guild_id;
        GW::GHKey key;
        std::string name;
        std::string tag;
        std::string wrapped_tag;
        uint32_t rank;
        uint32_t rating;
        uint32_t faction; // 0 - kurz, 1 - lux
        uint32_t faction_point;
        uint32_t qualifier_point;
        uint32_t cape_trim;
    };

    // Represents a skill in an ObserverMode
    // Usage is tracked
    class ObservableSkill {
    public:
        ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill);

        GW::Constants::SkillID skill_id;
        ObserverModule& parent;
        const GW::Skill& gw_skill;

        const wchar_t* DecName() const { return name_dec; }
        const bool HasExhaustion() const { return gw_skill.special & 0x1; }
        const bool IsMaintained() const { return gw_skill.duration0 == 0x20000; }
        const bool IsPvE() const { return (gw_skill.special & 0x80000) != 0; }
        const bool IsElite() const { return (gw_skill.special & 0x4) != 0; }
        ObservableSkillStats stats = ObservableSkillStats();

        const std::string Name();
        std::string DebugName();

    private:
        wchar_t name_enc[64] = {0};
        wchar_t name_dec[256] = {0};

        std::string _name = "";
    };


    class ObservableParty {
    public:
        ObservableParty(ObserverModule& parent, const GW::PartyInfo& info);
        ~ObservableParty();

        uint32_t party_id;

        uint32_t guild_id = NO_GUILD;
        uint32_t rating = NO_RATING;
        uint32_t rank = NO_RANK;
        std::string rank_str; // 0 -> "N/A"
        std::string name = "";
        std::string display_name = "";

        std::vector<MoraleBoostEvent> morale_boosts;
        std::vector<CaptureEvent> shrine_captures;
        std::vector<CaptureEvent> tower_captures;
        bool is_victorious = false;
        bool is_defeated = false;

        ObserverModule& parent;

        ObservablePartyStats stats = ObservablePartyStats();

        bool SynchroniseParty();

        std::vector<uint32_t> agent_ids = {};

        // Aggregate party health snapshots (every 15 seconds)
        std::vector<HealthSnapshot> health_snapshots = {};

        std::string DebugName() const
        {
            std::string _debug_name = "(" + std::to_string(party_id) + ") " + display_name;
            return _debug_name;
        }
    };

    // closely rlated to GW::AreaInfo
    class ObservableMap {
    public:
        ObservableMap(GW::Constants::MapID map_id, const GW::AreaInfo& area_info);

        GW::Constants::MapID map_id;
        GW::Constants::Campaign campaign;
        GW::Continent continent;
        GW::Region region;
        GW::RegionType type;
        uint32_t flags;
        uint32_t name_id;
        uint32_t description_id;

        std::string Name();
        std::string Description();
        [[nodiscard]] bool GetIsPvP() const { return (flags & 0x1) != 0; }
        [[nodiscard]] bool GetIsGuildHall() const { return (flags & 0x800000) != 0; }

    private:
        std::string name = "";
        std::wstring name_w = L"";
        std::string description = "";
        std::wstring description_w = L"";
        wchar_t name_enc[8]{};
        wchar_t description_enc[8]{};
    };

    enum class TargetType {
        no_target = 0,
        anyone    = 1,
        // 2?
        ally       = 3,
        other_ally = 4,
        enemy      = 5,
    };

    static ObserverModule& Instance()
    {
        static ObserverModule instance;
        return instance;
    }

    const bool IsActive() const;

    [[nodiscard]] const char* Name() const override { return "Observer Module"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_EYE; }

    struct Settings {
        bool is_enabled = false;
        bool trim_hench_names = false;
        bool enable_in_explorable_areas = false;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawSettingsInternal() override;

    bool InitializeObserverSession(GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0));
    void Reset();

    ObservableGuild* GetObservableGuildById(uint32_t guild_id);
    ObservableAgent* GetObservableAgentById(uint32_t agent_id);
    ObservableSkill* GetObservableSkillById(GW::Constants::SkillID skill_id);
    ObservableParty* GetObservablePartyById(uint32_t party_id);

    ObservableMap* GetMap() const;
    const std::vector<uint32_t>& GetObservableGuildIds();
    const std::vector<uint32_t>& GetObservableAgentIds();
    const std::vector<uint32_t>& GetObservablePartyIds();
    const std::vector<GW::Constants::SkillID>& GetObservableSkillIds();
    const std::unordered_map<uint32_t, ObservableGuild*>& GetObservableGuilds();
    const std::unordered_map<uint32_t, ObservableAgent*>& GetObservableAgents();
    const std::unordered_map<uint32_t, ObservableParty*>& GetObservableParties();
    const std::unordered_map<GW::Constants::SkillID, ObservableSkill*>& GetObservableSkills();
    uint32_t GetCachedMaxHP(uint32_t agent_id);
    uint32_t GetCachedEnergy(uint32_t agent_id) const;
    uint32_t GetCachedMaxEnergy(uint32_t agent_id) const;

    bool match_finished = false;
    uint32_t winning_party_id = NO_PARTY;
    bool first_countdown_seen = false; // Track if we've seen the initial countdown at map load
    uint32_t match_start_instance_time = 0; // Instance time when match actually started (countdown finished)
    ObservableMap* match_start_map = nullptr; // Map when the match started (captured at second countdown)
    std::chrono::milliseconds match_duration_ms_total{};
    std::chrono::milliseconds match_duration_ms{};
    std::chrono::seconds match_duration_secs{};
    std::chrono::minutes match_duration_mins{};

};
