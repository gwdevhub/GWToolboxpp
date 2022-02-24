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


    using PartyIds = std::set<uint32_t>;
    using PartyIndicies = std::map<uint32_t, size_t>;
    using PartyNames = std::map<uint32_t, std::wstring*>;

    struct Skill {
        uint32_t id = 0;
        uint32_t count = 0;
        std::wstring* name = 0;
    };

    using Skills = std::vector<Skill>;

    struct PlayerSkills {
        uint32_t agent_id;
        Skills skills;
    };

    using PartySkills = std::vector<PlayerSkills>;

    static const GW::Skillbar* GetPlayerSkillbar(const uint32_t player_id);
    std::wstring& GetSkillName(const uint32_t skill_id);
    static std::wstring& GetPlayerName(uint32_t agent_id);
    static std::wstring GetSkillString(
        const std::wstring& agent_name, const std::wstring& skill_name, const uint32_t& skill_count);

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
    ~PartyStatisticsWindow(){
        UnsetPlayerStatistics();
    };

    static PartyStatisticsWindow& Instance() {
        static PartyStatisticsWindow instance;
        return instance;
    }

    const char* Name() const override { return "Party Statistics"; }
    const char* Icon() const override { return ICON_FA_TABLE; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    static void CmdSkillStatistics(const wchar_t *message, int argc, LPWSTR *argv);
    void UnsetPlayerStatistics();

    static void MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);

private:
    void DrawPartyMember(const PlayerSkills& player_stats, const size_t party_idx);
    void WritePlayerStatistics(const uint32_t player_idx = -1, const uint32_t skill_idx = -1);
    void WritePlayerStatisticsAllSkills(const uint32_t player_idx);
    void WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx);

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

    std::map<uint32_t, GuiUtils::EncString*> skill_names;
    std::map<GW::AgentID, GuiUtils::EncString*> agent_names;

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
