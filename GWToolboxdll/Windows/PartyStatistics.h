#pragma once

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

#include <Timer.h>
#include <ToolboxWindow.h>

class PartyStatisticsWindow : public ToolboxWindow {
protected:
    static constexpr auto MAX_NUM_SKILLS = size_t{8};
    static constexpr auto NONE_PLAYER_NAME = L"Hero/Henchman Slot";
    static constexpr auto NONE_SKILL = static_cast<uint32_t>(GW::Constants::SkillID::No_Skill);
    static constexpr auto NONE_SKILL_NAME = L"Unknown Skill";
    static constexpr auto BUFFER_LENGTH = size_t{256};

    using PartyIds = std::set<uint32_t>;
    using PartyIndicies = std::map<uint32_t, size_t>;
    using PartyNames = std::map<uint32_t, std::wstring>;

    typedef struct {
        uint32_t id;
        uint32_t count;
        std::wstring name;
    } Skill;

    using Skills = std::array<Skill, MAX_NUM_SKILLS>;

    typedef struct {
        uint32_t agent_id;
        Skills skills;
    } PlayerSkills;

    using PartySkills = std::vector<PlayerSkills>;

    static const GW::Skillbar* GetPlayerSkillbar(const uint32_t player_id);
    static void GetSkillName(const uint32_t id, wchar_t* const skill_name);
    static void GetPlayerName(const GW::Agent* const agent, wchar_t* const agent_name);
    static std::wstring GetSkillString(
        const std::wstring agent_name, const std::wstring& skill_name, const uint32_t& skill_count);

public:
    PartyStatisticsWindow()
        : in_explorable(false)
        , party_size(size_t{})
        , self_party_idx(static_cast<size_t>(-1))
        , party_ids(PartyIds{})
        , party_indicies(PartyIndicies{})
        , party_names(PartyNames{})
        , party_stats(PartySkills{})
        , send_timer(clock_t{})
        , chat_queue(std::queue<std::wstring>{}){};
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
    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    void WritePlayerStatistics(const uint32_t player_idx = -1, const uint32_t skill_idx = -1);
    void UnsetPlayerStatistics();

private:
    void DrawPartyMember(const PlayerSkills& player_stats, const size_t party_idx);
    void WritePlayerStatisticsAllSkills(const uint32_t player_idx);
    void WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx);

    void MapLoadedCallback();
    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
        const uint32_t value, const bool no_target);
    bool PartySizeChanged();

    void SetPlayerStatistics();
    void SetPartyIndicies();
    void SetPartyNames();
    void SetPartySkills();

    /* Internal data  */
    bool in_explorable;
    size_t party_size;
    size_t self_party_idx;
    PartyIds party_ids;
    PartyIndicies party_indicies;
    PartyNames party_names;
    PartySkills party_stats;

    /* Chat messaging */
    clock_t send_timer;
    std::queue<std::wstring> chat_queue;

    /* Callbacks */
    GW::HookEntry MapLoaded_Entry;
    GW::HookEntry GenericValueSelf_Entry;
    GW::HookEntry GenericValueTarget_Entry;

    /* Window settings */
    bool show_abs_values = true;
    bool show_perc_values = true;
    bool print_by_click = true;
};
