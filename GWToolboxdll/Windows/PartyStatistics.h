#pragma once

#include <Timer.h>
#include <array>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxWindow.h>

class PartyStatisticsWindow : public ToolboxWindow {
public:
    static constexpr float TIME_DIFF_THRESH = 600.0F;
    static constexpr uint32_t MAX_NUM_SKILLS = 8;
    static constexpr uint32_t NONE_SKILL = static_cast<uint32_t>(GW::Constants::SkillID::No_Skill);

    using PartyIds = std::set<uint32_t>;
    using PartyIndicies = std::map<uint32_t, size_t>;
    using PartyNames = std::map<uint32_t, std::string>;

    typedef struct {
        uint32_t id;
        uint32_t count;
        std::string name;
    } Skill;

    using Skills = std::array<Skill, MAX_NUM_SKILLS>;

    typedef struct {
        uint32_t agent_id;
        Skills skills;
    } PlayerSkills;

    using PartySkills = std::vector<PlayerSkills>;

    PartyStatisticsWindow()
        : party_ids(PartyIds{})
        , party_skills(PartySkills{})
        , party_indicies(PartyIndicies{})
        , party_names(PartyNames{})
        , chat_queue(std::queue<std::wstring>{})
        , send_timer(clock_t{}){};
    ~PartyStatisticsWindow(){};

    static PartyStatisticsWindow& Instance() {
        static PartyStatisticsWindow instance;
        return instance;
    }

    const char* Name() const override { return "PartyStatistics"; }
    const char* Icon() const override { return ICON_FA_TABLE; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawPartyMember(const PlayerSkills& party_member_stats);
    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    void MapLoadedCallback();
    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
        const uint32_t value, const bool no_target);

    void WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx);
    void WritePlayerStatisticsAllSkillsFullInfo(const uint32_t player_idx);
    void WritePlayerStatisticsAllSkills(const uint32_t player_idx);
    void WritePlayerStatistics(const uint32_t player_idx, const uint32_t skill_idx = -1, const bool full_info = false);
    void WritePartyStatistics();

    void UnsetPlayerStatistics();

private:
    static const GW::Skillbar* GetPlayerSkillbar(const uint32_t player_id);
    static void GetSkillName(const uint32_t id, char* const skill_name);
    static void GetPlayerName(const GW::Agent* const agent, char* const agent_name);
    static std::string GetSkillFullInfoString(
        const std::string agent_name, const std::string skill_name, const uint32_t skill_count);

    bool PartySizeChanged();

    void SetPlayerStatistics();

    void SetPartyIndicies();
    void SetPartyNames();
    void SetPartyStats();

    bool in_explorable = false;

    GW::HookEntry MapLoaded_Entry;
    GW::HookEntry GenericValueSelf_Entry;
    GW::HookEntry GenericValueTarget_Entry;

    PartyIds party_ids;
    PartyIndicies party_indicies;
    PartyNames party_names;
    PartySkills party_skills;

    clock_t send_timer;
    std::queue<std::wstring> chat_queue;

    bool show_abs_values = true;
    bool show_perc_values = true;
};
