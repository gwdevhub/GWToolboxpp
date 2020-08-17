#include "stdafx.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Logger.h>
#include <base64.h>
#include <Timer.h>

#include <Modules/Resources.h>
#include <Windows/FriendListWindow.h>

/* Out of scope namespecey lookups */
namespace
{
    static const std::wstring GetPlayerNameFromEncodedString(const wchar_t *message)
    {
        const wchar_t* start = wcschr(message, 0x107);
        if (!start) return L"";
        const wchar_t* end = wcschr(start, 0x1);
        if (!end) return L"";
        std::wstring name(start + 1, end);
        return name;
    }
    const ImColor ProfColors[11] = {0xFFFFFFFF, 0xFFEEAA33, 0xFF55AA00,
                                    0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                                    0xFFBB3333, 0xFFAA0088, 0xFF00AAAA,
                                    0xFF996600, 0xFF7777CC};
    const wchar_t *ProfNames[11] = {
        L"Unknown",     L"Warrior", L"Ranger",       L"Monk",
        L"Necromancer", L"Mesmer",  L"Elementalist", L"Assassin",
        L"Ritualist",   L"Paragon", L"Dervish"};
    static const ImColor StatusColors[4] = {
        IM_COL32(0x99, 0x99, 0x99, 255), // offline
        IM_COL32(0x0, 0xc8, 0x0, 255),   // online
        IM_COL32(0xc8, 0x0, 0x0, 255),   // busy
        IM_COL32(0xc8, 0xc8, 0x0, 255)   // away
    };
    static std::wstring current_map;
    static GW::Constants::MapID current_map_id = GW::Constants::MapID::None;
    static std::wstring *GetCurrentMapName()
    {
        GW::Constants::MapID map_id = GW::Map::GetMapID();
        if (current_map_id != map_id) {
            GW::AreaInfo *i = GW::Map::GetMapInfo(map_id);
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
    static char *GetStatusText(uint8_t status)
    {
        switch (static_cast<GW::FriendStatus>(status)) {
            case GW::FriendStatus::FriendStatus_Offline:
                return "Offline";
            case GW::FriendStatus::FriendStatus_Online:
                return "Online";
            case GW::FriendStatus::FriendStatus_DND:
                return "Do not disturb";
            case GW::FriendStatus::FriendStatus_Away:
                return "Away";
        }
        return "Unknown";
    }
    static std::map<uint32_t, wchar_t *> map_names;
    static GUID StringToGuid(const std::string &str)
    {
        GUID guid;
        sscanf(str.c_str(),
               "%8lx-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
               &guid.Data1, &guid.Data2, &guid.Data3, &guid.Data4[0],
               &guid.Data4[1], &guid.Data4[2], &guid.Data4[3], &guid.Data4[4],
               &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

        return guid;
    }

    static void GuidToString(GUID guid, char *guid_cstr)
    {
        snprintf(guid_cstr, 128,
                 "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
                 guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
                 guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
    static std::wstring last_whisper;
    static std::wstring ParsePlayerName(int argc, LPWSTR* argv) {
        std::wstring player_name;
        for (int i = 0; i < argc; i++) {
            std::wstring s(argv[i]);
            if (s.empty())
                continue;
            if (!player_name.empty())
                player_name += L" ";
            std::transform(s.begin() + 1, s.end(), s.begin() + 1, towlower);
            std::transform(s.begin(), s.begin() + 1, s.begin(), towupper);
            player_name += s.c_str();
        }
        return player_name;
    }
}
void FriendListWindow::CmdAddFriend(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc < 2)
        return Log::Error("Missing player name");
    std::wstring player_name = ParsePlayerName(argc - 1, &argv[1]);
    if (player_name.empty())
        return Log::Error("Missing player name");
    GW::FriendListMgr::AddFriend(player_name.c_str());
}
void FriendListWindow::CmdRemoveFriend(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc < 2)
        return Log::Error("Missing player name");
    std::wstring player_name = ParsePlayerName(argc - 1, &argv[1]);
    if (player_name.empty())
        return Log::Error("Missing player name");
    FriendListWindow::Friend* f = FriendListWindow::Instance().GetFriend(player_name.c_str());
    if (!f) return Log::Error("No friend '%ls' found", player_name.c_str());
    f->RemoveGWFriend();
}
/*  FriendListWindow::Friend    */

void FriendListWindow::Friend::GetMapName() {
    if (!current_map_id)
        return;
    GW::AreaInfo* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(current_map_id));
    if (!info) {
        current_map_name[0] = 0;
        return;
    }
    static wchar_t enc_str[16];
    if (!GW::UI::UInt32ToEncStr(info->name_id, enc_str, 16)) {
        current_map_name[0] = 0;
        return;
    }
    GW::UI::AsyncDecodeStr(enc_str, current_map_name, 128);

}
// Get the Guild Wars friend object for this friend (if it exists)
GW::Friend* FriendListWindow::Friend::GetFriend() {
    return GW::FriendListMgr::GetFriend((uint8_t*)&uuid_bytes);
}
// Start whisper to this player via their current char name.
void FriendListWindow::Friend::StartWhisper() {
    const wchar_t* alias_c = alias.c_str();
    const wchar_t* charname = current_char ? current_char->name.c_str() : '\0';
    
    GW::GameThread::Enqueue([charname, alias_c]() {
        if(!charname[0])
            return Log::Error("Player %S is not logged in", alias_c);
        GW::UI::SendUIMessage(GW::UI::kOpenWhisper, (wchar_t*)charname, nullptr);
    });
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::GetCharacter(const wchar_t* char_name) {
    std::unordered_map<std::wstring, Character>::iterator it = characters.find(char_name);
    if (it == characters.end())
        return nullptr; // Not found
    return &it->second;
}
// Get the character belonging to this friend (e.g. to find profession etc)
FriendListWindow::Character* FriendListWindow::Friend::SetCharacter(const wchar_t* char_name, uint8_t profession = 0) {
    Character* existing = GetCharacter(char_name);
    if (!existing) {
        Character c;
        c.name = std::wstring(char_name);
        characters.emplace(c.name, c);
        existing = GetCharacter(c.name.c_str());
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
    char* uuid_ptr = (char*)&uuid_bytes;
    for (size_t i = 0; i < sizeof(uuid_bytes); i++) {
        if (uuid_ptr[i] != 0)
            return true;
    }
    return false;
}

/* Setters */
// Update local friend record from raw info.
FriendListWindow::Friend* FriendListWindow::SetFriend(uint8_t* uuid, GW::FriendType type, GW::FriendStatus status, uint32_t map_id, const wchar_t* charname, const wchar_t* alias) {
    if (type != GW::FriendType::FriendType_Friend && type != GW::FriendType::FriendType_Ignore)
        return nullptr;
    // Validate UUID (When a friend is created GW doesn't immediately have the right UUID)
    bool is_valid_uuid = false;
    for (size_t i = 0; i < sizeof(UUID); i++) {
        is_valid_uuid = uuid[i] != 0;
    }
    if (!is_valid_uuid)
        return nullptr;
    Friend* lf = GetFriend(uuid);
    if (!lf && charname)
        lf = GetFriend(charname);
    if(!lf && alias)
        lf = GetFriend(alias);
    char uuid_c[128];
    GuidToString(*(UUID*)uuid, uuid_c);
    if(!lf) {
        // New friend
        lf = new Friend(this);
        lf->alias = std::wstring(alias);
        friends.emplace(uuid_c, lf);
    }
    lf->type = static_cast<uint8_t>(type);
    bool uuid_changed = strcmp(lf->uuid.c_str(), uuid_c) != 0;
    bool alias_changed = alias && wcscmp(alias, lf->alias.c_str()) != 0;
    if (uuid_changed) {
        // UUID is different. This could be because GW has assigned a UUID to this friend.
        lf->uuid = std::string(uuid_c);
        lf->uuid_bytes = StringToGuid(uuid_c);
    }
    if (alias && (alias_changed || uuid_changed)) {
        auto current = uuid_by_name.find(lf->alias);
        if (current != uuid_by_name.end())
            uuid_by_name.erase(current);
        lf->alias = std::wstring(alias);
        uuid_by_name.emplace(lf->alias, lf->uuid);
    }
    if (lf->current_map_id != map_id) {
        lf->current_map_id = map_id;
        memset(lf->current_map_name, 0, sizeof lf->current_map_name);
        lf->GetMapName();
    }       

    // Check and copy charnames, only if player is NOT offline
    if (!charname || status == GW::FriendStatus::FriendStatus_Offline)
        lf->current_char = nullptr;
    if (status != GW::FriendStatus::FriendStatus_Offline && charname) {
        lf->current_char = lf->SetCharacter(charname);
        uuid_by_name.emplace(charname, lf->uuid);
    }
    // @Cleanup: This 255 is really odd.
    if (status != static_cast<GW::FriendStatus>(255)) {
        if (lf->status != status)
            need_to_reorder_friends = true;
        lf->status = static_cast<uint8_t>(status);
    }
    friends_changed = true;
    return lf;
}
// Update local friend record from existing friend.
FriendListWindow::Friend* FriendListWindow::SetFriend(GW::Friend* f) {
    return SetFriend(f->uuid, f->type, f->status, f->zone_id, (wchar_t*)&f->charname[0], (wchar_t*)&f->alias[0]);
}

/* Getters */
const std::string FriendListWindow::Friend::GetCharactersHover(bool include_charname)
{
    if (!cached_charnames_hover) {
        std::wstring cached_charnames_hover_ws = L"Characters for ";
        cached_charnames_hover_ws += alias;
        cached_charnames_hover_ws += L":";
        for (std::unordered_map<std::wstring, Character>::iterator it2 =
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
        str += GuiUtils::WStringToString(current_char->name);
        str += "\n";
    }
    if (include_charname && current_map_name[0]) {
        str += current_map_name;
        str += "\n";
    }
    if (str.size())
        str += "\n";
    str += cached_charnames_hover_str;
    return str;
}
// Find existing record for friend by char name.
FriendListWindow::Friend* FriendListWindow::GetFriend(const wchar_t* name) {
    std::unordered_map<std::wstring, std::string>::iterator it = uuid_by_name.find(name);
    if (it == uuid_by_name.end())
        return nullptr; // Not found
    return GetFriendByUUID(it->second.c_str());
}
// Find existing record for friend by GW Friend object
FriendListWindow::Friend* FriendListWindow::GetFriend(GW::Friend* f) {
    return f ? GetFriend(f->uuid) : nullptr;
}
FriendListWindow::Friend* FriendListWindow::GetFriend(uint8_t* uuid) {
    char uuid_c[128];
    GuidToString(*(UUID*)uuid, uuid_c);
    return GetFriendByUUID(uuid_c);
}
// Find existing record for friend by uuid
FriendListWindow::Friend* FriendListWindow::GetFriendByUUID(const char* uuid) {
    //std::lock_guard<std::recursive_mutex> lock(friends_mutex);
    std::unordered_map<std::string, Friend*>::iterator it = friends.find(uuid);
    if (it == friends.end())
        return nullptr;
    return it->second; // Found in cache
}
bool FriendListWindow::RemoveFriend(Friend* f) {
    if (!f) 
        return false;
    std::unordered_map<std::string, Friend*>::iterator it1 = friends.find(f->uuid);
    if (it1 != friends.end()) {
        friends.erase(it1);
    }
    std::unordered_map<std::wstring, std::string>::iterator it2;
    for (const auto& character : f->characters) {
        it2 = uuid_by_name.find(character.first);
        if (it2 != uuid_by_name.end() && it2->second == f->uuid)
            uuid_by_name.erase(it2);
    }
    const auto& alias_it = uuid_by_name.find(f->alias);
    if(alias_it != uuid_by_name.end())
        uuid_by_name.erase(alias_it);
    delete f;
    return true;
}
/* FriendListWindow basic functions etc */
FriendListWindow::FriendListWindow() {
    inifile = new CSimpleIni(false, false, false);
}
FriendListWindow::~FriendListWindow() {
    delete inifile;
}
void FriendListWindow::Initialize() {
    ToolboxWindow::Initialize();

    GW::Chat::CreateCommand(L"addfriend", CmdAddFriend);
    GW::Chat::CreateCommand(L"removefriend", CmdRemoveFriend);
    GW::Chat::CreateCommand(L"deletefriend", CmdRemoveFriend);
    GW::Chat::CreateCommand(L"away", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY); });
    GW::Chat::CreateCommand(L"online", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::ONLINE); });
    GW::Chat::CreateCommand(L"offline", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::OFFLINE); });
    GW::Chat::CreateCommand(L"busy", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::DO_NOT_DISTURB); });
    GW::Chat::CreateCommand(L"dnd", [](...) { GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::DO_NOT_DISTURB); });

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusUpdate_Entry, OnFriendUpdated);
    GW::Chat::RegisterSendChatCallback(&SendChat_Entry, OnOutgoingWhisper);
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry,OnPlayerJoinInstance);
    GW::UI::RegisterUIMessageCallback(&OnUIMessage_Entry, OnUIMessage);
}
void FriendListWindow::OnUIMessage(GW::HookStatus* status, uint32_t message_id, void* wparam, void*) {
    if (message_id != GW::UI::kWriteToChatLog || !wparam)
        return;
    UIChatMessage* uimsg = static_cast<UIChatMessage*>(wparam);
    wchar_t* message = uimsg->message;
    std::wstring message_w = message;
    switch (message[0]) {
    case MessageType::CANNOT_ADD_YOURSELF_AS_A_FRIEND: // You cannot add yourself as a friend.
    case MessageType::EXCEEDED_MAX_NUMBER_OF_FRIENDS: // You have exceeded the maximum number of characters on your Friends list.
    case MessageType::CHARACTER_NAME_X_DOES_NOT_EXIST: // The Character name "" does not exist
        OnAddFriendError(status, message);
        break;
    case MessageType::FRIEND_ALREADY_ADDED_AS_X: // The Character you're trying to add is already in your friend list as "".
        OnFriendAlreadyAdded(status, message);
        break;
    case MessageType::INCOMING_WHISPER: // Incoming whisper
        AddFriendAliasToMessage(uimsg);
        break;
    case MessageType::OUTGOING_WHISPER: // Server has successfully sent your whisper
        OnOutgoingWhisperSuccess(status, message);
        AddFriendAliasToMessage(uimsg);
        break;
    case MessageType::PLAYER_X_NOT_ONLINE: // Player "" is not online. Redirect to the right person if we can find them!
        OnPlayerNotOnline(status, message);
        break;
    }
}
void FriendListWindow::OnFriendUpdated(GW::HookStatus*, GW::Friend* f, GW::FriendStatus status, const wchar_t* alias, const wchar_t* charname) {
    // Keep a log mapping char name to uuid. This is saved to disk.
    Friend* lf = Instance().SetFriend(f->uuid, f->type, status, f->zone_id, charname, alias);
    if (!lf) return;
    lf->last_update = clock();
}
bool FriendListWindow::WriteError(MessageType message_type, const wchar_t* character_name) {
    wchar_t buffer[122];
    switch (message_type) {
    case MessageType::CHARACTER_NAME_X_DOES_NOT_EXIST:
    case MessageType::FRIEND_ALREADY_ADDED_AS_X:
        ASSERT(swprintf(buffer, 122, L"%c\x107%s\x1", message_type, character_name) > 0);
        break;
    case MessageType::PLAYER_X_NOT_ONLINE:
        ASSERT(swprintf(buffer, 122, L"%c\x101\x100\x107%s\x1\x108\x1", message_type, character_name) > 0);
        break;
    default:
        return false;
    }
    std::wstring player_not_online = buffer;
    //GW::Chat::WriteChatEnc(GW::Chat::CHANNEL_WHISPER, buffer);
    return true;
}

void FriendListWindow::OnAddFriendError(GW::HookStatus* status, wchar_t* ) {
    FriendListWindow& instance = Instance();
    if (instance.pending_whisper.charname.size()) {
        ASSERT(instance.WriteError(MessageType::PLAYER_X_NOT_ONLINE, instance.pending_whisper.charname.c_str()));
        status->blocked = true;
        instance.pending_whisper.reset();
    }
}

// Record a friend's character profession when they join your map
void FriendListWindow::OnPlayerJoinInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak) {
    UNREFERENCED_PARAMETER(status);
    std::wstring player_name = GuiUtils::SanitizePlayerName(pak->player_name);
    GW::Player* a = GW::PlayerMgr::GetPlayerByName(pak->player_name);
    if (!a || !a->primary)
        return;
    uint8_t profession = static_cast<uint8_t>(a->primary);
    Friend* f = Instance().GetFriend(&player_name[0]);
    if (!f)
        return;
    Character* fc = f->GetCharacter(&player_name[0]);
    if (!fc)
        return;
    if (profession > 0 && profession < 11)
        fc->profession = profession;
}
// Record the pending outgoing whisper
void FriendListWindow::OnOutgoingWhisper(GW::HookStatus *, int channel, wchar_t *message)
{
    FriendListWindow& instance = Instance();
    // If this outgoing whisper was created due to a redirect, or its not a whisper, drop out here.
    if (instance.is_redirecting_whisper || static_cast<GW::Chat::Channel>(channel) != GW::Chat::CHANNEL_WHISPER)
        return;
    wchar_t* separator_pos = wcschr(message, ',');
    instance.pending_whisper.reset(std::wstring(message, separator_pos), std::wstring(&separator_pos[1]));
}

// Remove from pending whispers when whisper has been sent
void FriendListWindow::OnOutgoingWhisperSuccess(GW::HookStatus *, wchar_t *)
{
    FriendListWindow &instance = Instance();
    instance.pending_whisper.reset();
}
// Optionally add friend alias to incoming/outgoing messages
void FriendListWindow::AddFriendAliasToMessage(UIChatMessage* uimsg) {
    FriendListWindow& instance = Instance();
    if (!instance.show_alias_on_whisper)
        return;
    wchar_t* message = uimsg->message;
    const wchar_t* name_start = wcschr(message, 0x107);
    ASSERT(name_start != nullptr);
    const wchar_t* name_end = wcschr(name_start, 0x1);
    ASSERT(name_end != nullptr);
    std::wstring player_name(name_start + 1, name_end);
    Friend* f = instance.GetFriend(player_name.c_str());
    if (!f || f->alias == player_name)
        return;
    static std::wstring new_message;
    new_message = std::wstring(message, name_end - message);
    new_message += L" (";
    new_message += f->alias;
    new_message += L")";
    new_message.append(name_end);
    // TODO; Would doing this cause a memory leak on the previous wchar_t* ?
    uimsg->message = (wchar_t*)new_message.c_str();
}
void FriendListWindow::OnFriendAlreadyAdded(GW::HookStatus *status, wchar_t *message)
{
    FriendListWindow &instance = Instance();
    std::wstring player_name = GetPlayerNameFromEncodedString(message);
    Friend *f = instance.GetFriend(player_name.c_str());
    if (f) {
        f->SetCharacter(player_name.c_str());
    }
    if (instance.pending_whisper.charname.size()) {
        ASSERT(instance.WriteError(MessageType::PLAYER_X_NOT_ONLINE, instance.pending_whisper.charname.c_str()));
        instance.pending_whisper.reset();
        status->blocked = true;
    }
}
void FriendListWindow::OnPlayerNotOnline(GW::HookStatus *status, wchar_t *message)
{
    FriendListWindow &instance = Instance();
    std::wstring player_name = GetPlayerNameFromEncodedString(message);
    Friend *f = instance.GetFriend(player_name.c_str());
    if (f) {
        // If this player is already in my friend list, send the message directly.
        if (!f->IsOffline() && f->current_char->name != player_name) {
            instance.is_redirecting_whisper = true;
            GW::Chat::SendChat(f->current_char->name.c_str(), instance.pending_whisper.message.c_str());
            instance.is_redirecting_whisper = false;
            instance.pending_whisper.reset();
            status->blocked = true;
        }
        return;
    }
    else {
        // Otherwise if this player isn't already in my friend list, add them temporarily. OnFriendCreated will then resend the message and remove the friend.
        GW::FriendListMgr::AddFriend(instance.pending_whisper.charname.c_str());
        instance.pending_whisper.pending_add = TIMER_INIT();
        status->blocked = true;
    }    
}

void FriendListWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (loading)
        return;
    if (!GW::Map::GetIsMapLoaded())
        return;
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    friend_list_ready = fl && fl->friends.valid();
    if (!friend_list_ready) 
        return;
    if (!poll_queued) {
        int interval_check = poll_interval_seconds * CLOCKS_PER_SEC;
        if (!friends_list_checked || clock() - friends_list_checked > interval_check) {
            Poll();
        }
    }
    if (pending_whisper.pending_add != 0) {
        // GW::FriendListMgr::FriendStatusCallback doesn't correctly assign the UUID when a friend is added (this is done elsewhere in Auth manager), so we need to check manually
        if (TIMER_DIFF(pending_whisper.pending_add) > 5000) {
            // Timeout reached adding player to friend list.
            pending_whisper.reset();
        }
        else {
            // Check if pending player has been added.
            Poll();
            Friend* lf = GetFriend(pending_whisper.charname.c_str());
            if (lf && lf->ValidUuid()) {
                // This is a player that TB has added to friend list to find out who they're actually playing on.
                std::wstring& message = pending_whisper.message;
                // Temporary friend added successfully
                if (!lf->IsOffline()) {
                    // If they're online, send the original message...
                    ASSERT(lf->current_char);
                    is_redirecting_whisper = true;
                    GW::Chat::SendChat(lf->current_char->name.c_str(), message.c_str());
                    is_redirecting_whisper = false;
                }
                else {
                    // If they're still not online, then show the "Player is not online" error
                    ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
                }
                pending_whisper.reset();
                // ... then remove from GW.
                ASSERT(lf->RemoveGWFriend());
                ASSERT(RemoveFriend(lf));
                pending_whisper.reset();
            }
        }

    }
}
void FriendListWindow::Poll() {
    if (loading || polling) return;
    polling = true;
    clock_t now = clock();
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    if (!fl) return;
    for (unsigned int i = 0; i < fl->friends.size(); i++) {
        GW::Friend* f = fl->friends[i];
        if (!f) continue;
        Friend* lf = SetFriend(f->uuid, f->type, f->status, f->zone_id, f->charname, f->alias);
        if (!lf) continue;
        lf->last_update = now;
    }
    std::unordered_map<std::string, Friend*>::iterator it = friends.begin();
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
    //Log::Log("Friends list polled\n");
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
bool FriendListWindow::IsWidget() const
{
    return (explorable_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
        || (outpost_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost);
}
bool FriendListWindow::IsWindow() const
{
    return (explorable_show_as == 0 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
        || (outpost_show_as == 0 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost);
}
void FriendListWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible || GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        return;
    const bool is_widget = IsWidget();
    const bool is_window = IsWindow();
    if (!is_widget && !is_window)
        return;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 72.0f), ImGuiCond_FirstUseEver);
    ImGuiIO* io = &ImGui::GetIO();
    ImVec2 window_size = ImVec2(540.0f * io->FontGlobalScale, 512.0f * io->FontGlobalScale);
    float cols[3] = { 180.0f * io->FontGlobalScale, 360.0f * io->FontGlobalScale, 540.0f * io->FontGlobalScale };
    ImGui::SetNextWindowSize(window_size, ImGuiCond_FirstUseEver);
    if (is_widget)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0).Value);
    bool ok = ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
    if (is_widget)
        ImGui::PopStyleColor();
    if (!ok)
        return ImGui::End();

    unsigned int colIdx = 0;
    bool show_charname = ImGui::GetContentRegionAvailWidth() > 180.0f;
    bool _show_location = ImGui::GetContentRegionAvailWidth() > 360.0f;
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
    float height = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 pos;
    if (show_my_status) {
        char* status_c = "Offline";
        uint32_t status = GW::FriendListMgr::GetMyStatus();
        switch (status) {
            case static_cast<uint32_t>(GW::FriendStatus::FriendStatus_Online):
                status_c = "Online";
                break;
            case static_cast<uint32_t>(GW::FriendStatus::FriendStatus_DND):
                status_c = "Busy";
                break;
            case static_cast<uint32_t>(GW::FriendStatus::FriendStatus_Away):
                status_c = "Away";
                break;
        }
        ImGui::Text("You are:");
        ImGui::SameLine();
        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
        ImGui::TextColored(ImVec4(0, 0, 0, 1), status_c);
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
        ImGui::TextColored(StatusColors[status].Value, status_c);
    }
    for (std::unordered_map<std::string, Friend*>::iterator it = friends.begin(); it != friends.end(); ++it) {
        colIdx = 0;
        Friend* lfp = it->second;
        if (lfp->type != GW::FriendType::FriendType_Friend) continue;
        // Get actual object instead of pointer just in case it becomes invalid half way through the draw.
        Friend lf = *lfp;
        if (lf.IsOffline()) continue;
        if (lf.alias.empty()) continue;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover_background_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover_background_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
        ImGui::PushID(lf.uuid.c_str());
        ImGui::Button("", ImVec2(ImGui::GetContentRegionAvailWidth(), height));
        bool left_clicked = ImGui::IsItemClicked(0);
        bool right_clicked = ImGui::IsItemClicked(1);
        // TODO: Rename, Remove, Ignore.
        //ImGui::BeginPopupContextItem();

        bool hovered = ImGui::IsItemHovered();
        ImGui::PopStyleVar(4);
        ImGui::SameLine(2.0f,0);
        ImGui::PushStyleColor(ImGuiCol_Text, StatusColors[static_cast<size_t>(lf.status)].Value);
        ImGui::Bullet();
        ImGui::PopStyleColor(4);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip(GetStatusText(lf.status));
        ImGui::SameLine(0);
        std::string s = GuiUtils::WStringToString(lf.alias);
        if (is_widget) {
            pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
            ImGui::TextColored(ImVec4(0, 0, 0, 1), s.c_str());
            ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
        }
        ImGui::Text(s.c_str());
        hovered = hovered || ImGui::IsItemHovered();
        if (!show_charname && hovered) {
            ImGui::SetTooltip(lf.GetCharactersHover(true).c_str());
        }
        if (show_charname && lf.current_char != nullptr) {
            ImGui::SameLine(cols[colIdx]);
            std::string current_char_name_s = GuiUtils::WStringToString(lf.current_char->name);
            uint8_t prof = lf.current_char->profession;
            if (prof) ImGui::PushStyleColor(ImGuiCol_Text, ProfColors[lf.current_char->profession].Value);
            if (is_widget) {
                pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
                ImGui::TextColored(ImVec4(0, 0, 0, 1), "%s", current_char_name_s.c_str());
                ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
            }
            ImGui::Text("%s", current_char_name_s.c_str());
            if (prof) ImGui::PopStyleColor();
            if (lf.characters.size() > 1) {
                ImGui::SameLine(0,0);
                if (is_widget) {
                    pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
                    ImGui::TextColored(ImVec4(0, 0, 0, 1), " (+%d)", lf.characters.size() - 1);
                    ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
                }
                ImGui::Text(" (+%d)", lf.characters.size() - 1);
                hovered |= ImGui::IsItemHovered();
                if (hovered) {
                    ImGui::SetTooltip(lf.GetCharactersHover().c_str());
                }
            }
            if (show_location) {
                ImGui::SameLine(cols[++colIdx]);
                if (lf.current_map_name) {
                    if (is_widget) {
                        pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
                        ImGui::TextColored(ImVec4(0, 0, 0, 1), lf.current_map_name);
                        ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
                    }
                    ImGui::Text(lf.current_map_name);
                }
            }
        }
        ImGui::PopID();
        if (left_clicked && !lf.IsOffline())
            lf.StartWhisper();
        if (right_clicked) {
            
        }
    }
    if (!is_widget)
        ImGui::EndChild();
    ImGui::End();
}
void FriendListWindow::DrawSettingInternal() {
    bool edited = false;
    ImGui::SameLine();
    edited |= ImGui::Checkbox("Lock size as widget", &lock_size_as_widget);
    ImGui::SameLine();
    edited |= ImGui::Checkbox("Lock move as widget", &lock_move_as_widget);
    const float dropdown_width = 160.0f * ImGui::GetIO().FontGlobalScale;
    ImGui::Text("Show as");
    ImGui::SameLine(); 
    ImGui::PushItemWidth(dropdown_width);
    edited |= ImGui::Combo("###show_as_outpost", &outpost_show_as, "Window\0Widget\0Hidden");
    ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::Text("in outpost");

    ImGui::Text("Show as");
    ImGui::SameLine(); 
    ImGui::PushItemWidth(dropdown_width); 
    edited |= ImGui::Combo("###show_as_explorable", &explorable_show_as, "Window\0Widget\0Hidden");
    ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::Text("in explorable");
    Colors::DrawSettingHueWheel("Widget background hover color", &hover_background_color);

    ImGui::Checkbox("Show my status", &show_my_status);
    ImGui::ShowHelp("e.e. 'You are: Online'");

    DrawChatSettings();
}
void FriendListWindow::RegisterSettingsContent() {
    ToolboxUIElement::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent("Chat Settings",
        [this](const std::string* section, bool is_showing) {
            UNREFERENCED_PARAMETER(section);
            if (!is_showing) return;
            DrawChatSettings();
        },0.91f);
}

void FriendListWindow::DrawChatSettings() {
    ImGui::Checkbox("Show friend aliases when sending/receiving whispers", &show_alias_on_whisper);
    ImGui::ShowHelp("Only if your friend's alias is different to their character name");
}
void FriendListWindow::DrawHelp() {
    if (!ImGui::TreeNode("Friend List Commands"))
        return;
    ImGui::Bullet(); ImGui::Text("'/addfriend <character_name>' Add a character to your friend list.");
    ImGui::Bullet(); ImGui::Text("'/removefriend <character_name|alias>' Remove character to your friend list.");
    ImGui::Bullet(); ImGui::Text("'/away' Set your friend list status to 'Away'.");
    ImGui::Bullet(); ImGui::Text("'/online' Set your friend list status to 'Online'.");
    ImGui::Bullet(); ImGui::Text("'/offline' Set your friend list status to 'Offline'.");
    ImGui::Bullet(); ImGui::Text("'/busy' or '/dnd' Set your friend list status to 'Do Not Disturb'.");
    ImGui::TreePop();
}
void FriendListWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    lock_move_as_widget = ini->GetBoolValue(Name(), VAR_NAME(lock_move_as_widget), lock_move_as_widget);
    lock_size_as_widget = ini->GetBoolValue(Name(), VAR_NAME(lock_size_as_widget), lock_size_as_widget);
    show_alias_on_whisper = ini->GetBoolValue(Name(), VAR_NAME(show_alias_on_whisper), show_alias_on_whisper);

    outpost_show_as = ini->GetLongValue(Name(), VAR_NAME(outpost_show_as), outpost_show_as);
    explorable_show_as = ini->GetLongValue(Name(), VAR_NAME(explorable_show_as), explorable_show_as);
    show_my_status = ini->GetBoolValue(Name(), VAR_NAME(show_my_status), show_my_status);
    
    Colors::Load(ini, Name(), VAR_NAME(hover_background_color), hover_background_color);

    LoadFromFile();
}
void FriendListWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(lock_move_as_widget), lock_move_as_widget);
    ini->SetBoolValue(Name(), VAR_NAME(lock_size_as_widget), lock_size_as_widget);
    ini->SetBoolValue(Name(), VAR_NAME(show_alias_on_whisper), show_alias_on_whisper);

    ini->SetLongValue(Name(), VAR_NAME(outpost_show_as), outpost_show_as);
    ini->SetLongValue(Name(), VAR_NAME(explorable_show_as), explorable_show_as);
    ini->SetBoolValue(Name(), VAR_NAME(show_my_status), show_my_status);

    Colors::Save(ini, Name(), VAR_NAME(hover_background_color), hover_background_color);

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
}
void FriendListWindow::LoadCharnames(const char* section, std::unordered_map<std::wstring, uint8_t>* out) {
    // Grab char names
    CSimpleIniA::TNamesDepend values;
    inifile->GetAllValues(section, "charname", values);
    CSimpleIniA::TNamesDepend::const_iterator i;
    for (i = values.begin(); i != values.end(); ++i) {
        std::wstring char_wstr = GuiUtils::StringToWString(i->pItem), temp;
        std::vector<std::wstring> parts;
        std::wstringstream wss(char_wstr);
        while (std::getline(wss, temp, L','))
            parts.push_back(temp);
        std::wstring name = parts[0];
        uint8_t profession = 0;
        if (parts.size() > 1) {
            int p = _wtoi(&parts[1][0]);
            if (p > 0 && p < 11)
                profession = (uint8_t)p;
        }
        out->emplace(name, profession);
    }
}
void FriendListWindow::LoadFromFile() {
    loading = true;
    Log::Log("%s: Loading friends from ini\n", Name());
    // clear builds from toolbox
    uuid_by_name.clear();
    while (friends.begin() != friends.end()) {
        RemoveFriend(friends.begin()->second);
    }
    friends.clear();

    inifile->Reset();
    inifile->SetMultiKey(true);
    inifile->LoadFile(Resources::GetPath(ini_filename).c_str());

    CSimpleIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    for (CSimpleIni::Entry& entry : entries) {
        Friend* lf = new Friend(this);
        lf->uuid = entry.pItem;
        lf->uuid_bytes = StringToGuid(lf->uuid);
        lf->alias = GuiUtils::StringToWString(inifile->GetValue(entry.pItem, "alias", ""));
        lf->type = static_cast<uint8_t>(inifile->GetLongValue(entry.pItem, "type", static_cast<long>(lf->type)));
        if (lf->uuid.empty() || lf->alias.empty()) {
            delete lf;
            continue; // Error, alias or uuid empty.
        }

        // Grab char names
        std::unordered_map<std::wstring, uint8_t> charnames;
        LoadCharnames(entry.pItem, &charnames);
        for (auto it : charnames) {
            lf->SetCharacter(it.first.c_str(), it.second);
        }
        if (lf->characters.empty()) {
            delete lf;
            continue; // Error, should have at least 1 charname...
        }
        friends.emplace(lf->uuid, lf);
        for (std::unordered_map<std::wstring, Character>::iterator it2 = lf->characters.begin(); it2 != lf->characters.end(); ++it2) {
            uuid_by_name.emplace(it2->first, lf->uuid);
        }
        uuid_by_name.emplace(lf->alias, lf->uuid);
    }
    Log::Log("%s: Loaded friends from ini\n", Name());
    friends_list_checked = false;
    loading = false;
}
void FriendListWindow::SaveToFile() {
    if (!friends_changed)
        return;
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
        Friend lf = *it->second;
        const char* uuid = lf.uuid.c_str();
        inifile->SetLongValue(uuid, "type", static_cast<long>(lf.type),NULL,false,true);
        inifile->SetValue(uuid, "alias", GuiUtils::WStringToString(lf.alias).c_str(), NULL, true);
        // Append to existing charnames, but don't duplicate. This allows multiple accounts to contribute to the friend list.
        std::unordered_map<std::wstring, uint8_t> charnames;
        LoadCharnames(uuid, &charnames);
        for (auto char_it : lf.characters) {
            const auto &found = charnames.find(char_it.first);
            // Note: Don't overwrite the profession
            if (found == charnames.end() || char_it.second.profession != 0)
                charnames.emplace(char_it.first, char_it.second.profession);
        }
        inifile->DeleteValue(uuid, "charname", NULL);
        for (auto char_it : charnames) {
            char charname[128] = { 0 };
            snprintf(charname, 128, "%s,%d",
                     GuiUtils::WStringToString(char_it.first).c_str(),
                     char_it.second);
            inifile->SetValue(uuid, "charname", charname);
        }
    }
    inifile->SaveFile(Resources::GetPath(ini_filename).c_str());
}
