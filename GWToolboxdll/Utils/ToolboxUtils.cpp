#include "stdafx.h"

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <Utils/GuiUtils.h>

#include "ToolboxUtils.h"

namespace ToolboxUtils {
    bool IsOutpost() {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost;
    }
    bool IsExplorable() {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    GW::Player* GetPlayerByName(const wchar_t* _name) {
        if (!_name) return NULL;
        std::wstring name = GuiUtils::SanitizePlayerName(_name);
        GW::PlayerArray* players = GW::PlayerMgr::GetPlayerArray();
        if (!players) return nullptr;
        for (GW::Player& player : *players) {
            if (!player.name) continue;
            if (name == GuiUtils::SanitizePlayerName(player.name))
                return &player;
        }
        return nullptr;
    }
    std::wstring GetPlayerName(uint32_t player_number) {
        GW::Player* player = nullptr;
        if (!player_number) {
            player = GW::PlayerMgr::GetPlayerByID(GW::PlayerMgr::GetPlayerNumber());
            if (!player || !player->name) {
                // Map not loaded; try to get from character context
                auto c = GW::GetCharContext();
                return c ? c->player_name : L"";
            }
        }
        else {
            player = GW::PlayerMgr::GetPlayerByID(player_number);
        }
        return player && player->name ? GuiUtils::SanitizePlayerName(player->name) : L"";
    }
    GW::Array<wchar_t>* GetMessageBuffer() {
        auto* w = GW::GetWorldContext();
        return w && w->message_buff.valid() ? &w->message_buff : nullptr;
    }
    const wchar_t* GetMessageCore() {
        auto* buf = GetMessageBuffer();
        return buf ? buf->begin() : nullptr;
    }
    bool ClearMessageCore() {
        auto* buf = GetMessageBuffer();
        if (!buf)
            return false;
        buf->clear();
        return true;
    }
    const std::wstring GetSenderFromPacket(GW::Packet::StoC::PacketBase* packet) {
        switch (packet->header) {
        case GAME_SMSG_CHAT_MESSAGE_GLOBAL: {
            auto p = (GW::Packet::StoC::MessageGlobal*)packet;
            return p->sender_name;
        } break;
        case GAME_SMSG_CHAT_MESSAGE_LOCAL:
        case GAME_SMSG_TRADE_REQUEST:
            return GetPlayerName(((uint32_t*)packet)[1]);
        }
        return L"";
    }
    GW::Player* GetPlayerByAgentId(uint32_t agent_id, GW::AgentLiving** info_out) {
        const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!(agent && agent->GetIsLivingType() && agent->IsPlayer()))
            return nullptr;
        if (info_out)
            *info_out = agent;
        return GW::PlayerMgr::GetPlayerByID(agent->login_number);

    }

    GW::HeroInfo* GetHeroInfo(uint32_t hero_id) {
        auto w = GW::GetWorldContext();
        if (!(w && w->hero_info.size()))
            return nullptr;
        for (auto& a : w->hero_info) {
            if (a.hero_id == hero_id)
                return &a;
        }
        return nullptr;
    }
    bool IsHenchman(uint32_t agent_id) {
        if (!IsOutpost()) {
            return IsHenchmanInParty(agent_id);
        }
        auto w = GW::GetWorldContext();
        if (!(w && w->henchmen_agent_ids.size()))
            return false;
        for (auto a : w->henchmen_agent_ids) {
            if (a == agent_id)
                return true;
        }
        return false;
    }
    bool IsHero(uint32_t agent_id, GW::HeroInfo** info_out) {
        if (!IsOutpost()) {
            // NB: HeroInfo array is only populated in outposts
            return IsHeroInParty(agent_id);
        }
        auto w = GW::GetWorldContext();
        if (!(w && w->hero_info.size()))
            return false;
        for (auto& a : w->hero_info) {
            if (a.agent_id == agent_id) {
                if (info_out)
                    *info_out = &a;
                return true;
            }
        }
        return false;
    }


    GW::Array<GW::PartyInfo*>* GetParties() {
        auto* p = GW::GetPartyContext();
        return p ? &p->parties : nullptr;
    }

    const GW::HenchmanPartyMember* GetHenchmanPartyMember(uint32_t agent_id, GW::PartyInfo** party_out) {
        const auto* parties = GetParties();
        if (!parties) return nullptr;
        for (const auto party : *parties) {
            for (const auto& p : party->henchmen) {
                if (p.agent_id != agent_id)
                    continue;
                if (party_out)
                    *party_out = party;
                return &p;
            }
        }
        return nullptr;
    }
    bool IsHenchmanInParty(uint32_t agent_id) {
        GW::PartyInfo* party = nullptr;
        return GetHenchmanPartyMember(agent_id, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    const GW::HeroPartyMember* GetHeroPartyMember(uint32_t agent_id, GW::PartyInfo** party_out) {
        const auto* parties = GetParties();
        if (!parties) return nullptr;
        for (const auto party : *parties) {
            for (const auto& p : party->heroes) {
                if (p.agent_id != agent_id)
                    continue;
                if (party_out)
                    *party_out = party;
                return &p;
            }
        }
        return nullptr;
    }
    bool IsHeroInParty(uint32_t agent_id) {
        GW::PartyInfo* party = nullptr;
        return GetHeroPartyMember(agent_id, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    const GW::PlayerPartyMember* GetPlayerPartyMember(uint32_t login_number, GW::PartyInfo** party_out) {
        const auto* parties = GetParties();
        if (!parties) return nullptr;
        for (const auto party : *parties) {
            for (const auto& p : party->players) {
                if (p.login_number != login_number)
                    continue;
                if (party_out)
                    *party_out = party;
                return &p;
            }
        }
        return nullptr;
    }
    bool IsPlayerInParty(uint32_t login_number) {
        GW::PartyInfo* party = nullptr;
        return GetHeroPartyMember(login_number, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    bool IsAgentInParty(uint32_t agent_id) {
        const auto* party = GW::PartyMgr::GetPartyInfo();
        if (!party)
            return false;
        if (IsHenchmanInParty(agent_id) || IsHeroInParty(agent_id))
            return true;
        const auto player = GetPlayerByAgentId(agent_id);
        return player && IsPlayerInParty(player->player_number);
    }
}
