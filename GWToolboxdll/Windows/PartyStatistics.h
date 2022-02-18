#pragma once

#include <Timer.h>
#include <array>
#include <map>
#include <queue>
#include <string>
#include <utility>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxWindow.h>

class PartyStatisticsWindow : public ToolboxWindow {
public:
    static constexpr uint32_t MAX_NUM_SKILLS = 8;

    typedef struct {
        uint32_t skill_id;
        uint32_t skill_count;
    } SkillCount;

    using SkillCounts = std::array<SkillCount, MAX_NUM_SKILLS>;

    typedef struct {
        uint32_t agent_id;
        SkillCounts skill_counts;
    } PlayerSkillCounts;

    using PartySkillCounts = std::vector<PlayerSkillCounts>;
    using PartyIndicies = std::map<uint32_t, size_t>;

    PartyStatisticsWindow()
        : party_stats(PartySkillCounts{})
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

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawPartyMember(const PlayerSkillCounts& party_member_stats);
    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    static void MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded* packet);
    static void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
        const uint32_t value, const bool no_target);

    void WritePlayerStatistics(const uint32_t player_idx, const uint32_t skill_idx = -1, const bool full_info = false);
    void WritePartyStatistics();
    void ResetPlayerStatistics();

private:
    static const GW::Skillbar* GetPlayerSkillbar(const uint32_t player_id);
    static void GetSkillName(const uint32_t skill_id, char* skill_name);
    static void GetPlayerName(const GW::Agent* const agent, char* agent_name);

    void ClearPartyIndicies();
    void ClearPartyStats();
    void ClearCallback();

    void SetPartyIndicies();
    void SetPartyStats();
    void SetPartySkills();
    void SetPartyData();

    GW::HookEntry MapLoaded_Entry;
    GW::HookEntry GenericValue_Entry;
    GW::HookEntry GenericValueTarget_Entry;

    PartyIndicies party_indicies;
    PartySkillCounts party_stats;

    clock_t send_timer;
    std::queue<std::wstring> chat_queue;

    bool show_abs_values = true;
    bool show_perc_values = true;
};
