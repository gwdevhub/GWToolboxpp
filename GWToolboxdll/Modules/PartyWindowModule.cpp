#include "stdafx.h"

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/GameContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Timer.h>
#include <Defines.h>
#include <Modules/PartyWindowModule.h>
#include <Windows/FriendListWindow.h>
#include "Resources.h"
#include <GWCA/GameEntities/Frame.h>
#include <Widgets/SnapsToPartyWindow.h>
#include <Utils/ToolboxUtils.h>

namespace {
    std::map<uint32_t, GW::Constants::SkillID> summon_elites = {
        {GW::Constants::ModelID::SummoningStone::ImperialCripplingSlash, GW::Constants::SkillID::Crippling_Slash},
        {GW::Constants::ModelID::SummoningStone::ImperialTripleChop, GW::Constants::SkillID::Triple_Chop},
        {GW::Constants::ModelID::SummoningStone::ImperialBarrage, GW::Constants::SkillID::Barrage},
        {GW::Constants::ModelID::SummoningStone::ImperialQuiveringBlade, GW::Constants::SkillID::Quivering_Blade},
        {GW::Constants::ModelID::SummoningStone::TenguHundredBlades, GW::Constants::SkillID::Hundred_Blades},
        {GW::Constants::ModelID::SummoningStone::TenguBroadHeadArrow, GW::Constants::SkillID::Broad_Head_Arrow},
        {GW::Constants::ModelID::SummoningStone::TenguPalmStrike, GW::Constants::SkillID::Palm_Strike},
        {GW::Constants::ModelID::SummoningStone::TenguLifeSheath, GW::Constants::SkillID::Life_Sheath},
        {GW::Constants::ModelID::SummoningStone::TenguAngchuElementalist, GW::Constants::SkillID::No_Skill},
        {GW::Constants::ModelID::SummoningStone::TenguFeveredDreams, GW::Constants::SkillID::Fevered_Dreams},
        {GW::Constants::ModelID::SummoningStone::TenguSpitefulSpirit, GW::Constants::SkillID::Spiteful_Spirit},
        {GW::Constants::ModelID::SummoningStone::TenguPreservation, GW::Constants::SkillID::Preservation},
        {GW::Constants::ModelID::SummoningStone::TenguPrimalRage, GW::Constants::SkillID::Primal_Rage},
        {GW::Constants::ModelID::SummoningStone::TenguGlassArrows, GW::Constants::SkillID::Glass_Arrows},
        {GW::Constants::ModelID::SummoningStone::TenguWayOftheAssassin, GW::Constants::SkillID::Way_of_the_Assassin},
        {GW::Constants::ModelID::SummoningStone::TenguPeaceandHarmony, GW::Constants::SkillID::Peace_and_Harmony},
        {GW::Constants::ModelID::SummoningStone::TenguSandstorm, GW::Constants::SkillID::Sandstorm},
        {GW::Constants::ModelID::SummoningStone::TenguPanic, GW::Constants::SkillID::Panic},
        {GW::Constants::ModelID::SummoningStone::TenguAuraOftheLich, GW::Constants::SkillID::Aura_of_the_Lich},
        {GW::Constants::ModelID::SummoningStone::TenguDefiantWasXinrae, GW::Constants::SkillID::Defiant_Was_Xinrae},
    };

    struct PendingAddToParty {
        PendingAddToParty(const uint32_t _agent_id, const uint32_t _allegiance_bits, const uint32_t _player_number) : agent_id(_agent_id), player_number(_player_number), allegiance_bits(_allegiance_bits) { add_timer = TIMER_INIT(); }

        clock_t add_timer;
        uint32_t agent_id;
        uint32_t player_number;
        uint32_t allegiance_bits;
        GW::AgentLiving* GetAgent() const;
    };

    struct SpecialNPCToAdd {
        SpecialNPCToAdd(const char* _alias, const int _model_id, const GW::Constants::MapID _map_id) : alias(_alias), model_id(static_cast<uint32_t>(_model_id)), map_id(_map_id) {};
        std::string alias;
        const char* GetMapName() { return map_id > GW::Constants::MapID::None ? Resources::GetMapName(map_id)->string().c_str() : "Any"; }
        uint32_t model_id = 0;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
    };

    struct SummonPending {
        uint32_t agent_id;
        GW::Constants::SkillID skill_id;
    };

    bool custom_sort_party_window = false;
    bool add_npcs_to_party_window = true; // Quick tickbox to disable the module without restarting TB
    bool add_player_numbers_to_party_window = false;
    bool add_elite_skill_to_summons = false;
    bool remove_dead_imperials = false;

    char new_npc_alias[128] = {0};
    int new_npc_model_id = 0;
    int new_npc_map_id = 0;
    bool map_name_to_translate = true;
    size_t pending_clear = 0;

    GW::HookEntry AgentState_Entry;
    GW::HookEntry AgentRemove_Entry;
    GW::HookEntry AgentAdd_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry GameThreadCallback_Entry;

    GW::HookEntry Summon_AgentAdd_Entry;
    GW::HookEntry Summon_GameThreadCallback_Entry;

    std::vector<uint32_t> allies_added_to_party;
    std::vector<PendingAddToParty> pending_add;
    std::queue<uint32_t> pending_remove;
    std::vector<uint32_t> removed_canthans;
    std::queue<SummonPending> summons_pending;
    std::vector<SpecialNPCToAdd*> user_defined_npcs;
    std::map<uint32_t, SpecialNPCToAdd*> user_defined_npcs_by_model_id;
    std::map<GW::Constants::MapID, std::wstring> map_names_by_id;

    bool is_explorable = false;

    bool IsPvE()
    {
        const GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        if (!map) {
            return false;
        }
        switch (map->type) {
            case GW::RegionType::AllianceBattle:
            case GW::RegionType::Arena:
            case GW::RegionType::GuildBattleArea:
            case GW::RegionType::CompetitiveMission:
            case GW::RegionType::ZaishenBattle:
            case GW::RegionType::HeroesAscent:
            case GW::RegionType::HeroBattleArea:
                return false;
            default:
                return true;
        }
    }

    struct PartyInfo : GW::PartyInfo {
        size_t GetPartySize() const { return players.size() + henchmen.size() + heroes.size(); }
    };

    PartyInfo* GetPartyInfo(const uint32_t party_id = 0)
    {
        if (!party_id) {
            return static_cast<PartyInfo*>(GW::PartyMgr::GetPartyInfo());
        }
        GW::PartyContext* p = GW::GetGameContext()->party;
        if (!p || !p->parties.valid() || party_id >= p->parties.size()) {
            return nullptr;
        }
        return static_cast<PartyInfo*>(p->parties[party_id]);
    }

    void SetPlayerNumber(wchar_t* player_name, const uint32_t player_number)
    {
        wchar_t buf[32] = {0};
        swprintf(buf, 32, L"%s (%d)", player_name, player_number);
        if (wcsncmp(player_name, buf, wcslen(buf)) != 0) {
            wcscpy(player_name, buf);
        }
    }


    bool SetAgentName(const uint32_t agent_id, const wchar_t* name)
    {
        const auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!a || !name) {
            return false;
        }
        const wchar_t* current_name = GW::Agents::GetAgentEncName(a);
        if (!current_name) {
            return false;
        }
        if (wcscmp(name, current_name) == 0) {
            return true;
        }
        GW::Packet::StoC::AgentName packet;
        packet.header = GW::Packet::StoC::AgentName::STATIC_HEADER;
        packet.agent_id = agent_id;
        wcscpy(packet.name_enc, name);
        GW::StoC::EmulatePacket(&packet);
        return true;
    }

    bool IsPvP()
    {
        const GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
        if (!i) {
            return false;
        }
        switch (i->type) {
            case GW::RegionType::AllianceBattle:
            case GW::RegionType::Arena:
            case GW::RegionType::CompetitiveMission:
            case GW::RegionType::GuildBattleArea:
            case GW::RegionType::HeroBattleArea:
            case GW::RegionType::HeroesAscent:
            case GW::RegionType::ZaishenBattle:
                return true;
            default:
                return false;
        }
    }

    void AddSpecialNPC(const SpecialNPCToAdd& npc)
    {
        auto new_npc = new SpecialNPCToAdd(npc);
        user_defined_npcs.push_back(new_npc);
        user_defined_npcs_by_model_id.emplace(npc.model_id, new_npc);
    }

    void RemoveSpecialNPC(const uint32_t model_id)
    {
        user_defined_npcs_by_model_id.erase(model_id);
        for (auto& user_defined_npc : user_defined_npcs) {
            if (!user_defined_npc) {
                continue;
            }
            if (user_defined_npc->model_id == model_id) {
                delete user_defined_npc;
                user_defined_npc = nullptr;
                // Don't actually call erase() because its mad dodgy, but set to nullptr instead.
                break;
            }
        }
    }

    void ClearSpecialNPCs()
    {
        user_defined_npcs_by_model_id.clear();
        for (const auto& user_defined_npc : user_defined_npcs) {
            if (!user_defined_npc) {
                continue;
            }
            delete user_defined_npc;
        }
        user_defined_npcs.clear();
    }

    void ClearAddedAllies()
    {
        for (const auto ally_id : allies_added_to_party) {
            pending_remove.push(ally_id);
        }
    }

    bool ShouldAddAgentToPartyWindow(const uint32_t agent_type)
    {
        if ((agent_type & 0x20000000) == 0) {
            return false; // Not an NPC
        }
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return false; // Not in an explorable area
        }
        const GW::Constants::MapID map_id = GW::Map::GetMapID();
        const uint32_t player_number = agent_type ^ 0x20000000;
        const auto it = user_defined_npcs_by_model_id.find(player_number);
        if (it == user_defined_npcs_by_model_id.end()) {
            return false;
        }
        if (it->second->map_id != GW::Constants::MapID::None && it->second->map_id != map_id) {
            return false;
        }
        return true;
    }

    bool ShouldAddAgentToPartyWindow(GW::Agent* _a)
    {
        const GW::AgentLiving* a = _a ? _a->GetAsAgentLiving() : nullptr;
        if (!a || !a->IsNPC()) {
            return false;
        }
        if (a->GetIsDead() || a->GetIsDeadByTypeMap() || a->allegiance == GW::Constants::Allegiance::Enemy) {
            return false; // Dont add dead NPCs.
        }
        if (std::ranges::contains(allies_added_to_party, a->agent_id)) {
            return false;
        }
        return ShouldAddAgentToPartyWindow(0x20000000u | a->player_number);
    }

    void CheckMap()
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return;
        }
        if (!add_npcs_to_party_window) {
            ClearAddedAllies();
            return;
        }
        GW::AgentArray* agents_ptr = GW::Agents::GetAgentArray();
        if (!agents_ptr) {
            return;
        }
        GW::AgentArray& agents = *agents_ptr;
        for (const auto ally_id : allies_added_to_party) {
            if (ally_id >= agents.size()) {
                pending_remove.push(ally_id);
                continue;
            }
            const GW::AgentLiving* a = agents[ally_id] ? agents[ally_id]->GetAsAgentLiving() : nullptr;
            if (!a || !a->IsNPC()) {
                pending_remove.push(ally_id);
                continue;
            }
            const auto it = user_defined_npcs_by_model_id.find(a->player_number);
            if (it != user_defined_npcs_by_model_id.end()) {
                continue;
            }
            pending_remove.push(ally_id);
        }
        for (const auto& agent : agents) {
            GW::AgentLiving* a = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!a || !ShouldAddAgentToPartyWindow(a)) {
                continue;
            }
            pending_add.emplace_back(a->agent_id, 0, a->player_number);
        }
    }

    bool ShouldRemoveAgentFromPartyWindow(const uint32_t agent_id)
    {
        const auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!a || !a->GetIsLivingType() || !(a->type_map & 0x20000)) {
            return false; // Not in party window
        }
        for (const unsigned int i : allies_added_to_party) {
            if (a->agent_id != i) {
                continue;
            }
            // Ally turned enemy, or is dead.
            return a->allegiance == GW::Constants::Allegiance::Enemy || a->GetIsDead() || a->GetIsDeadByTypeMap();
        }
        return false;
    }

    void RemoveAllyActual(const uint32_t agent_id)
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            const auto it = std::ranges::find(allies_added_to_party, agent_id);
            if (it != allies_added_to_party.end()) {
                allies_added_to_party.erase(it);
            }
            return;
        }
        GW::Packet::StoC::PartyRemoveAlly packet;
        packet.header = GW::Packet::StoC::PartyRemoveAlly::STATIC_HEADER;
        packet.agent_id = agent_id;

        const auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        wchar_t prev_name[8] = {0};
        if (a) {
            wcscpy(prev_name, GW::Agents::GetAgentEncName(a));
        }
        // 1. Remove NPC from window.
        GW::StoC::EmulatePacket(&packet);
        SetAgentName(agent_id, prev_name);
        const auto it = std::ranges::find(allies_added_to_party, agent_id);
        if (it != allies_added_to_party.end()) {
            allies_added_to_party.erase(it);
        }
    }

    void AddAllyActual(const PendingAddToParty& p)
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return;
        }
        const GW::AgentLiving* a = p.GetAgent();
        if (!a || a->GetIsDead() || a->GetIsDeadByTypeMap()) {
            return;
        }
        wchar_t prev_name[8] = {0};
        wcscpy(prev_name, GW::Agents::GetAgentEncName(a));
        GW::Packet::StoC::PartyAllyAdd packet;

        packet.header = GW::Packet::StoC::PartyAllyAdd::STATIC_HEADER;
        packet.agent_id = p.agent_id;
        packet.agent_type = p.player_number | 0x20000000;
        packet.allegiance_bits = 1886151033;

        // 1. Remove NPC from window.
        GW::StoC::EmulatePacket(&packet);
        SetAgentName(p.agent_id, prev_name);

        allies_added_to_party.push_back(p.agent_id);
    }

    GW::AgentLiving* PendingAddToParty::GetAgent() const
    {
        auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!a || !a->GetIsLivingType() || a->player_number != player_number || a->allegiance == GW::Constants::Allegiance::Enemy) {
            return nullptr;
        }
        const GW::NPC* npc = GW::Agents::GetNPCByID(player_number);
        if (!npc) {
            return nullptr;
        }
        return a;
    }

    struct PartySorting {
        GW::Constants::MapID map_id;
        uint32_t party_size;
        std::vector<uint16_t> sorting_by_profession; // This is (uint8_t)primary | (uint8_t)secondary
    };

    std::vector<PartySorting> party_sortings;

    PartySorting* chosen_sorting_vector = 0;

    int PartySortHandler(uint32_t frame_id_1, uint32_t frame_id_2)
    {
        if (!chosen_sorting_vector) return 0;

        struct PlayerFrameContext {
            uint32_t h0000[0x13];
        };

        auto ctx = (PlayerFrameContext*)GW::UI::GetFrameContext(GW::UI::GetFrameById(frame_id_1));
        const auto player_number_1 = ctx->h0000[9];
        ctx = (PlayerFrameContext*)GW::UI::GetFrameContext(GW::UI::GetFrameById(frame_id_2));
        const auto player_number_2 = ctx->h0000[9];

        const auto p1 = (GW::AgentLiving*)GW::Agents::GetPlayerByID(player_number_1);
        const auto p2 = (GW::AgentLiving*)GW::Agents::GetPlayerByID(player_number_2);

        if (!p1 || !p2) return 0;

        const auto sorting_size = chosen_sorting_vector->sorting_by_profession.size();

        auto CalcSortPos = [sorting_size](GW::AgentLiving* agent, size_t* sort_pos_out, int* match_quality_out) {
            for (size_t i = 0; i < sorting_size; i++) {
                uint8_t sorting_primary = chosen_sorting_vector->sorting_by_profession[i] >> 8;
                uint8_t sorting_secondary = chosen_sorting_vector->sorting_by_profession[i] & 0xff;

                int match_quality = 0;
                if (sorting_primary != 0 && sorting_primary == agent->primary) {
                    match_quality++; // Primary match is worth 2 points
                }
                if (sorting_secondary != 0 && sorting_secondary == agent->secondary) {
                    match_quality++; // Secondary match is worth 1 point
                }

                // Choose this position if it's a better match than what we have
                if (match_quality > *match_quality_out || (match_quality == *match_quality_out && i < *sort_pos_out)) {
                    *sort_pos_out = i;
                    *match_quality_out = match_quality;
                }
            }
        };
        // Find best match for first player
        size_t p1_sort_pos = SIZE_MAX;
        int p1_match_quality = -1; // Higher = better match
        CalcSortPos(p1, &p1_sort_pos, &p1_match_quality);

        // Find best match for second player
        size_t p2_sort_pos = SIZE_MAX;
        int p2_match_quality = -1;
        CalcSortPos(p2, &p2_sort_pos, &p2_match_quality);

        // If neither player matches any sorting criteria, maintain original order
        if (p1_sort_pos == SIZE_MAX && p2_sort_pos == SIZE_MAX) {
            return 0;
        }

        // Players with no match go to the end
        if (p1_sort_pos == SIZE_MAX) return 1;
        if (p2_sort_pos == SIZE_MAX) return 0;

        // Compare sort positions - lower position comes first
        if (p1_sort_pos < p2_sort_pos) return 0;
        if (p1_sort_pos > p2_sort_pos) return 1;

        // Same position - use match quality as tiebreaker
        if (p1_match_quality > p2_match_quality) return 0;
        if (p1_match_quality < p2_match_quality) return 1;

        // Same position and quality - maintain original order
        return wcscmp(GW::Agents::GetAgentEncName(p1->agent_id), GW::Agents::GetAgentEncName(p2->agent_id)) > 0;
    }

    const std::string GetProfessionName(uint8_t prof)
    {
        return GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(prof));
    }

    bool OverridePartySortOrder(bool _override = true)
    {
        const auto player_list = (GW::ScrollableFrame*)GW::UI::GetChildFrame(SnapsToPartyWindow::GetPartyWindowHealthBars(), 0);
        if (!player_list) return false;
        uint32_t count = 0;
        player_list->GetCount(&count);
        return player_list->SetSortHandler(0) && player_list->SetSortHandler(_override ? PartySortHandler : 0);
    }
    void RefreshPartySortHandler()
    {
        if (!custom_sort_party_window) {
            OverridePartySortOrder(false);
            return;
        }
        const auto map_id = GW::Map::GetMapID();
        const auto party_size = GW::PartyMgr::GetPartyPlayerCount();
        PartySorting* best_match = 0;
        for (auto& sorting : party_sortings) {
            if (sorting.map_id != GW::Constants::MapID::None && sorting.map_id != map_id) continue;
            if (sorting.party_size != 0 && sorting.party_size != party_size) continue;
            if (best_match) {
                if (sorting.map_id == GW::Constants::MapID::None && best_match->map_id != GW::Constants::MapID::None) continue;
                if (sorting.party_size == 0 && best_match->party_size != 0) continue;
            }
            best_match = &sorting;
        }
        chosen_sorting_vector = best_match;
        OverridePartySortOrder(true);
    }


    GW::HookEntry OnPostUIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kSetAgentProfession: {
                const auto agent_id = *(uint32_t*)wparam;
                if (GW::PartyMgr::IsAgentInParty(agent_id)) RefreshPartySortHandler();
            } break;
            case GW::UI::UIMessage::kPartyRemovePlayer:
            case GW::UI::UIMessage::kPartyAddPlayer: {
                const auto party_id = *(uint32_t*)wparam;
                const auto my_party = GW::PartyMgr::GetPartyInfo();
                if (!my_party) break;
                if (party_id == 0 || party_id == my_party->party_id) RefreshPartySortHandler();
            } break;
            case GW::UI::UIMessage::kMapLoaded: {
                RefreshPartySortHandler();
            } break;
        }
    }

    void DrawCustomNPCSettings() {
        ImGui::TextDisabled("Only works in an explorable area. Only works on NPCs; not enemies, minions or spirits.");
        const float fontScale = ImGui::GetIO().FontGlobalScale;
        const float cols[3] = {256.0f * fontScale, 352.0f * fontScale, 448.0f * fontScale};

        ImGui::Text("Name");
        ImGui::SameLine(cols[0]);
        ImGui::Text("Model ID");
        ImGui::SameLine(cols[1]);
        ImGui::Text("Map");
        ImGui::Separator();
        ImGui::BeginChild("user_defined_npcs_to_add_scroll", ImVec2(0, 200.0f));
        for (const auto& npc : user_defined_npcs) {
            if (!npc) {
                continue;
            }
            if (!npc->model_id) {
                continue;
            }
            ImGui::PushID(static_cast<int>(npc->model_id));
            ImGui::TextUnformatted(npc->alias.c_str());
            ImGui::SameLine(cols[0]);
            ImGui::Text("%d", npc->model_id);
            ImGui::SameLine(cols[1]);
            ImGui::TextUnformatted(npc->GetMapName());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f * fontScale);
            const bool clicked = ImGui::Button(" X ");
            ImGui::PopID();
            if (clicked) {
                Log::Flash("Removed special NPC %s (%d)", npc->alias.c_str(), npc->model_id);
                RemoveSpecialNPC(npc->model_id);
                CheckMap();
                break;
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        bool submitted = false;
        ImGui::Text("Add new:");
        ImGui::InputText("Name", new_npc_alias, 128);
        ImGui::InputInt("Model ID", &new_npc_model_id);
        ImGui::InputInt("Map ID (0 = Any)", &new_npc_map_id);
        submitted |= ImGui::Button("Add");
        if (submitted) {
            if (new_npc_model_id < 1) {
                return Log::Error("Invalid model id");
            }
            if (new_npc_map_id < 0 || new_npc_map_id > static_cast<int>(GW::Constants::MapID::Count)) {
                return Log::Error("Invalid map id");
            }
            const std::string alias_str(new_npc_alias);
            if (alias_str.empty()) {
                return Log::Error("Empty value for Name");
            }
            const auto it = user_defined_npcs_by_model_id.find(static_cast<uint32_t>(new_npc_model_id));
            if (it != user_defined_npcs_by_model_id.end()) {
                return Log::Error("Special NPC %s is already defined for model_id %d", it->second->alias.c_str(), new_npc_model_id);
            }
            AddSpecialNPC({alias_str.c_str(), new_npc_model_id, static_cast<GW::Constants::MapID>(new_npc_map_id)});
            Log::Flash("Added special NPC %s (%d)", alias_str.c_str(), new_npc_model_id);
            CheckMap();
        }
    }

    void DrawCustomPartySortingSettings() {
        const float fontScale = ImGui::GetIO().FontGlobalScale;

        static int edit_sorting_index = -1;
        static int edit_map_id = 0;
        static int edit_party_size = 0;
        static std::vector<uint16_t> edit_profession_order;

        float offset = 0.f;
        const float sort_cols[3] = {(offset += 200.f) * fontScale, (offset += 100.f) * fontScale, ImGui::GetContentRegionAvail().x - 100.0f * fontScale};

        ImGui::Text("Map");
        ImGui::SameLine(sort_cols[0]);
        ImGui::Text("Party Size");
        ImGui::SameLine(sort_cols[1]);
        ImGui::Text("Sort Order");
        ImGui::SameLine(sort_cols[2]);
        ImGui::Text("Actions");
        ImGui::Separator();

        ImGui::BeginChild("party_sorting_scroll", ImVec2(0, 200.0f));
        for (size_t i = 0; i < party_sortings.size(); i++) {
            auto& sorting = party_sortings[i];
            ImGui::PushID(static_cast<int>(i));

            // Map name
            const char* map_name = "Any Map";
            if (sorting.map_id != GW::Constants::MapID::None) {
                auto* enc_name = Resources::GetMapName(sorting.map_id);
                if (enc_name && enc_name->string().size()) {
                    map_name = enc_name->string().c_str();
                }
                else {
                    static char map_id_str[32];
                    snprintf(map_id_str, sizeof(map_id_str), "Map %d", static_cast<int>(sorting.map_id));
                    map_name = map_id_str;
                }
            }
            ImGui::TextUnformatted(map_name);
            ImGui::SameLine(sort_cols[0]);

            // Party size
            if (sorting.party_size == 0) {
                ImGui::Text("Any");
            }
            else {
                ImGui::Text("%u", sorting.party_size);
            }
            ImGui::SameLine(sort_cols[1]);

            // Sort order display
            std::string sort_display;
            for (size_t j = 0; j < sorting.sorting_by_profession.size(); j++) {
                if (j > 0) sort_display += " -> ";
                uint8_t primary = sorting.sorting_by_profession[j] >> 8;
                uint8_t secondary = sorting.sorting_by_profession[j] & 0xff;
                sort_display += GetProfessionName(primary) + "/" + GetProfessionName(secondary);
            }
            ImGui::TextUnformatted(sort_display.c_str());

            ImGui::SameLine(sort_cols[2]);

            // Edit button
            if (ImGui::Button("Edit")) {
                edit_sorting_index = i;
                edit_map_id = static_cast<int>(sorting.map_id);
                edit_party_size = static_cast<int>(sorting.party_size);
                edit_profession_order = sorting.sorting_by_profession;
            }
            ImGui::SameLine();

            // Delete button
            if (ImGui::Button("Delete")) {
                party_sortings.erase(party_sortings.begin() + i);
                if (chosen_sorting_vector == &sorting) {
                    chosen_sorting_vector = nullptr;
                }
                RefreshPartySortHandler();
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::Separator();




        // Add/Edit party sorting
        const bool is_editing = (edit_sorting_index >= 0);
        ImGui::Text(is_editing ? "Edit Party Sorting:" : "Add New Party Sorting:");

        // Map selection
        ImGui::Text("Map ID (0 = Any):");
        ImGui::SameLine(200.0f * fontScale);
        ImGui::SetNextItemWidth(100.0f * fontScale);
        ImGui::InputInt("##map_id", &edit_map_id);
        if (edit_map_id) {
            ImGui::SameLine();
            ImGui::TextDisabled(Resources::GetMapName((GW::Constants::MapID)edit_map_id)->string().c_str());
        }
        // Party size
        ImGui::Text("Party Size (0 = Any):");
        ImGui::SameLine(200.0f * fontScale);
        ImGui::SetNextItemWidth(100.0f * fontScale);
        ImGui::InputInt("##party_size", &edit_party_size);
        if (edit_party_size < 0) edit_party_size = 0;
        if (edit_party_size > 12) edit_party_size = 12;

        // Profession order
        ImGui::Text("Profession Order:");
        ImGui::BeginChild("profession_order_edit", ImVec2(0, 150.0f), true);

        for (size_t i = 0; i < edit_profession_order.size(); i++) {
            ImGui::PushID(static_cast<int>(i));

            uint8_t primary = edit_profession_order[i] >> 8;
            uint8_t secondary = edit_profession_order[i] & 0xff;

            ImGui::Text("%zu.", i + 1);
            ImGui::SameLine();

            // Primary profession combo
            ImGui::SetNextItemWidth(120.0f * fontScale);
            if (ImGui::BeginCombo("##primary", GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(primary)))) {
                for (uint8_t prof = 0; prof <= 10; prof++) {
                    if (ImGui::Selectable(GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(prof)), primary == prof)) {
                        primary = prof;
                        edit_profession_order[i] = (static_cast<uint16_t>(primary) << 8) | secondary;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            ImGui::Text("/");
            ImGui::SameLine();

            // Secondary profession combo
            ImGui::SetNextItemWidth(120.0f * fontScale);
            if (ImGui::BeginCombo("##secondary", GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(secondary)))) {
                for (uint8_t prof = 0; prof <= 10; prof++) {
                    if (ImGui::Selectable(GW::Constants::GetProfessionAcronym(static_cast<GW::Constants::Profession>(prof)), secondary == prof)) {
                        secondary = prof;
                        edit_profession_order[i] = (static_cast<uint16_t>(primary) << 8) | secondary;
                    }
                }
                ImGui::EndCombo();
            }

            

            // Move up button
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_ARROW_UP) && i > 0) 
                std::swap(edit_profession_order[i], edit_profession_order[i - 1]);

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_ARROW_DOWN) && i < edit_profession_order.size() - 1)
                std::swap(edit_profession_order[i], edit_profession_order[i + 1]);

            ImGui::SameLine();
            // Remove button
            if (ImGui::Button("Remove")) {
                edit_profession_order.erase(edit_profession_order.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }
        ImGui::EndChild();

        // Add profession button
        if (ImGui::Button("Add Profession")) {
            edit_profession_order.push_back(0); // Any/Any
        }

        ImGui::SameLine();

        // Save button
        if (ImGui::Button(is_editing ? "Save Changes" : "Add Sorting")) {
            if (edit_profession_order.empty()) {
                Log::Error("At least one profession entry is required");
            }
            else {
                PartySorting new_sorting;
                new_sorting.map_id = static_cast<GW::Constants::MapID>(edit_map_id);
                new_sorting.party_size = static_cast<uint32_t>(edit_party_size);
                new_sorting.sorting_by_profession = edit_profession_order;

                if (is_editing) {
                    party_sortings[edit_sorting_index] = new_sorting;
                    Log::Flash("Updated party sorting");
                    edit_sorting_index = -1;
                }
                else {
                    party_sortings.push_back(new_sorting);
                    Log::Flash("Added new party sorting");
                }

                // Clear edit state
                edit_map_id = 0;
                edit_party_size = 0;
                edit_profession_order.clear();

                RefreshPartySortHandler();
            }
        }

        if (is_editing) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                edit_sorting_index = -1;
                edit_map_id = 0;
                edit_party_size = 0;
                edit_profession_order.clear();
            }
        }
    }
} // namespace

void PartyWindowModule::Update(float delta) {
    ToolboxModule::Update(delta);
    while (!summons_pending.empty()) {
        const SummonPending summon = summons_pending.front();

        const uint32_t agent_id = summon.agent_id;
        const GW::Constants::SkillID skill_id = summon.skill_id;

        const GW::Skill* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);

        const GW::AgentLiving* agentLiving = skill ? static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(summon.agent_id)) : nullptr;
        if (!agentLiving) {
            return;
        }

        wchar_t skill_name_enc[8] = { 0 };
        GW::UI::UInt32ToEncStr(skill->name, skill_name_enc, 8);

        SetAgentName(agent_id, skill_name_enc);

        summons_pending.pop();
    }
}

void PartyWindowModule::Initialize()
{
    ToolboxModule::Initialize();
    // Remove certain NPCs from party window when dead
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &AgentState_Entry,
        [&](const GW::HookStatus*, const GW::Packet::StoC::AgentState* pak) -> void {
            if (!add_npcs_to_party_window || pak->state != 16) {
                return; // Not dead.
            }
            if (!std::ranges::contains(allies_added_to_party, pak->agent_id)) {
                return; // Not added via toolbox
            }
            pending_remove.push(pak->agent_id);
        });
    // Remove certain NPCs from party window when despawned
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(
        &AgentRemove_Entry,
        [&](const GW::HookStatus*, const GW::Packet::StoC::AgentRemove* pak) -> void {
            if (remove_dead_imperials) {
                if (const auto* agent = GW::Agents::GetAgentByID(pak->agent_id); agent && agent->GetAsAgentLiving() && agent->GetAsAgentLiving()->GetIsDead()) {
                    const auto player_number = agent->GetAsAgentLiving()->player_number;
                    if (player_number == GW::Constants::ModelID::SummoningStone::ImperialCripplingSlash ||
                        player_number == GW::Constants::ModelID::SummoningStone::ImperialQuiveringBlade ||
                        player_number == GW::Constants::ModelID::SummoningStone::ImperialTripleChop ||
                        player_number == GW::Constants::ModelID::SummoningStone::ImperialBarrage) {
                        if (!std::ranges::contains(removed_canthans, pak->agent_id)) {
                            pending_remove.push(pak->agent_id);
                            removed_canthans.push_back(pak->agent_id);
                        }
                    }
                    return;
                }
            }
            if (std::ranges::find(allies_added_to_party, pak->agent_id) == allies_added_to_party.end()) {
                return; // Not added via toolbox
            }
            pending_remove.push(pak->agent_id);
        });
    // Add certain NPCs to party window when spawned
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(
        &AgentAdd_Entry,
        [&](const GW::HookStatus*, GW::Packet::StoC::AgentAdd* pak) -> void {
            if (!add_npcs_to_party_window) {
                return;
            }
            if (pak->type != 1) {
                return; // Not a living agent.
            }
            if (!ShouldAddAgentToPartyWindow(pak->agent_type)) {
                return;
            }
            pending_add.emplace_back(pak->agent_id, pak->allegiance_bits, pak->agent_type ^ 0x20000000);
        });
    // Flash/focus window on zoning (and a bit of housekeeping)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &GameSrvTransfer_Entry,
        [&](const GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* pak) -> void {
            allies_added_to_party.clear();
            removed_canthans.clear();
            pending_remove = {};
            pending_add.clear();
            is_explorable = pak->is_explorable != 0;
            aliased_player_names.clear();
        });
    // Player numbers in party window
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(
        &GameSrvTransfer_Entry,
        [&](const GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance* pak) -> void {
            if (!add_player_numbers_to_party_window || !is_explorable || IsPvP()) {
                return;
            }
            SetPlayerNumber(pak->player_name, pak->player_number);
        });
    GW::GameThread::RegisterGameThreadCallback(&GameThreadCallback_Entry, [&](GW::HookStatus*) {
        while (!pending_remove.empty()) {
            RemoveAllyActual(pending_remove.front());
            pending_remove.pop();
        }
        for (size_t i = 0; i < pending_add.size(); i++) {
            if (TIMER_DIFF(pending_add[i].add_timer) < 100) {
                continue;
            }
            AddAllyActual(pending_add[i]);
            pending_add.erase(pending_add.begin() + static_cast<int>(i));
            break; // Continue next frame
        }
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(
        &Summon_AgentAdd_Entry,
        [&](GW::HookStatus*, const GW::Packet::StoC::AgentAdd* pak) -> void {
            if (!add_elite_skill_to_summons) {
                return;
            }
            if (pak->type != 1) {
                return; // Not a living agent.
            }
            const uint32_t player_number = pak->agent_type ^ 0x20000000;
            const auto summon_elite = summon_elites.find(player_number);
            if (summon_elite == summon_elites.end()) {
                return;
            }
            if (summon_elite->second == GW::Constants::SkillID::No_Skill) {
                return;
            }
            summons_pending.push({pak->agent_id, summon_elite->second});
        }
    );

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kSetAgentProfession, 
        GW::UI::UIMessage::kPartyRemovePlayer, 
        GW::UI::UIMessage::kPartyAddPlayer,
        GW::UI::UIMessage::kMapLoaded
    };
    for (auto ui_message : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, ui_message, OnPostUIMessage, 0x8000);
    }
}

void PartyWindowModule::Terminate()
{
    GW::GameThread::RemoveGameThreadCallback(&Summon_GameThreadCallback_Entry);
    ClearSpecialNPCs();
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
}

void PartyWindowModule::SignalTerminate()
{
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&AgentRemove_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentState>(&AgentState_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&Summon_AgentAdd_Entry);
    ClearAddedAllies();
}

bool PartyWindowModule::CanTerminate()
{
    return allies_added_to_party.empty();
}

void PartyWindowModule::LoadDefaults()
{
    ClearSpecialNPCs();

    AddSpecialNPC({"Vale friendly spirit 1", GW::Constants::ModelID::UW::TorturedSpirit1, GW::Constants::MapID::The_Underworld});
    AddSpecialNPC({"Vale friendly spirit 2", GW::Constants::ModelID::UW::TorturedSpirit1 + 1, GW::Constants::MapID::The_Underworld});
    AddSpecialNPC({"Pits friendly spirit 1", GW::Constants::ModelID::UW::PitsSoul1, GW::Constants::MapID::The_Underworld});
    AddSpecialNPC({"Pits friendly spirit 2", GW::Constants::ModelID::UW::PitsSoul2, GW::Constants::MapID::The_Underworld});
    AddSpecialNPC({"Pits friendly spirit 3", GW::Constants::ModelID::UW::PitsSoul3, GW::Constants::MapID::The_Underworld});
    AddSpecialNPC({"Pits friendly spirit 4", GW::Constants::ModelID::UW::PitsSoul4, GW::Constants::MapID::The_Underworld});

    AddSpecialNPC({"FoW Griffs", GW::Constants::ModelID::FoW::Griffons, GW::Constants::MapID::The_Fissure_of_Woe});
    AddSpecialNPC({"FoW Forgemaster", GW::Constants::ModelID::FoW::Forgemaster, GW::Constants::MapID::The_Fissure_of_Woe});

    AddSpecialNPC({"Mursaat Elementalist (Polymock)", GW::Constants::ModelID::PolymockSummon::MursaatElementalist, GW::Constants::MapID::None});
    AddSpecialNPC({"Flame Djinn (Polymock)", GW::Constants::ModelID::PolymockSummon::FlameDjinn, GW::Constants::MapID::None});
    AddSpecialNPC({"Ice Imp (Polymock)", GW::Constants::ModelID::PolymockSummon::IceImp, GW::Constants::MapID::None});
    AddSpecialNPC({"Naga Shaman (Polymock)", GW::Constants::ModelID::PolymockSummon::NagaShaman, GW::Constants::MapID::None});

    AddSpecialNPC({"Ebon Vanguard Assassin", GW::Constants::ModelID::EbonVanguardAssassin, GW::Constants::MapID::None});

    AddSpecialNPC({"Ben Wolfson Pre-Searing", 1512, GW::Constants::MapID::None});

    // Important NPCs for missions
    AddSpecialNPC({"Gyala Hatchery siege turtle", 3582, GW::Constants::MapID::Gyala_Hatchery_outpost_mission});
    AddSpecialNPC({"Rornak Stonesledge (Bonus NPC)", 1559, GW::Constants::MapID::The_Frost_Gate});
    AddSpecialNPC({"Oink (Bonus NPC)", 1710, GW::Constants::MapID::Gates_of_Kryta});
    AddSpecialNPC({"Restless Spirit (Bonus NPC)", 1965, GW::Constants::MapID::Sanctum_Cay});
    AddSpecialNPC({"Captain Besuz (Bonus NPC)", 5271, GW::Constants::MapID::Blacktide_Den});
}

void PartyWindowModule::DrawSettingsInternal()
{
    ImGui::Checkbox("Add player numbers to party window", &add_player_numbers_to_party_window);
    ImGui::ShowHelp("Will update on next map");
    ImGui::Checkbox("Rename Tengu and Imperial Guard Ally summons to their respective elite skill", &add_elite_skill_to_summons);
    ImGui::ShowHelp("Only works on newly spawned summons.");
    ImGui::Checkbox(
        "Remove dead imperial guard allies", &remove_dead_imperials);
    if (ImGui::Checkbox("Add special NPCs to party window", &add_npcs_to_party_window)) {
        CheckMap();
    }
    ImGui::ShowHelp("Adds special NPCs to the Allies section of the party window within compass range.");
    if (add_npcs_to_party_window) {
        ImGui::Indent();
        DrawCustomNPCSettings();
        ImGui::Unindent();
    }
    ImGui::Separator();

    if (ImGui::Checkbox("Add custom sorting to party window", &custom_sort_party_window)) {
        RefreshPartySortHandler();
    }
    ImGui::ShowHelp("Automatically sort players in your party window depending on profession and/or map");
    if (custom_sort_party_window) {
        ImGui::Indent();
        DrawCustomPartySortingSettings();
        ImGui::Unindent();
    }
}
void PartyWindowModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    // Clear existing ini settings
    ini->Delete(Name(), nullptr, NULL);

    SAVE_BOOL(add_player_numbers_to_party_window);
    SAVE_BOOL(add_elite_skill_to_summons);
    SAVE_BOOL(remove_dead_imperials);
    SAVE_BOOL(custom_sort_party_window);

    // - Re-fill settings.
    SAVE_BOOL(add_npcs_to_party_window);
    for (const auto& user_defined_npc : user_defined_npcs) {
        if (!user_defined_npc || !user_defined_npc->model_id) {
            continue;
        }
        std::string s(user_defined_npc->alias);
        s += "\x1";
        s += std::to_string(std::to_underlying(user_defined_npc->map_id));
        ini->SetValue(Name(), std::to_string(user_defined_npc->model_id).c_str(), s.c_str());
    }

    // Save party sorting configurations
    ini->SetLongValue(Name(), "party_sorting_count", static_cast<long>(party_sortings.size()));
    for (size_t i = 0; i < party_sortings.size(); i++) {
        const auto& sorting = party_sortings[i];

        std::string prefix = "party_sorting_" + std::to_string(i) + "_";

        // Save map ID and party size
        ini->SetLongValue(Name(), (prefix + "map_id").c_str(), static_cast<long>(sorting.map_id));
        ini->SetLongValue(Name(), (prefix + "party_size").c_str(), static_cast<long>(sorting.party_size));

        // Save profession order count
        ini->SetLongValue(Name(), (prefix + "profession_count").c_str(), static_cast<long>(sorting.sorting_by_profession.size()));

        // Save each profession entry
        for (size_t j = 0; j < sorting.sorting_by_profession.size(); j++) {
            std::string prof_key = prefix + "profession_" + std::to_string(j);
            ini->SetLongValue(Name(), prof_key.c_str(), static_cast<long>(sorting.sorting_by_profession[j]));
        }
    }
}

void PartyWindowModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    // get all keys in a section
    CSimpleIni::TNamesDepend keys;
    ini->GetAllKeys(Name(), keys);
    if (keys.empty()) {
        return LoadDefaults();
    }

    LOAD_BOOL(add_npcs_to_party_window);
    LOAD_BOOL(add_player_numbers_to_party_window);
    LOAD_BOOL(add_elite_skill_to_summons);
    LOAD_BOOL(remove_dead_imperials);
    LOAD_BOOL(custom_sort_party_window);

    ClearSpecialNPCs();
    for (const auto& key : keys) {
        char* p;
        long model_id = strtol(key.pItem, &p, 10);
        if (!p || model_id < 1) {
            continue; // Not a model_id
        }
        std::string value(ini->GetValue(Name(), key.pItem, ""));
        if (value.empty()) {
            continue;
        }
        const size_t name_end_pos = value.find("\x1");
        if (name_end_pos == std::string::npos) {
            continue;
        }
        std::string alias(value.substr(0, name_end_pos));
        if (alias.empty()) {
            continue;
        }
        p = nullptr;
        long map_id = strtol(value.substr(name_end_pos + 1).c_str(), &p, 10);
        if (!p || map_id < 0 || map_id >= static_cast<long>(GW::Constants::MapID::Count)) {
            continue; // Invalid map_id
        }
        AddSpecialNPC({alias.c_str(), model_id, static_cast<GW::Constants::MapID>(map_id)});
    }

    // Load party sorting configurations
    party_sortings.clear();
    long sorting_count = ini->GetLongValue(Name(), "party_sorting_count", 0);

    for (long i = 0; i < sorting_count; i++) {
        std::string prefix = "party_sorting_" + std::to_string(i) + "_";

        PartySorting sorting;

        // Load map ID and party size
        sorting.map_id = static_cast<GW::Constants::MapID>(ini->GetLongValue(Name(), (prefix + "map_id").c_str(), 0));
        sorting.party_size = static_cast<uint32_t>(ini->GetLongValue(Name(), (prefix + "party_size").c_str(), 0));

        // Load profession order
        long profession_count = ini->GetLongValue(Name(), (prefix + "profession_count").c_str(), 0);
        sorting.sorting_by_profession.reserve(profession_count);

        for (long j = 0; j < profession_count; j++) {
            std::string prof_key = prefix + "profession_" + std::to_string(j);
            uint16_t profession_combo = static_cast<uint16_t>(ini->GetLongValue(Name(), prof_key.c_str(), 0));
            sorting.sorting_by_profession.push_back(profession_combo);
        }

        // Only add if we have at least one profession entry
        if (!sorting.sorting_by_profession.empty()) {
            party_sortings.push_back(sorting);
        }
    }

    CheckMap();
    GW::GameThread::Enqueue(RefreshPartySortHandler);
}

const std::map<std::wstring, std::wstring>& PartyWindowModule::GetAliasedPlayerNames()
{
    return aliased_player_names;
}
