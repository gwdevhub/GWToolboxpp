#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include <Timer.h>
#include <Windows/PartyStatisticsWindow.h>

/*************************/
/* Static Helper Methods */
/*************************/

namespace {
    constexpr wchar_t NONE_PLAYER_NAME[] = L"Hero/Henchman Slot";
    constexpr uint32_t NONE_SKILL = static_cast<uint32_t>(GW::Constants::SkillID::No_Skill);
    constexpr wchar_t UNKNOWN_SKILL_NAME[] = L"Unknown Skill";
    constexpr wchar_t UNKNOWN_PLAYER_NAME[] = L"Unknown Player";

    std::map<GW::Constants::SkillID, GuiUtils::EncString*> skill_names;

    GuiUtils::EncString* GetSkillName(const GW::Constants::SkillID skill_id)
    {
        const auto found_it = skill_names.find(skill_id);

        if (found_it == skill_names.end()) {
            const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            ASSERT(skill_data);
            skill_names[skill_id] = new GuiUtils::EncString(skill_data->name);
        }
        return skill_names[skill_id];
    }

    IDirect3DTexture9* GetSkillImage(const GW::Constants::SkillID skill_id)
    {
        return *Resources::GetSkillImage(skill_id);
    }


    struct Skill {
        Skill(const GW::Constants::SkillID _id)
            : id(_id)
        {
            name = GetSkillName(id);
            name->wstring();
        }

        GW::Constants::SkillID id = static_cast<GW::Constants::SkillID>(0);
        uint32_t count = 0;
        GuiUtils::EncString* name = nullptr;
    };

    struct PartyMember {
        PartyMember(const wchar_t* _name_enc, const uint32_t _agent_id, const uint32_t _party_idx)
            : name_enc(_name_enc), agent_id(_agent_id), party_idx(_party_idx)
        {
            name.reset(name_enc.c_str());
            name.wstring();
        }

        std::wstring name_enc;
        uint32_t agent_id = 0;
        uint32_t total_skills_used = 0;
        uint32_t party_idx = 0;
        GuiUtils::EncString name;
        std::vector<Skill> skills{};
    };


    /* Internal data  */
    std::vector<PartyMember*> party_members;
    bool pending_party_members = true;
    bool in_explorable = false;
    PartyMember* player_party_member = nullptr;

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


    const GW::Skillbar* GetAgentSkillbar(const uint32_t agent_id)
    {
        const GW::SkillbarArray* skillbar_array = GW::SkillbarMgr::GetSkillbarArray();

        if (!skillbar_array) {
            return nullptr;
        }

        for (const GW::Skillbar& skillbar : *skillbar_array) {
            if (skillbar.agent_id == agent_id) {
                return &skillbar;
            }
        }

        return nullptr;
    }

    PartyMember* GetPartyMemberByPartyIdx(const uint32_t party_idx)
    {
        if (pending_party_members)
            return nullptr;
        const auto found = std::ranges::find_if(party_members, [party_idx](const auto party_member) {
            return party_member->party_idx == party_idx;
        });
        return found != party_members.end() ? *found : nullptr;
    }

    PartyMember* GetPartyMemberByAgentId(const uint32_t agent_id)
    {
        if (pending_party_members)
            return nullptr;
        // NB: This function is called on the game thread whenever a skill is used. Would it be much performance difference to keep a std::map for this?
        const auto found = std::ranges::find_if(party_members, [agent_id](const auto party_member) {
            return party_member->agent_id == agent_id;
        });
        return found != party_members.end() ? *found : nullptr;
    }
    PartyMember* GetPartyMemberByEncName(const wchar_t* enc_name)
    {
        /* 
        @Cleanup: 
         - What happens when 3 players each bring the same hero ? "Ebon Vanguard Mesmer" x 2
         - GW sometimes sends the enc_name packet after the NPC is created; would this affect henchmen?
         - Players can have names that match heros or henchmen in some edge cases
         - Obfuscator could be a problem

         Instead maybe hook into the PartyPlayerAdd / PartyAllyAdd / PartyHeroAdd packets, then identify by player owner / hero id instead of just using name
         */

        if (pending_party_members)
            return nullptr;
        const auto found = std::ranges::find_if(party_members, [enc_name](const auto party_member) {
            return party_member->name_enc == enc_name;
        });
        return found != party_members.end() ? *found : nullptr;
    }

    int GetSkillString(const std::wstring& agent_name, const std::wstring& skill_name,
                       const uint32_t skill_count, wchar_t* out, const size_t len)
    {
        const auto written = swprintf(out, len, skill_count == 1 ? L"%s used %s %d time." : L"%s used %s %d times.",
                                      agent_name.c_str(), skill_name.c_str(), skill_count);
        ASSERT(written != -1);
        return written;
    }

    void WritePlayerStatisticsSingleSkill(PartyMember* party_member, const uint32_t skill_idx)
    {
        if (!(party_member && skill_idx < party_member->skills.size())) {
            return;
        }

        const Skill& skill = party_member->skills[skill_idx];

        wchar_t chat_str[256];
        GetSkillString(party_member->name.wstring(), skill.name->wstring(), skill.count, chat_str, _countof(chat_str));
        chat_queue.push(chat_str);
    }

    void WritePlayerStatisticsAllSkills(PartyMember* party_member)
    {
        if (!party_member) {
            return;
        }
        for (size_t i = 0; i < party_member->skills.size(); i++) {
            const auto& skill = party_member->skills[i];
            if (0U == skill.count) {
                continue;
            }
            WritePlayerStatisticsSingleSkill(party_member, i);
        }
    }

    /***********************/
    /* Draw Helper Methods */
    /***********************/

    void DrawPartyMember(PartyMember& party_member)
    {
        char header_label[256];
        snprintf(header_label, _countof(header_label), "%s###%u", party_member.name.string().c_str(), party_member.party_idx);

        if (ImGui::CollapsingHeader(header_label)) {
            const float start_y = ImGui::GetCursorPosY();

            char table_name[16];
            snprintf(table_name, _countof(table_name), "###Table%d", party_member.party_idx);

            const float width = ImGui::GetContentRegionAvail().x;
            if (party_member.skills.size() == 0) {
                return;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
            if (ImGui::BeginTable(table_name, party_member.skills.size())) {
                const float column_width = width / party_member.skills.size();
                const float scale = ImGui::GetIO().FontGlobalScale;
                const ImVec2 icon_size = {32.f * scale, 32.f * scale};
                for (size_t i = 0; i < party_member.skills.size(); i++) {
                    char column_name[32];
                    snprintf(column_name, _countof(table_name), "###%sColumn%d", table_name, i);
                    ImGui::TableSetupColumn(column_name, ImGuiTableColumnFlags_WidthStretch, column_width);
                }
                ImGui::TableNextRow();
                for (size_t i = 0; i < party_member.skills.size(); i++) {
                    const Skill& skill = party_member.skills[i];
                    if (skill.id == GW::Constants::SkillID::No_Skill) {
                        continue; // Skip empty skill slots (for heroes and yourself)
                    }
                    ImGui::TableNextColumn();
                    const float percentage = skill.count
                                                 ? static_cast<float>(skill.count) /
                                                   static_cast<float>(party_member.total_skills_used) * 100.f
                                                 : 0.f;
                    if (const auto texture = GetSkillImage(skill.id)) {
                        ImGui::ImageCropped(texture, icon_size);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(skill.name->string().c_str());
                        }
                    }
                    if (show_abs_values) {
                        ImGui::Text("%u", skill.count);
                    }
                    if (show_perc_values) {
                        ImGui::Text("%.2f%%", percentage);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            if (print_by_click) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
                char button_name[32];
                snprintf(button_name, _countof(button_name), "###WriteStatistics%d", party_member.party_idx);
                const float height = ImGui::GetCursorPosY() - start_y;
                ImGui::SetCursorPosY(start_y);
                if (ImGui::Button(button_name, ImVec2(width, height)) && ImGui::IsKeyDown(ImGuiKey_ModCtrl)) {
                    WritePlayerStatisticsAllSkills(&party_member);
                }
                ImGui::PopStyleVar();
            }
        }
    }

    /********************/
    /* Set Data Methods */
    /********************/

    void UnsetPartyStatistics()
    {
        for (const auto entry : party_members) {
            delete entry;
        }
        party_members.clear();

        for (const auto skill_name : skill_names | std::views::values) {
            delete skill_name;
        }
        skill_names.clear();
    }

    bool SetPartyMembers()
    {
        if (!GW::PartyMgr::GetIsPartyLoaded()) {
            return false;
        }
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }

        GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
        if (info == nullptr) {
            return false;
        }

        const uint32_t my_player_id = GW::Agents::GetPlayerId();
        const GW::Skillbar* my_skillbar = GetAgentSkillbar(my_player_id);
        if (!my_skillbar) {
            return false;
        }
        size_t party_idx = 0;

        player_party_member = nullptr;

        std::vector<PartyMember*> valid_party_members;

        auto set_party_member = [&valid_party_members,&party_idx](const uint32_t agent_id) {
            const wchar_t* agent_name = GW::Agents::GetAgentEncName(agent_id);
            if (!agent_name) {
                return static_cast<PartyMember*>(nullptr); // Can fail if game hasn't got all the goodies yet
            }
            // NB: Sanitising removes [henchman type] and player numbers
            const auto sanitised = GuiUtils::SanitizePlayerName(agent_name);
            auto party_member = GetPartyMemberByEncName(sanitised.c_str());
            if (!party_member) {
                party_member = new PartyMember(sanitised.c_str(), agent_id, party_idx);
                party_members.push_back(party_member);
            }
            valid_party_members.push_back(party_member);
            party_member->party_idx = party_idx;
            party_member->agent_id = agent_id;
            party_idx++;
            return party_member;
        };
        auto set_member_skill = [](PartyMember* party_member, const GW::Constants::SkillID skill_id) {
            auto& existing_skills = party_member->skills;
            for (const auto& skill : existing_skills) {
                if (skill.id == skill_id) {
                    return;
                }
            }
            existing_skills.push_back(skill_id);
        };

        for (const GW::PlayerPartyMember& player : info->players) {
            const auto agent_id = GW::Agents::GetAgentIdByLoginNumber(player.login_number);

            auto party_member = set_party_member(agent_id);
            if (!party_member) {
                return false;
            }

            if (agent_id == my_player_id) {
                player_party_member = party_member;
            }
            if (!info->heroes.valid()) {
                continue;
            }
            for (const GW::HeroPartyMember& hero : info->heroes) {
                if (hero.owner_player_id != player.login_number) {
                    continue;
                }

                party_member = set_party_member(hero.agent_id);
                if (!party_member) {
                    return false;
                }
                const GW::Skillbar* skillbar = GetAgentSkillbar(hero.agent_id);
                if (!skillbar) {
                    continue;
                }
                /* Skillbar for other players and henchmen is unknown in outpost init with No_Skill */

                for (const GW::SkillbarSkill& skill : skillbar->skills) {
                    set_member_skill(party_member, skill.skill_id);
                }
            }
        }
        if (info->henchmen.valid()) {
            for (const GW::HenchmanPartyMember& hench : info->henchmen) {
                set_party_member(hench.agent_id);
            }
        }
        ASSERT(player_party_member);

        // Add player skills
        for (const GW::SkillbarSkill& skill : my_skillbar->skills) {
            set_member_skill(player_party_member, skill.skill_id);
        }

        // Clear out any party members that are no longer in the party.
        auto it = party_members.begin();
        while (it != party_members.end()) {
            const auto found = std::ranges::find_if(valid_party_members, [it](const auto valid) {
                return valid == *it;
            });
            if (found == valid_party_members.end()) {
                party_members.erase(it);
                it = party_members.begin();
            }
            else {
                ++it;
            }
        }

        return true;
    }

    /************************/
    /* Chat Command Methods */
    /************************/

    void WritePlayerStatistics(const uint32_t player_idx = -1, const uint32_t skill_idx = -1)
    {
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) {
            return;
        }

        /* all skills for self player */
        if (static_cast<size_t>(-1) == player_idx) {
            WritePlayerStatisticsAllSkills(player_party_member);
            /* single skill for some player */
        }
        else if (static_cast<uint32_t>(-1) != skill_idx) {
            WritePlayerStatisticsSingleSkill(GetPartyMemberByPartyIdx(player_idx), skill_idx);
            /* all skills for some player */
        }
        else {
            WritePlayerStatisticsAllSkills(GetPartyMemberByPartyIdx(player_idx));
        }
    }

    void CmdSkillStatistics(const wchar_t*, const int argc, const LPWSTR* argv)
    {
        /* command: /skillstats */
        /* will write the stats of the self player */
        if (argc < 2) {
            WritePlayerStatistics();
            return;
        }

        const std::wstring arg1 = GuiUtils::ToLower(argv[1]);

        if (argc == 2) {
            /* command: /skillstats reset */
            if (arg1 == L"reset") {
                UnsetPartyStatistics();
                pending_party_members = true;
            }
            /* command: /skllstats playerNum */
            else {
                uint32_t player_number = 0;
                if (GuiUtils::ParseUInt(argv[1], &player_number) && player_number > 0 &&
                    player_number <= party_members.size()) {
                    --player_number; // List will start at index zero
                    WritePlayerStatistics(player_number);
                }
                else {
                    Log::Error("Invalid player number '%ls', please use an integer value of 1 to %u", argv[1],
                               party_members.size() + 1);
                }
            }

            return;
        }

        /* command: /skillstats playerNum skillNum */
        if (argc >= 3) {
            uint32_t player_number = 0;
            if (GuiUtils::ParseUInt(argv[1], &player_number) && player_number > 0 &&
                player_number <= party_members.size()) {
                --player_number;
                uint32_t skill_number = 0;
                if (GuiUtils::ParseUInt(argv[2], &skill_number) && skill_number > 0 && skill_number < 9) {
                    --skill_number;
                    WritePlayerStatistics(player_number, skill_number);
                }
                else {
                    Log::Error("Invalid skill number '%ls', please use an integer value of 1 to 8", argv[2]);
                }
            }
            else {
                Log::Error("Invalid player number '%ls', please use an integer value of 1 to %u", argv[1],
                           party_members.size());
            }
        }
    }

    /********************/
    /* Callback Methods */
    /********************/

    void MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded*)
    {
        if (!in_explorable) {
            // Just left an outpost.
            UnsetPartyStatistics();
        }
        pending_party_members = true;
        in_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
                       const uint32_t value, const bool no_target)
    {
        uint32_t agent_id = caster_id;
        const auto activated_skill_id = static_cast<GW::Constants::SkillID>(value);

        switch (value_id) {
            case GW::Packet::StoC::GenericValueID::instant_skill_activated:
            case GW::Packet::StoC::GenericValueID::skill_activated:
            case GW::Packet::StoC::GenericValueID::skill_finished:
            case GW::Packet::StoC::GenericValueID::attack_skill_activated:
            case GW::Packet::StoC::GenericValueID::attack_skill_finished: {
                if (!no_target) {
                    agent_id = target_id;
                }
                break;
            }
            default: {
                return;
            }
        }

        if (NONE_SKILL == value) {
            return;
        }

        Skill* found_skill = nullptr;
        const auto party_member = GetPartyMemberByAgentId(agent_id);
        if (!party_member) {
            return;
        }
        for (auto& skill : party_member->skills) {
            if (skill.id == activated_skill_id) {
                found_skill = &skill;
                break;
            }
        }
        if (!found_skill) {
            party_member->skills.push_back(activated_skill_id);
            found_skill = &party_member->skills.back();
        }
        party_member->total_skills_used++;
        found_skill->count++;
    }
} // namespace


/**********************/
/* Overridden Methods */
/**********************/

void PartyStatisticsWindow::Initialize()
{
    ToolboxWindow::Initialize();

    send_timer = TIMER_INIT();

    GW::Chat::CreateCommand(L"skillstats", CmdSkillStatistics);

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, &MapLoadedCallback);

    /* Skill on self or party player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet) -> void {
            const uint32_t value_id = packet->value_id;
            const uint32_t caster_id = packet->agent_id;
            constexpr uint32_t target_id = 0U;
            const uint32_t value = packet->value;
            constexpr bool no_target = true;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    /* Skill on enemy player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(
        &GenericValueTarget_Entry,
        [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* packet) -> void {
            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            constexpr bool no_target = false;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    UnsetPartyStatistics();
    pending_party_members = true;
}

void PartyStatisticsWindow::Update(const float)
{
    if (pending_party_members && SetPartyMembers()) {
        pending_party_members = false;
    }

    if (constexpr auto time_diff_threshold = 600.0f; !chat_queue.empty() && TIMER_DIFF(send_timer) > time_diff_threshold) {
        send_timer = TIMER_INIT();
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) {
            return;
        }
        if (!GW::Agents::GetPlayer()) {
            return;
        }

        GW::Chat::SendChat('#', chat_queue.front().c_str());
        chat_queue.pop();
    }
}

void PartyStatisticsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    if (!GW::Map::GetIsMapLoaded()) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (!in_explorable) {
            ImGui::TextDisabled("Statistics will update in explorable area");
        }
        for (const auto party_member : party_members) {
            DrawPartyMember(*party_member);
        }
    }
    ImGui::End();
}

void PartyStatisticsWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(show_abs_values);
    LOAD_BOOL(show_perc_values);
    LOAD_BOOL(print_by_click);
}

void PartyStatisticsWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(show_abs_values);
    SAVE_BOOL(show_perc_values);
    SAVE_BOOL(print_by_click);
}

void PartyStatisticsWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Show the absolute skill count", &show_abs_values);
    ImGui::SameLine();
    ImGui::Checkbox("Show the percentage skill count", &show_perc_values);
    ImGui::SameLine();
    ImGui::Checkbox("Print skill statistics by Ctrl+LeftClick", &print_by_click);
}

void PartyStatisticsWindow::Terminate()
{
    ToolboxWindow::Terminate();

    GW::Chat::DeleteCommand(L"skillstats");

    GW::StoC::RemoveCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GenericValue>(&GenericValueSelf_Entry);

    UnsetPartyStatistics();
}
