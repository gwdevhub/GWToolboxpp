#include "stdafx.h"

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/GameContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
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

#include <Modules/PartyWindowModule.h>

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
        PendingAddToParty(uint32_t _agent_id, uint32_t _allegiance_bits, uint32_t _player_number)
            : agent_id(_agent_id)
            , player_number(_player_number)
            , allegiance_bits(_allegiance_bits)
        {
            add_timer = TIMER_INIT();
        }

        clock_t add_timer;
        uint32_t agent_id;
        uint32_t player_number;
        uint32_t allegiance_bits;
        GW::AgentLiving* GetAgent();
    };
    struct SpecialNPCToAdd {
        SpecialNPCToAdd(const char* _alias, int _model_id, GW::Constants::MapID _map_id)
            : alias(_alias)
            , model_id(static_cast<uint32_t>(_model_id))
            , map_id(_map_id)
        {
        };

        std::wstring map_name;
        bool decode_pending = false;
        std::wstring* GetMapName();
        std::string alias;
        uint32_t model_id = 0;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
    };

    struct SummonPending
    {
        uint32_t agent_id;
        GW::Constants::SkillID skill_id;
    };

    bool add_npcs_to_party_window = true; // Quick tickbox to disable the module without restarting TB
    bool add_player_numbers_to_party_window = false;
    bool add_elite_skill_to_summons = false;
    bool remove_dead_imperials = false;

    char new_npc_alias[128] = { 0 };
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

    bool IsPvE() {
        GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        if (!map) return false;
        switch (static_cast<GW::RegionType>(map->type)) {
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
        size_t GetPartySize() {
            return players.size() + henchmen.size() + heroes.size();
        }
    };

    PartyInfo* GetPartyInfo(uint32_t party_id = 0) {
        if (!party_id)
            return (PartyInfo*)GW::PartyMgr::GetPartyInfo();
        GW::PartyContext* p = GW::GetGameContext()->party;
        if (!p || !p->parties.valid() || party_id >= p->parties.size())
            return nullptr;
        return (PartyInfo*)p->parties[party_id];
    }
    void SetPlayerNumber(wchar_t* player_name, uint32_t player_number) {
        wchar_t buf[32] = { 0 };
        swprintf(buf, 32, L"%s (%d)", player_name, player_number);
        if(wcsncmp(player_name,buf,wcslen(buf)) != 0)
            wcscpy(player_name, buf);
    }


    bool SetAgentName(const uint32_t agent_id, const wchar_t* name) {
        auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!a || !name)
            return false;
        const wchar_t* current_name = GW::Agents::GetAgentEncName(a);
        if (!current_name)
            return false;
        if (wcscmp(name, current_name) == 0)
            return true;
        GW::Packet::StoC::AgentName packet;
        packet.header = GW::Packet::StoC::AgentName::STATIC_HEADER;
        packet.agent_id = agent_id;
        wcscpy(packet.name_enc, name);
        GW::StoC::EmulatePacket(&packet);
        return true;
    }

    bool IsPvP() {
        GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
        if (!i) return false;
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
    void AddSpecialNPC(SpecialNPCToAdd npc) {
        SpecialNPCToAdd* new_npc = new SpecialNPCToAdd(npc);
        user_defined_npcs.push_back(new_npc);
        user_defined_npcs_by_model_id.emplace(npc.model_id, new_npc);
    }
    void RemoveSpecialNPC(uint32_t model_id) {
        user_defined_npcs_by_model_id.erase(model_id);
        for (auto& user_defined_npc : user_defined_npcs) {
            if (!user_defined_npc)
                continue;
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
            if (!user_defined_npc) continue;
            delete user_defined_npc;
        }
        user_defined_npcs.clear();
    }
    void ClearAddedAllies() {
        for (unsigned int& ally_id : allies_added_to_party) {
            pending_remove.push(ally_id);
        }
    }
    bool ShouldAddAgentToPartyWindow(uint32_t agent_type) {
        if ((agent_type & 0x20000000) == 0)
            return false; // Not an NPC
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return false; // Not in an explorable area
        GW::Constants::MapID map_id = GW::Map::GetMapID();
        uint32_t player_number = (agent_type ^ 0x20000000);
        const auto it = user_defined_npcs_by_model_id.find(player_number);
        if (it == user_defined_npcs_by_model_id.end())
            return false;
        if (it->second->map_id != GW::Constants::MapID::None && it->second->map_id != map_id)
            return false;
        return true;
    }
    bool ShouldAddAgentToPartyWindow(GW::Agent* _a) {
        GW::AgentLiving* a = _a ? _a->GetAsAgentLiving() : nullptr;
        if (!a || !a->IsNPC())
            return false;
        if (a->GetIsDead() || a->GetIsDeadByTypeMap() || a->allegiance == GW::Constants::Allegiance::Enemy)
            return false; // Dont add dead NPCs.
        const auto it = std::ranges::find(allies_added_to_party, a->agent_id);
        if (it != allies_added_to_party.end())
            return false;
        return ShouldAddAgentToPartyWindow(0x20000000u | a->player_number);
    }
    void CheckMap() {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return;
        if (!add_npcs_to_party_window) {
            ClearAddedAllies();
            return;
        }
        GW::AgentArray* agents_ptr = GW::Agents::GetAgentArray();
        if (!agents_ptr)
            return;
        GW::AgentArray& agents = *agents_ptr;
        for (const unsigned int& ally_id : allies_added_to_party) {
            if (ally_id >= agents.size()) {
                pending_remove.push(ally_id);
                continue;
            }
            GW::AgentLiving* a = agents[ally_id] ? agents[ally_id]->GetAsAgentLiving() : nullptr;
            if (!a || !a->IsNPC()) {
                pending_remove.push(ally_id);
                continue;
            }
            const auto it = user_defined_npcs_by_model_id.find(a->player_number);
            if (it != user_defined_npcs_by_model_id.end())
                continue;
            pending_remove.push(ally_id);
        }
        for (const auto& agent : agents) {
            GW::AgentLiving* a = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!a || !ShouldAddAgentToPartyWindow(a))
                continue;
            pending_add.emplace_back(a->agent_id, 0, a->player_number);
        }
    }
    bool ShouldRemoveAgentFromPartyWindow(uint32_t agent_id) {
        auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!a || !a->GetIsLivingType() || !(a->type_map & 0x20000))
            return false; // Not in party window
        for (unsigned int i : allies_added_to_party) {
            if (a->agent_id != i)
                continue;
            // Ally turned enemy, or is dead.
            return a->allegiance == GW::Constants::Allegiance::Enemy || a->GetIsDead() || a->GetIsDeadByTypeMap();
        }
        return false;
    }
    void RemoveAllyActual(uint32_t agent_id) {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            const auto it = std::ranges::find(allies_added_to_party, agent_id);
            if (it != allies_added_to_party.end()) allies_added_to_party.erase(it);
            return;
        }
        GW::Packet::StoC::PartyRemoveAlly packet;
        packet.header = GW::Packet::StoC::PartyRemoveAlly::STATIC_HEADER;
        packet.agent_id = agent_id;

        auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        float prev_hp = 0.0f;
        wchar_t prev_name[8] = { 0 };
        if (a) {
            wcscpy(prev_name, GW::Agents::GetAgentEncName(a));
            prev_hp = a->hp;
        }
        // 1. Remove NPC from window.
        GW::StoC::EmulatePacket(&packet);
        SetAgentName(agent_id, prev_name);
        const auto it = std::ranges::find(allies_added_to_party, agent_id);
        if (it != allies_added_to_party.end()) allies_added_to_party.erase(it);
    }
    void AddAllyActual(PendingAddToParty& p) {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return;
        GW::AgentLiving* a = p.GetAgent();
        if (!a || a->GetIsDead() || a->GetIsDeadByTypeMap()) return;
        float prev_hp = a->hp;
        wchar_t prev_name[8] = { 0 };
        wcscpy(prev_name, GW::Agents::GetAgentEncName(a));
        prev_hp = a->hp;
        GW::Packet::StoC::PartyAllyAdd packet;
        GW::Packet::StoC::AgentName packet2;

        packet.header = GW::Packet::StoC::PartyAllyAdd::STATIC_HEADER;
        packet.agent_id = p.agent_id;
        packet.agent_type = p.player_number | 0x20000000;
        packet.allegiance_bits = 1886151033;

        packet2.header = GW::Packet::StoC::AgentName::STATIC_HEADER;
        packet2.agent_id = p.agent_id;

        // 1. Remove NPC from window.
        GW::StoC::EmulatePacket(&packet);
        SetAgentName(p.agent_id, prev_name);

        allies_added_to_party.push_back(p.agent_id);
    }
    GW::AgentLiving* PendingAddToParty::GetAgent() {
        auto* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!a || !a->GetIsLivingType() || a->player_number != player_number || a->allegiance == GW::Constants::Allegiance::Enemy)
            return nullptr;
        GW::NPC* npc = GW::Agents::GetNPCByID(player_number);
        if (!npc)
            return nullptr;
        return a;
    }
    std::wstring* SpecialNPCToAdd::GetMapName() {
        if (decode_pending)
            return &map_name;
        decode_pending = true;
        if (map_id != GW::Constants::MapID::None) {
            GW::GameThread::Enqueue([this]() {
                GW::AreaInfo* info = GW::Map::GetMapInfo(map_id);
                if (!info)
                    return;
                static wchar_t enc_str[16];
                if (!GW::UI::UInt32ToEncStr(info->name_id, enc_str, 16))
                    return;
                GW::UI::AsyncDecodeStr(enc_str, &map_name);
                });
        }
        else {
            map_name = std::wstring(L"Any");
        }
        return &map_name;
    }
}
void PartyWindowModule::Initialize() {
    ToolboxModule::Initialize();
    // Remove certain NPCs from party window when dead
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &AgentState_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::AgentState* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!add_npcs_to_party_window || pak->state != 16)
                return; // Not dead.
            if (std::ranges::find(allies_added_to_party, pak->agent_id) == allies_added_to_party.end())
                return; // Not added via toolbox
            pending_remove.push(pak->agent_id);
        });
    // Remove certain NPCs from party window when despawned
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(
        &AgentRemove_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::AgentRemove* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            if (remove_dead_imperials) {
                if (const auto* agent = GW::Agents::GetAgentByID(pak->agent_id); agent && agent->GetAsAgentLiving() && agent->GetAsAgentLiving()->GetIsDead()) {
                    const auto player_number = agent->GetAsAgentLiving()->player_number;
                    if (player_number == GW::Constants::ModelID::SummoningStone::ImperialCripplingSlash ||
                        player_number == GW::Constants::ModelID::SummoningStone::ImperialQuiveringBlade ||
                        player_number == GW::Constants::ModelID::SummoningStone::ImperialTripleChop ||
                        player_number == GW::Constants::ModelID::SummoningStone::ImperialBarrage) {
                        if (std::ranges::find(removed_canthans, pak->agent_id) ==
                            removed_canthans.end()) {
                            pending_remove.push(pak->agent_id);
                            removed_canthans.push_back(pak->agent_id);
                        }
                    }
                    return;
                }
            }
            if (std::ranges::find(allies_added_to_party, pak->agent_id) == allies_added_to_party.end())
                return; // Not added via toolbox
            pending_remove.push(pak->agent_id);
        });
    // Add certain NPCs to party window when spawned
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(
        &AgentAdd_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::AgentAdd* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!add_npcs_to_party_window)
                return;
            if (pak->type != 1)
                return; // Not a living agent.
            if (!ShouldAddAgentToPartyWindow(pak->agent_type))
                return;
            pending_add.emplace_back(pak->agent_id, pak->allegiance_bits, pak->agent_type ^ 0x20000000);
        });
    // Flash/focus window on zoning (and a bit of housekeeping)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &GameSrvTransfer_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            allies_added_to_party.clear();
            removed_canthans.clear();
            pending_remove = {};
            pending_add.clear();
            is_explorable = pak->is_explorable != 0;
        });
    // Player numbers in party window
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(
        &GameSrvTransfer_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            if (!add_player_numbers_to_party_window || !is_explorable || ::IsPvP())
                return;
            SetPlayerNumber(pak->player_name, pak->player_number);
        });
    GW::GameThread::RegisterGameThreadCallback(&GameThreadCallback_Entry, [&](GW::HookStatus*) {
        while (!pending_remove.empty()) {
            RemoveAllyActual(pending_remove.front());
            pending_remove.pop();
        }
        for (size_t i = 0; i < pending_add.size();i++) {
            if (TIMER_DIFF(pending_add[i].add_timer) < 100)
                continue;
            AddAllyActual(pending_add[i]);
            pending_add.erase(pending_add.begin() + static_cast<int>(i));
            break; // Continue next frame
        }
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(
        &Summon_AgentAdd_Entry,
        [&](GW::HookStatus*, GW::Packet::StoC::AgentAdd* pak) -> void {
            if (!add_elite_skill_to_summons) return;
            if (pak->type != 1) return; // Not a living agent.
            uint32_t player_number = (pak->agent_type ^ 0x20000000);
            auto summon_elite = summon_elites.find(player_number);
            if (summon_elite == summon_elites.end()) return;
            if (summon_elite->second == GW::Constants::SkillID::No_Skill) return;
            summons_pending.push({pak->agent_id, summon_elite->second});
        }
    );

    GW::GameThread::RegisterGameThreadCallback(
        &Summon_GameThreadCallback_Entry,
        [&](GW::HookStatus*) -> void {
            while (!summons_pending.empty()) {
                const SummonPending summon = summons_pending.front();

                const uint32_t agent_id = summon.agent_id;
                const GW::Constants::SkillID skill_id = summon.skill_id;

                GW::Skill* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);

                GW::AgentLiving* agentLiving = skill ? static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(summon.agent_id)) : nullptr;
                if (!agentLiving) return;

                wchar_t skill_name_enc[8] = {0};
                GW::UI::UInt32ToEncStr(skill->name, skill_name_enc, 8);;

                SetAgentName(agent_id, skill_name_enc);

                summons_pending.pop();
            }
        }
    );
}

void PartyWindowModule::Terminate()
{
    ClearSpecialNPCs();
}

void PartyWindowModule::SignalTerminate() {
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&AgentRemove_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentState>(&AgentState_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&Summon_AgentAdd_Entry);
    ClearAddedAllies();
}
bool PartyWindowModule::CanTerminate() {
    return allies_added_to_party.empty();
}

void PartyWindowModule::LoadDefaults() {
    ClearSpecialNPCs();

    AddSpecialNPC({ "Vale friendly spirit 1", GW::Constants::ModelID::UW::TorturedSpirit1, GW::Constants::MapID::The_Underworld });
    AddSpecialNPC({ "Vale friendly spirit 2", GW::Constants::ModelID::UW::TorturedSpirit1 + 1, GW::Constants::MapID::The_Underworld });
    AddSpecialNPC({ "Pits friendly spirit 1", GW::Constants::ModelID::UW::PitsSoul1, GW::Constants::MapID::The_Underworld });
    AddSpecialNPC({ "Pits friendly spirit 2", GW::Constants::ModelID::UW::PitsSoul2, GW::Constants::MapID::The_Underworld });
    AddSpecialNPC({ "Pits friendly spirit 3", GW::Constants::ModelID::UW::PitsSoul3, GW::Constants::MapID::The_Underworld });
    AddSpecialNPC({ "Pits friendly spirit 4", GW::Constants::ModelID::UW::PitsSoul4, GW::Constants::MapID::The_Underworld });

    AddSpecialNPC({ "Gyala Hatchery siege turtle", 3582, GW::Constants::MapID::Gyala_Hatchery_outpost_mission });

    AddSpecialNPC({ "FoW Griffs", GW::Constants::ModelID::FoW::Griffons, GW::Constants::MapID::The_Fissure_of_Woe });
    AddSpecialNPC({ "FoW Forgemaster", GW::Constants::ModelID::FoW::Forgemaster, GW::Constants::MapID::The_Fissure_of_Woe });

    AddSpecialNPC({ "Mursaat Elementalist (Polymock)", GW::Constants::ModelID::PolymockSummon::MursaatElementalist, GW::Constants::MapID::None });
    AddSpecialNPC({ "Flame Djinn (Polymock)", GW::Constants::ModelID::PolymockSummon::FlameDjinn, GW::Constants::MapID::None });
    AddSpecialNPC({ "Ice Imp (Polymock)", GW::Constants::ModelID::PolymockSummon::IceImp, GW::Constants::MapID::None });
    AddSpecialNPC({ "Naga Shaman (Polymock)", GW::Constants::ModelID::PolymockSummon::NagaShaman, GW::Constants::MapID::None });

    //AddSpecialNPC({ "Ebon Vanguard Assassin", 5848, GW::Constants::MapID::None });
    AddSpecialNPC({ "Ebon Vanguard Assassin", GW::Constants::ModelID::EbonVanguardAssassin, GW::Constants::MapID::None });

    AddSpecialNPC({ "Ben Wolfson Pre-Searing", 1512, GW::Constants::MapID::None });
}

void PartyWindowModule::DrawSettingInternal() {
    ImGui::Checkbox("Add player numbers to party window", &add_player_numbers_to_party_window);
    ImGui::ShowHelp("Will update on next map");
    ImGui::Checkbox("Rename Tengu and Imperial Guard Ally summons to their respective elite skill", &add_elite_skill_to_summons);
    ImGui::ShowHelp("Only works on newly spawned summons.");
    ImGui::Checkbox(
        "Remove dead imperial guard allies", &remove_dead_imperials);
    if (ImGui::Checkbox("Add special NPCs to party window", &add_npcs_to_party_window))
        CheckMap();
    ImGui::ShowHelp("Adds special NPCs to the Allies section of the party window within compass range.");
    if (!add_npcs_to_party_window)
        return;
    ImGui::TextDisabled("Only works in an explorable area. Only works on NPCs; not enemies, minions or spirits.");
    float fontScale = ImGui::GetIO().FontGlobalScale;
    float cols[3] = { 256.0f * fontScale, 352.0f * fontScale, 448.0f * fontScale };

    ImGui::Text("Name");
    ImGui::SameLine(cols[0]);
    ImGui::Text("Model ID");
    ImGui::SameLine(cols[1]);
    ImGui::Text("Map");
    ImGui::Separator();
    ImGui::BeginChild("user_defined_npcs_to_add_scroll",ImVec2(0,200.0f));
    for (const auto& npc : user_defined_npcs) {
        if (!npc)
            continue;
        if (!npc->model_id)
            continue;
        ImGui::PushID(static_cast<int>(npc->model_id));
        ImGui::Text("%s",npc->alias.c_str());
        ImGui::SameLine(cols[0]);
        ImGui::Text("%d", npc->model_id);
        ImGui::SameLine(cols[1]);
        ImGui::Text("%ls", npc->GetMapName()->c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - (48.0f * fontScale));
        bool clicked = ImGui::Button(" X ");
        ImGui::PopID();
        if(clicked) {
            Log::Info("Removed special NPC %s (%d)", npc->alias.c_str(), npc->model_id);
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
        if (new_npc_model_id < 1)
            return Log::Error("Invalid model id");
        if (new_npc_map_id < 0 || new_npc_map_id > static_cast<int>(GW::Constants::MapID::Count))
            return Log::Error("Invalid map id");
        std::string alias_str(new_npc_alias);
        if (alias_str.empty())
            return Log::Error("Empty value for Name");
        const auto it = user_defined_npcs_by_model_id.find(static_cast<uint32_t>(new_npc_model_id));
        if (it != user_defined_npcs_by_model_id.end())
            return Log::Error("Special NPC %s is already defined for model_id %d", it->second->alias.c_str(), new_npc_model_id);
        AddSpecialNPC({ alias_str.c_str(), new_npc_model_id,static_cast<GW::Constants::MapID>(new_npc_map_id) });
        Log::Info("Added special NPC %s (%d)", alias_str.c_str(), new_npc_model_id);
        CheckMap();
    }
}
void PartyWindowModule::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    // Clear existing ini settings
    ini->Delete(Name(), NULL, NULL);

    ini->SetBoolValue(Name(), VAR_NAME(add_player_numbers_to_party_window), add_player_numbers_to_party_window);
    ini->SetBoolValue(Name(), VAR_NAME(add_elite_skill_to_summons), add_elite_skill_to_summons);
    ini->SetBoolValue(Name(), VAR_NAME(remove_dead_imperials), remove_dead_imperials);

    // - Re-fill settings.
    ini->SetBoolValue(Name(), VAR_NAME(add_npcs_to_party_window), add_npcs_to_party_window);
    for (const auto& user_defined_npc : user_defined_npcs) {
        if(!user_defined_npc || !user_defined_npc->model_id)
            continue;
        std::string s(user_defined_npc->alias);
        s += "\x1";
        s += std::to_string((uint32_t)user_defined_npc->map_id);
        ini->SetValue(Name(), std::to_string(user_defined_npc->model_id).c_str(), s.c_str());
    }
}
void PartyWindowModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    // get all keys in a section
    CSimpleIni::TNamesDepend keys;
    ini->GetAllKeys(Name(), keys);
    if (keys.empty())
        return LoadDefaults();

    add_npcs_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(add_npcs_to_party_window), add_npcs_to_party_window);
    add_player_numbers_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(add_player_numbers_to_party_window), add_player_numbers_to_party_window);
    add_elite_skill_to_summons = ini->GetBoolValue(Name(), VAR_NAME(add_elite_skill_to_summons), add_elite_skill_to_summons);
    remove_dead_imperials = ini->GetBoolValue(Name(), VAR_NAME(remove_dead_imperials), remove_dead_imperials);

    ClearSpecialNPCs();
    for (const auto& key : keys) {
        char* p;
        long model_id = strtol(key.pItem, &p, 10);
        if (!p || model_id < 1)
            continue; // Not a model_id
        std::string value(ini->GetValue(Name(), key.pItem, ""));
        if (value.empty())
            continue;
        size_t name_end_pos = value.find("\x1");
        if (name_end_pos == std::string::npos)
            continue;
        std::string alias(value.substr(0, name_end_pos));
        if (alias.empty())
            continue;
        p = nullptr;
        long map_id = strtol(value.substr(name_end_pos + 1).c_str(), &p, 10);
        if (!p || map_id < 0 || map_id >= static_cast<long>(GW::Constants::MapID::Count))
            continue; // Invalid map_id
        AddSpecialNPC({ alias.c_str(), model_id,static_cast<GW::Constants::MapID>(map_id) });
    }
    CheckMap();
}

