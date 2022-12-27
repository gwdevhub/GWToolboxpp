#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>

#include <Timer.h>

#define NO_AGENT 0
#define NO_PARTY 0
#define NO_SKILL (GW::Constants::SkillID)0
#define NO_TEAM 0
#define NO_GUILD 0
#define NO_RATING 0
#define NO_RANK 0

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
            , skill_id(skill_id)
        {}
        const uint32_t caster_id;
        const uint32_t target_id;
        const bool is_attack;
        const bool is_skill;
        const GW::Constants::SkillID skill_id;

        // if the action was interrupted after it was finished (e.g. an attack skill that gets interrupted
        // in the aftersting [once the skill has completed activation]) then we don't count as "interrupted"
        // even if receiving an "interrupted" signal
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

        // should be zero at all times except when an action is not yet concluded
        // used to indicate there may be innacuracies in the stats due to due to
        // innacuracies in our modelling of the game engine
        int integrity = 0;

        void Reduce(const TargetAction* action, ActionStage stage);
    };

    // an agents statistics for an action (skill)
    struct ObservedSkill : ObservedAction {
    public:
        ObservedSkill(const GW::Constants::SkillID skill_id) : skill_id(skill_id) {}
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

        // fired when the agent dies
        void HandleDeath();

        // fired when the agent scores a kill
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
        ObservableAgentStats stats;;
    public:
        // name fns with excessive caching & lazy loading
        std::string DisplayName();
        std::string RawName();
        std::wstring RawNameW();
        std::string DebugName();
        std::string SanitizedName();
        std::wstring SanitizedNameW();

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
        uint32_t cape_trim; // TODO: which is gold (1)?
    };

    // Represents a skill in an ObserverMode
    // Usage is tracked
    class ObservableSkill {
    public:
        ObservableSkill(ObserverModule& parent, const GW::Skill& _gw_skill);

        GW::Constants::SkillID skill_id;
        ObserverModule& parent;
        const GW::Skill& gw_skill;

        const wchar_t* DecName() { return name_dec; }
        const bool HasExhaustion() { return gw_skill.special & 0x1; }
        const bool IsMaintained() { return gw_skill.duration0 == 0x20000;  }
        const bool IsPvE() { return (gw_skill.special & 0x80000) != 0;  }
        const bool IsElite() { return (gw_skill.special & 0x4) != 0; }
        ObservableSkillStats stats = ObservableSkillStats();

        const std::string Name();
        const std::string DebugName();
    private:
        wchar_t name_enc[64] = { 0 };
        wchar_t name_dec[256] = { 0 };

        std::string _name = "";
    };


    class ObservableParty {
    public:
        ObservableParty(ObserverModule& parent, const GW::PartyInfo& info);
        ~ObservableParty();

        uint32_t party_id;

        // guild_id and name are inferred from the guilds members
        // TODO: get the actual teams guild/name from memory,
        // instead of guessing
        uint32_t guild_id = NO_GUILD;
        uint32_t rating = NO_RATING;
        uint32_t rank = NO_RANK;
        std::string rank_str; // 0 -> "N/A"
        std::string name = "";
        std::string display_name = "";

        uint32_t morale_boosts = 0;
        bool is_victorious = false;
        bool is_defeated = false;

        ObserverModule& parent;

        ObservablePartyStats stats = ObservablePartyStats();

        bool SynchroniseParty();

        // agent_ids representing the players
        std::vector<uint32_t> agent_ids = {};

        std::string DebugName() {
            std::string _debug_name = "(" + std::to_string(party_id) + ") " + display_name;
            return _debug_name;
        }
    };

    // closely rlated to GW::AreaInfo
    class ObservableMap {
    public:
        ObservableMap(const GW::AreaInfo& area_info);

        uint32_t campaign;
        uint32_t continent;
        GW::Region region;
        GW::RegionType type;
        uint32_t flags;
        uint32_t name_id;
        uint32_t description_id;

        std::string Name();
        std::string Description();
        bool GetIsPvP()          const { return (flags & 0x1) != 0; }
        bool GetIsGuildHall()    const { return (flags & 0x800000) != 0; }

    private:
        std::string name = "";
        std::wstring name_w = L"";
        std::string description = "";
        std::wstring description_w = L"";
        wchar_t name_enc[8];
        wchar_t description_enc[8];
    };

    // TODO: push this to GWCA
    // applies to skill.target
    enum class TargetType {
        no_target = 0,
        anyone = 1,
        // 2?
        ally = 3,
        other_ally = 4,
        enemy = 5,
    };

    ~ObserverModule();


public:
    static ObserverModule& Instance() {
        static ObserverModule instance;
        return instance;
    }

    const bool IsActive();
    bool is_enabled = false;

    const char* Name() const override { return "Observer Module"; }
    const char* Icon() const override { return ICON_FA_EYE; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

    bool InitializeObserverSession();
    void Reset();

    ObservableGuild* GetObservableGuildById(uint32_t guild_id);
    ObservableAgent* GetObservableAgentById(uint32_t agent_id);
    ObservableSkill* GetObservableSkillById(GW::Constants::SkillID skill_id);
    ObservableParty* GetObservablePartyById(uint32_t party_id);

    ObservableMap* GetMap() { return map; }
    const std::vector<uint32_t>& GetObservableGuildIds() { return observable_guild_ids; }
    const std::vector<uint32_t>& GetObservableAgentIds() { return observable_agent_ids; }
    const std::vector<uint32_t>& GetObservablePartyIds() { return observable_party_ids; }
    const std::vector<GW::Constants::SkillID>& GetObservableSkillIds() { return observable_skill_ids; }
    const std::unordered_map<uint32_t, ObservableGuild*>& GetObservableGuilds() { return observable_guilds; }
    const std::unordered_map<uint32_t, ObservableAgent*>& GetObservableAgents() { return observable_agents; }
    const std::unordered_map<uint32_t, ObservableParty*>& GetObservableParties() { return observable_parties; }
    const std::unordered_map<GW::Constants::SkillID, ObservableSkill*>& GetObservableSkills() { return observable_skills; }

    bool match_finished = false;
    uint32_t winning_party_id = NO_PARTY;
    std::chrono::milliseconds match_duration_ms_total;
    std::chrono::milliseconds match_duration_ms;
    std::chrono::seconds match_duration_secs;
    std::chrono::minutes match_duration_mins;

private:
    ObservableGuild* CreateObservableGuild(const GW::Guild& guild);
    ObservableAgent* CreateObservableAgent(const GW::AgentLiving& agent_living);
    ObservableSkill* CreateObservableSkill(const GW::Skill& gw_skill);
    ObservableParty* CreateObservableParty(const GW::PartyInfo& party_info);
    ObservableParty* GetObservablePartyByPartyInfo(const GW::PartyInfo& party_info);

    clock_t party_sync_timer = 0;

    // agent name settings
    bool trim_hench_names = false;
    bool trim_player_indexes = false;
    bool enable_in_explorable_areas = false;

    // can be force enabled in non-explorable explorable areas

    bool observer_session_initialized = false;
    bool is_observer = false;
    bool is_explorable = false;

    // packet handlers

    void HandleInstanceLoadInfo(GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet);
    void HandleJumboMessage(uint8_t type, uint32_t value);
    void HandleAgentProjectileLaunched(const GW::Packet::StoC::AgentProjectileLaunched* packet);

    // generic handlers

    void HandleInterrupted(uint32_t agent_id);
    void HandleKnockedDown(uint32_t agent_id, float duration);
    void HandleAgentState(uint32_t agent_id, uint32_t state);
    void HandleDamageDone(uint32_t caster_id, uint32_t target_id, float amount_pc, bool is_crit);
    void HandleAgentAdd(uint32_t agent_id);

    void HandleAttackFinished(uint32_t agent_id);
    void HandleAttackStopped(uint32_t agent_id);
    void HandleAttackStarted(uint32_t caster_id, uint32_t target_id);

    void HandleInstantSkillActivated(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id);

    void HandleAttackSkillFinished(uint32_t agent_id);
    void HandleAttackSkillStopped(uint32_t agent_id);
    void HandleAttackSkillStarted(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id);

    void HandleSkillFinished(uint32_t agent_id);
    void HandleSkillStopped(uint32_t agent_id);
    void HandleSkillActivated(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id);

    void HandleGenericPacket(uint32_t value_id, uint32_t caster_id,
                             uint32_t target_id, float value, bool no_target);

    void HandleGenericPacket(uint32_t value_id, uint32_t caster_id,
                             uint32_t target_id, uint32_t value, bool no_target);

    // Update the state of the module based on an Action & Stage
    // return false means action was not assigned and may need freeing by the caller
    bool ReduceAction(ObservableAgent* caster, ActionStage stage, TargetAction* new_action = nullptr);

    uint32_t JumboMessageValueToPartyId(uint32_t value);
    void HandleMoraleBoost(ObservableParty* boosting_party);
    void HandleVictory(ObservableParty* winning_party);

    ObservableMap* map;

    // lazy loaded observed guilds
    std::unordered_map<uint32_t, ObservableGuild*> observable_guilds = {};
    std::vector<uint32_t> observable_guild_ids = {};

    // lazy loaded observed agents
    std::unordered_map<uint32_t, ObservableAgent*> observable_agents = {};
    std::vector<uint32_t> observable_agent_ids = {};

    // lazy loaded observed skills
    std::unordered_map<GW::Constants::SkillID, ObservableSkill*> observable_skills = {};
    std::vector<GW::Constants::SkillID> observable_skill_ids = {};

    // lazy loaded observed parties
    std::unordered_map<uint32_t, ObservableParty*> observable_parties = {};
    std::vector<uint32_t> observable_party_ids = {};

    bool SynchroniseParties();

    // hooks
    GW::HookEntry JumboMessage_Entry;
    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry AgentState_Entry;
    GW::HookEntry AgentAdd_Entry;
    GW::HookEntry AgentProjectileLaunched_Entry;
    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericValue_Entry;
    GW::HookEntry GenericFloat_Entry;
};

