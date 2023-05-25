#include "stdafx.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/TradeMgr.h>

#include <Logger.h>
#include <Timer.h>
#include <Color.h>

#include <Modules/Resources.h>
#include <Windows/FriendListWindow.h>

#include <Utils/ToolboxUtils.h>


/* Out of scope namespecey lookups */
using namespace ToolboxUtils;
namespace
{
    std::thread settings_thread;
    FriendListWindow& Instance() {
        return FriendListWindow::Instance();
    }

    const ImColor ProfColors[11] = {0xFFFFFFFF, 0xFFEEAA33, 0xFF55AA00,
                                    0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                                    0xFFBB3333, 0xFFAA0088, 0xFF00AAAA,
                                    0xFF996600, 0xFF7777CC};
    const wchar_t *ProfNames[11] = {
        L"Unknown",     L"Warrior", L"Ranger",       L"Monk",
        L"Necromancer", L"Mesmer",  L"Elementalist", L"Assassin",
        L"Ritualist",   L"Paragon", L"Dervish"};
    const ImColor StatusColors[5] = {
        IM_COL32(0x99, 0x99, 0x99, 255), // offline
        IM_COL32(0x0, 0xc8, 0x0, 255),   // online
        IM_COL32(0xc8, 0x0, 0x0, 255),   // busy
        IM_COL32(0xc8, 0xc8, 0x0, 255),   // away
        IM_COL32(0x99, 0x99, 0x99, 255) // offline
    };
    const char* statuses[] = { "Offline", "Online", "Busy", "Away", "Disconnected" };
    std::wstring current_map;
    GW::Constants::MapID current_map_id = GW::Constants::MapID::None;
    std::wstring *GetCurrentMapName()
    {
        const GW::Constants::MapID map_id = GW::Map::GetMapID();
        if (current_map_id != map_id) {
            const GW::AreaInfo *i = GW::Map::GetMapInfo(map_id);
            if (!i)
                return nullptr;
            wchar_t name_enc[16];
            if (!GW::UI::UInt32ToEncStr(i->name_id, name_enc, 16))
                return nullptr;
            current_map.clear();
            GW::UI::AsyncDecodeStr(name_enc, &current_map);
            current_map_id = map_id;
        }
        return &current_map;
    }
    const char* GetStatusText(GW::FriendStatus status)
    {
        switch (status) {
            case GW::FriendStatus::Offline:
                return "Offline";
            case GW::FriendStatus::Online:
                return "Online";
            case GW::FriendStatus::DND:
                return "Do not disturb";
            case GW::FriendStatus::Away:
                return "Away";
        }
        return "Unknown";
    }

    std::map<uint32_t, wchar_t *> map_names;
    GUID StringToGuid(const std::string &str)
    {
        GUID guid{};
        sscanf(str.c_str(),
               "%8lx-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
               &guid.Data1, &guid.Data2, &guid.Data3, &guid.Data4[0],
               &guid.Data4[1], &guid.Data4[2], &guid.Data4[3], &guid.Data4[4],
               &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

        return guid;
    }

    std::string GuidToString(const GUID& guid)
    {
        return std::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
            guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
            guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
    std::wstring last_whisper;
    std::wstring ParsePlayerName(int argc, LPWSTR* argv) {
        std::wstring player_name;
        for (int i = 0; i < argc; i++) {
            std::wstring s(argv[i]);
            if (s.empty())
                continue;
            if (!player_name.empty())
                player_name += L" ";
            std::transform(s.begin() + 1, s.end(), s.begin() + 1, towlower);
            std::transform(s.begin(), s.begin() + 1, s.begin(), towupper);
            player_name += s;
        }
        return player_name;
    }
    // Encoded message types as received via encoded chat message
    enum class MessageType : wchar_t {
        CANNOT_ADD_YOURSELF_AS_A_FRIEND = 0x2f3,
        EXCEEDED_MAX_NUMBER_OF_FRIENDS,
        CHARACTER_NAME_X_DOES_NOT_EXIST,
        FRIEND_ALREADY_ADDED_AS_X,
        INCOMING_WHISPER = 0x76d,
        OUTGOING_WHISPER,
        PLAYER_NAME_IS_INVALID = 0x880,
        PLAYER_X_NOT_ONLINE
    };
    bool WriteError(MessageType message_type, const wchar_t* character_name) {
        wchar_t buffer[122];
        const GW::Chat::Channel channel = GW::Chat::CHANNEL_GLOBAL;
        switch (message_type) {
        case MessageType::CHARACTER_NAME_X_DOES_NOT_EXIST:
        case MessageType::FRIEND_ALREADY_ADDED_AS_X:
            ASSERT(swprintf(buffer, _countof(buffer), L"%c\x107%s\x1", message_type, character_name) > 0);
            break;
        case MessageType::PLAYER_X_NOT_ONLINE:
            ASSERT(swprintf(buffer, _countof(buffer), L"%c\x101\x100\x107%s\x1\x108\x1", message_type, character_name) > 0);
            break;
        default:
            return false;
        }
        GW::Chat::WriteChatEnc(channel, buffer);
        return true;
    }
    // When a whisper is being redirected by this module, this flag is set. Stops infinite redirect loops.
    bool is_redirecting_whisper = false;
    struct PendingWhisper {
        std::wstring charname;
        std::wstring message;
        clock_t pending_add = 0;
        inline void reset(std::wstring _charname = L"", std::wstring _message = L"") {
            charname = _charname;
            message = _message;
            pending_add = 0;
        }
    };
    PendingWhisper pending_whisper;
    void UpdatePendingWhisper() {
        if (!pending_whisper.pending_add)
            return;
        if (TIMER_DIFF(pending_whisper.pending_add) > 5000) {
            pending_whisper.reset(); // Timeout reached adding player to friend list.
            return;
        }
        // Check if pending player has been added.
        auto& instance = Instance();
        instance.Poll();
        const auto lf = instance.GetFriend(pending_whisper.charname.c_str());
        if (!(lf && lf->ValidUuid()))
            return;
        // This is a player that TB has added to friend list to find out who they're actually playing on.

        if (lf->IsOffline()) {
            // If they're still not online, then show the "Player is not online" error
            ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
            pending_whisper.reset();
            return;
        }

        // If they're online, send the original message...
        ASSERT(lf->current_char);
        is_redirecting_whisper = true;
        GW::Chat::SendChat(lf->current_char->getNameW().c_str(), pending_whisper.message.c_str());
        is_redirecting_whisper = false;
        pending_whisper.reset();

        // ... then remove from GW.
        ASSERT(lf->RemoveGWFriend());
        ASSERT(instance.RemoveFriend(lf));
    }
    void OnAddFriendError(GW::HookStatus* status, wchar_t*) {
        if (!pending_whisper.charname.empty()) {
            ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
            pending_whisper.reset();
            status->blocked = true;
        }
    }
    // Record the pending outgoing whisper
    void OnOutgoingWhisper(GW::HookStatus *, int channel, wchar_t *message)
    {
        // If this outgoing whisper was created due to a redirect, or its not a whisper, drop out here.
        if (is_redirecting_whisper || static_cast<GW::Chat::Channel>(channel) != GW::Chat::CHANNEL_WHISPER)
            return;
        wchar_t* separator_pos = wcschr(message, ',');
        if (!separator_pos)
            return;
        pending_whisper.reset(std::wstring(message, separator_pos), std::wstring(&separator_pos[1]));
    }
    // Remove from pending whispers when whisper has been sent
    void OnOutgoingWhisperSuccess(GW::HookStatus *, wchar_t *)
    {
        pending_whisper.reset();
    }
    void OnPlayerNotOnline(GW::HookStatus *status, const wchar_t* message)
    {
        const auto player_name = GuiUtils::GetPlayerNameFromEncodedString(message);
        const auto friend_ = Instance().GetFriend(player_name.c_str());
        if (friend_) {
            // If this player is already in my friend list, send the message directly.
            if (!friend_->IsOffline() && friend_->current_char->getNameW() != player_name) {
                is_redirecting_whisper = true;
                GW::Chat::SendChat(friend_->current_char->getNameW().c_str(), pending_whisper.message.c_str());
                is_redirecting_whisper = false;
                pending_whisper.reset();
                status->blocked = true;
            }
            return;
        }
        if (pending_whisper.pending_add && player_name == pending_whisper.charname)
            return; // This is an error message generated by toolbox
        // Otherwise if this player isn't already in my friend list, add them temporarily. OnFriendCreated will then resend the message and remove the friend.
        if (!pending_whisper.charname.empty()) {
            GW::FriendListMgr::AddFriend(pending_whisper.charname.c_str());
            pending_whisper.pending_add = TIMER_INIT();
            status->blocked = true;
        }
    }
    void OnFriendAlreadyAdded(GW::HookStatus *status, const wchar_t* message)
    {
        const auto player_name = GuiUtils::GetPlayerNameFromEncodedString(message);
        const auto friend_ = Instance().GetFriend(player_name.c_str());
        if (friend_) {
            friend_->SetCharacter(player_name.c_str());
        }
        if (!pending_whisper.charname.empty()) {
            ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
            pending_whisper.reset();
            status->blocked = true;
        }
    }

    bool GetIsMapReady() {
        if (!GW::Map::GetIsMapLoaded())
            return false;
        const auto instance_type = GW::Map::GetInstanceType();
        return instance_type == GW::Constants::InstanceType::Explorable || instance_type == GW::Constants::InstanceType::Outpost;
    }

    bool cached_is_friend_list_ready = false;
    bool GetIsFriendListReady(bool fresh = false) {
        if (fresh) {
            const auto fl = GW::FriendListMgr::GetFriendList();
            cached_is_friend_list_ready = fl && fl->friends.size() > 0;
        }
        return cached_is_friend_list_ready;
    }

}
// Find out whether the player related to this packet is on the current player's ignore list.
bool FriendListWindow::GetIsPlayerIgnored(GW::Packet::StoC::PacketBase* pak) {
    switch (pak->header) {
        case GAME_SMSG_CHAT_MESSAGE_LOCAL:
        case GAME_SMSG_TRADE_REQUEST:
            return GetIsPlayerIgnored(((uint32_t*)pak)[1]);
        case GAME_SMSG_CHAT_MESSAGE_GLOBAL: {
            const auto p = (GW::Packet::StoC::MessageGlobal*)pak;
            return GetIsPlayerIgnored(std::wstring(p->sender_name));
        }
        case GAME_SMSG_PARTY_REQUEST_CANCEL:
        case GAME_SMSG_PARTY_REQUEST_RESPONSE:
        case GAME_SMSG_PARTY_JOIN_REQUEST: {
            const uint32_t party_id = ((uint32_t*)pak)[1];
            GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo(party_id);
            if (p && p->players.size()) {
                return GetIsPlayerIgnored(p->players[0].login_number);
            }
        } break;
    }
    return false;
}
// Find out whether a player in the current map is on the current player's ignore list.
bool FriendListWindow::GetIsPlayerIgnored(uint32_t player_number) {
    return GetIsPlayerIgnored(GetPlayerName(player_number));
}
// Find out whether this player's name is on the current player's ignore list.
bool FriendListWindow::GetIsPlayerIgnored(const std::wstring& player_name) {
    const auto* f = player_name.empty() ? nullptr : FriendListWindow::Instance().GetFriend(player_name.c_str());
    return f && f->type == GW::FriendType::Ignore;
}
void FriendListWindow::CmdAddFriend(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc < 2)
        return Log::Error("Missing player name");
    const auto player_name = ParsePlayerName(argc - 1, &argv[1]);
    if (player_name.empty())
        return Log::Error("Missing player name");
    GW::FriendListMgr::AddFriend(player_name.c_str());
}
void FriendListWindow::CmdRemoveFriend(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc < 2)
        return Log::Error("Missing player name");
    const auto player_name = ParsePlayerName(argc - 1, &argv[1]);
    if (player_name.empty())
        return Log::Error("Missing player name");
    FriendListWindow::Friend* f = FriendListWindow::Instance().GetFriend(player_name.c_str());
    if (!f) return Log::Error("No friend '%ls' found", player_name.c_str());
    f->RemoveGWFriend();
}
// Redirect /whisper player_name, message to GW::SendChat
void FriendListWindow::CmdWhisper(const wchar_t* message, int , LPWSTR* ) {
    const wchar_t* msg = wcschr(message, ' ');
    if(msg)
        GW::Chat::SendChat('"', msg + 1);
}
/*  FriendListWindow::Friend    */
// Get the Guild Wars friend object for this friend (if it exists)
GW::Friend* FriendListWindow::Friend::GetFriend() {
    return GW::FriendListMgr::GetFriend((uint8_t*)&uuid_bytes);
}
// Start whisper to this player via their current char name.
void FriendListWindow::Friend::StartWhisper() {
    if (!(current_char && !current_char->getNameW().empty())) {
        return Log::ErrorW(L"Player %s is not logged in", alias.c_str());
    }
    GW::GameThread::Enqueue([charname = current_char->getNameW()]() {
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, (wchar_t*)charname.data());
    });
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::GetCharacter(const wchar_t* char_name) {
    const auto it = characters.find(char_name);
    if (it == characters.end())
        return nullptr; // Not found
    return &it->second;
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::SetCharacter(const wchar_t* char_name, uint8_t profession) {
    Character* existing = GetCharacter(char_name);
    if (!existing) {
        Character c;
        c.setName(char_name);
        characters.emplace(c.getNameW(), c);
        existing = GetCharacter(c.getNameW().c_str());
        cached_charnames_hover = false;
    }
    if (profession && profession != existing->profession) {
        existing->profession = profession;
        cached_charnames_hover = false;
    }
    return existing;
}
// Remove this friend from the GW Friend list e.g. if its a toolbox friend, and we only added them for info.
bool FriendListWindow::Friend::RemoveGWFriend() {
    GW::Friend* f = GetFriend();
    if (!f) return false;
    GW::FriendListMgr::RemoveFriend(f);
    last_update = clock();
    return true;
}
bool FriendListWindow::Friend::ValidUuid() {
    const char* uuid_ptr = (char*)&uuid_bytes;
    for (size_t i = 0; i < sizeof(uuid_bytes); i++) {
        if (uuid_ptr[i] != 0)
            return true;
    }
    return false;
}

/* Setters */
// Update local friend record from raw info.
FriendListWindow::Friend* FriendListWindow::SetFriend(const uint8_t* uuid, const GW::FriendType type, const GW::FriendStatus status, const uint32_t map_id, const wchar_t* charname, const wchar_t* alias) {
    if (type != GW::FriendType::Friend && type != GW::FriendType::Ignore)
        return nullptr;
    // Validate UUID (When a friend is created GW doesn't immediately have the right UUID)
    bool is_valid_uuid = false;
    for (size_t i = 0;!is_valid_uuid && i < sizeof(UUID); i++) {
        is_valid_uuid = uuid[i] != 0;
    }
    if (!is_valid_uuid)
        return nullptr;
    // Try to get the existing Friend entry via uuid or charname
    Friend* lf = GetFriend(uuid);
    if (!lf && charname)
        lf = GetFriend(charname);
    if(!lf && alias)
        lf = GetFriend(alias);

    if(!lf) {
        // New friend (uuid_changed will trigger the add later)
        lf = new Friend(this);
    }
    const bool type_changed = lf->type != type;
    lf->type = type;
    const bool uuid_changed = memcmp(&lf->uuid_bytes,uuid,sizeof(UUID));
    const bool alias_changed = alias != lf->getAliasW();
    if (uuid_changed) {
        // UUID is different. This could be because GW has assigned a UUID to this friend.
        friends.erase(lf->uuid);
        lf->uuid_bytes = *(UUID*)uuid;
        lf->uuid = GuidToString(lf->uuid_bytes);
        friends.emplace(lf->uuid, lf);
    }
    if (alias && alias_changed) {
        // Friend's alias for this uuid has changed, or the uuid for this alias has changed.
        uuid_by_name.erase(lf->getAliasW());
        lf->setAlias(alias);
        uuid_by_name.emplace(lf->getAliasW(),lf);
    }
    if (lf->current_map_id != map_id) {
        // Map changed
        lf->current_map_name = Resources::GetMapName(static_cast<GW::Constants::MapID>(map_id));
    }

    // Check and copy charnames, only if player is NOT offline
    if (!charname || status == GW::FriendStatus::Offline)
        lf->current_char = nullptr;
    if (status != GW::FriendStatus::Offline && charname) {
        lf->current_char = lf->SetCharacter(charname);
        uuid_by_name.emplace(charname, lf);
    }
    const bool status_changed = lf->status != status;
    lf->status = status;

    if (status_changed || alias_changed || uuid_changed || type_changed) {
        need_to_reorder_friends = true;
    }
    
    friends_changed = true;
    return lf;
}
// Update local friend record from existing friend.
FriendListWindow::Friend* FriendListWindow::SetFriend(const GW::Friend* f) {
    return SetFriend(f->uuid, f->type, f->status, f->zone_id, (wchar_t*)&f->charname[0], (wchar_t*)&f->alias[0]);
}

/* Getters */
std::string FriendListWindow::Friend::GetCharactersHover(bool include_charname)
{
    if (!cached_charnames_hover) {
        std::wstring cached_charnames_hover_ws = L"Characters for ";
        cached_charnames_hover_ws += alias;
        cached_charnames_hover_ws += L":";
        for (auto it2 =
                 characters.begin();
             it2 != characters.end(); ++it2) {
            cached_charnames_hover_ws += L"\n  ";
            cached_charnames_hover_ws += it2->first;
            if (it2->second.profession) {
                cached_charnames_hover_ws += L" (";
                cached_charnames_hover_ws += ProfNames[it2->second.profession];
                cached_charnames_hover_ws += L")";
            }
        }
        cached_charnames_hover_str =
            GuiUtils::WStringToString(cached_charnames_hover_ws);
        cached_charnames_hover = true;
    }
    std::string str;
    if (include_charname && current_char) {
        str += current_char->getNameA();
        str += "\n";
    }
    if (include_charname && current_map_name && !current_map_name->string().empty()) {
        str += current_map_name->string();
        str += "\n";
    }
    if (!str.empty())
        str += "\n";
    str += cached_charnames_hover_str;
    return str;
}
// Find existing record for friend by char name.
FriendListWindow::Friend* FriendListWindow::GetFriend(const wchar_t* name) {
    const auto it = uuid_by_name.find(name);
    return it == uuid_by_name.end() ? nullptr : it->second;
}
// Find existing record for friend by GW Friend object
FriendListWindow::Friend* FriendListWindow::GetFriend(const GW::Friend* f) {
    return f ? GetFriend(f->uuid) : nullptr;
}
FriendListWindow::Friend* FriendListWindow::GetFriend(const uint8_t* uuid) {
    return GetFriendByUUID(GuidToString(*(UUID*)uuid));
}
// Find existing record for friend by uuid
FriendListWindow::Friend* FriendListWindow::GetFriendByUUID(const std::string& uuid) {
    const auto it = friends.find(uuid);
    return it == friends.end() ? nullptr : it->second;
}
bool FriendListWindow::RemoveFriend(Friend* f) {
    if (!f)
        return false;
    friends.erase(f->uuid);
    for (const auto& character : f->characters) {
        uuid_by_name.erase(character.first);
    }
    uuid_by_name.erase(f->getAliasW());
    delete f;
    return true;
}
/* FriendListWindow basic functions etc */
void FriendListWindow::Initialize() {
    ToolboxWindow::Initialize();

    inifile = new ToolboxIni(false, false, false);

    GW::Chat::CreateCommand(L"addfriend", CmdAddFriend);
    GW::Chat::CreateCommand(L"removefriend", CmdRemoveFriend);
    GW::Chat::CreateCommand(L"deletefriend", CmdRemoveFriend);
    GW::Chat::CreateCommand(L"tell", CmdWhisper);
    GW::Chat::CreateCommand(L"whisper", CmdWhisper);
    GW::Chat::CreateCommand(L"t", CmdWhisper);
    GW::Chat::CreateCommand(L"w", CmdWhisper);
    GW::Chat::CreateCommand(L"away", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Away); });
    GW::Chat::CreateCommand(L"online", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Online); });
    GW::Chat::CreateCommand(L"offline", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Offline); });
    GW::Chat::CreateCommand(L"busy", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::DND); });
    GW::Chat::CreateCommand(L"dnd", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::DND); });

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusUpdate_Entry, OnFriendUpdated);
    GW::Chat::RegisterSendChatCallback(&SendChat_Entry, OnOutgoingWhisper);
    GW::Chat::RegisterPrintChatCallback(&SendChat_Entry, OnPrintChat);
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry,OnPlayerJoinInstance);

    const GW::UI::UIMessage hook_messages[] = {
        GW::UI::UIMessage::kSetAgentNameTagAttribs,
        GW::UI::UIMessage::kShowAgentNameTag,
        GW::UI::UIMessage::kWriteToChatLog
    };
    for (const auto message_id : hook_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_Entry, message_id, OnUIMessage);
    }

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PartyInviteReceived_Create>(&PlayerJoinInstance_Entry, OnPostPartyInvite);

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::TradeStart>(&PlayerJoinInstance_Entry, OnPostTradePacket);
}
void FriendListWindow::OnPrintChat(GW::HookStatus*, GW::Chat::Channel, wchar_t** message_ptr, FILETIME, int) {
    switch (static_cast<MessageType>(*message_ptr[0])) {
    case MessageType::INCOMING_WHISPER:
    case MessageType::OUTGOING_WHISPER:
        AddFriendAliasToMessage(message_ptr);
        break;
    }
}
void FriendListWindow::OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
    switch (message_id) {
    case GW::UI::UIMessage::kSetAgentNameTagAttribs:
    case GW::UI::UIMessage::kShowAgentNameTag: {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost || !Instance().friend_name_tag_enabled)
            break;
        const auto tag = static_cast<GW::UI::AgentNameTagInfo*>(wparam);
        const auto player_name = GuiUtils::GetPlayerNameFromEncodedString(tag->name_enc);
        const Friend* f = Instance().GetFriend(player_name.c_str());
        if (f && f->type == GW::FriendType::Friend)
            tag->text_color = Instance().friend_name_tag_color;
    } break;
    case GW::UI::UIMessage::kWriteToChatLog: {
        const auto uimsg = static_cast<UIChatMessage*>(wparam);
        wchar_t* message = uimsg->message;
        std::wstring message_w = message;
        switch (static_cast<MessageType>(message[0])) {
        case MessageType::CANNOT_ADD_YOURSELF_AS_A_FRIEND: // You cannot add yourself as a friend.
        case MessageType::EXCEEDED_MAX_NUMBER_OF_FRIENDS: // You have exceeded the maximum number of characters on your Friends list.
        case MessageType::PLAYER_NAME_IS_INVALID: // The player name is invalid
        case MessageType::CHARACTER_NAME_X_DOES_NOT_EXIST: // The Character name "" does not exist
            OnAddFriendError(status, message);
            break;
        case MessageType::FRIEND_ALREADY_ADDED_AS_X: // The Character you're trying to add is already in your friend list as "".
            OnFriendAlreadyAdded(status, message);
            break;
        case MessageType::OUTGOING_WHISPER: // Server has successfully sent your whisper
            OnOutgoingWhisperSuccess(status, message);
            break;
        case MessageType::PLAYER_X_NOT_ONLINE: // Player "" is not online. Redirect to the right person if we can find them!
            OnPlayerNotOnline(status, message);
            break;
        }
    } break;
    }
}
void FriendListWindow::OnFriendUpdated(GW::HookStatus*, const GW::Friend* old_state, const GW::Friend* new_state) {
    auto& instance = Instance();
    // Keep a log mapping char name to uuid. This is saved to disk.
    if (!new_state) {
        // Friend removed from friend list.
        if (!old_state) {
            return; // No old state or new state; ignore this event
        }
        instance.RemoveFriend(instance.GetFriend(old_state));
        return;
    }
    const auto lf = Instance().SetFriend(new_state);
    if (!lf) return;
    lf->last_update = clock();
}
void FriendListWindow::OnPostPartyInvite(GW::HookStatus*, GW::Packet::StoC::PartyInviteReceived_Create* pak) {
    if (GetIsPlayerIgnored(pak))
        GW::PartyMgr::RespondToPartyRequest(pak->target_party_id, false);
}
void FriendListWindow::OnPostTradePacket(GW::HookStatus*, GW::Packet::StoC::TradeStart* pak) {
    if (GetIsPlayerIgnored(pak))
        GW::Trade::CancelTrade();
}
// Record a friend's character profession when they join your map
void FriendListWindow::OnPlayerJoinInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak) {
    UNREFERENCED_PARAMETER(status);
    const auto player_name = GuiUtils::SanitizePlayerName(pak->player_name);
    const auto a = GW::PlayerMgr::GetPlayerByName(pak->player_name);
    if (!a || !a->primary)
        return;
    const auto profession = static_cast<uint8_t>(a->primary);
    Friend* f = Instance().GetFriend(player_name.data());
    if (!f)
        return;
    Character* fc = f->GetCharacter(player_name.data());
    if (!fc)
        return;
    if (profession > 0 && profession < 11)
        fc->profession = profession;
}
// Optionally add friend alias to incoming/outgoing messages
void FriendListWindow::AddFriendAliasToMessage(wchar_t** message_ptr) {
    FriendListWindow& instance = Instance();
    if (!instance.show_alias_on_whisper)
        return;
    wchar_t* message = *message_ptr;
    const wchar_t* name_start = wcschr(message, 0x107);
    ASSERT(name_start != nullptr);
    const wchar_t* name_end = wcschr(name_start, 0x1);
    ASSERT(name_end != nullptr);
    const std::wstring player_name(name_start + 1, name_end);
    const auto friend_ = instance.GetFriend(player_name.c_str());
    if (!friend_ || friend_->getAliasW() == player_name)
        return;
    static std::wstring new_message;
    new_message = std::wstring(message, (name_end - message));
    new_message += L" (";
    new_message += friend_->getAliasW();
    new_message += L")";
    new_message.append(name_end);
    // TODO; Would doing this cause a memory leak on the previous wchar_t* ?
    *message_ptr = (wchar_t*)new_message.c_str();
}
void FriendListWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (loading)
        return;
    if (!GW::Map::GetIsMapLoaded())
        return;
    const GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    friend_list_ready = fl && fl->friends.valid();
    if (!friend_list_ready)
        return;
    if (!poll_queued) {
        const int interval_check = poll_interval_seconds * CLOCKS_PER_SEC;
        if (!friends_list_checked || clock() - friends_list_checked > interval_check) {
            Poll();
        }
    }
    UpdatePendingWhisper();
}
void FriendListWindow::Poll() {
    if (loading || polling) return;
    if (!GetIsMapReady()) return;
    if (!GetIsFriendListReady(true)) return;
    polling = true;
    const clock_t now = clock();


    // 1. Remove friends from toolbox list that are no longer in gw list
    auto it = friends.begin();
    while (it != friends.end()) {
        Friend* lf = it->second;
        if (lf->GetFriend()) {
            // Friend exists in friend list, don't need to mess
            it++;
            continue;
        }
        // Removed from in-game friend list, delete from tb friend list.
        ASSERT(RemoveFriend(lf));
        it = friends.begin();
    }

    // 2. Update or add friends from gw list into toolbox list
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    ASSERT(fl);
    for (unsigned int i = 0; i < fl->friends.size(); i++) {
        const GW::Friend* f = fl->friends[i];
        if (!f) continue;
        Friend* lf = SetFriend(f->uuid, f->type, f->status, f->zone_id, f->charname, f->alias);
        if (!lf) continue;
        lf->last_update = now;
    }

    friends_list_checked = now;
    polling = false;
}
ImGuiWindowFlags FriendListWindow::GetWinFlags(ImGuiWindowFlags flags) const {
    if (IsWidget()) {
        flags |= ImGuiWindowFlags_NoTitleBar;
        flags |= ImGuiWindowFlags_NoScrollbar;
        if (lock_size_as_widget) {
            flags |= ImGuiWindowFlags_NoResize;
            flags |= ImGuiWindowFlags_AlwaysAutoResize;
        }
        if (lock_move_as_widget)
            flags |= ImGuiWindowFlags_NoMove;
        return flags;
    }
    return ToolboxWindow::GetWinFlags(flags);
}
const bool FriendListWindow::IsWidget() const
{
    return (explorable_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
        || (outpost_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        || (loading_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading);
}
const bool FriendListWindow::IsWindow() const
{
    return (explorable_show_as == 0 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
           || (outpost_show_as == 0 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
           || (loading_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading);
}
void FriendListWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!(visible && GetIsFriendListReady() && GetIsMapReady()))
        return;
    const bool is_widget = IsWidget();
    const bool is_window = IsWindow();
    if (!is_widget && !is_window)
        return;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 72.0f), ImGuiCond_FirstUseEver);
    const ImGuiIO* io = &ImGui::GetIO();
    const auto window_size = ImVec2(540.0f * io->FontGlobalScale, 512.0f * io->FontGlobalScale);
    const float cols[3] = { 180.0f * io->FontGlobalScale, 360.0f * io->FontGlobalScale, 540.0f * io->FontGlobalScale };
    ImGui::SetNextWindowSize(window_size, ImGuiCond_FirstUseEver);
    if (is_widget)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0).Value);
    const bool ok = ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
    if (is_widget)
        ImGui::PopStyleColor();
    if (!ok)
        return ImGui::End();

    unsigned int colIdx = 0;
    const bool show_charname = ImGui::GetContentRegionAvail().x > cols[0];
    const bool _show_location = ImGui::GetContentRegionAvail().x > cols[1];
    if (!is_widget) {
        ImGui::Text("Name");
        if (show_charname) {
            ImGui::SameLine(cols[colIdx]);
            ImGui::Text("Character(s)");
        }
        if (_show_location) {
            ImGui::SameLine(cols[++colIdx]);
            ImGui::Text("Map");
        }
        ImGui::Separator();
        ImGui::BeginChild("friend_list_scroll");
    }
    const float height = ImGui::GetTextLineHeightWithSpacing();
    if (show_my_status) {
        auto status = static_cast<uint32_t>(GW::FriendListMgr::GetMyStatus());
        ImGui::Text("You are:");
        ImGui::SameLine();
        const ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
        ImGui::TextColored(ImVec4(0, 0, 0, 1), statuses[status]);
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
        ImGui::TextColored(StatusColors[status].Value, statuses[status]);
        if (ImGui::IsItemClicked()) {
            status++;
            if (status == 4)
                status = 0;
            GW::FriendListMgr::SetFriendListStatus(static_cast<GW::FriendStatus>(status));
        }
    }
    std::vector<Friend*> friends_online;
    for (const auto& it : friends) {
        Friend* lfp = it.second;
        if (lfp->type != GW::FriendType::Friend) continue;
        // Get actual object instead of pointer just in case it becomes invalid half way through the draw.
        if (lfp->IsOffline()) continue;
        if (lfp->getAliasW().empty()) continue;
        friends_online.push_back(lfp);
    }
    std::ranges::sort(friends_online, [](const Friend* lhs, const Friend* rhs) {
        return lhs->getAliasW().compare(rhs->getAliasW()) < 0;
        });
    char tmpbuf[32];
    for (Friend* lfp : friends_online) {
        colIdx = 0;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover_background_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover_background_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
        ImGui::PushID(lfp->uuid.c_str());
        ImGui::Button("", ImVec2(ImGui::GetContentRegionAvail().x, height));
        const bool left_clicked = ImGui::IsItemClicked(0);
        const bool right_clicked = ImGui::IsItemClicked(1);
        // TODO: Rename, Remove, Ignore.
        //ImGui::BeginPopupContextItem();

        bool hovered = ImGui::IsItemHovered();
        ImGui::PopStyleVar(4);
        ImGui::SameLine(2.0f, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, StatusColors[static_cast<size_t>(lfp->status)].Value);
        ImGui::Bullet();
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(GetStatusText(lfp->status));
        ImGui::SameLine(0);
        const auto& alias = lfp->getAliasA();
        if (is_widget)
            ImGui::TextShadowed(alias.c_str());
        else
            ImGui::Text(alias.c_str());
        hovered = hovered || ImGui::IsItemHovered();
        if (!show_charname && hovered) {
            ImGui::SetTooltip(lfp->GetCharactersHover(true).c_str());
        }
        if (show_charname && lfp->current_char != nullptr) {
            ImGui::SameLine(cols[colIdx]);
            const auto& current_char_name_s = lfp->current_char->getNameA();
            const uint8_t prof = lfp->current_char->profession;
            if (prof) ImGui::PushStyleColor(ImGuiCol_Text, ProfColors[lfp->current_char->profession].Value);
            if (is_widget)
                ImGui::TextShadowed(current_char_name_s.c_str());
            else
                ImGui::Text(current_char_name_s.c_str());
            if (prof) ImGui::PopStyleColor();
            if (lfp->characters.size() > 1) {
                ImGui::SameLine(0, 0);
                snprintf(tmpbuf, sizeof(tmpbuf), " (+%d)", lfp->characters.size() - 1);
                if (is_widget)
                    ImGui::TextShadowed(tmpbuf);
                else
                    ImGui::Text(tmpbuf);
                hovered |= ImGui::IsItemHovered();
                if (hovered) {
                    ImGui::SetTooltip(lfp->GetCharactersHover().c_str());
                }
            }
            if (show_location) {

                if (lfp->current_map_name) {
                    ImGui::SameLine(cols[++colIdx]);
                    if (is_widget)
                        ImGui::TextShadowed(lfp->current_map_name->string().c_str());
                    else
                        ImGui::Text(lfp->current_map_name->string().c_str());
                }
            }
        }
        ImGui::PopID();
        if (left_clicked && !lfp->IsOffline())
            lfp->StartWhisper();
        if (right_clicked) {

        }
    }
    if (!is_widget)
        ImGui::EndChild();
    ImGui::End();
}
void FriendListWindow::DrawSettingInternal() {
    bool edited = false;
    edited |= ImGui::Checkbox("Lock size as widget", &lock_size_as_widget);
    ImGui::SameLine();
    edited |= ImGui::Checkbox("Lock move as widget", &lock_move_as_widget);
    const float dropdown_width = 160.0f * ImGui::GetIO().FontGlobalScale;
    ImGui::Text("Show as");
    ImGui::SameLine();
    ImGui::PushItemWidth(dropdown_width);
    edited |= ImGui::Combo("###show_as_outpost", &outpost_show_as, "Window\0Widget\0Hidden");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("in outpost");

    ImGui::Text("Show as");
    ImGui::SameLine();
    ImGui::PushItemWidth(dropdown_width);
    edited |= ImGui::Combo("###show_as_explorable", &explorable_show_as, "Window\0Widget\0Hidden");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("in explorable");

    ImGui::Text("Show as");
    ImGui::SameLine();
    ImGui::PushItemWidth(dropdown_width);
    edited |= ImGui::Combo("###show_as_loading", &loading_show_as, "Window\0Widget\0Hidden");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("while loading");

    Colors::DrawSettingHueWheel("Widget background hover color", &hover_background_color);
    ImGui::Checkbox("Show my status", &show_my_status);
    ImGui::ShowHelp("e.g. 'You are: Online'");

    ImGui::Checkbox("Custom name tag color for friends",&friend_name_tag_enabled);
    ImGui::ShowHelp("When targeting friends in an outpost");
    if (friend_name_tag_enabled) {
        Colors::DrawSettingHueWheel("Friend name tag color", &friend_name_tag_color);
    }
    DrawChatSettings();
}
void FriendListWindow::RegisterSettingsContent() {
    ToolboxUIElement::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent("Chat Settings", nullptr,
        [this](const std::string&, bool is_showing) {
            if (!is_showing) return;
            DrawChatSettings();
        },0.91f);
}

void FriendListWindow::DrawChatSettings() {
    ImGui::Checkbox("Show friend aliases when sending/receiving whispers", &show_alias_on_whisper);
    ImGui::ShowHelp("Only if your friend's alias is different to their character name");
}
void FriendListWindow::DrawHelp() {
    if (!ImGui::TreeNodeEx("Friend List Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth))
        return;
    ImGui::Bullet(); ImGui::Text("'/addfriend <character_name>' Add a character to your friend list.");
    ImGui::Bullet(); ImGui::Text("'/removefriend <character_name|alias>' Remove character to your friend list.");
    ImGui::Bullet(); ImGui::Text("'/away' Set your friend list status to 'Away'.");
    ImGui::Bullet(); ImGui::Text("'/online' Set your friend list status to 'Online'.");
    ImGui::Bullet(); ImGui::Text("'/offline' Set your friend list status to 'Offline'.");
    ImGui::Bullet(); ImGui::Text("'/busy' or '/dnd' Set your friend list status to 'Do Not Disturb'.");
    ImGui::TreePop();
}
void FriendListWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    lock_move_as_widget = ini->GetBoolValue(Name(), VAR_NAME(lock_move_as_widget), lock_move_as_widget);
    lock_size_as_widget = ini->GetBoolValue(Name(), VAR_NAME(lock_size_as_widget), lock_size_as_widget);
    show_alias_on_whisper = ini->GetBoolValue(Name(), VAR_NAME(show_alias_on_whisper), show_alias_on_whisper);

    outpost_show_as = ini->GetLongValue(Name(), VAR_NAME(outpost_show_as), outpost_show_as);
    loading_show_as = ini->GetLongValue(Name(), VAR_NAME(loading_show_as), loading_show_as);
    explorable_show_as = ini->GetLongValue(Name(), VAR_NAME(explorable_show_as), explorable_show_as);
    show_my_status = ini->GetBoolValue(Name(), VAR_NAME(show_my_status), show_my_status);

    hover_background_color = Colors::Load(ini, Name(), VAR_NAME(hover_background_color), hover_background_color);
    friend_name_tag_enabled = ini->GetBoolValue(Name(), VAR_NAME(friend_name_tag_enabled), friend_name_tag_enabled);
    friend_name_tag_color = Colors::Load(ini, Name(), VAR_NAME(friend_name_tag_color), friend_name_tag_color);

    LoadFromFile();
}
void FriendListWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(lock_move_as_widget), lock_move_as_widget);
    ini->SetBoolValue(Name(), VAR_NAME(lock_size_as_widget), lock_size_as_widget);
    ini->SetBoolValue(Name(), VAR_NAME(show_alias_on_whisper), show_alias_on_whisper);

    ini->SetLongValue(Name(), VAR_NAME(outpost_show_as), outpost_show_as);
    ini->SetLongValue(Name(), VAR_NAME(loading_show_as), loading_show_as);
    ini->SetLongValue(Name(), VAR_NAME(explorable_show_as), explorable_show_as);
    ini->SetBoolValue(Name(), VAR_NAME(show_my_status), show_my_status);

    Colors::Save(ini, Name(), VAR_NAME(hover_background_color), hover_background_color);
    ini->SetBoolValue(Name(), VAR_NAME(friend_name_tag_enabled), friend_name_tag_enabled);
    Colors::Save(ini, Name(), VAR_NAME(friend_name_tag_color), friend_name_tag_color);

    SaveToFile();
}
void FriendListWindow::SignalTerminate() {
    // Try to remove callbacks here.
    GW::FriendListMgr::RemoveFriendStatusCallback(&FriendStatusUpdate_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry);
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Entry);
}
void FriendListWindow::Terminate() {
    ToolboxWindow::Terminate();
    // Try to remove callbacks AGAIN here.
    GW::FriendListMgr::RemoveFriendStatusCallback(&FriendStatusUpdate_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry);
    // Free memory for Friends list.
    while (friends.begin() != friends.end()) {
        RemoveFriend(friends.begin()->second);
    }
    friends.clear();
    if (settings_thread.joinable())
        settings_thread.join();
    if (inifile) {
        delete inifile;
        inifile = nullptr;
    }
}
void FriendListWindow::LoadCharnames(const char* section, std::unordered_map<std::wstring, uint8_t>* out) {
    // Grab char names
    CSimpleIni::TNamesDepend values;
    inifile->GetAllValues(section, "charname", values);
    CSimpleIni::TNamesDepend::const_iterator i;
    for (i = values.begin(); i != values.end(); ++i) {
        std::wstring char_wstr = GuiUtils::StringToWString(i->pItem), temp;
        std::vector<std::wstring> parts;
        std::wstringstream wss(char_wstr);
        while (std::getline(wss, temp, L','))
            parts.push_back(temp);
        std::wstring name = parts[0];
        uint8_t profession = 0;
        if (parts.size() > 1) {
            const int p = _wtoi(&parts[1][0]);
            if (p > 0 && p < 11)
                profession = static_cast<uint8_t>(p);
        }
        out->emplace(name, profession);
    }
}
void FriendListWindow::LoadFromFile() {
    if (loading)
        return;
    loading = true;
    Log::Log("%s: Loading friends from ini\n", Name());
    if(settings_thread.joinable())
        settings_thread.join();
    settings_thread = std::thread([this]() {
        // clear builds from toolbox
        uuid_by_name.clear();
        while (friends.begin() != friends.end()) {
            RemoveFriend(friends.begin()->second);
        }
        friends.clear();

        inifile->Reset();
        inifile->SetMultiKey(true);
        inifile->LoadFile(Resources::GetPath(ini_filename).c_str());

        ToolboxIni::TNamesDepend entries;
        inifile->GetAllSections(entries);
        for (const ToolboxIni::Entry& entry : entries) {
            auto lf = new Friend(this);
            lf->uuid = entry.pItem;
            lf->uuid_bytes = StringToGuid(lf->uuid);
            lf->setAlias(GuiUtils::StringToWString(inifile->GetValue(entry.pItem, "alias", "")));
            lf->type = static_cast<GW::FriendType>(inifile->GetLongValue(entry.pItem, "type", static_cast<long>(lf->type)));
            if (lf->uuid.empty() || lf->getAliasW().empty()) {
                delete lf;
                continue; // Error, alias or uuid empty.
            }

            // Grab char names
            std::unordered_map<std::wstring, uint8_t> charnames;
            LoadCharnames(entry.pItem, &charnames);
            for (const auto& it : charnames) {
                lf->SetCharacter(it.first.c_str(), it.second);
            }
            if (lf->characters.empty()) {
                delete lf;
                continue; // Error, should have at least 1 charname...
            }
            friends.emplace(lf->uuid, lf);
            for (const auto& it : lf->characters) {
                uuid_by_name[it.first] = lf;
            }
            uuid_by_name[lf->getAliasW()] = lf;
        }
        Log::Log("%s: Loaded friends from ini\n", Name());
        friends_list_checked = false;
        loading = false;
        });
}
void FriendListWindow::SaveToFile() {
    if (!friends_changed)
        return;
    if (settings_thread.joinable())
        settings_thread.join();
    settings_thread = std::thread([this]() {
        friends_changed = false;
        inifile->Reset();
        // Load the existing file in, and amend the info
        inifile->LoadFile(Resources::GetPath(ini_filename).c_str());
        inifile->SetMultiKey(true);
        if (friends.empty())
            return; // Error, should have at least 1 friend
        //std::lock_guard<std::recursive_mutex> lock(friends_mutex);
        for (auto it = friends.begin(); it != friends.end(); ++it) {
            // do something
            Friend& lf = *it->second;
            const char* uuid = lf.uuid.c_str();
            inifile->SetLongValue(uuid, "type", static_cast<long>(lf.type), NULL, false, true);
            inifile->SetValue(uuid, "alias", lf.getAliasA().c_str(), NULL, true);
            // Append to existing charnames, but don't duplicate. This allows multiple accounts to contribute to the friend list.
            std::unordered_map<std::wstring, uint8_t> charnames;
            LoadCharnames(uuid, &charnames);
            for (const auto& char_it : lf.characters) {
                const auto& found = charnames.find(char_it.first);
                // Note: Don't overwrite the profession
                if (found == charnames.end() || char_it.second.profession != 0)
                    charnames.emplace(char_it.first, char_it.second.profession);
            }
            inifile->DeleteValue(uuid, "charname", NULL);
            for (const auto& char_it : charnames) {
                char charname[128] = { 0 };
                snprintf(charname, 128, "%s,%d",
                    GuiUtils::WStringToString(char_it.first).c_str(),
                    char_it.second);
                inifile->SetValue(uuid, "charname", charname);
            }
        }
        inifile->SaveFile(Resources::GetPath(ini_filename).c_str());
    });
}
