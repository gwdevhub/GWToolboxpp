#include "stdafx.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GuildContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/ChatSettings.h>
#include <Modules/Obfuscator.h>
#include <Utils/GuiUtils.h>
#include <Windows/FriendListWindow.h>

#include <Defines.h>
#include <ImGuiAddons.h>
#include <Logger.h>


// #define DETECT_STREAMING_APPLICATION
#ifdef DETECT_STREAMING_APPLICATION
#include <Psapi.h>
#endif

#define ASSERT_MESSAGE_PTR_HASNT_CHANGED(var)  static wchar_t* mp = nullptr; ASSERT(mp == nullptr || mp == var.data()); mp = var.data();


namespace {
    /*IWbemServices* pSvc = 0;
    IWbemLocator* pLoc = 0;
    HRESULT CoInitializeEx_result = -1;*/
    bool rename_other_players = false;
    char own_player_name[20]{};
    std::wstring own_player_name_w{};
    bool rename_friends_to_alias = true;
    bool rename_self = false;
    MSG msg;
    std::default_random_engine dre = std::default_random_engine(static_cast<uint32_t>(time(nullptr)));
    GW::HookEntry stoc_hook;
    GW::HookEntry ctos_hook;

#ifdef DETECT_STREAMING_APPLICATION
    HWND streaming_window_handle = 0;
    HWINEVENTHOOK hook = 0;
#endif

    // This value won't be obfuscated to is always safe to check against
    const wchar_t* GetPlayerName()
    {
        const GW::CharContext* c = GW::GetCharContext();
        return c ? c->player_name : nullptr;
    }

    const wchar_t* GetGuildPlayerName()
    {
        const GW::GuildContext* g = GW::GetGuildContext();
        return g ? g->player_name : nullptr;
    }

    std::vector obfuscated_name_pool = {
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
    // Static variable; GW will use a pointer to this object for the Account tab of the hero screen (and other bits)
    GW::AccountInfo account_info_obfuscated;
    // Static variable; GW will use a pointer to this object via account_info_obfuscated
    std::wstring account_info_obfuscated_name(20, 0);
    // Static variable; GW will use a pointer to this object for the character summary screen
    std::wstring character_summary_obfuscated_name(20, 0);
    // Ease of access to avoid having to call ObfuscateName() every time.
    std::wstring player_guild_invited_name;
    std::wstring player_email;
    // Static variable; GW will use a pointer to this object for UI messages
    std::wstring ui_message_temp_message;
    std::wstring speech_message_temp_message;


    // List of obfuscated names, keyed by obfuscated
    std::map<std::wstring, std::wstring> obfuscated_by_obfuscation;
    // List of obfuscated names, keyed by original
    std::map<std::wstring, std::wstring> obfuscated_by_original;
    // Current position in the list of obfuscated names
    size_t pool_index = 0;

    // Current state
    enum class ObfuscatorState : uint8_t {
        Disabled,
        Enabled
    };

    ObfuscatorState obfuscator_state = ObfuscatorState::Disabled;
    ObfuscatorState pending_state = ObfuscatorState::Disabled;
    // Used to avoid re-obfuscating roster.
    bool guild_roster_obfuscated = false;
    bool pending_guild_obfuscate = false;

    bool IsObfuscatorEnabled()
    {
        return obfuscator_state == ObfuscatorState::Enabled;
    }

    void Reset();
    bool ObfuscateGuildRoster(bool obfuscate = true);
    std::wstring& GetPlayerInvitedName();

    using GetCharacterSummary_pt = void(__fastcall*)(void* ctx, uint32_t edx, wchar_t* character_name);
    GetCharacterSummary_pt GetCharacterSummary_Func = nullptr;
    GetCharacterSummary_pt RetGetCharacterSummary = nullptr;
    GW::MemoryPatcher GetCharacterSummary_AssertionPatch;

    using GetAccountData_pt = GW::AccountInfo*(__cdecl*)();
    GetAccountData_pt GetAccountData_Func = nullptr;
    GetAccountData_pt GetAccountData_Ret = nullptr;

    std::wstring GetObfuscatedName(const std::wstring_view original_name, const bool in_char_select)
    {
        if (!own_player_name_w.empty() && rename_self && (original_name == GetPlayerName() || in_char_select && !GW::Map::GetIsMapLoaded())) {
            return own_player_name_w;
        }
        if (rename_friends_to_alias && (!in_char_select || GW::Map::GetIsMapLoaded())) {
            static std::map<std::wstring, std::wstring> friends_aliases;
            if (const auto frnd = FriendListWindow::GetFriend(original_name.data())) {
                if (friends_aliases.contains(std::wstring{original_name})) {
                    return {};
                }
                if (!frnd->GetAliasW().empty() && frnd->GetAliasW() != original_name) {
                    friends_aliases.emplace(frnd->GetAliasW(), original_name);
                    return frnd->GetAliasW();
                }
            }
        }
        if (!rename_self && (original_name == GetPlayerName() || original_name == GetPlayerInvitedName() || in_char_select && GW::Map::GetIsMapLoaded())) {
            return {};
        }
        if (!rename_other_players && !in_char_select) {
            return {};
        }
        if (pool_index >= obfuscated_name_pool.size()) {
            pool_index = 0;
        }
        return obfuscated_name_pool[pool_index++];
    }

    bool ObfuscateName(const std::wstring& _original_name, std::wstring& out, const bool in_char_select = false)
    {
        std::wstring original_name = GuiUtils::SanitizePlayerName(_original_name);
        if (_original_name.empty()) {
            return false;
        }
        const auto found_original = obfuscated_by_original.find(original_name);
        if (found_original != obfuscated_by_original.end()) {
            out.assign(found_original->second);
            return true;
        }

        const auto tmp_out = GetObfuscatedName(original_name, in_char_select);
        if (tmp_out.empty()) {
            return false;
        }
        if (obfuscated_by_original.contains(original_name)) {
            out.assign(obfuscated_by_original.at(original_name));
            return true;
        }
        if (obfuscated_by_obfuscation.contains(original_name)) {
            const auto& obfuscated_orig = obfuscated_by_obfuscation.at(original_name);
            if (obfuscated_by_original.contains(obfuscated_orig)) {
                out.assign(obfuscated_orig);
                return true;
            }
        }
        if (!obfuscated_by_original.contains(original_name)) {
            obfuscated_by_obfuscation.emplace(tmp_out, original_name);
            obfuscated_by_original.emplace(original_name, tmp_out);
            out.assign(tmp_out);
            return true;
        }
        return false;
    }

    bool UnobfuscateName(const std::wstring_view _obfuscated_name, std::wstring& out)
    {
        if (_obfuscated_name.empty()) {
            return false;
        }
        const auto obfuscated_name = GuiUtils::SanitizePlayerName(std::wstring(_obfuscated_name));
        const auto found = obfuscated_by_obfuscation.find(obfuscated_name);
        if (found == obfuscated_by_obfuscation.end()) {
            return false;
        }
        out.assign(found->second);
        return true;
    }

    bool ObfuscateMessage(const std::wstring_view message, std::wstring& out, const bool obfuscate = true)
    {
        if (!wcschr(message.data(),0x107))
            return false; // Message contains no player names
        std::wstring replacemsg{message};
        const auto& to_search = obfuscate ? obfuscated_by_original : obfuscated_by_obfuscation;
        bool was_changed = false;
        for (const auto& [from, to] : to_search) {
            if (from.empty()) {
                break;
            }
            size_t start_pos = 0;
            while ((start_pos = replacemsg.find(from, start_pos)) != std::string::npos) {
                replacemsg.replace(start_pos, from.length(), to);
                start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
                was_changed = true;
            }
        }
        out.assign(replacemsg);
        return !out.empty() && was_changed;
    }

    bool UnobfuscateMessage(const wchar_t* message, std::wstring& out)
    {
        return ObfuscateMessage(message, out, false);
    }

    // We do this here instead of in WorldContext to intercept without rewriting memory
    // This is called when the game accesses account into via world contect e.g. to find account name when opening hero window
    GW::AccountInfo* OnGetAccountInfo()
    {
        GW::HookBase::EnterHook();
        GW::AccountInfo* accountInfo = GetAccountData_Ret();
        if (accountInfo && IsObfuscatorEnabled()) {
            ObfuscateGuildRoster(false);
            if (!GW::Map::GetIsMapLoaded()) {
                Reset();
            }
            account_info_obfuscated = *accountInfo;
            ObfuscateName(accountInfo->account_name, account_info_obfuscated_name, true);
            ASSERT_MESSAGE_PTR_HASNT_CHANGED(account_info_obfuscated_name);
            account_info_obfuscated.account_name = account_info_obfuscated_name.data();
            accountInfo = &account_info_obfuscated;
        }
        GW::HookBase::LeaveHook();
        return accountInfo;
    }

    // We do this here instead of in PreGameContext to intercept without rewriting memory (it would also mess up logging in)
    // This function is called when the login screen wants to display your currently highlighted character name at the top of the screen.
    void __fastcall OnGetCharacterSummary(void* ctx, const uint32_t edx, wchar_t* character_name)
    {
        GW::HookBase::EnterHook();
        if (edx != 2 && edx != 3) {
            RetGetCharacterSummary(ctx, edx, character_name);
            GW::HookBase::LeaveHook();
            return;
        }
        if (pending_state == ObfuscatorState::Enabled) {
            ObfuscateGuildRoster(false);
            if (character_summary_obfuscated_name.at(0) == L'\0') {
                Reset();
            }
            if (ObfuscateName(character_name, character_summary_obfuscated_name, true)) {
                ASSERT_MESSAGE_PTR_HASNT_CHANGED(character_summary_obfuscated_name);
                character_name = character_summary_obfuscated_name.data();
            }
        }
        RetGetCharacterSummary(ctx, edx, character_name);
        GW::HookBase::LeaveHook();
    }

    // This should return the original unobfuscated player's invited name
    std::wstring& GetPlayerInvitedName()
    {
        if (!player_guild_invited_name.empty()) {
            return player_guild_invited_name;
        }
        GW::GuildContext* g = GW::GetGuildContext();
        if (!g) {
            return player_guild_invited_name;
        }
        const wchar_t* player_guild_name = GetGuildPlayerName();
        if (!player_guild_name) {
            return player_guild_invited_name;
        }
        GW::GuildRoster& roster = g->player_roster;
        if (!roster.valid()) {
            return player_guild_invited_name;
        }
        for (const GW::GuildPlayer* player : roster) {
            if (player && wcscmp(player->current_name, player_guild_name) == 0) {
                player_guild_invited_name = player->invited_name;
                break;
            }
        }
        return player_guild_invited_name;
    }

    // Hide or show guild member names. Note that the guild roster persists across map changes so be careful with it
    bool ObfuscateGuildRoster(const bool obfuscate)
    {
        if (obfuscate == guild_roster_obfuscated) {
            return true;
        }
        GW::GuildContext* g = GW::GetGuildContext();
        if (!g) {
            Log::Log("Tried to obfuscate guild, but no guild context");
            return false;
        }
        GW::GuildRoster& roster = g->player_roster;
        if (!roster.valid() || !roster.m_buffer || !roster.size()) {
            Log::Log("Tried to obfuscate guild, but no valid roster");
            return false;
        }
        const std::wstring& invited_name = GetPlayerInvitedName();
        if (invited_name.empty()) {
            Log::Log("Tried to obfuscate guild, failed to find current player in roster");
            return false;
        }
        for (GW::GuildPlayer* player : roster) {
            if (!player) {
                continue;
            }
            std::wstring tmp{};
            bool guild_member_updated = false;
            if (player->current_name
                && player->current_name[0]
                && (obfuscate ? ObfuscateName(player->current_name, tmp) : UnobfuscateName(player->current_name, tmp))
                && tmp != player->current_name) {
                wcscpy(player->current_name, tmp.c_str());
                guild_member_updated = true;
                if (player->invited_name && player->invited_name[0]) {
                    wcscpy(player->invited_name, tmp.c_str());
                }
            }
            else {
                if (player->invited_name
                    && player->invited_name[0]
                    && (obfuscate ? ObfuscateName(player->invited_name, tmp) : UnobfuscateName(player->invited_name, tmp))
                    && tmp != player->invited_name) {
                    wcscpy(player->invited_name, tmp.c_str());
                    guild_member_updated = true;
                }
            }
            if (guild_member_updated) {
                SendUIMessage(GW::UI::UIMessage::kGuildMemberUpdated, &player->name_ptr);
            }
        }
        guild_roster_obfuscated = obfuscate;
        return true;
    }

    void CmdObfuscate(const wchar_t*, int, wchar_t**)
    {
        Obfuscator::Obfuscate(pending_state != ObfuscatorState::Enabled);
    }

    void Reset()
    {
        ObfuscateGuildRoster(pending_state == ObfuscatorState::Enabled);
        const auto c = GW::GetCharContext();
        if (!c || c->player_email != player_email) {
            player_guild_invited_name.clear();
            if (c && c->player_email) {
                player_email = c->player_email;
            }
        }
        std::ranges::shuffle(obfuscated_name_pool, dre);
        pool_index = 0;
        obfuscated_by_obfuscation.clear();
        obfuscated_by_original.clear();
        // Don't use clear() on this; the game uses the pointer so we don't want to mess with it
        account_info_obfuscated_name[0] = '\0';
        // Don't use clear() on this; the game uses the pointer so we don't want to mess with it
        character_summary_obfuscated_name[0] = '\0';

        obfuscator_state = pending_state;
        pending_guild_obfuscate = IsObfuscatorEnabled();
    }

    void OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage msg_id, void* wParam, void*)
    {
        switch (msg_id) {
            case GW::UI::UIMessage::kLogout: {
                //Reset();
                ObfuscateGuildRoster(pending_state == ObfuscatorState::Enabled);
                obfuscator_state = pending_state;
                pending_guild_obfuscate = IsObfuscatorEnabled();
            }
            break;
        }
        if (!IsObfuscatorEnabled()) {
            return;
        }
        switch (msg_id) {
            case GW::UI::UIMessage::kShowMapEntryMessage: {
                const auto packet_actual = static_cast<GW::UI::MapEntryMessage*>(wParam);
                if (packet_actual->subtitle && ObfuscateMessage(packet_actual->subtitle, ui_message_temp_message)) {
                    packet_actual->subtitle = ui_message_temp_message.data();
                }
            }
            break;
            case GW::UI::UIMessage::kDialogBody: {
                // Dialog body
                const auto packet_actual = static_cast<GW::UI::DialogBodyInfo*>(wParam);
                if (packet_actual->message_enc && ObfuscateMessage(packet_actual->message_enc, ui_message_temp_message)) {
                    packet_actual->message_enc = ui_message_temp_message.data();
                }
            }
            break;
            case GW::UI::UIMessage::kWriteToChatLog: {
                const auto packet_actual = static_cast<GW::UI::UIChatMessage*>(wParam);
                // Because we've already obfuscated the player name in-game, the name in the message will be obfuscated. Unobfuscate it here, and re-obfuscate it later.
                // This allows the player to toggle obfuscate on/off between map loads and it won't bork up the message log.
                constexpr std::array<uint32_t, 3> channels_to_ignore = {
                    GW::Chat::CHANNEL_GWCA1,
                    GW::Chat::CHANNEL_GWCA2,
                    GW::Chat::CHANNEL_GWCA3,
                };
                if (std::ranges::find(channels_to_ignore, packet_actual->channel) != std::ranges::end(channels_to_ignore)) {
                    break; // do not unobfuscate messages from e.g. twitch
                }
                if (packet_actual->message && UnobfuscateMessage(packet_actual->message, ui_message_temp_message)) {
                    packet_actual->message = ui_message_temp_message.data();
                }
            }
            case GW::UI::UIMessage::kPlayerChatMessage: {
                const auto player_chat_message = static_cast<PlayerChatMessage*>(wParam);
                if (player_chat_message->message && player_chat_message->message[0] && ObfuscateMessage(player_chat_message->message, speech_message_temp_message)) {
                    player_chat_message->message = speech_message_temp_message.data();
                }
            }
            break;
        }
    }

    // TODO: Jon need to hook into gw party window invite and invite current_char rather than the obfuscated name
    [[maybe_unused]] void OnInvitePlayerInPartyWindow([[maybe_unused]] void* packet) { }



    void OnStoCPacket(GW::HookStatus*, GW::Packet::StoC::PacketBase* packet)
    {
        switch (packet->header) {
            // Temporarily obfuscate player name on resign (affected modules: InfoWindow)
            case GAME_SMSG_CHAT_MESSAGE_CORE: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                const auto packet_actual = static_cast<GW::Packet::StoC::MessageCore*>(packet);
                if (wmemcmp(packet_actual->message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) == 0) {
                    // We need to make sure that the resignlog (which holds obfuscated player names) gets the obfuscated name
                    if (ObfuscateMessage(packet_actual->message, ui_message_temp_message, true)) {
                        wcscpy(packet_actual->message, ui_message_temp_message.c_str());
                    }
                }
            }
            break;
            // Obfuscate cinematic names
            case GAME_SMSG_CINEMATIC_TEXT: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                struct Packet {
                    uint32_t header;
                    uint32_t other_atts[2];
                    wchar_t message[80];
                }* packet_actual = (Packet*)packet;
                if (ObfuscateMessage(packet_actual->message, ui_message_temp_message)) {
                    wcscpy(packet_actual->message, ui_message_temp_message.c_str());
                }
            }
            break;
            // Obfuscate incoming party searches (affected modules: Trade Window)
            case GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                struct Packet {
                    uint32_t header;
                    uint32_t other_atts[7];
                    wchar_t message[32];
                    wchar_t name[20];
                }* packet_actual = (Packet*)packet;
                if (ObfuscateName(packet_actual->name, ui_message_temp_message)) {
                    wcscpy(packet_actual->name, ui_message_temp_message.c_str());
                }
            }
            break;
            // Hide Player name on spawn
            case GAME_SMSG_AGENT_CREATE_PLAYER: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                const auto packet_actual = static_cast<GW::Packet::StoC::PlayerJoinInstance*>(packet);
                if (ObfuscateName(packet_actual->player_name, ui_message_temp_message)) {
                    wcscpy(packet_actual->player_name, ui_message_temp_message.c_str());
                }
            }
            break;
            // Hide Mercenary Hero name
            case GAME_SMSG_MERCENARY_INFO: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                const auto packet_actual = static_cast<GW::Packet::StoC::MercenaryHeroInfo*>(packet);
                if (ObfuscateName(packet_actual->name, ui_message_temp_message, true)) {
                    wcscpy(packet_actual->name, ui_message_temp_message.c_str());
                }
            }
            break;
            // Hide Mercenary Hero name after being added to party or in explorable area
            case GAME_SMSG_AGENT_UPDATE_NPC_NAME: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                const auto packet_actual = static_cast<GW::Packet::StoC::AgentName*>(packet);
                if (wcsstr(packet_actual->name_enc, L"\x108\x107") != packet_actual->name_enc) {
                    return; // Not a mercenary name
                }
                const auto end_pos = wcschr(packet_actual->name_enc, '\x1');
                ASSERT(end_pos);
                *end_pos = 0;
                const std::wstring tmp(&packet_actual->name_enc[2]);
                *end_pos = '\x1';
                if (ObfuscateName(tmp, ui_message_temp_message, true)) {
                    swprintf(packet_actual->name_enc, _countof(packet_actual->name_enc), L"\x108\x107%s\x1", ui_message_temp_message.c_str());
                }
            }
            break;
            // Hide "Customised for <player_name>". Packet header is poorly named, this is actually something like GAME_SMSG_ITEM_CUSTOMISED_NAME
            // (affected modules: HotkeysWindow)
            case GAME_SMSG_ITEM_UPDATE_NAME: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                struct Packet {
                    uint32_t chaff[2];
                    wchar_t name[32];
                }* packet_actual = (Packet*)packet;
                if (ObfuscateName(packet_actual->name, ui_message_temp_message)) {
                    wcscpy(packet_actual->name, ui_message_temp_message.c_str());
                }
            }
            break;
            case GAME_SMSG_GUILD_PLAYER_CHANGE_COMPLETE:
            case GAME_SMSG_GUILD_PLAYER_INFO:
            case GAME_SMSG_GUILD_PLAYER_ROLE: {
                ObfuscateGuildRoster(false);
                pending_guild_obfuscate = IsObfuscatorEnabled();
            }
            break;
            // Obfuscate player name in NPC dialogs
            case GAME_SMSG_SIGNPOST_BODY:
            case GAME_SMSG_DIALOG_BODY: {
                if (!IsObfuscatorEnabled()) {
                    break;
                }
                const auto packet_actual = static_cast<GW::Packet::StoC::DialogBody*>(packet);
                const wchar_t* player_name_start = wcsstr(packet_actual->message, L"\xBA9\x107");
                if (player_name_start && ObfuscateMessage(packet_actual->message, ui_message_temp_message)) {
                    wcscpy(packet_actual->message, ui_message_temp_message.c_str());
                }
            }
            break;
        }
    }

    // Function called right before GW tries to send a chat message to a player; if the player is obfuscated, we need to unobfuscate it for the server.
    void OnSendChat(GW::HookStatus* status, const GW::Chat::Channel channel, wchar_t* message)
    {
        if (channel != GW::Chat::Channel::CHANNEL_WHISPER) {
            return;
        }
        if (!IsObfuscatorEnabled()) {
            return;
        }
        // Static flag to avoid recursive callback
        static bool processing = false;
        if (processing) {
            return;
        }
        const wchar_t* whisper_separator = wcschr(message, ',');
        if (!whisper_separator) {
            return;
        }
        const size_t len = whisper_separator - message;
        const std::wstring recipient_obfuscated(message, len);
        std::wstring recipient_unobfuscated;
        if (UnobfuscateName(recipient_obfuscated, recipient_unobfuscated)) {
            // NB: Block and send a copy with the new message content; current wchar_t* may not have enough allocated memory to just replace the content.
            status->blocked = true;
            processing = true;
            GW::Chat::SendChat(recipient_unobfuscated.c_str(), &whisper_separator[1]);
            processing = false;
        }
    }

    void OnPrintChat(GW::HookStatus*, GW::Chat::Channel, wchar_t** message_ptr, FILETIME, int)
    {
        if (!IsObfuscatorEnabled()) {
            return;
        }
        // We unobfuscated the message in OnPreUIMessage - now we need to re-obfuscate it for display
        if (ObfuscateMessage(*message_ptr, ui_message_temp_message)) {
            // I think this is copied away later?
            *message_ptr = ui_message_temp_message.data();
        }
    }
#ifdef DETECT_STREAMING_APPLICATION

    [[maybe_unused]] void CALLBACK Obfuscator::OnWindowEvent(HWINEVENTHOOK _hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
        if (!running)
            return;
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
}

void Obfuscator::Obfuscate(const bool obfuscate)
{
    if (obfuscate == (pending_state == ObfuscatorState::Enabled)) {
        return;
    }
    if (obfuscate) {
        pending_state = ObfuscatorState::Enabled;
        Log::Info("Player name will be hidden on next map change");
    }
    else {
        pending_state = ObfuscatorState::Disabled;
        Log::Info("Player name will be visible on next map change");
    }
}

void Obfuscator::Initialize()
{
    ToolboxModule::Initialize();
    Reset();

    const auto GetCharacterSummary_Assertion = GW::Scanner::FindAssertion(R"(p:\code\gw\ui\char\uichinfo.cpp)", "!StrCmp(m_characterName, characterInfo.characterName)");
    if (GetCharacterSummary_Assertion) {
        // Hook to override character names on login screen
        GetCharacterSummary_Func = reinterpret_cast<GetCharacterSummary_pt>(GetCharacterSummary_Assertion - 0x4F);
        GW::HookBase::CreateHook(GetCharacterSummary_Func, OnGetCharacterSummary, reinterpret_cast<void**>(&RetGetCharacterSummary));
        GW::HookBase::EnableHooks(GetCharacterSummary_Func);
        // Patch to allow missing character summary
        GetCharacterSummary_AssertionPatch.SetPatch(GetCharacterSummary_Assertion - 0x7, "\xEB", 1);
        GetCharacterSummary_AssertionPatch.TogglePatch(true);
    }
    const uintptr_t address = GW::Scanner::FindAssertion(R"(p:\code\gw\ui\game\vendor\vnacctnameset.cpp)", "charName", -0x30);
    GetAccountData_Func = reinterpret_cast<GetAccountData_pt>(GW::Scanner::FunctionFromNearCall(address));
    if (GetAccountData_Func) {
        GW::HookBase::CreateHook(GetAccountData_Func, OnGetAccountInfo, reinterpret_cast<void**>(&GetAccountData_Ret));
        GW::HookBase::EnableHooks(GetAccountData_Func);
    }

    constexpr auto pre_hook_altitude = -0x9000; // Hooks that run before other RegisterPacketCallback hooks
    constexpr auto post_gw_altitude = 0x8000;   // Hooks that run after gw has processed the event

    constexpr uint32_t pre_hook_headers[] = {
        GAME_SMSG_AGENT_CREATE_PLAYER,
        GAME_SMSG_MERCENARY_INFO,
        GAME_SMSG_AGENT_UPDATE_NPC_NAME,
        GAME_SMSG_ITEM_UPDATE_NAME,
        GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT,
        GAME_SMSG_GUILD_PLAYER_INFO,
        GAME_SMSG_GUILD_PLAYER_CHANGE_COMPLETE,
        GAME_SMSG_GUILD_PLAYER_ROLE,
        GAME_SMSG_CHAT_MESSAGE_CORE, // Pre resignlog hook
        GAME_SMSG_CINEMATIC_TEXT,
    };
    for (const auto header : pre_hook_headers) {
        GW::StoC::RegisterPacketCallback(&stoc_hook, header, OnStoCPacket, pre_hook_altitude);
    }
    constexpr std::array pre_hook_ui_messages = {
        GW::UI::UIMessage::kShowMapEntryMessage,
        GW::UI::UIMessage::kDialogBody,
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kPlayerChatMessage
    };
    for (const auto header : pre_hook_ui_messages) {
        RegisterUIMessageCallback(&stoc_hook, header, OnUIMessage, pre_hook_altitude);
    }
    constexpr std::array post_gw_ui_messages = {
        GW::UI::UIMessage::kLogout,
        GW::UI::UIMessage::kWriteToChatLog
    };
    for (const auto header : post_gw_ui_messages) {
        RegisterUIMessageCallback(&stoc_hook, header, OnUIMessage, post_gw_altitude);
    }

    RegisterPrintChatCallback(&ctos_hook, OnPrintChat);

    RegisterSendChatCallback(&ctos_hook, OnSendChat);

    GW::Chat::CreateCommand(L"obfuscate", CmdObfuscate);
    GW::Chat::CreateCommand(L"hideme", CmdObfuscate);

#ifdef DETECT_STREAMING_APPLICATION

    CoInitialize(NULL);
    hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_DESTROY, NULL, OnWindowEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

#endif
}

void Obfuscator::SignalTerminate()
{
    if (GetCharacterSummary_Func) {
        GW::HookBase::RemoveHook(GetCharacterSummary_Func);
        GetCharacterSummary_AssertionPatch.Reset();
    }
    Obfuscate(false);
    Reset();

#ifdef DETECT_STREAMING_APPLICATION

    if (hook) {
        ASSERT(UnhookWinEvent(hook));
        CoUninitialize();
        hook = 0;
    }

#endif
}

bool Obfuscator::CanTerminate()
{
    return !GetCharacterSummary_AssertionPatch.GetIsEnable();
}

void Obfuscator::Update(float)
{
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (pending_guild_obfuscate && GW::Map::GetIsMapLoaded()) {
        ObfuscateGuildRoster(IsObfuscatorEnabled());
        pending_guild_obfuscate = false;
    }
}

void Obfuscator::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(rename_other_players);
    LOAD_BOOL(rename_friends_to_alias);
    LOAD_BOOL(rename_self);
    const auto own_name = ini->GetValue(Name(), VAR_NAME(own_player_name), own_player_name);
    if (own_name && own_name[0] != '\0') {
        strncpy_s(own_player_name, own_name, strnlen_s(own_name, _countof(own_player_name)));
        own_player_name_w = GuiUtils::StringToWString(own_player_name);
    }
    if (ini->GetBoolValue(Name(), VAR_NAME(obfuscate), pending_state == ObfuscatorState::Enabled)) {
        Obfuscate(true);
    }
}

void Obfuscator::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(obfuscate), pending_state == ObfuscatorState::Enabled);
    SAVE_BOOL(rename_other_players);
    SAVE_BOOL(rename_friends_to_alias);
    SAVE_BOOL(rename_self);
    ini->SetValue(Name(), VAR_NAME(own_player_name), own_player_name);
}

void Obfuscator::DrawSettingsInternal()
{
    bool enabled = pending_state == ObfuscatorState::Enabled;
    if (ImGui::Checkbox("Randomize character names on-screen", &enabled)) {
        Obfuscate(enabled);
        pending_guild_obfuscate = true;
    }
    ImGui::ShowHelp("Hides and overrides player names at character selection and in-game.\nThis change is applied on next map change.");
    ImGui::TextDisabled("You can also use the /hideme or /obfuscate command to toggle this at any time");

    if (ImGui::Checkbox("Rename friends to their alias", &rename_friends_to_alias)) {
        Obfuscate(enabled);
        pending_guild_obfuscate = true;
    }
    ImGui::ShowHelp("May require a GW restart to take effect.");
    if (ImGui::Checkbox("Rename other players", &rename_other_players)) {
        Obfuscate(enabled);
        pending_guild_obfuscate = true;
    }
    ImGui::ShowHelp("May lead to bugs. May require a GW restart to take effect.");
    if (ImGui::Checkbox("Rename self", &rename_self)) {
        Obfuscate(enabled);
        pending_guild_obfuscate = true;
    }
    ImGui::ShowHelp("Renames yourself in-game and in character select.");
    if (ImGui::InputText("Set own character name", own_player_name, _countof(own_player_name))) {
        if (own_player_name[0] != '\0') {
            own_player_name_w = GuiUtils::StringToWString(own_player_name);
        }
        else {
            own_player_name_w = {};
        }
        Obfuscate(enabled);
        Reset();
    }
    ImGui::ShowHelp("If empty, a random name will be chosen. May require a GW restart to take effect.");
}

bool Obfuscator::IsObfuscatedName(const std::wstring& name)
{
    return obfuscated_by_original.contains(name) || obfuscated_by_obfuscation.contains(name);
}
