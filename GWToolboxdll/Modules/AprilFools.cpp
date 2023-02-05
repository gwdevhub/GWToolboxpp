#include "stdafx.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Player.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Logger.h>
#include <Modules/AprilFools.h>

namespace {
    bool enabled = false;
    

    std::map<uint32_t, GW::Agent*> player_agents;

    GW::HookEntry AgentAdd_Hook;
    GW::HookEntry AgentRemove_Hook;
    GW::HookEntry GameSrvTransfer_Hook;



    void OnAgentAdd(GW::HookStatus*, GW::Packet::StoC::AgentAdd* packet) {
        if (!enabled)
            return;
        if ((packet->agent_type & 0x30000000) != 0x30000000)
            return; // Not a player
        uint32_t player_number = packet->agent_type ^ 0x30000000;
        GW::AgentLiving* agent = (GW::AgentLiving*)GW::Agents::GetAgentByID(GW::Agents::GetAgentIdByLoginNumber(player_number));
        if (!agent || !agent->GetIsLivingType() || !agent->IsPlayer())
            return; // Not a valid agent
        player_agents.emplace(agent->agent_id, agent);
    }
    void OnAgentRemove(GW::HookStatus*, GW::Packet::StoC::AgentRemove* packet) {
        if (!enabled)
            return;
        auto found = player_agents.find(packet->agent_id);
        if (found != player_agents.end())
            player_agents.erase(found);
    }
    void OnGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) {
        player_agents.clear();
    }
    bool listeners_added = false;
    void AddListeners() {
        if (listeners_added) return;
        GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Hook,OnAgentAdd);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(&AgentRemove_Hook,OnAgentRemove);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Hook,OnGameSrvTransfer);
        listeners_added = true;
    }
    void RemoveListeners() {
        if (!listeners_added) return;
        GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&AgentAdd_Hook);
        GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&AgentRemove_Hook);
        GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Hook);
        listeners_added = false;
    }
}

static const wchar_t* af_2020_quotes[] = {
    L"Happy April Fools Da-- *cough*",
    L"I don't feel so good...",
    L"*cough* *cough*",
    L"I'm down to my last roll of toilet paper!",
    L"Day 5 of self isolation...",
    L"Get ready for the baby boom after all this blows over!",
    L"Hell of a commute this morning from my bed to my PC...",
    L"Do I look ill to you?",
    L"Urpp... feel sick...",
    L"Schools are closed, my kids are driving me mad and I just want to play Guild Wars!",
    L"DoA Quarantineway anyone?",
    L"'Working from home' is just a good excuse to play video games during the day",
    L"Nothing like a global pandemic to bring people togeth-- nevermind.",
    L"Stay away from crowded places? You mean like Kamadan? No way!",
    L"Nothing left to do but 'Netflix and ill'!",
    L"Knew I should have put shares into the paracetamol stock market this year",
    L"I swear thats the last time I eat Canthan food again!"
};
static const int af_quotes_length = sizeof(af_2020_quotes) / 4;
void AprilFools::Initialize() {
    ToolboxModule::Initialize();
    GW::Chat::CreateCommand(L"aprilfools", [this](const wchar_t*, int, LPWSTR*) -> void {
        SetEnabled(!enabled);
    });
    
    time_t now = time(NULL);
    struct tm* ltm = gmtime(&now);
    SetEnabled(ltm->tm_mon == 3 && ((ltm->tm_mday == 1 && ltm->tm_hour > 6) || (ltm->tm_mday == 2 && ltm->tm_hour < 7)));
}
void AprilFools::Terminate() {
    ToolboxModule::Terminate();
    GW::Chat::DeleteCommand(L"aprilfools");
    RemoveListeners();
}
void AprilFools::SetEnabled(bool is_enabled) {
    if (enabled == is_enabled)
        return;
    enabled = is_enabled;
    if (enabled) {
        GW::PlayerArray* players = GW::Agents::GetPlayerArray();
        if (!players) return;
        for (auto& player : *players) {
            auto agent = GW::Agents::GetAgentByID(player.agent_id);
            if (agent)
                player_agents.emplace(player.agent_id, agent);
        }
        Log::Info("April Fools 2020 enabled. Type '/aprilfools' to disable it");
        AddListeners();

    }
    else {
        for (const auto& agent : player_agents) {
            SetInfected(agent.second, false);
        }
        player_agents.clear();
        Log::Info("April Fools 2020 disabled. Type '/aprilfools' to enable it");
        RemoveListeners();
    }
}
void AprilFools::SetInfected(GW::Agent* agent,bool is_infected) {
    uint32_t agent_id = agent->agent_id;
    if (!is_infected) {
        GW::GameThread::Enqueue([agent_id]() {
            GW::Packet::StoC::GenericValue packet;
            packet.agent_id = agent_id;
            packet.value_id = 7; // Remove effect
            packet.value = 26; // Disease
            GW::StoC::EmulatePacket(&packet);
            });
        return;
    }
    static bool infection_queued = false;
    if (infection_queued)
        return;
    infection_queued = true;
    SetInfected(agent, false);
    static int last_quote_idx = -1;
    GW::GameThread::Enqueue([agent_id]() {
        GW::Packet::StoC::GenericValue packet;
        packet.agent_id = agent_id;
        packet.value_id = 6; // Add effect
        packet.value = 26; // Disease
        GW::StoC::EmulatePacket(&packet);
        GW::Packet::StoC::SpeechBubble packet2;
        packet2.agent_id = agent_id;
        int quote_idx = last_quote_idx; 
        while(quote_idx == last_quote_idx)
            quote_idx = rand() % af_quotes_length;
        last_quote_idx = quote_idx;
        swprintf(packet2.message, 122, L"\x108\x107%s\x1", af_2020_quotes[quote_idx]);
        GW::StoC::EmulatePacket(&packet2);
        infection_queued = false;
        });
}
void AprilFools::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (!enabled)
        return;
    // @Cleanup: This is why you have an object, don't use static here.
    static clock_t next_infection = 0;
    static GW::Constants::InstanceType last_instance_type = GW::Map::GetInstanceType();
    if (GW::Map::GetInstanceType() != last_instance_type) {
        next_infection = clock() + 5000;
        last_instance_type = GW::Map::GetInstanceType();
    }
    if (last_instance_type != GW::Constants::InstanceType::Outpost) {
        if (!player_agents.empty())
            player_agents.clear();
        return;
    }
    if (clock() > next_infection && !player_agents.empty()) {
        if (player_agents.size()) {
            auto it = player_agents.begin();
            std::advance(it, rand() % player_agents.size());
            SetInfected(it->second, true);
        }
        uint32_t infection_interval = ((60 / player_agents.size()) * CLOCKS_PER_SEC);
        if (infection_interval < 2500)
            infection_interval = 2500;
        // @Cleanup: Not sure this is correct
        next_infection = clock() + static_cast<clock_t>(infection_interval);
    }
}
