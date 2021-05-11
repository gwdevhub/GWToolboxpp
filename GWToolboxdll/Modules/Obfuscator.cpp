#include "stdafx.h"

#include <algorithm>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GuildContext.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/MemoryPatcher.h>

#include <GuiUtils.h>
#include <Logger.h>
#include <ImGuiAddons.h>
#include <Modules/Obfuscator.h>


#ifndef GAME_SMSG_MERCENARY_INFO
#define GAME_SMSG_MERCENARY_INFO 115
#endif
#define ONLY_CURRENT_PLAYER 1;

#include <Psapi.h>


namespace {
    /*IWbemServices* pSvc = 0;
    IWbemLocator* pLoc = 0;
    HRESULT CoInitializeEx_result = -1;*/
    MSG msg;
    HWND streaming_window_handle = 0;
    std::default_random_engine dre = std::default_random_engine((uint32_t)time(0));
    HWINEVENTHOOK hook = 0;
    bool running = false;

    const wchar_t* getPlayerName() {
        GW::GameContext* g = GW::GameContext::instance();
        return g ? g->character->player_name : nullptr;
    }
    const wchar_t* getGuildPlayerName() {
        GW::GameContext* g = GW::GameContext::instance();
        return g && g->guild ? g->guild->player_name : nullptr;
    }

    std::vector<const wchar_t*> obfuscated_name_pool = {
        L"Abbot Ramoth",
        L"Acolyte Of Balthazar",
        L"Acolyte Of Dwayna",
        L"Acolyte Of Grenth",
        L"Acolyte Of Lyssa",
        L"Acolyte Of Melandru",
        L"Admiral Chiggen",
        L"Admiral Kantoh",
        L"Admiral Kaya",
        L"Ajun Xi Deft Blade",
        L"Am Fah Courier",
        L"Am Fah Leader",
        L"Apep Unending Night",
        L"Argo",
        L"Arius Dark Apostle",
        L"Ashlyn Spiderfriend",
        L"Auri The Skull Wand",
        L"Aurora",
        L"Bairn The Sinless",
        L"Bearn The Implacable",
        L"Bonetti",
        L"Bosun Mohrti",
        L"Braima The Callous",
        L"Brogan The Punisher",
        L"Calamitous",
        L"Captain Alsin",
        L"Captain Bei Chi",
        L"Captain Blood Farid",
        L"Captain Chichor",
        L"Captain Denduru",
        L"Captain Kavaka",
        L"Captain Kuruk",
        L"Captain Lumanda",
        L"Captain Mhedi",
        L"Captain Mwende",
        L"Captain Nebo",
        L"Captain Shehnahr",
        L"Carnak The Hungry",
        L"Cerris",
        L"Chae Plan",
        L"Chazek Plague Herder",
        L"Cho Spirit Empath",
        L"Chung The Attuned",
        L"Colonel Chaklin",
        L"Colonel Custo",
        L"Colonel Kajo",
        L"Commander Bahreht",
        L"Commander Chui Kantu",
        L"Commander Kubeh",
        L"Commander Noss",
        L"Commander Sadi-Belai",
        L"Commander Sehden",
        L"Commander Wahli",
        L"Commander Werishakul",
        L"Confessor Isaiah",
        L"Corbin The Upright",
        L"Corporal Argon",
        L"Corporal Luluh",
        L"Corporal Suli",
        L"Corsair Commander",
        L"Countess Nadya",
        L"Cultist Milthuran",
        L"Cultist Rajazan",
        L"Cursed Salihm",
        L"Cuthbert The Chaste",
        L"Daeman",
        L"Daisuke Crimson Edge",
        L"Danthor The Adamant",
        L"Darwym The Spiteful",
        L"Degaz The Cynical",
        L"Dim Sii",
        L"Dimsur Cheefai",
        L"Drinkmaster Tahnu",
        L"Edgar The Iron Fist",
        L"Edred The Bruiser",
        L"Elvina The Pious",
        L"En Fa The Awakened",
        L"Ensign Charehli",
        L"Ensign Jahan",
        L"Ensign Lumi",
        L"Eri Heart Of Fire",
        L"Erulai The Inimical",
        L"Fo Mahn",
        L"Galrath",
        L"Ganshu The Scribe",
        L"Gao Han Of The Rings",
        L"Garr The Merciful",
        L"General Kahyet",
        L"Geoffer Pain Bringer",
        L"Gilroy The Stoic",
        L"Gowan Chobak",
        L"Heifan Kanko",
        L"Imuk The Pungent",
        L"Initiate Jeng Sunjoo",
        L"Initiate Shen Wojong",
        L"Inquisitor Bauer",
        L"Inquisitor Lashona",
        L"Inquisitor Lovisa",
        L"Insatiable Vakar",
        L"Ironfist",
        L"Irwyn The Severe",
        L"Jacqui The Reaver",
        L"Jang Wen",
        L"Jiao Kuai The Swift",
        L"Jin The Skull Bow",
        L"Jin The Purifier",
        L"Joh The Hostile",
        L"Julen The Devout",
        L"Justiciar Amilyn",
        L"Justiciar Hablion",
        L"Justiciar Kasandra",
        L"Justiciar Kimii",
        L"Justiciar Marron",
        L"Justiciar Sevaan",
        L"Kahli, The Stiched",
        L"Kai Shi Jo",
        L"Kathryn The Cold",
        L"Kayali The Brave",
        L"Kenric The Believer",
        L"Kenshi Steelhand",
        L"Koon Jizang",
        L"Lai Graceful Blade",
        L"Lale The Vindictive",
        L"Lars The Obeisant",
        L"Leijun Ano",
        L"Lerita The Lewd",
        L"Lev The Condemned",
        L"Li Ho Yan",
        L"Liam Shanglui",
        L"Lian Dragons Petal",
        L"Lieutenant Kayin",
        L"Lieutenant Mahrik",
        L"Lieutenant Nali",
        L"Lieutenant Shagu",
        L"Lieutenant Silmok",
        L"Lieutenant Vanahk",
        L"Lorelle Jade Cutter",
        L"Lou Of The Knives",
        L"Major Jeahr",
        L"Manton The Indulgent",
        L"Markis",
        L"Marnta Doomspeaker",
        L"Maxine Coldstone",
        L"Meijun Vengeful Eye",
        L"Merki The Reaver",
        L"Midshipman Bennis",
        L"Midshipman Beraidun",
        L"Midshipman Morolah",
        L"Mina Shatter Storm",
        L"Minea The Obscene",
        L"Ming The Judgment",
        L"Oswald The Amiable",
        L"Overseer Boktek",
        L"Overseer Haubeh",
        L"Pah Pei",
        L"Pei The Skull Blade",
        L"Pleoh The Ugly",
        L"Quufu",
        L"Ramm The Benevolent",
        L"Rei Bi",
        L"Reiko",
        L"Reisen The Phoenix",
        L"Rho Ki",
        L"Rien The Martyr",
        L"Riseh The Harmless",
        L"Royen Beastkeeper",
        L"Samira Dhulnarim",
        L"Seaguard Eli",
        L"Seaguard Gita",
        L"Seaguard Hala",
        L"Seiran Of The Cards",
        L"Selenas The Blunt",
        L"Selwin The Fervent",
        L"Sergeant Behnwa",
        L"Shen The Magistrate",
        L"Sheng Pai",
        L"Shensang Jinzao",
        L"Shiro Tagachi",
        L"Sun The Quivering",
        L"Suuga Rei",
        L"Suunshi Haisang",
        L"Tachi Forvent",
        L"Tai Soon",
        L"Talous The Mad",
        L"Taskmaster Suli",
        L"Taskmaster Vanahk",
        L"Teral The Punisher",
        L"The Dark Blade",
        L"Torr The Relentless",
        L"Tuila The Club",
        L"Uris Tong Of Ash",
        L"Valis The Rampant",
        L"Ven The Conservator",
        L"Vess The Disputant",
        L"Waeng",
        L"Watari The Infinite",
        L"Wing Three Blade",
        L"Xi Lin Of The Flames",
        L"Xien",
        L"Yayoi Of The Orders",
        L"Yinnai Qi",
        L"Zaln The Jaded",
        L"Zu Jin The Quick"
    };

    typedef void( __fastcall* GetCharacterSummary_pt)(void* ctx, uint32_t edx, wchar_t* character_name);
    GetCharacterSummary_pt GetCharacterSummary_Func = 0;
    GetCharacterSummary_pt RetGetCharacterSummary = 0;
    GW::MemoryPatcher GetCharacterSummary_AssertionPatch;

    static void __fastcall OnGetCharacterSummary(void* ctx, uint32_t edx, wchar_t* character_name) {
        GW::HookBase::EnterHook();
        if (edx != 2 && edx != 3)
            goto leave;
        static wchar_t new_character_name[20];
        auto& instance = Obfuscator::Instance();
        if (instance.status != Obfuscator::Disabled) {
            instance.ObfuscateName(character_name, new_character_name, 20, true);
            character_name = new_character_name;
        }
    leave:
        RetGetCharacterSummary(ctx, edx, character_name);
        GW::HookBase::LeaveHook();
    }
}
wchar_t* Obfuscator::getPlayerInvitedName() {
    if (player_guild_invited_name[0])
        return player_guild_invited_name;
    GW::GameContext* g = GW::GameContext::instance();
    if (!g || !g->guild)
        return player_guild_invited_name;
    const wchar_t* player_guild_name = getGuildPlayerName();
    if (!player_guild_name)
        return player_guild_invited_name;
    GW::GuildRoster& roster = g->guild->player_roster;
    if (!roster.valid())
        return player_guild_invited_name;
    for (GW::GuildPlayer* player : roster) {
        if (player && wcscmp(player->current_name, player_guild_name) == 0) {
            wcscpy(player_guild_invited_name, player->invited_name);
            break;
        }
    }
    return player_guild_invited_name;
}

void CALLBACK Obfuscator::OnWindowEvent(HWINEVENTHOOK _hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
     if (!running)
         return;
     UNREFERENCED_PARAMETER(_hook);
     UNREFERENCED_PARAMETER(dwEventThread);
     UNREFERENCED_PARAMETER(dwmsEventTime);
     switch (event) {
     case EVENT_OBJECT_DESTROY:
         if (hwnd != streaming_window_handle)
             return;
         streaming_window_handle = 0;
         Log::Info("Streaming mode deactivated");
         break;
     case EVENT_SYSTEM_FOREGROUND: {
         if (streaming_window_handle)
             return;
         TCHAR window_class_name[MAX_PATH] = { 0 };
         DWORD dwProcId = 0;

         GetWindowThreadProcessId(hwnd, &dwProcId);

         HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcId);
         if (!hProc) {
             Log::Log("Failed to OpenProcess, %d", GetLastError());
             return;
         }
         DWORD res = GetModuleFileNameExA((HMODULE)hProc, NULL, window_class_name, MAX_PATH);
         CloseHandle(hProc);
         if (!res) {
             Log::Log("Failed to GetModuleFileNameExA, %d", GetLastError());
             return;
         }
         if (strstr(window_class_name, "obs64.exe")) {
             streaming_window_handle = hwnd;
             Log::Info("Streaming mode activated");
         }
         Log::Log("%p, %p, %p, %p - %s", event, hwnd, idObject, idChild, window_class_name);
     } break;
     }
}
void Obfuscator::OnSendChat(GW::HookStatus* status, GW::Chat::Channel channel, wchar_t* message) {
    if (channel != GW::Chat::Channel::CHANNEL_WHISPER)
        return;
    if (Instance().status != Enabled)
        return;
    static bool processing = false;
    if (processing)
        return;
    wchar_t* whisper_separator = wcschr(message, ',');
    if (!whisper_separator)
        return;
    size_t len = (whisper_separator - message);
    wchar_t unobfuscated[32] = { 0 };
    wcsncpy(unobfuscated, message, len);
    Instance().UnobfuscateName(unobfuscated, unobfuscated, _countof(unobfuscated));
    wchar_t new_message[138] = { 0 };
    swprintf(new_message, _countof(new_message), L"%s%s", unobfuscated, whisper_separator);
    status->blocked = true;
    // NB: Block and send a copy with the new message content; current wchar_t* may not have enough allocated memory to just replace the content.
    processing = true;
    GW::Chat::SendChat(unobfuscated, &whisper_separator[1]);
    processing = false;
}
wchar_t* Obfuscator::ObfuscateMessage(GW::Chat::Channel channel, wchar_t* message, bool obfuscate) {
    // NB: Static for ease, but be sure copy this away before accessing this function again as needed
    static std::wstring new_message(L"");
    wchar_t* player_name_start = nullptr;
    switch (message[0]) {
    case 0x76B: // Incoming player message (chat/guild/alliance/team)
    //case 0x76D: // Incoming whisper
    //case 0x76E: // Outgoing whisper
    //case 0x880: // Player name <name> is invalid.
    //case 0x881: // Player <name> is not online.
    //case 0x817: // Player x gained a skill point
        player_name_start = wcschr(message, 0x107);
        break;
    default:
        if (channel == GW::Chat::Channel::CHANNEL_GLOBAL) {
            if (
                wmemcmp(message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) == 0 // <player name> has resigned
                || wmemcmp(message, L"\x8101\x3b02\xb2eb\xc1f4\x41af\x0107", 6) == 0 // <player name> has restored communication with the server
                || wmemcmp(message, L"\x8101\x475c\x010a\x0ba9\x0107", 5) == 0 // Skill template named "<blah>" has been loaded onto <player name>
                || wmemcmp(message, L"\x7f1\x9a9d\xe943\x0b33\x010a",5) == 0 // <monster name> drops an <item name> which your party reserves for <player name>
                ) {
                player_name_start = wcschr(message, 0x107);
            }
        }
        break;
    }
    if (!player_name_start)
        return message;
    player_name_start += 1;
    wchar_t* player_name_end = wcschr(player_name_start, 0x1);
    if (!player_name_end)
        return message;
    bool need_to_free_message = message == new_message.data();
    if (need_to_free_message) {
        // Check to avoid recursive string assignment
        message = new wchar_t[new_message.size() + 1];
        wcscpy(message, new_message.c_str());
        player_name_start = &message[player_name_start - new_message.data()];
        player_name_end = &message[player_name_end - new_message.data()];
    }
    new_message.clear();
    new_message.append(message, (player_name_start - message));
    wchar_t new_name[64] = { 0 };
    wcsncpy(new_name, player_name_start, (player_name_end - player_name_start));
    if (obfuscate) {
        ObfuscateName(new_name, new_name, _countof(new_name));
    }
    else {
        UnobfuscateName(new_name, new_name, _countof(new_name));
    }
    new_message.append(new_name);
    new_message.append(player_name_end);
    if (need_to_free_message)
        delete[] message;
    return new_message.data();
}
void Obfuscator::CmdObfuscate(const wchar_t* , int , wchar_t** ) {
    Instance().Obfuscate(Instance().status == Disabled);
}
void Obfuscator::Obfuscate(bool obfuscate) {
    bool changed = false;
    if (obfuscate) {
        if (status == Disabled) {
            changed = true;
            status = Pending;
        }
    }
    else {
        if (status != Disabled) {
            changed = true;
            status = Disabled;
        }
    }
    if (changed) {
        Log::Info(status == Disabled ? "Player name will be visible on next map change" : "Player name will be hidden on next map change");
    }
}
void Obfuscator::OnPrintChat(GW::HookStatus* , GW::Chat::Channel channel, wchar_t** message_ptr, FILETIME, int) {
    if (Instance().status != Enabled)
        return;
    *message_ptr = Instance().ObfuscateMessage(channel, *message_ptr);
}
void Obfuscator::OnPreUIMessage(GW::HookStatus*, uint32_t msg_id, void* wParam, void* ) {
    if (Instance().status != Enabled)
        return;
    switch (msg_id) {
    case GW::UI::UIMessage::kWriteToChatLog: {
        struct PlayerChatMessage {
            GW::Chat::Channel channel;
            wchar_t* message;
            uint32_t player_number;
        } *packet_actual = (PlayerChatMessage*) wParam;
        // Because we've already obfuscated the player name in-game, the name in the message will be obfuscated. Unobfuscate it here, and re-obfuscate it later.
        // This allows the player to toggle obfuscate on/off between map loads and it won't bork up the message log.
        packet_actual->message = Instance().UnobfuscateMessage(packet_actual->channel, packet_actual->message);
        Log::LogW(packet_actual->message);
    } break;
    }
}
void Obfuscator::OnStoCPacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet) {
    auto& self = Instance();
    switch (packet->header) {
    case GAME_SMSG_INSTANCE_LOAD_HEAD: {
        self.Reset();
        return;
    } break;
    // Temporarily obfuscate player name on resign (affected modules: InfoWindow)
    case GAME_SMSG_CHAT_MESSAGE_CORE: {
        if (self.status != Enabled)
            break;
        GW::Packet::StoC::MessageCore* packet_actual = (GW::Packet::StoC::MessageCore*)packet;
        static bool obfuscated = false;
        if (wmemcmp(packet_actual->message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) == 0) {
            // This hook is called twice - once before resign log module, once after.
            obfuscated = !obfuscated;
            wchar_t* obfuscated_wc = self.ObfuscateMessage(GW::Chat::Channel::CHANNEL_GLOBAL, packet_actual->message, obfuscated);
            wcscpy(packet_actual->message, obfuscated_wc);
        }
    } break;
    // Obfuscate incoming party searches (affected modules: Trade Window)
    case GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT: {
        if (self.status != Enabled)
            break;
        struct Packet {
            uint32_t header;
            uint32_t other_atts[7];
            wchar_t message[32];
            wchar_t name[20];
        } *packet_actual = (Packet*)packet;
        self.ObfuscateName(packet_actual->name, packet_actual->name, _countof(packet_actual->name));
    } break;
    // Hide Player name on spawn
    case GAME_SMSG_AGENT_CREATE_PLAYER: {
        if (self.status != Enabled)
            break;
        struct Packet {
            uint32_t chaff[7];
            wchar_t name[32];
        } *packet_actual = (Packet*)packet;
        self.ObfuscateName(packet_actual->name, packet_actual->name, _countof(packet_actual->name));
        return;
    } break;
    // Hide Mercenary Hero name
    case GAME_SMSG_MERCENARY_INFO: {
        if (self.status != Enabled)
            break;
        struct Packet {
            uint32_t chaff[20];
            wchar_t name[32];
        } *packet_actual = (Packet*)packet;
        self.ObfuscateName(packet_actual->name, packet_actual->name, _countof(packet_actual->name), true);
    } break;
    // Hide Mercenary Hero name after being added to party or in explorable area
    case GAME_SMSG_AGENT_UPDATE_NPC_NAME: {
        if (self.status != Enabled)
            break;
        struct Packet {
            uint32_t chaff[2];
            wchar_t name[32];
        } *packet_actual = (Packet*)packet;
        if (wcsncmp(L"\x108\x107", packet_actual->name, 2) != 0)
            return; // Not a mercenary name
        wchar_t original_name[20];
        size_t len = 0;
        for (size_t i = 2; i < _countof(original_name) - 1; i++) {
            if (!packet_actual->name[i] || packet_actual->name[i] == 0x1)
                break;
            original_name[len] = packet_actual->name[i];
            len++;
        }
        original_name[len] = 0;
        self.ObfuscateName(original_name, &packet_actual->name[2], _countof(packet_actual->name) - 3, true);
        len = wcslen(packet_actual->name);
        packet_actual->name[len] = 0x1;
        packet_actual->name[len+1] = 0;
    } break;
    // Hide "Customised for <player_name>". Packet header is poorly named, this is actually something like GAME_SMSG_ITEM_CUSTOMISED_NAME
    // (affected modules: HotkeysWindow)
    case GAME_SMSG_ITEM_UPDATE_NAME: {
        if (self.status != Enabled)
            break;
        struct Packet {
            uint32_t chaff[2];
            wchar_t name[32];
        } *packet_actual = (Packet*)packet;
        self.ObfuscateName(packet_actual->name, packet_actual->name, _countof(packet_actual->name));
    } break;
    case GAME_SMSG_GUILD_PLAYER_ROLE: {
        self.ObfuscateGuild(false);
        self.pending_guild_obfuscate = true;
    } break;
    // When current player is updated in guild window (e.g. change character), ensure this is updated in the obfuscator.
    case GAME_SMSG_GUILD_PLAYER_INFO: {
        struct Packet {
            uint32_t header;
            wchar_t invited_name[20];
            wchar_t current_name[20];
            wchar_t invited_by[20];
            wchar_t context_info[64];
            uint32_t unk; // Something to do with promoted date?
            uint32_t minutes_since_login;
            uint32_t join_date; // This is seconds, but since when? 1471089600 ?
            uint32_t status;
            uint32_t member_type;
        } *packet_actual = (Packet*)packet;
        self.ObfuscateGuild(false);
        if (wcscmp(packet_actual->current_name, getGuildPlayerName()) == 0 || wcscmp(packet_actual->current_name, getPlayerName()) == 0) {
            wcscpy(self.player_guild_invited_name, packet_actual->invited_name);
        }
        self.pending_guild_obfuscate = true;
    } break;
        // If player name is obfuscated in guild window, ensure that any updates (e.g. status) are mapped correctly
    case GAME_SMSG_GUILD_PLAYER_CHANGE_COMPLETE: {
        struct Packet {
            uint32_t header;
            wchar_t invited_name[20];
        } *packet_actual = (Packet*)packet;
        self.ObfuscateGuild(false);
        self.pending_guild_obfuscate = true;
        Log::LogW(L"Guild player change for %s", packet_actual->invited_name);
    } break;
    }
}

void Obfuscator::ObfuscateGuild(bool obfuscate) {
    if (obfuscate && player_guild_obfuscated_name[0]) {
        Log::Log("Tried to obfuscate guild, but already obfuscated");
        return;
    }
    if (!obfuscate && !player_guild_obfuscated_name[0]) {
        Log::Log("Tried to unobfuscate guild, but already unobfuscated");
        return;
    }
    GW::GameContext* g = GW::GameContext::instance();
    if (!g || !g->guild) {
        Log::Log("Tried to obfuscate guild, but no guild context");
        return;
    }
    GW::GuildRoster& roster = g->guild->player_roster;
    if (!roster.valid() || !roster.m_buffer || !roster.size()) {
        Log::Log("Tried to obfuscate guild, but no valid roster");
        return;
    }
    wchar_t* invited_name = getPlayerInvitedName();
    if (!invited_name || !invited_name[0]) {
        Log::Log("Tried to obfuscate guild, failed to find current player in roster");
        return;
    }
    for (GW::GuildPlayer* player : roster) {
        if (!player)
            continue;
        if (wcscmp(invited_name, player->invited_name) != 0 && wcscmp(player_guild_obfuscated_name, player->invited_name) != 0)
            continue;
        wchar_t original[20];
        wcscpy(original, player->invited_name);
        if (obfuscate) {
            if(player->current_name[0])
                ObfuscateName(player->current_name, player->current_name, _countof(player->current_name),true);
            ObfuscateName(player->invited_name, player->invited_name, _countof(player->invited_name), true);
            wcscpy(player_guild_obfuscated_name, player->invited_name);
            Log::LogW(L"Guild player obfuscated from %s to %s",original, player->invited_name);
            // Redraw roster position for obfuscated name
            GW::UI::SendUIMessage(0x100000d8, &player->name_ptr);
        } else {
            if (player->current_name[0])
                UnobfuscateName(player->current_name, player->current_name, _countof(player->current_name));
            UnobfuscateName(player->invited_name, player->invited_name, _countof(player->invited_name));
            player_guild_obfuscated_name[0] = 0;
            Log::LogW(L"Guild player unobfuscated from %s to %s", original, player->invited_name);
        }
    }
}

wchar_t* Obfuscator::ObfuscateName(const wchar_t* _original_name, wchar_t* out, int length, bool force) {
    std::wstring original_name = GuiUtils::SanitizePlayerName(_original_name);
#ifdef ONLY_CURRENT_PLAYER
    // Only obfuscate current player; undefine to use this on other players
    if (!force && wcscmp(original_name.c_str(), getPlayerName()) != 0)
        return _original_name == _original_name ? out : wcsncpy(out, _original_name, length);
#endif
    auto found = obfuscated_by_original.find(original_name);
    if (found != obfuscated_by_original.end())
        return wcsncpy(out, found->second.c_str(), length);
    const wchar_t* res = obfuscated_name_pool[pool_index];
    pool_index++;
    if (pool_index >= obfuscated_name_pool.size())
        pool_index = 0;
    swprintf(out, length, L"%s", res);
    for(size_t cnt = 0;cnt < 100;cnt++) {
        found = obfuscated_by_obfuscation.find(out);
        if (found == obfuscated_by_obfuscation.end()) {
            obfuscated_by_obfuscation.emplace(out, original_name.c_str());
            obfuscated_by_original.emplace(original_name.c_str(), out);
            break;
        }
        ASSERT(swprintf(out, length, L"%.16s %d", res, cnt) != -1);
    }
    ASSERT(out[0]);
    return out;
}
wchar_t* Obfuscator::UnobfuscateName(const wchar_t* _obfuscated_name, wchar_t* out, int length) {
    std::wstring obfuscated_name = GuiUtils::SanitizePlayerName(_obfuscated_name);
    auto found = obfuscated_by_obfuscation.find(obfuscated_name);
    if (found != obfuscated_by_obfuscation.end())
        return wcsncpy(out, found->second.c_str(), length);
    return out == _obfuscated_name ? out : wcsncpy(out, _obfuscated_name, length);
}
Obfuscator::~Obfuscator() {
    Reset();
    Terminate();
}
void Obfuscator::Terminate() {
    if (GetCharacterSummary_Func) {
        GW::HookBase::RemoveHook(GetCharacterSummary_Func);
        GetCharacterSummary_AssertionPatch.Reset();
    }
    

    ObfuscateGuild(false);
    running = false;
    if (hook) {
        ASSERT(UnhookWinEvent(hook));
        CoUninitialize();
        hook = 0;
    }
}


void Obfuscator::Initialize() {
    ToolboxModule::Initialize();
    Reset();
    
    uintptr_t GetCharacterSummary_Assertion = GW::Scanner::FindAssertion("p:\\code\\gw\\ui\\char\\uichinfo.cpp", "!StrCmp(m_characterName, characterInfo.characterName)");
    if (GetCharacterSummary_Assertion) {
        // Hook to override character names on login screen
        GetCharacterSummary_Func = (GetCharacterSummary_pt)(GetCharacterSummary_Assertion - 0x4F);
        GW::HookBase::CreateHook(GetCharacterSummary_Func, OnGetCharacterSummary, (void**)&RetGetCharacterSummary);
        GW::HookBase::EnableHooks(GetCharacterSummary_Func);
        // Patch to allow missing character summary
        GetCharacterSummary_AssertionPatch.SetPatch(GetCharacterSummary_Assertion - 0x7, "\xEB", 1);
        GetCharacterSummary_AssertionPatch.TogglePatch(true);
    }

    const int pre_hook_altitude = -0x9000; // Hooks that run before other RegisterPacketCallback hooks
    const int post_hook_altitude = -0x7000; // Hooks that run after other RegisterPacketCallback hooks, but BEFORE the game processes the packet
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_AGENT_CREATE_PLAYER, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_MERCENARY_INFO, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_AGENT_UPDATE_NPC_NAME, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_ITEM_UPDATE_NAME, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_INSTANCE_LOAD_HEAD, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_GUILD_PLAYER_INFO, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_GUILD_PLAYER_CHANGE_COMPLETE, OnStoCPacket, pre_hook_altitude);
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_GUILD_PLAYER_ROLE, OnStoCPacket, pre_hook_altitude);
    
    // Pre resignlog hook
    GW::StoC::RegisterPacketCallback(&stoc_hook, GAME_SMSG_CHAT_MESSAGE_CORE, OnStoCPacket, pre_hook_altitude);
    // Post resignlog hook
    GW::StoC::RegisterPacketCallback(&stoc_hook2, GAME_SMSG_CHAT_MESSAGE_CORE, OnStoCPacket, post_hook_altitude);

    GW::UI::RegisterUIMessageCallback(&stoc_hook, OnPreUIMessage, pre_hook_altitude);

    GW::Chat::RegisterSendChatCallback(&ctos_hook, OnSendChat);

    GW::Chat::RegisterPrintChatCallback(&ctos_hook, OnPrintChat);

    GW::Chat::CreateCommand(L"obfuscate", CmdObfuscate);
    GW::Chat::CreateCommand(L"hideme", CmdObfuscate);

    CoInitialize(NULL);
    hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_DESTROY, NULL, OnWindowEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    running = true;
}
void Obfuscator::Update(float) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (GW::PreGameContext::instance() && player_guild_obfuscated_name[0]) {
        ObfuscateGuild(false);
    }
    else if (pending_guild_obfuscate) {
        ObfuscateGuild(status == Enabled);
        pending_guild_obfuscate = false;
    }
    /*if (status == Enabled && !obfuscated_login_screen) {
        auto* context = GW::PreGameContext::instance();
        if (context && context->chars.valid() && context->chars.size()) {
            // On login screen; obfuscate if needed
            for (auto& character : context->chars) {
                ObfuscateName(character.character_name, character.character_name, _countof(character.character_name), true);
            }
            obfuscated_login_screen = true;
        }
    }*/
}
void Obfuscator::Reset() {
    ObfuscateGuild(false);
    std::shuffle(std::begin(obfuscated_name_pool), std::end(obfuscated_name_pool), dre);
    pool_index = 0;
    obfuscated_by_obfuscation.clear();
    obfuscated_by_original.clear();
    obfuscated_login_screen = false;
    if (status == Pending)
        status = Enabled;
    pending_guild_obfuscate = status == Enabled;
}
void Obfuscator::DrawSettingInternal() {
    bool enabled = status != Disabled;
    if (ImGui::Checkbox("Obfuscate player names", &enabled)) {
        Obfuscate(enabled);
    }
    ImGui::ShowHelp("Replace player names in-game with aliases.\nThis change is applied on next map change.");
}
