#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/GameContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Modules/PartyWindowModule.h>

namespace {
    static bool IsPvE() {
        GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        if (!map) return false;
        switch (static_cast<GW::RegionType>(map->type)) {
        case GW::RegionType::RegionType_AllianceBattle:
        case GW::RegionType::RegionType_Arena:
        case GW::RegionType::RegionType_GuildBattleArea:
        case GW::RegionType::RegionType_CompetitiveMission:
        case GW::RegionType::RegionType_ZaishenBattle:
        case GW::RegionType::RegionType_HeroesAscent:
        case GW::RegionType::RegionType_HeroBattleArea:
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
        GW::PartyContext* p = GW::GameContext::instance()->party;
        if (!p || !p->parties.valid() || party_id >= p->parties.size())
            return nullptr;
        return (PartyInfo*)p->parties[party_id];
    }
    static void SetPlayerNumber(wchar_t* player_name, uint32_t player_number) {
        wchar_t buf[32] = { 0 };
        wnsprintfW(buf, 32, L"%s (%d)", player_name, player_number);
        if(wcsncmp(player_name,buf,wcslen(buf)) != 0)
            wcscpy(player_name, buf);
    }
    static bool is_explorable = false;

    static bool SetAgentName(const uint32_t agent_id, const wchar_t* name) {
        GW::AgentLiving* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
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

    static bool IsPvP() {
        GW::AreaInfo* i = GW::Map::GetCurrentMapInfo();
        if (!i) return false;
        switch (i->type) {
        case GW::RegionType_AllianceBattle:
        case GW::RegionType_Arena:
        case GW::RegionType_CompetitiveMission:
        case GW::RegionType_GuildBattleArea:
        case GW::RegionType_HeroBattleArea:
        case GW::RegionType_HeroesAscent:
        case GW::RegionType_ZaishenBattle:
            return true;
        default:
            return false;
        }
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
            if (std::find(allies_added_to_party.begin(), allies_added_to_party.end(), pak->agent_id) == allies_added_to_party.end())
                return; // Not added via toolbox
            pending_remove.push(pak->agent_id);
        });
    // Remove certain NPCs from party window when despawned
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(
        &AgentRemove_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::AgentRemove* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            if (std::find(allies_added_to_party.begin(), allies_added_to_party.end(), pak->agent_id) == allies_added_to_party.end())
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
            pending_add.push_back({ pak->agent_id, pak->allegiance_bits, pak->agent_type ^ 0x20000000 });
        });
    // Flash/focus window on zoning (and a bit of housekeeping)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &GameSrvTransfer_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* pak) -> void {
            UNREFERENCED_PARAMETER(status);
            allies_added_to_party.clear();
            while (pending_remove.size())
                pending_remove.pop();
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
        while (pending_remove.size()) {
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
}
void PartyWindowModule::CheckMap() {
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
        return;
    if (!add_npcs_to_party_window) {
        ClearAddedAllies();
        return;
    }
    GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid())
        return;
    for (unsigned int i = 0; i < allies_added_to_party.size(); i++) {
        if (allies_added_to_party[i] >= agents.size()) {
            pending_remove.push(allies_added_to_party[i]);
            continue;
        }
        GW::AgentLiving* a = agents[allies_added_to_party[i]] ? agents[allies_added_to_party[i]]->GetAsAgentLiving() : nullptr;
        if (!a || !a->IsNPC()) {
            pending_remove.push(allies_added_to_party[i]);
            continue;
        }
        std::map<uint32_t, SpecialNPCToAdd*>::iterator it = user_defined_npcs_by_model_id.find(a->player_number);
        if (it != user_defined_npcs_by_model_id.end())
            continue;
        pending_remove.push(allies_added_to_party[i]);
    }
    for (unsigned int i = 0; i < agents.size(); i++) {
        GW::AgentLiving* a = agents[i] ? agents[i]->GetAsAgentLiving() : nullptr;
        if (!ShouldAddAgentToPartyWindow(a))
            continue;
        pending_add.push_back({ a->agent_id,0,a->player_number });
    }
}
void PartyWindowModule::SignalTerminate() {
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&AgentRemove_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentState>(&AgentState_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry);
    ClearAddedAllies();
}
bool PartyWindowModule::CanTerminate() {
    return allies_added_to_party.empty();
}
void PartyWindowModule::RemoveAllyActual(uint32_t agent_id) {
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
        goto leave;
    GW::Packet::StoC::PartyRemoveAlly packet;
    packet.header = GW::Packet::StoC::PartyRemoveAlly::STATIC_HEADER;
    packet.agent_id = agent_id;

    GW::AgentLiving* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
    float prev_hp = 0.0f;
    wchar_t prev_name[8] = { 0 };
    if (a) {
        wcscpy(prev_name, GW::Agents::GetAgentEncName(a));
        prev_hp = a->hp;
    }
    // 1. Remove NPC from window.
    GW::StoC::EmulatePacket(&packet);
    SetAgentName(agent_id, prev_name);
leave:
    std::vector<uint32_t>::iterator it = std::find(allies_added_to_party.begin(), allies_added_to_party.end(), agent_id);
    if (it != allies_added_to_party.end())
        allies_added_to_party.erase(it);
}
void PartyWindowModule::AddAllyActual(PendingAddToParty &p) {
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
void PartyWindowModule::ClearAddedAllies() {
    for (size_t i = 0; i < allies_added_to_party.size(); i++) {
        pending_remove.push(allies_added_to_party[i]);
    }
}
bool PartyWindowModule::ShouldRemoveAgentFromPartyWindow(uint32_t agent_id) {
    GW::AgentLiving* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
    if (!a || !a->GetIsLivingType() || !(a->type_map & 0x20000))
        return false; // Not in party window
    for (size_t i = 0; i < allies_added_to_party.size(); i++) {
        if (a->agent_id != allies_added_to_party[i])
            continue;
        // Ally turned enemy, or is dead.
        return a->allegiance == 0x3 || a->GetIsDead() || a->GetIsDeadByTypeMap();
    }
    return false;
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
}
bool PartyWindowModule::ShouldAddAgentToPartyWindow(uint32_t agent_type) {
    if ((agent_type & 0x20000000) == 0)
        return false; // Not an NPC
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
        return false; // Not in an explorable area
    GW::Constants::MapID map_id = GW::Map::GetMapID();
    uint32_t player_number = (agent_type ^ 0x20000000);
    std::map<uint32_t, SpecialNPCToAdd*>::iterator it = user_defined_npcs_by_model_id.find(player_number);
    if (it == user_defined_npcs_by_model_id.end())
        return false;
    if (it->second->map_id != GW::Constants::MapID::None && it->second->map_id != map_id)
        return false;
    return true;
}
bool PartyWindowModule::ShouldAddAgentToPartyWindow(GW::Agent* _a) {
    GW::AgentLiving* a = _a ? _a->GetAsAgentLiving() : nullptr;
    if (!a || !a->IsNPC())
        return false;
    if (a->GetIsDead() || a->GetIsDeadByTypeMap() || a->allegiance == 0x3)
        return false; // Dont add dead NPCs.
    std::vector<uint32_t>::iterator it = std::find(allies_added_to_party.begin(), allies_added_to_party.end(), a->agent_id);
    if (it != allies_added_to_party.end())
        return false;
    return ShouldAddAgentToPartyWindow(0x20000000u | a->player_number);
}
void PartyWindowModule::DrawSettingInternal() {
    ImGui::Checkbox("Add player numbers to party window", &add_player_numbers_to_party_window);
    ImGui::ShowHelp("Will update on next map");
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
    for (size_t i = 0; i < user_defined_npcs.size();i++) {
        SpecialNPCToAdd* npc = user_defined_npcs[i];
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
        ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - (48.0f * fontScale));
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
            return Error("Invalid model id");
        if (new_npc_map_id < 0 || new_npc_map_id > static_cast<int>(GW::Constants::MapID::Count))
            return Error("Invalid map id");
        std::string alias_str(new_npc_alias);
        if (alias_str.empty())
            return Error("Empty value for Name");
        std::map<uint32_t, SpecialNPCToAdd*>::iterator it = user_defined_npcs_by_model_id.find(static_cast<uint32_t>(new_npc_model_id));
        if (it != user_defined_npcs_by_model_id.end())
            return Error("Special NPC %s is already defined for model_id %d", it->second->alias.c_str(), new_npc_model_id);
        AddSpecialNPC({ alias_str.c_str(), new_npc_model_id,static_cast<GW::Constants::MapID>(new_npc_map_id) });
        Log::Info("Added special NPC %s (%d)", alias_str.c_str(), new_npc_model_id);
        CheckMap();
    }
}
void PartyWindowModule::Error(const char* format, ...) {
    char buffer[600];
    va_list args;
    va_start(args, format);
    snprintf(buffer, 599, format, args);
    va_end(args);

    // @Fix: Visual Studio 2015 doesn't seem to accept to capture c-style arrays
    std::string sbuffer(buffer);
    GW::GameThread::Enqueue([sbuffer]() {
        Log::Error(sbuffer.c_str());
    });
}
void PartyWindowModule::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    // Clear existing ini settings
    ini->Delete(Name(), NULL, NULL);

    ini->SetBoolValue(Name(), VAR_NAME(add_player_numbers_to_party_window), add_player_numbers_to_party_window);
    
    // - Re-fill settings.
    ini->SetBoolValue(Name(), VAR_NAME(add_npcs_to_party_window), add_npcs_to_party_window);
    for (size_t i = 0; i < user_defined_npcs.size(); i++) {
        if(!user_defined_npcs[i] || !user_defined_npcs[i]->model_id)
            continue;
        std::string s(user_defined_npcs[i]->alias);
        s += "\x1";
        s += std::to_string((uint32_t)user_defined_npcs[i]->map_id);
        ini->SetValue(Name(), std::to_string(user_defined_npcs[i]->model_id).c_str(), s.c_str());
    }
}
void PartyWindowModule::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    // get all keys in a section
    CSimpleIniA::TNamesDepend keys;
    ini->GetAllKeys(Name(), keys);
    if (keys.empty())
        return LoadDefaults();

    add_npcs_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(add_npcs_to_party_window), add_npcs_to_party_window);
    add_player_numbers_to_party_window = ini->GetBoolValue(Name(), VAR_NAME(add_player_numbers_to_party_window), add_player_numbers_to_party_window);

    ClearSpecialNPCs();
    for (CSimpleIniA::TNamesDepend::const_iterator i = keys.begin(); i != keys.end(); ++i) {
        char* p;
        long model_id = strtol(i->pItem, &p, 10);
        if (!p || model_id < 1) 
            continue; // Not a model_id
        std::string value(ini->GetValue(Name(), i->pItem, ""));
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
GW::AgentLiving* PartyWindowModule::PendingAddToParty::GetAgent() {
    GW::AgentLiving* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
    if (!a || !a->GetIsLivingType() || a->player_number != player_number || a->allegiance == 0x3)
        return nullptr;
    GW::NPC* npc = GW::Agents::GetNPCByID(player_number);
    if (!npc)
        return nullptr;
    return a;
}
std::wstring* PartyWindowModule::SpecialNPCToAdd::GetMapName() {
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
