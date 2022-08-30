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
    PartyStatisticsWindow() = default;
    ~PartyStatisticsWindow();

protected:
    struct Skill {
        Skill(GW::Constants::SkillID _id) : id(_id) {
            name = Instance().GetSkillName(id);
            name->wstring();
        }
        GW::Constants::SkillID id = (GW::Constants::SkillID)0;
        uint32_t count = 0;
        GuiUtils::EncString* name = 0;
    };

    struct PartyMember {
        PartyMember(uint32_t _agent_id) : agent_id(_agent_id) {
            name = Instance().GetAgentName(agent_id);
            name->wstring();
        }
        uint32_t agent_id = 0;
        uint32_t total_skills_used = 0;
        GuiUtils::EncString* name = 0;
        std::vector<Skill> skills;
    };

    const GW::Skillbar* GetAgentSkillbar(const uint32_t agent_id);
    GuiUtils::EncString* GetSkillName(const GW::Constants::SkillID skill_id);
    GuiUtils::EncString* GetAgentName(const uint32_t agent_id);
    IDirect3DTexture9* GetSkillImage(const GW::Constants::SkillID skill_id);
    int GetSkillString(
        const std::wstring& agent_name, const std::wstring& skill_name, const uint32_t skill_count, wchar_t* out, size_t len);

public:

    static PartyStatisticsWindow& Instance() {
        static PartyStatisticsWindow instance;
        return instance;
    }

    const char* Name() const override { return "Party Statistics"; }
    const char8_t* Icon() const override { return ICON_FA_TABLE; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    static void CmdSkillStatistics(const wchar_t *message, int argc, LPWSTR *argv);
    void UnsetPartyStatistics();

    static void MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);

private:
    void DrawPartyMember(const size_t party_idx);
    void WritePlayerStatistics(const uint32_t player_idx = -1, const uint32_t skill_idx = -1);
    void WritePlayerStatisticsAllSkills(const uint32_t player_idx);
    void WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx);
    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
        const uint32_t value, const bool no_target);

    bool SetPartyMembers();

    /* Internal data  */
    size_t max_player_skills = 8;
    bool pending_party_members = true;
    bool in_explorable = false;
    uint32_t player_party_idx = 0xff;
    std::vector<PartyMember> party_members;
    struct PendingSkillIcon {
        bool loading = false;
        IDirect3DTexture9* texture = 0;
    };
    std::map<uint32_t, PendingSkillIcon*> skill_images;
    std::map<GW::Constants::SkillID, GuiUtils::EncString*> skill_names;
    std::map<GW::AgentID, GuiUtils::EncString*> agent_names;

    /* Chat messaging */
    clock_t send_timer = 0;
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
