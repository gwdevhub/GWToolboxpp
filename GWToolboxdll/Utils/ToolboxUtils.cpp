#include "stdafx.h"

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Player.h>

#include <Utils/GuiUtils.h>

#include "ToolboxUtils.h"
#include <GWCA/Packets/Opcodes.h>

namespace ToolboxUtils {
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
                auto c = GW::CharContext::instance();
                return c ? c->player_name : L"";
            }
        }
        else {
            player = GW::PlayerMgr::GetPlayerByID(player_number);
        }
        return player && player->name ? GuiUtils::SanitizePlayerName(player->name) : L"";
    }
    GW::Array<wchar_t>* GetMessageBuffer() {
        auto* w = GW::WorldContext::instance();
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
}