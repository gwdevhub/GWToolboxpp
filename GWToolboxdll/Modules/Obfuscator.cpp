#include "stdafx.h"

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
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Utils/GuiUtils.h>
#include <Logger.h>
#include <ImGuiAddons.h>
#include <Modules/Obfuscator.h>

#define ONLY_CURRENT_PLAYER 1;
// #define DETECT_STREAMING_APPLICATION 1;

#include <Psapi.h>


namespace {
    /*IWbemServices* pSvc = 0;
    IWbemLocator* pLoc = 0;
    HRESULT CoInitializeEx_result = -1;*/
    MSG msg;
    HWND streaming_window_handle = 0;
    std::default_random_engine dre = std::default_random_engine((uint32_t)time(0));

#ifdef DETECT_STREAMING_APPLICATION
    HWINEVENTHOOK hook = 0;
#endif

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
        L"Acolyte of Dwayna",
        L"Acolyte of Grenth",
        L"Acolyte of Lyssa",
        L"Acolyte of Melandru",
        L"Admiral Chiggen",
        L"Admiral Kantoh",
        L"Admiral Kaya",
        L"Am Fah Courier",
        L"Am Fah Leader",
        L"Argo",
        L"Arius Dark Apostle",
        L"Ashlyn Spiderfriend",
        L"Auri the Skull Wand",
        L"Aurora",
        L"Bairn the Sinless",
        L"Bonetti",
        L"Bosun Mohrti",
        L"Braima the Callous",
        L"Brogan the Punisher",
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
        L"Carnak the Hungry",
        L"Cerris",
        L"Chae Plan",
        L"Cho Spirit Empath",
        L"Chung the Attuned",
        L"Colonel Chaklin",
        L"Colonel Custo",
        L"Colonel Kajo",
        L"Commander Bahreht",
        L"Commander Kubeh",
        L"Commander Noss",
        L"Commander SadiBelai",
        L"Commander Sehden",
        L"Commander Wahli",
        L"Confessor Isaiah",
        L"Corbin the Upright",
        L"Corporal Argon",
        L"Corporal Luluh",
        L"Corporal Suli",
        L"Corsair Commander",
        L"Cultist Milthuran",
        L"Cultist Rajazan",
        L"Cursed Salihm",
        L"Cuthbert the Chaste",
        L"Daeman",
        L"Danthor the Adamant",
        L"Darwym the Spiteful",
        L"Degaz the Cynical",
        L"Dimsur Cheefai",
        L"Drinkmaster Tahnu",
        L"Edgar the Iron Fist",
        L"Edred the Bruiser",
        L"Elvina the Pious",
        L"En Fa the Awakened",
        L"Ensign Charehli",
        L"Ensign Jahan",
        L"Ensign Lumi",
        L"Erulai the Inimical",
        L"Executioner Vekil",
        L"Galrath",
        L"Ganshu the Scribe",
        L"Garr the Merciful",
        L"General Kahyet",
        L"Gilroy the Stoic",
        L"Gowan Chobak",
        L"Heifan Kanko",
        L"Imuk the Pungent",
        L"Inquisitor Bauer",
        L"Inquisitor Lashona",
        L"Inquisitor Lovisa",
        L"Insatiable Vakar",
        L"Ironfist",
        L"Irwyn the Severe",
        L"Jacqui The Reaver",
        L"Jang Wen",
        L"Jin the Skull Bow",
        L"Jin the Purifier",
        L"Joh the Hostile",
        L"Julen the Devout",
        L"Justiciar Amilyn",
        L"Justiciar Hablion",
        L"Justiciar Kasandra",
        L"Justiciar Kimii",
        L"Justiciar Marron",
        L"Justiciar Sevaan",
        L"Kahli the Stiched",
        L"Kai Shi Jo",
        L"Kathryn the Cold",
        L"Kayali the Brave",
        L"Kenric the Believer",
        L"Kenshi Steelhand",
        L"Lai Graceful Blade",
        L"Lale the Vindictive",
        L"Lars the Obeisant",
        L"Leijun Ano",
        L"Lerita the Lewd",
        L"Lev the Condemned",
        L"Li Ho Yan",
        L"Liam Shanglui",
        L"Lian Dragon's Petal",
        L"Lieutenant Kayin",
        L"Lieutenant Mahrik",
        L"Lieutenant Nali",
        L"Lieutenant Shagu",
        L"Lieutenant Silmok",
        L"Lieutenant Vanahk",
        L"Lorelle Jade Cutter",
        L"Lou of the Knives",
        L"Major Jeahr",
        L"Markis",
        L"Marnta Doomspeaker",
        L"Maxine Coldstone",
        L"Melikos",
        L"Merki The Reaver",
        L"Midshipman Bennis",
        L"Midshipman Beraidun",
        L"Midshipman Morolah",
        L"Mina Shatter Storm",
        L"Minea the Obscene",
        L"Ming the Judgment",
        L"Oswald the Amiable",
        L"Overseer Boktek",
        L"Overseer Haubeh",
        L"Pah Pei",
        L"Pei the Skull Blade",
        L"Pleoh the Ugly",
        L"Quufu",
        L"Ramm the Benevolent",
        L"Rei Bi",
        L"Reisen the Phoenix",
        L"Rien the Martyr",
        L"Riseh the Harmless",
        L"Royen Beastkeeper",
        L"Samira Dhulnarim",
        L"Seaguard Eli",
        L"Seaguard Gita",
        L"Seaguard Hala",
        L"Seiran of the Cards",
        L"Selenas the Blunt",
        L"Selwin the Fervent",
        L"Sergeant Behnwa",
        L"Shen the Magistrate",
        L"Sheng Pai",
        L"Shensang Jinzao",
        L"Sun the Quivering",
        L"Suunshi Haisang",
        L"Tachi Forvent",
        L"Talous the Mad",
        L"Taskmaster Suli",
        L"Taskmaster Vanahk",
        L"Teral the Punisher",
        L"The Dark Blade",
        L"Torr the Relentless",
        L"Tuila the Club",
        L"Uris Tong of Ash",
        L"Valis the Rampant",
        L"Ven the Conservator",
        L"Vess the Disputant",
        L"Waeng",
        L"Wing Three Blade",
        L"Xien",
        L"Yinnai Qi",
        L"Zaln the Jaded",
        L"Zu Jin the Quick"
    };

    typedef void( __fastcall* GetCharacterSummary_pt)(void* ctx, uint32_t edx, wchar_t* character_name);
    GetCharacterSummary_pt GetCharacterSummary_Func = 0;
    GetCharacterSummary_pt RetGetCharacterSummary = 0;
    GW::MemoryPatcher GetCharacterSummary_AssertionPatch;

    void __fastcall OnGetCharacterSummary(void* ctx, uint32_t edx, wchar_t* character_name) {
        GW::HookBase::EnterHook();
        if (edx != 2 && edx != 3) {
            RetGetCharacterSummary(ctx, edx, character_name);
            GW::HookBase::LeaveHook();
            return;
        }
        static wchar_t new_character_name[20];
        auto& instance = Obfuscator::Instance();
        if (instance.status != Obfuscator::Disabled) {
            instance.ObfuscateName(character_name, new_character_name, 20, true);
            character_name = new_character_name;
        }
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
#ifdef DETECT_STREAMING_APPLICATION

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

#endif

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
bool FindPlayerNameInMessage(GW::Chat::Channel channel, wchar_t* message, wchar_t** player_name_start_p, wchar_t** player_name_end_p) {
    *player_name_start_p = 0;
    auto findPlayerName = [message, player_name_start_p, player_name_end_p]() {
        // Find any other generic instance of the current player name
        const wchar_t* player_name = getPlayerName();
        *player_name_start_p = wcsstr(message, player_name);
        if (*player_name_start_p)
            *player_name_end_p = *player_name_start_p + wcslen(player_name);
    };
    switch (message[0]) {
    case 0x76B: // Incoming player message (chat/guild/alliance/team)
        findPlayerName();
        break;
    default:
        switch (channel) {
        case GW::Chat::Channel::CHANNEL_GROUP:
        case GW::Chat::Channel::CHANNEL_GWCA2: {
            findPlayerName();
        } break;
        case GW::Chat::Channel::CHANNEL_GLOBAL:
            if (
                wmemcmp(message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) == 0 // <player name> has resigned
                || wmemcmp(message, L"\x8101\x3b02\xb2eb\xc1f4\x41af\x0107", 6) == 0 // <player name> has restored communication with the server
                || wmemcmp(message, L"\x8101\x475c\x010a\x0ba9\x0107", 5) == 0 // Skill template named "<blah>" has been loaded onto <player name>
                || wmemcmp(message, L"\x7f1\x9a9d\xe943\x0b33\x010a", 5) == 0 // <monster name> drops an <item name> which your party reserves for <player name>
                ) {
                findPlayerName();
            }
            break;
        }
        break;
    }
    return *player_name_start_p && *player_name_end_p;
}
wchar_t* Obfuscator::ObfuscateMessage(GW::Chat::Channel channel, wchar_t* message, bool obfuscate) {
    // NB: Static for ease, but be sure copy this away before accessing this function again as needed
    static std::wstring new_message(L"");
    wchar_t* player_name_start = nullptr;
    wchar_t* player_name_end = nullptr;
    wchar_t* offset = message;
    size_t player_name_len = 0;
    bool found = FindPlayerNameInMessage(channel, offset, &player_name_start, &player_name_end);
    if (!found)
        return message;
    bool need_to_free_message = message == new_message.data();
    if (need_to_free_message) {
        // Check to avoid recursive string assignment
        message = new wchar_t[new_message.size() + 1];
        wcscpy(message, new_message.c_str());
        player_name_start = &message[player_name_start - new_message.data()];
        player_name_end = &message[player_name_end - new_message.data()];
        offset = message;
    }
    new_message.clear();
    do {
        player_name_len = player_name_end - player_name_start;
        new_message.append(offset, (player_name_start - offset));
        offset = player_name_start + player_name_len;
        constexpr size_t new_name_len = 32;
        wchar_t* new_name = new wchar_t[new_name_len];
        wmemset(new_name, 0, new_name_len);
        wcsncpy(new_name, player_name_start, player_name_len);
        if (obfuscate) {
            ObfuscateName(new_name, new_name, new_name_len);
        }
        else {
            UnobfuscateName(new_name, new_name, new_name_len);
        }
        new_message.append(new_name);
        delete[] new_name;
        found = FindPlayerNameInMessage(channel, offset, &player_name_start, &player_name_end);
    } while (found);
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
void Obfuscator::OnPreUIMessage(GW::HookStatus*, GW::UI::UIMessage msg_id, void* wParam, void* ) {
    if (Instance().status != Enabled)
        return;
    switch (msg_id) {
    case GW::UI::UIMessage::kShowMapEntryMessage: {
        GW::UI::MapEntryMessage* packet_actual = (GW::UI::MapEntryMessage*)wParam;
        if(packet_actual->subtitle)
            packet_actual->subtitle = Instance().ObfuscateMessage(GW::Chat::Channel::CHANNEL_GWCA2, packet_actual->subtitle);
    } break;
    case GW::UI::UIMessage::kDialogBody: { // Dialog body
        GW::UI::DialogBodyInfo* packet_actual = (GW::UI::DialogBodyInfo*)wParam;
        if(packet_actual->message_enc)
            packet_actual->message_enc = Instance().ObfuscateMessage(GW::Chat::Channel::CHANNEL_GWCA2, packet_actual->message_enc);
    } break;
    case GW::UI::UIMessage::kWriteToChatLog: {
        GW::UI::UIChatMessage* packet_actual = (GW::UI::UIChatMessage*) wParam;
        // Because we've already obfuscated the player name in-game, the name in the message will be obfuscated. Unobfuscate it here, and re-obfuscate it later.
        // This allows the player to toggle obfuscate on/off between map loads and it won't bork up the message log.
        packet_actual->message = Instance().UnobfuscateMessage(static_cast<GW::Chat::Channel>(packet_actual->channel), packet_actual->message);
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
        // Obfuscate player name in NPC dialogs
    case GAME_SMSG_SIGNPOST_BODY:
    case GAME_SMSG_DIALOG_BODY: {
        if (self.status != Enabled)
            break;
        GW::Packet::StoC::DialogBody* packet_actual = (GW::Packet::StoC::DialogBody*)packet;
        wchar_t* player_name_start = wcsstr(packet_actual->message, L"\xBA9\x107");
        while (player_name_start) {
            player_name_start += 2;
            wchar_t* player_name_end = wcschr(player_name_start, 0x1);
            if (!player_name_end)
                break;
            wchar_t player_name[20] = { 0 };
            wcsncpy(player_name, player_name_start, player_name_end - player_name_start);
            static wchar_t new_message[122];
            wmemset(new_message, 0, _countof(new_message));
            wcsncpy(new_message, packet_actual->message, player_name_start - packet_actual->message);
            self.ObfuscateName(player_name, &new_message[wcslen(new_message)], 20, true);
            
            wcscpy(&new_message[wcslen(new_message)], player_name_end);
            wcsncpy(packet_actual->message, new_message, _countof(new_message));
            player_name_start = wcsstr(player_name_start, L"\xBA9\x107");
        }
    } break;

    }
}

void Obfuscator::ObfuscateGuild(bool obfuscate) {
    if (obfuscate && player_guild_obfuscated_name[0]) {
        //Log::Log("Tried to obfuscate guild, but already obfuscated");
        return;
    }
    if (!obfuscate && !player_guild_obfuscated_name[0]) {
        //Log::Log("Tried to unobfuscate guild, but already unobfuscated");
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
            GW::UI::SendUIMessage(GW::UI::UIMessage::kGuildMemberUpdated, &player->name_ptr);
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

#ifdef DETECT_STREAMING_APPLICATION

    if (hook) {
        ASSERT(UnhookWinEvent(hook));
        CoUninitialize();
        hook = 0;
    }

#endif

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

    GW::UI::RegisterUIMessageCallback(&stoc_hook, GW::UI::UIMessage::kShowMapEntryMessage, OnPreUIMessage, pre_hook_altitude);
    GW::UI::RegisterUIMessageCallback(&stoc_hook, GW::UI::UIMessage::kDialogBody, OnPreUIMessage, pre_hook_altitude);
    GW::UI::RegisterUIMessageCallback(&stoc_hook, GW::UI::UIMessage::kWriteToChatLog, OnPreUIMessage, pre_hook_altitude);
    GW::Chat::RegisterSendChatCallback(&ctos_hook, OnSendChat);

    GW::Chat::RegisterPrintChatCallback(&ctos_hook, OnPrintChat);

    GW::Chat::CreateCommand(L"obfuscate", CmdObfuscate);
    GW::Chat::CreateCommand(L"hideme", CmdObfuscate);

#if DETECT_STREAMING_APPLICATION

    CoInitialize(NULL);
    hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_DESTROY, NULL, OnWindowEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

#endif

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
void Obfuscator::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(obfuscate), status != Disabled);
}
void Obfuscator::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);

    if (ini->GetBoolValue(Name(), VAR_NAME(obfuscate), status != Disabled)) {
        Obfuscate(true);
    }
}

void Obfuscator::DrawSettingInternal() {
    bool enabled = status != Disabled;
    if (ImGui::Checkbox("Hide my character names on-screen", &enabled)) {
        Obfuscate(enabled);
    }
    ImGui::ShowHelp("Hides and overrides current player name at character selection and in-game.\nThis change is applied on next map change.");
    ImGui::TextDisabled("You can also use the /hideme or /obfuscate command to toggle this at any time");
}
