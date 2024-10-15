#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Hook.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Keys.h>
#include <Logger.h>

#include <Modules/ChatCommands.h>
#include <Modules/GameSettings.h>
#include <Modules/ChatSettings.h>
#include <Modules/InventoryManager.h>
#include <Widgets/PartyDamage.h>
#include <Windows/BuildsWindow.h>
#include <Windows/Hotkeys.h>
#include <Windows/MainWindow.h>
#include <Windows/SettingsWindow.h>
#include <Widgets/TimerWidget.h>
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/DialogModule.h>
#include <Modules/Resources.h>

constexpr auto CMDTITLE_KEEP_CURRENT = 0xfffe;
constexpr auto CMDTITLE_REMOVE_CURRENT = 0xffff;



namespace {
    const wchar_t* next_word(const wchar_t* str)
    {
        while (*str && !isspace(*str)) {
            str++;
        }
        while (*str && isspace(*str)) {
            str++;
        }
        return *str ? str : nullptr;
    }

    bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }

    float GetAngle(const GW::GamePos& pos)
    {
        constexpr float pi = DirectX::XM_PI;
        float tan_angle;
        if (pos.x == 0.0f) {
            if (pos.y >= 0.0f) {
                tan_angle = pi / 2;
            }
            else {
                tan_angle = pi / 2 * -1.0f;
            }
        }
        else if (pos.x < 0.0f) {
            if (pos.y >= 0.0f) {
                tan_angle = atan(pos.y / pos.x) + pi;
            }
            else {
                tan_angle = atan(pos.y / pos.x) - pi;
            }
        }
        else {
            tan_angle = atan(pos.y / pos.x);
        }
        tan_angle *= 180.0f / pi;
        return tan_angle;
    };

    void TargetVipers()
    {
        // target best vipers target (closest)
        GW::AgentArray* agents = GW::Agents::GetAgentArray();
        const GW::Agent* me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me == nullptr) {
            return;
        }

        const auto wanted_angle = me->rotation_angle * 180.0f / DirectX::XM_PI;
        constexpr auto max_angle_diff = 22.5f; // Acceptable angle for vipers
        float max_distance = GW::Constants::SqrRange::Spellcast;

        auto closest = static_cast<size_t>(-1);
        for (size_t i = 0, size = agents->size(); i < size; i++) {
            const auto agent = static_cast<GW::AgentLiving*>(agents->at(i));
            if (agent == nullptr || agent == me || !agent->GetIsLivingType() || agent->GetIsDead()) {
                continue;
            }
            const float this_distance = GetSquareDistance(me->pos, agent->pos);
            if (this_distance > max_distance) {
                continue;
            }
            const float agent_angle = GetAngle(me->pos - agent->pos);
            const float this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff) {
                continue;
            }
            closest = i;
            max_distance = this_distance;
        }
        if (closest != static_cast<size_t>(-1)) {
            GW::Agents::ChangeTarget(agents->at(closest));
        }
    }

    void TargetEE()
    {
        // target best ebon escape target
        GW::AgentArray* agents = GW::Agents::GetAgentArray();
        const GW::Agent* me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me == nullptr) {
            return;
        }

        const auto facing_angle = me->rotation_angle * 180.0f / DirectX::XM_PI;
        const auto wanted_angle = facing_angle > 0.0f ? facing_angle - 180.0f : facing_angle + 180.0f;
        constexpr auto max_angle_diff = 22.5f; // Acceptable angle for ebon escape
        constexpr auto max_distance = GW::Constants::SqrRange::Spellcast;
        float distance = 0.0f;

        auto closest = static_cast<size_t>(-1);
        for (size_t i = 0, size = agents->size(); i < size; i++) {
            const auto agent = static_cast<GW::AgentLiving*>(agents->at(i));
            if (agent == nullptr || agent == me
                || !agent->GetIsLivingType() || agent->GetIsDead()
                || agent->allegiance == GW::Constants::Allegiance::Enemy
                || !GW::Agents::GetIsAgentTargettable(agent)) {
                continue;
            }
            const float this_distance = GetSquareDistance(me->pos, agent->pos);
            if (this_distance > max_distance || distance > this_distance) {
                continue;
            }
            const auto agent_angle = GetAngle(me->pos - agent->pos);
            const auto this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff) {
                continue;
            }
            closest = i;
            distance = this_distance;
        }
        if (closest != static_cast<size_t>(-1)) {
            GW::Agents::ChangeTarget(agents->at(closest));
        }
    }

    // Fetches agent id of party member (hero, player or henchman) by index. This is NOT player number!
    uint32_t GetPartyMemberAgentId(uint32_t party_member_index) {
        uint32_t current_idx = (uint32_t)-1;
        const auto party = GW::PartyMgr::GetPartyInfo();
        if (!party) return 0;
        for (const auto& player_member : party->players) {
            current_idx++;
            if (current_idx == party_member_index) {
                const auto player = GW::PlayerMgr::GetPlayerByID(player_member.login_number);
                return player ? player->agent_id : 0;
            }
            for (const auto& hero : party->heroes) {
                if (hero.owner_player_id != player_member.login_number)
                    continue;
                current_idx++;
                if (current_idx == party_member_index) {
                    return hero.agent_id;
                }
            }
        }
        for (const auto& hench_member : party->henchmen) {
            current_idx++;
            if (current_idx == party_member_index) {
                return hench_member.agent_id;
            }
        }
        return 0;
    }

    bool IsNearestStr(const wchar_t* str)
    {
        return wcscmp(str, L"nearest") == 0 || wcscmp(str, L"closest") == 0;
    }

    std::map<std::string, ChatCommands::PendingTransmo> npc_transmos;

    struct DecodedTitleName {
        DecodedTitleName(const GW::Constants::TitleID in)
            : title(in)
        {
            name.reset(GW::PlayerMgr::GetTitleData(title)->name_id);
        };
        GW::Constants::TitleID title;
        GuiUtils::EncString name;
    };

    std::vector<DecodedTitleName*> title_names;
    bool title_names_sorted = false;

    GW::Array<GW::Title>* GetTitles()
    {
        const auto w = GW::GetWorldContext();
        return w ? &w->titles : nullptr;
    }

    using SetMuted_pt = void(__cdecl*)(bool mute);
    SetMuted_pt SetMuted_Func;
    using PostMute_pt = void(__cdecl*)(int param);
    PostMute_pt PostMuted_Func;

    GW::UI::UIInteractionCallback OnChatInteraction_Callback_Func = nullptr;
    GW::UI::UIInteractionCallback OnChatInteraction_Callback_Ret = nullptr;

    // '/chat [all|guild|team|trade|alliance|whisper|close]'
    const char* chat_tab_syntax = "'/chat [all|guild|team|trade|alliance|whisper]' open chat channel.";

    void CHAT_CMD_FUNC(CmdChatTab)
    {
        if (argc < 2) {
            return Log::Error(chat_tab_syntax);
        }
        uint32_t channel = 0xff;
        if (wcscmp(argv[1], L"all") == 0) {
            channel = 0;
        }
        else if (wcscmp(argv[1], L"guild") == 0) {
            channel = 2;
        }
        else if (wcscmp(argv[1], L"team") == 0) {
            channel = 3;
        }
        else if (wcscmp(argv[1], L"trade") == 0) {
            channel = 4;
        }
        else if (wcscmp(argv[1], L"alliance") == 0) {
            channel = 1;
        }
        else if (wcscmp(argv[1], L"whisper") == 0) {
            channel = 5;
        }
        else {
            return Log::Error(chat_tab_syntax);
        }
        channel |= 0x8000;
        GW::GameThread::Enqueue([channel] {
            // See OnChatUI_Callback for intercept
            SendUIMessage(GW::UI::UIMessage::kAppendMessageToChat, (void*)L"", (void*)channel);
        });
    }

    using FocusChatTab_pt = void(__fastcall*)(void* chat_frame, void* edx, uint32_t tab);
    FocusChatTab_pt FocusChatTab_Func = nullptr;

    void OnChatUI_Callback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        // If a channel was given in the UI message, set it now.
        if (static_cast<GW::UI::UIMessage>(message->message_id) == GW::UI::UIMessage::kAppendMessageToChat && lParam) {
            uint32_t* frame_ptr = *(uint32_t**)message->wParam;
            const auto tab = (uint32_t)lParam ^ 0x8000;
            if (tab < 6 && FocusChatTab_Func) {
                FocusChatTab_Func(frame_ptr, nullptr, tab);
                GW::Hook::LeaveHook();
                return;
            }
        }

        OnChatInteraction_Callback_Ret(message, wParam, lParam);
        GW::Hook::LeaveHook();
    }

    bool* is_muted = nullptr;

    void CHAT_CMD_FUNC(CmdSkillImage)
    {
        uint32_t skill_id = 0;
        GuiUtils::ParseUInt(argv[1], &skill_id);
        auto s = new GW::Packet::StoC::UpdateSkillbarSkill();
        s->agent_id = GW::Agents::GetControlledCharacterId();
        s->skill_slot = 0;
        s->skill_id = skill_id;
        GW::GameThread::Enqueue([s] {
            GW::StoC::EmulatePacket(s);
            delete s;
        });
    }

    void CHAT_CMD_FUNC(CmdPlayEffect)
    {
        const auto player = GW::Agents::GetObservingAgent();
        if (!player) return;
        auto packet = new GW::Packet::StoC::PlayEffect();
        memset(packet, 0, sizeof(*packet));
        packet->header = GW::Packet::StoC::PlayEffect::STATIC_HEADER;
        packet->agent_id = player->agent_id;
        packet->coords = player->pos;
        GuiUtils::ParseUInt(argv[1], &packet->effect_id);
        GW::GameThread::Enqueue([packet] {
            GW::StoC::EmulatePacket(packet);
            delete packet;
            });
    }

    HallOfMonumentsAchievements hom_achievements;
    bool hom_loading = false;

    void OnAchievementsLoaded(HallOfMonumentsAchievements* result)
    {
        hom_loading = false;
        result->OpenInBrowser();
    }

    const char* fps_syntax = "'/fps [limit (15-400)]' sets a hard frame limit for Guild Wars. Pass '0' to remove the limit.\n'/fps' shows current frame limit";

    void CHAT_CMD_FUNC(CmdFps)
    {
        if (argc < 2) {
            const auto current_limit = GW::UI::GetFrameLimit();
            if (!current_limit) {
                Log::Info("Frame limit is not set");
            }
            else {
                Log::Info("Frame limit set to %dfps", current_limit);
            }
        }
        uint32_t frame_limit = 0;
        if (!GuiUtils::ParseUInt(argv[1], &frame_limit)) {
            return Log::Error(fps_syntax);
        }
        if (frame_limit && frame_limit < 15) {
            frame_limit = 15;
        }
        if (frame_limit && frame_limit > 400) {
            frame_limit = 400;
        }
        GW::UI::SetFrameLimit(frame_limit);
    }

    const char* pref_syntax = "'/pref [preference] [number (0-4)]' set the in-game preference setting in Guild Wars.\n'/pref list' to list the preferences available to set.";
    using CmdPrefCB = void(__cdecl*)(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id);

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdValuePref(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id)
    {
        const auto pref = static_cast<GW::UI::NumberPreference>(pref_id);

        // Find value and set preference
        uint32_t value = 0xff;
        if (argc > 2 && GuiUtils::ParseUInt(argv[2], &value) && SetPreference(pref, value)) {
            return; // Success
        }

        // Print current value
        if (argc < 3) {
            Log::InfoW(L"Current preference value for %s is %d", argv[1], GetPreference(pref));
        }
        return Log::Error(pref_syntax);
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdEnumPref(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id)
    {
        const auto pref = static_cast<GW::UI::EnumPreference>(pref_id);

        // Find value and set preference
        uint32_t value = 0xff;
        if (argc > 2 && GuiUtils::ParseUInt(argv[2], &value) && SetPreference(pref, value)) {
            return; // Success
        }

        // Print current value
        if (argc < 3) {
            Log::InfoW(L"Current preference value for %s is %d", argv[1], GetPreference(pref));
        }

        // Got this far; print out available values for this preference.
        uint32_t* values = nullptr;
        const auto available = GetPreferenceOptions(pref, &values);
        wchar_t available_vals_buffer[120];
        uint32_t offset = 0;
        offset += swprintf(&available_vals_buffer[offset], offset - _countof(available_vals_buffer), L"Available values for %s: ", argv[1]);
        for (size_t i = 0; i < available; i++) {
            offset += swprintf(&available_vals_buffer[offset], offset - _countof(available_vals_buffer), i > 0 ? L", %d" : L"%d", values[i]);
        }
        Log::InfoW(available_vals_buffer);
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdFlagPref(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id)
    {
        const auto pref = static_cast<GW::UI::FlagPreference>(pref_id);

        if (argc > 2) {
            // Setting value
            if (wcscmp(argv[2], L"toggle") == 0) {
                SetPreference(pref, !GetPreference(pref));
                return;
            }
            uint32_t value = 0xff;
            if (GuiUtils::ParseUInt(argv[2], &value)) {
                SetPreference(pref, value == 1 ? 1 : 0);
            }
            return;
        }
        // Print current value
        Log::InfoW(L"Current preference value for %s is %d", argv[1], GetPreference(pref));
    }

    struct PrefMapCommand {
        PrefMapCommand(GW::UI::EnumPreference p)
            : preference_id(std::to_underlying(p))
        {
            preference_callback = CmdEnumPref;
        }

        PrefMapCommand(GW::UI::NumberPreference p)
            : preference_id(std::to_underlying(p))
        {
            preference_callback = CmdValuePref;
        }

        PrefMapCommand(GW::UI::FlagPreference p)
            : preference_id(std::to_underlying(p))
        {
            preference_callback = CmdFlagPref;
        }

        uint32_t preference_id;
        CmdPrefCB preference_callback;
    };

    using PrefMap = std::map<const std::wstring, const PrefMapCommand>;
    PrefMap pref_map;

    const PrefMap& getPrefCommandOptions()
    {
        if (pref_map.empty()) {
            pref_map = {
                {L"antialiasing", GW::UI::EnumPreference::AntiAliasing},
                {L"shaderquality", GW::UI::EnumPreference::ShaderQuality},
                {L"terrainquality", GW::UI::EnumPreference::TerrainQuality},
                {L"reflections", GW::UI::EnumPreference::Reflections},
                {L"shadowquality", GW::UI::EnumPreference::ShadowQuality},
                {L"interfacesize", GW::UI::EnumPreference::InterfaceSize},
                {L"texturequality", GW::UI::NumberPreference::TextureQuality},
                {L"channel", GW::UI::NumberPreference::ChatTab},
                {L"alliance", GW::UI::FlagPreference::ChannelAlliance},
                {L"guild", GW::UI::FlagPreference::ChannelGuild},
                {L"team", GW::UI::FlagPreference::ChannelGroup},
                {L"alliance", GW::UI::FlagPreference::ChannelAlliance},
                {L"music", GW::UI::NumberPreference::MusicVolume},
                {L"disablemousewalking", GW::UI::FlagPreference::DisableMouseWalking}
            };
        }
        return pref_map;
    };


    void CHAT_CMD_FUNC(CmdPref)
    {
        const auto& options = getPrefCommandOptions();
        if (argc > 1 && wcscmp(argv[1], L"list") == 0) {
            const auto buf_size = 16 * options.size(); // Roughly 16 chars per option label
            const auto buffer = new wchar_t[buf_size];
            auto offset = 0;
            for (auto it = options.begin(); it != options.end(); ++it) {
                const auto written = swprintf(&buffer[offset], offset - buf_size, offset > 0 ? L", %s" : L"%s", it->first.c_str());
                ASSERT(written != -1);
                offset += written;
            }
            Log::InfoW(L"/pref options:\n%s", buffer);
            delete[] buffer;
        }
        if (argc < 2) {
            return Log::Error(pref_syntax);
        }

        // Find preference by name
        const auto found = options.find(argv[1]);
        if (found == options.end()) {
            return Log::Error(pref_syntax);
        }
        const PrefMapCommand* pref = &found->second;

        pref->preference_callback(message, argc, argv, pref->preference_id);
    }


    constexpr auto withdraw_syntax = "'/withdraw <quantity (1-65535)> [model_id1 model_id2 ...]' tops up your inventory "
        "with a minimum quantity of 1 or more items, identified by model_id\n"
        "If no model_ids are passed, withdraws <quantity>[k] gold from storage\n"
        "If quantity is 'all' and you do not pass model_ids, withdraws all gold you have or can hold.";
    constexpr auto deposit_syntax = "'/deposit <quantity (1-65535)> [model_id1 model_id2 ...]' deposits <quantity> items, "
        "identified by model ids, from your inventory to your storage.\n"
        "If no model_ids are passed, deposits <quantity>[k] gold from your inventory\n"
        "If quantity is 'all' and you do not pass model_ids, deposits all gold [platinum] from your inventory to your storage.";

    struct CmdAlias {
        char alias_cstr[256] = {};
        wchar_t alias_wstr[128] = {};
        char command_cstr[512] = {};
        wchar_t command_wstr[256] = {};
        bool processing = false;
    };

    std::vector<CmdAlias*> cmd_aliases;

    void sort_cmd_aliases() {
        std::ranges::stable_sort(cmd_aliases, [](const auto* a, const auto* b) {
            if (a->alias_cstr[0] == '\0' && b->alias_cstr[0] != '\0') {
                return false;
            }
            if (b->alias_cstr[0] == '\0' && a->alias_cstr[0] != '\0') {
                return true;
            }
            return strcmp(a->alias_cstr, b->alias_cstr) < 0;
        });
    }

    GW::HookEntry OnSentChat_HookEntry;

    void OnSendChat(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        ASSERT(message_id == GW::UI::UIMessage::kSendChatMessage);
        const auto message = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam)->message;
        if (!(message && *message))
            return;
        const auto channel = GW::Chat::GetChannel(*message);
        if (channel != GW::Chat::CHANNEL_COMMAND || status->blocked) {
            return;
        }
        for (const auto alias : cmd_aliases) {
            const auto sent_alias = GuiUtils::ToLower(&message[1]);
            if (wcscmp(alias->alias_wstr, sent_alias.c_str()) == 0 && !alias->processing && wcslen(alias->command_wstr) > 1) {
                status->blocked = true;
                alias->processing = true;
                std::wstring tmp;
                std::vector<std::wstring> parts;
                std::wstringstream wss(alias->command_wstr);
                while (std::getline(wss, tmp, L'\n')) {
                    if (tmp.length() < 2)
                        continue;
                    GW::Chat::SendChat((char)tmp[0], &tmp[1]);
                }
                alias->processing = false;
            }
        }
    }

    void CHAT_CMD_FUNC(CmdTick) {
        GW::PartyMgr::Tick(!GW::PartyMgr::GetIsPlayerTicked());
    }
    void CHAT_CMD_FUNC(CmdCallTarget) {
        const auto* target = GW::Agents::GetTarget();
        if (!target)
            return;
        auto call_packet = GW::UI::UIPacket::kSendCallTarget{ GW::CallTargetType::AttackingOrTargetting, target->agent_id };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &call_packet);
    }

    std::vector<std::pair< const wchar_t*, GW::Chat::ChatCommandCallback>> chat_commands;
} // namespace

void ChatCommands::CreateAlias(const wchar_t* alias, const wchar_t* message) {
    if (alias && *alias == L'/')
        alias++;
    if (!(alias && *alias && message && *message))
        return;
    const auto found = std::ranges::find_if(cmd_aliases, [alias, message](const CmdAlias* cmp) {
        return wcscmp(alias, cmp->alias_wstr) == 0 && wcscmp(message, cmp->command_wstr) == 0;
    });
    CmdAlias* alias_obj = nullptr;
    if (found != cmd_aliases.end()) {
        alias_obj = *found;
    }
    else {
        alias_obj = new CmdAlias();
        const auto alias_cstr = GuiUtils::WStringToString(alias);
        strcpy(alias_obj->alias_cstr, alias_cstr.c_str());
        wcscpy(alias_obj->alias_wstr, alias);
        cmd_aliases.push_back(alias_obj);
    }

    const auto message_cstr = GuiUtils::WStringToString(message);
    strcpy(alias_obj->command_cstr, message_cstr.c_str());
    wcscpy(alias_obj->command_wstr, message);
}
void ChatCommands::TransmoAgent(DWORD agent_id, PendingTransmo& transmo)
{
    if (!transmo.npc_id || !agent_id) {
        return;
    }
    const auto a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
    if (!a || !a->GetIsLivingType()) {
        return;
    }
    DWORD& npc_id = transmo.npc_id;
    DWORD& scale = transmo.scale;
    const GW::NPCArray& npcs = GW::GetGameContext()->world->npcs;
    if (npc_id == static_cast<DWORD>(std::numeric_limits<int>::max() - 1)) {
        // Scale only
        npc_id = a->player_number;
        if (a->transmog_npc_id & 0x20000000) {
            npc_id = a->transmog_npc_id ^ 0x20000000;
        }
    }
    else if (npc_id == static_cast<DWORD>(std::numeric_limits<int>::max())) {
        // Reset
        npc_id = 0;
        scale = 0x64000000;
    }
    else if (npc_id >= npcs.size() || !npcs[npc_id].model_file_id) {
        const DWORD& npc_model_file_id = transmo.npc_model_file_id;
        const DWORD& npc_model_file_data = transmo.npc_model_file_data;
        const DWORD& flags = transmo.flags;
        if (!npc_model_file_id) {
            return;
        }
        // Need to create the NPC.
        // Those 2 packets (P074 & P075) are used to create a new model, for instance if we want to "use" a tonic.
        // We have to find the data that are in the NPC structure and feed them to those 2 packets.
        GW::NPC npc = {0};
        npc.model_file_id = npc_model_file_id;
        npc.npc_flags = flags;
        npc.primary = 1;
        npc.scale = scale;
        npc.default_level = 0;
        GW::GameThread::Enqueue([npc_id, npc] {
            GW::Packet::StoC::NpcGeneralStats packet{};
            packet.npc_id = npc_id;
            packet.file_id = npc.model_file_id;
            packet.data1 = 0;
            packet.scale = npc.scale;
            packet.data2 = 0;
            packet.flags = npc.npc_flags;
            packet.profession = npc.primary;
            packet.level = npc.default_level;
            packet.name[0] = 0;
            GW::StoC::EmulatePacket(&packet);
        });
        if (npc_model_file_data) {
            GW::GameThread::Enqueue([npc_id, npc_model_file_data] {
                GW::Packet::StoC::NPCModelFile packet;
                packet.npc_id = npc_id;
                packet.count = 1;
                packet.data[0] = npc_model_file_data;

                GW::StoC::EmulatePacket(&packet);
            });
        }
    }
    GW::GameThread::Enqueue([npc_id, agent_id, scale] {
        if (npc_id) {
            const GW::NPCArray& npcs = GW::GetGameContext()->world->npcs;
            const GW::NPC npc = npcs[npc_id];
            if (!npc.model_file_id) {
                return;
            }
        }
        GW::Packet::StoC::AgentScale packet1;
        packet1.header = GW::Packet::StoC::AgentScale::STATIC_HEADER;
        packet1.agent_id = agent_id;
        packet1.scale = scale;
        GW::StoC::EmulatePacket(&packet1);

        GW::Packet::StoC::AgentModel packet2;
        packet2.header = GW::Packet::StoC::AgentModel::STATIC_HEADER;
        packet2.agent_id = agent_id;
        packet2.model_id = npc_id;
        GW::StoC::EmulatePacket(&packet2);
    });
}

bool ChatCommands::GetNPCInfoByName(const std::string& name, PendingTransmo& transmo)
{
    for (const auto& npc_transmo : npc_transmos) {
        const size_t found_len = npc_transmo.first.find(name);
        if (found_len == std::string::npos) {
            continue;
        }
        transmo = npc_transmo.second;
        return true;
    }
    return false;
}

bool ChatCommands::GetNPCInfoByName(const std::wstring& name, PendingTransmo& transmo)
{
    return GetNPCInfoByName(GuiUtils::WStringToString(name), transmo);
}

void ChatCommands::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }

    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::TextDisabled("Below, <xyz> denotes an argument, use an appropriate value without the quotes.\n"
        "(a|b) denotes a mandatory argument, in this case 'a' or 'b'.\n"
        "[a|b] denotes an optional argument, in this case nothing, 'a' or 'b'.");

    ImGui::Bullet();
    ImGui::Text("'/age2' prints the instance time to chat.");
    ImGui::Bullet();
    ImGui::Text("'/animation [me|target] [animation_id (1-2076)]' to make you (or a target) do a specific animation");
    ImGui::Bullet();
    ImGui::Text("'/armor' is an alias for '/pingitem armor'.");
    ImGui::Bullet();
    ImGui::Text("'/bonds [remove|add] [party_member_index|all] [all|skill_id]' remove or add bonds from a single party member, or all party members");
    ImGui::Bullet();
    ImGui::Text("'/borderless [on|off]' toggles, enables or disables borderless window.");
    ImGui::Bullet();
    ImGui::Text("'/call' ping current target.");
    ImGui::Bullet();
    ImGui::Text("'/camera (lock|unlock)' to lock or unlock the camera.");
    ImGui::Bullet();
    ImGui::Text("'/camera fog (on|off)' sets game fog effect on or off.");
    ImGui::Bullet();
    ImGui::Text("'/camera fov <value>' sets the field of view. '/camera fov' resets to default.");
    ImGui::Bullet();
    ImGui::Text("'/camera speed <value>' sets the unlocked camera speed.");
    ImGui::Bullet();
    ImGui::Text(chat_tab_syntax);
    ImGui::Bullet();
    ImGui::Text("'/chest' opens xunlai in outposts.");
    ImGui::Bullet();
    ImGui::Text("'/damage' or '/dmg' to print party damage to chat.\n"
        "'/damage me' sends your own damage only.\n"
        "'/damage <number>' sends the damage of a party member (e.g. '/damage 3').\n"
        "'/damage reset' resets the damage in party window.");
    ImGui::Bullet();
    ImGui::Text("'/dialog <id>' sends a dialog id to the current NPC you're talking to.\n"
        "'/dailog take' automatically takes the first available quest/reward from the NPC you're talking to.");
    ImGui::Bullet();
    ImGui::Text("'/enter [fow|uw]' to enter the mission for your outpost.\n"
        "If in embark, toa, urgoz or deep, it will use a scroll.\n"
        "If in an outpost with an available mission, it will begin the mission countdown.");
    ImGui::Bullet();
    ImGui::Text("'/ff' alias for '/resign'");
    ImGui::Bullet();
    ImGui::Text("'/flag [all|clear|<number>]' to flag a hero in the minimap (same as using the buttons by the minimap).");
    ImGui::Bullet();
    ImGui::Text("'/flag [all|<number>] [x] [y]' to flag a hero to coordinates [x],[y].");
    ImGui::Bullet();
    ImGui::Text("'/flag <number> clear' to clear flag for a hero.");
    ImGui::Bullet();
    ImGui::Text(fps_syntax);
    ImGui::Bullet();
    ImGui::Text("'/hero [avoid|guard|attack]' to set your hero behavior in an explorable area.");
    const auto toggle_hint = "<name> options: helm, costume, costume_head, cape, <window_or_widget_name>";
    ImGui::Bullet();
    ImGui::Text("'/hide <name>' closes the window, in-game feature or widget titled <name>.");
    ImGui::ShowHelp(toggle_hint);
    ImGui::Bullet();
    ImGui::Text("'/hm' or '/hardmode' to set hard mode difficulty in an outpost.");
    ImGui::Bullet();
    ImGui::Text("'/hom' opens the Hall of Monuments calculator with the current target player's achievements.");
    ImGui::Bullet();
    ImGui::Text("'/load [build template|build name] [Hero index]' loads a build. The build name must be between quotes if it contains spaces. First Hero index is 1, last is 7. Leave out for player");
    ImGui::Bullet();
    ImGui::Text("'/loadprefs' to load GW settings from '<GWToolbox Dir>/<Current GW Account Email>_GuildWarsSettings.ini'\n"
        "'/loadprefs <filename>' to load GW settings from '<GWToolbox Dir>/<filename>.ini'");
    ImGui::Bullet();
    ImGui::Text("'/nm' or '/normalmode' to set normal mode difficulty in an outpost.");
    ImGui::Bullet();
    ImGui::Text("'/morale' to send your current morale/death penalty info to team chat.");
    ImGui::Bullet();
    ImGui::Text("'/marktarget' to highlight the current target on the gwtoolbox minimap.\n"
                "'/marktarget clear' to unhighlight the current target on the gwtoolbox minimap.\n"
                "'/marktarget clearall' to clear all highlighted targets on the gwtoolbox minimap.");
    ImGui::Bullet();
    ImGui::Text("'/observer:reset' resets observer mode data.");
    ImGui::Bullet();
    ImGui::Text("'/pingitem <equipped_item>' to ping your equipment in chat.\n"
        "<equipped_item> options: armor, head, chest, legs, boots, gloves, offhand, weapon, weapons, costume");
    ImGui::Bullet();
    ImGui::Text("'/pcons [on|off]' toggles, enables or disables pcons.");
    ImGui::Bullet();
    ImGui::Text(pref_syntax);
    ImGui::Bullet();
    ImGui::Text("'/resize <width> <height>' resize the GW window");
    ImGui::Bullet();
    ImGui::Text("'/saveprefs' to save GW settings to '<GWToolbox Dir>/<Current GW Account Email>_GuildWarsSettings.ini'\n"
        "'/saveprefs <filename>' to save GW settings to '<GWToolbox Dir>/<filename>.ini'");
    ImGui::Bullet();
    ImGui::Text("'/scwiki [<search_term>]' search https://wiki.fbgmguild.com.");
    ImGui::Bullet();
    ImGui::Text("'/show <name>' opens the window, in-game feature or widget titled <name>.");
    ImGui::ShowHelp(toggle_hint);
    ImGui::Bullet();
    ImGui::Text("'/toggle <name> [on|off|toggle]' toggles the window, in-game feature or widget titled <name>.");
    ImGui::ShowHelp(toggle_hint);
    ImGui::Bullet();
    ImGui::Text("'/target closest' to target the closest agent to you.\n"
        "'/target ee' to target best ebon escape agent.\n"
        "'/target hos' to target best vipers/hos agent.\n"
        "'/target [name|model_id] [index]' target nearest NPC by name or model_id. \n\tIf index is specified, it will target index-th by ID.\n"
        "'/target player [name|player_number]' target nearest player by name or player number.\n"
        "'/target gadget [name|gadget_id]' target nearest interactive object by name or gadget_id.\n"
        "'/target priority [partymember]' to target priority target of party member.");
    ImGui::Bullet();
    ImGui::Text("'/tb <name>' toggles the window or widget titled <name>.");
    ImGui::Bullet();
    ImGui::Text("'/tb reset' moves Toolbox and Settings window to the top-left corner.");
    ImGui::Bullet();
    ImGui::Text("'/tb quit' or '/tb exit' completely closes toolbox and all its windows.");
    const auto transmo_hint = "<npc_name> options: eye, zhu, kuunavang, beetle, polar, celepig, \n"
        "  destroyer, koss, bonedragon, smite, kanaxai, skeletonic, moa";
    ImGui::Bullet();
    ImGui::Text("'/transmo <npc_name> [size (6-255)]' to change your appearance into an NPC.\n"
        "'/transmo' to change your appearance into target NPC.\n"
        "'/transmo reset' to reset your appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet();
    ImGui::Text("'/transmoparty <npc_name> [size (6-255)]' to change your party's appearance into an NPC.\n"
        "'/transmoparty' to change your party's appearance into target NPC.\n"
        "'/transmoparty reset' to reset your party's appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet();
    ImGui::Text("'/transmotarget <npc_name> [size (6-255)]' to change your target's appearance into an NPC.\n"
        "'/transmotarget reset' to reset your target's appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet();
    ImGui::Text("'/travel <town> [dis]', '/tp <town> [dis]' or '/to <town> [dis]' travel to outpost best matching <town> name. \n"
        "[dis] can be any of: ae, ae1, ee, eg, int, etc");
    ImGui::Bullet();
    ImGui::Text("'/travel [zv|zb|zm]' travel to nearest unlocked outpost to daily quest.");
    ImGui::Bullet();
    ImGui::Text("'/useskill <skill>' starts using the skill on recharge. "
        "Use the skill number instead of <skill> (e.g. '/useskill 5'). "
        "Use empty '/useskill' or '/useskill stop' to stop all. "
        "Use '/useskill <skill>' to stop the skill.");
    ImGui::Bullet();
    ImGui::Text("'/volume [master|music|background|effects|dialog|ui] <amount (0-100)>' set in-game volume.");
    ImGui::Bullet();
    ImGui::Text("'/wiki [quest|<search_term>]' search GWW for current quest or search term. By default, will search for the current map.");
    ImGui::Bullet();
    ImGui::Text(withdraw_syntax);
    ImGui::Bullet();
    ImGui::Text(deposit_syntax);

    ImGui::TreePop();
}

void ChatCommands::DrawSettingsInternal()
{
    ImGui::Text("'/cam unlock' options");
    ImGui::Indent();
    ImGui::Checkbox("Fix height when moving forward", &forward_fix_z);
    ImGui::InputFloat("Camera speed", &cam_speed);
    ImGui::Unindent();
    std::string preview = "Select...";
    switch (default_title_id) {
        case CMDTITLE_KEEP_CURRENT:
            preview = "Keep current title";
            break;
        case CMDTITLE_REMOVE_CURRENT:
            preview = "Remove title";
            break;
        default:
            const auto selected = std::ranges::find_if(title_names, [&](auto* it) { return std::to_underlying(it->title) == default_title_id; });

            if (selected != title_names.end()) {
                preview = (*selected)->name.string();
            }
            break;
    }

    ImGui::Text("Default '/title' command fallback action");
    ImGui::ShowHelp("Toolbox will reapply this title if there isn't an approriate title for the area you're in.\nIf your current character doesn't have the selected title, nothing will happen.");
    ImGui::Indent();
    if (ImGui::BeginCombo("###title_command_fallback", preview.c_str())) {
        if (ImGui::Selectable("Keep current title", CMDTITLE_KEEP_CURRENT == default_title_id)) {
            default_title_id = CMDTITLE_KEEP_CURRENT;
        }
        if (ImGui::Selectable("Remove title", CMDTITLE_REMOVE_CURRENT == default_title_id)) {
            default_title_id = CMDTITLE_REMOVE_CURRENT;
        }
        for (auto* it : title_names) {
            if (ImGui::Selectable(it->name.string().c_str(), std::to_underlying(it->title) == default_title_id)) {
                default_title_id = std::to_underlying(it->title);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Unindent();

    ImGui::TextUnformatted("Chat Command aliases");
    ImGui::TextDisabled("First matching command alias found will be triggered");

    const auto avail_w = ImGui::GetContentRegionAvail().x - 128.f;
    for (auto it = cmd_aliases.begin(); it != cmd_aliases.end(); ++it) {
        ImGui::PushID(it._Ptr);

        ImGui::PushItemWidth(avail_w * .3f);
        if (ImGui::InputText("###cmd_alias", (*it)->alias_cstr, _countof(CmdAlias::alias_cstr))) {
            swprintf((*it)->alias_wstr, _countof(CmdAlias::alias_wstr), L"%S", (*it)->alias_cstr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Alias for this command");
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        const auto text_height = ImGui::GetTextLineHeightWithSpacing();
        const auto num_newlines = 1 + std::count((*it)->command_cstr, (*it)->command_cstr + _countof(CmdAlias::command_cstr), '\n');
        if (ImGui::InputTextMultiline("##cmd_command", (*it)->command_cstr,
            _countof(CmdAlias::command_cstr), ImVec2(avail_w * .6f, text_height + num_newlines * ImGui::GetTextLineHeight()))) {
            swprintf((*it)->command_wstr, _countof(CmdAlias::command_wstr), L"%S", (*it)->command_cstr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Chat command to trigger");
        }
        ImGui::SameLine(avail_w);
        static bool confirm_delete = false;
        if (ImGui::SmallConfirmButton("Delete", &confirm_delete, "Are you sure you want to delete this entry?")) {
            cmd_aliases.erase(it);
            confirm_delete = false;
            ImGui::PopID();
            break; // Skip this frame
        }
        ImGui::PopID();
    }
    if (ImGui::Button("Add New Alias")) {
        wchar_t tmp[32];
        swprintf(tmp, _countof(tmp), L"alias_%d", cmd_aliases.size());
        CreateAlias(tmp, L"#hello world");
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort")) {
        sort_cmd_aliases();
    }
}

void ChatCommands::LoadSettings(ToolboxIni* ini)
{
    LOAD_BOOL(forward_fix_z);
    LOAD_FLOAT(cam_speed);
    LOAD_UINT(default_title_id);

    for (const auto* it : cmd_aliases) {
        delete it;
    }
    cmd_aliases.clear();
    const auto section_name = "Chat Command Aliases";

    ToolboxIni::TNamesDepend entries;
    ini->GetAllKeys(section_name, entries);
    for (const auto& entry : entries) {
        if (!entry.pItem[0]) {
            continue;
        }
        auto alias = std::string(entry.pItem);
        std::string cmd = ini->GetValue(section_name, entry.pItem, "");
        if (cmd.empty()) {
            continue;
        }
        std::ranges::replace(cmd, '\x2', '\n');
        static const std::regex index_regex("(\\d+):(.+)");
        std::smatch match;
        if (std::regex_match(alias, match, index_regex)) {
            alias = match[2];
        }
        const auto alias_wstr = GuiUtils::StringToWString(alias);
        const auto command_wstr = GuiUtils::StringToWString(cmd);
        CreateAlias(alias_wstr.c_str(), command_wstr.c_str());
    }
    if (cmd_aliases.empty()) {
        CreateAlias(L"ff", L"/resign");
        CreateAlias(L"gh", L"/tp gh");
        CreateAlias(L"armor", L"/pingitem armor");
    }
    sort_cmd_aliases();
}

void ChatCommands::SaveSettings(ToolboxIni* ini)
{
    SAVE_BOOL(forward_fix_z);
    SAVE_FLOAT(cam_speed);
    SAVE_UINT(default_title_id);

    const auto section_name = "Chat Command Aliases";

    ini->Delete("Chat Command Aliases", nullptr);
    sort_cmd_aliases();

    for (const auto [index, alias] : cmd_aliases | std::views::enumerate) {
        if (!alias->alias_cstr && alias->alias_cstr[0]) {
            continue;
        }

        std::string cmd_copy = alias->command_cstr;
        std::ranges::replace(cmd_copy, '\n', '\x2');

        ini->SetValue(section_name, std::format("{}:{}", index, alias->alias_cstr).c_str(), cmd_copy.c_str());
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdPingQuest)
{
    Instance().quest_ping.Init();
}

void ChatCommands::Initialize()
{
    ToolboxModule::Initialize();

    constexpr DWORD def_scale = 0x64000000;
    // Available Transmo NPCs
    // @Enhancement: Ability to target an NPC in-game and add it to this list via a GUI
    npc_transmos = {
        {"charr", {163, def_scale, 0x0004c409, 0, 98820}},
        {"reindeer", {5, def_scale, 277573, 277576, 32780}},
        {"gwenpre", {244, def_scale, 116377, 116759, 98820}},
        {"gwenchan", {245, def_scale, 116377, 283392, 98820}},
        {"eye", {0x1f4, def_scale, 0x9d07, 0, 0}},
        {"zhu", {298, def_scale, 170283, 170481, 98820}},
        {"kuunavang", {309, def_scale, 157438, 157527, 98820}},
        {"beetle", {329, def_scale, 207331, 279211, 98820}},
        {"polar", {313, def_scale, 277551, 277556, 98820}},
        {"celepig", {331, def_scale, 279205, 0, 0}},
        {"mallyx", {315, def_scale, 243812, 0, 98820}},
        {"bonedragon", {231, def_scale, 16768, 0, 0}},
        {"destroyer", {312, def_scale, 285891, 285900, 98820}},
        {"destroyer2", {146, def_scale, 285886, 285890, 32780}},
        {"koss", {250, def_scale, 243282, 245053, 98820}},
        {"smite", {346, def_scale, 129664, 0, 98820}},
        {"dorian", {8299, def_scale, 86510, 0, 98820}},
        {"kanaxai", {317, def_scale, 184176, 185319, 98820}},
        {"skeletonic", {359, def_scale, 52356, 0, 98820}},
        {"moa", {504, def_scale, 16689, 0, 98820}}
    };

    //TODO: Move all of these callbacks into pvt namespace
    chat_commands = {
        {L"chat", CmdChatTab},
        {L"enter", CmdEnterMission},
        {L"age2", CmdAge2},
        {L"dialog", CmdDialog},
        {L"show", CmdShow},
        {L"hide", CmdHide},
        {L"toggle", CmdToggle},
        {L"tb", CmdTB},
        {L"zoom", CmdZoom},
        {L"camera", CmdCamera},
        {L"cam", CmdCamera},
        {L"chest", CmdChest},
        {L"xunlai", CmdChest},
        {L"afk", CmdAfk},
        {L"target", CmdTarget},
        {L"tgt", CmdTarget},
        {L"xunlai", CmdChest},
        {L"useskill", CmdUseSkill},
        {L"scwiki", CmdSCWiki},
        {L"load", CmdLoad},
        {L"pingbuild", CmdPingBuild},
        {L"quest", CmdPingQuest},
        {L"transmo", CmdTransmo},
        {L"transmotarget", CmdTransmoTarget},
        {L"transmoparty", CmdTransmoParty},
        {L"transmoagent", CmdTransmoAgent},
        {L"resize", CmdResize},
        {L"settitle", CmdReapplyTitle},
        {L"title", CmdReapplyTitle},
        {L"withdraw", CmdWithdraw},
        {L"deposit", CmdDeposit},
        {L"pingitem", CmdPingEquipment},
        {L"tick", CmdTick},
        {L"hero", CmdHeroBehaviour},
        {L"morale", CmdMorale},
        {L"volume", CmdVolume},
        {L"nm", CmdSetNormalMode},
        {L"normalmode", CmdSetNormalMode},
        {L"hm", CmdSetHardMode},
        {L"hardmode", CmdSetHardMode},
        {L"animation", CmdAnimation},
        {L"hom", CmdHom},
        {L"fps", CmdFps},
        {L"pref", CmdPref},
        {L"call", CmdCallTarget}
    };


    GW::UI::RegisterUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage, OnSendChat);

    // Experimental chat commands
    uintptr_t address = 0;
#if _DEBUG
    chat_commands.push_back({ L"skillimage", CmdSkillImage });
    address = GW::Scanner::Find("\x83\xc4\x04\xc7\x45\x08\x00\x00\x00\x00", "xxxxxxxxxx", -5);
    if (address) {
        SetMuted_Func = (SetMuted_pt)GW::Scanner::FunctionFromNearCall(address);
        PostMuted_Func = (PostMute_pt)GW::Scanner::FunctionFromNearCall(address + 0x10);
        is_muted = *(bool**)((uintptr_t)SetMuted_Func + 0x6);
    }
    chat_commands.push_back({ L"mute", CmdMute }); // Doesn't unmute!
    chat_commands.push_back({ L"effect", CmdPlayEffect });

#endif

    address = GW::Scanner::Find("\x3d\x7d\x00\x00\x10\x0f\x87\xe5\x02\x00\x00", "xxxxxxxxxxx", -0x11);
    if (address) {
        OnChatInteraction_Callback_Func = (GW::UI::UIInteractionCallback)address;
        FocusChatTab_Func = (FocusChatTab_pt)GW::Scanner::FunctionFromNearCall(address + 0x248);
        GW::HookBase::CreateHook((void**)&OnChatInteraction_Callback_Func, OnChatUI_Callback, (void**)&OnChatInteraction_Callback_Ret);
        GW::HookBase::EnableHooks();
    }

#ifdef _DEBUG
    ASSERT(SetMuted_Func);
    ASSERT(PostMuted_Func);
    ASSERT(is_muted);
    ASSERT(OnChatInteraction_Callback_Func);
    ASSERT(FocusChatTab_Func);
#endif

    for (auto& it : chat_commands) {
        GW::Chat::CreateCommand(it.first, it.second);
    }
}

void ChatCommands::Terminate()
{
    for (auto& it : chat_commands) {
        GW::Chat::DeleteCommand(it.first);
    }
    chat_commands.clear();
    if (FocusChatTab_Func) {
        GW::HookBase::RemoveHook(FocusChatTab_Func);
    }
    if (OnChatInteraction_Callback_Func) {
        GW::HookBase::RemoveHook(OnChatInteraction_Callback_Func);
    }

    GW::UI::RemoveUIMessageCallback(&OnSentChat_HookEntry);

    for (const auto it : title_names) {
        delete it;
    }
    title_names.clear();
    for (const auto it : cmd_aliases) {
        delete it;
    }
    cmd_aliases.clear();
}

bool ChatCommands::WndProc(const UINT Message, const WPARAM wParam, const LPARAM)
{
    if (!GW::CameraMgr::GetCameraUnlock()) {
        return false;
    }
    if (GW::Chat::GetIsTyping()) {
        return false;
    }
    if (ImGui::GetIO().WantTextInput) {
        return false;
    }

    switch (Message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            switch (wParam) {
                case VK_A:
                case VK_D:
                case VK_E:
                case VK_Q:
                case VK_R:
                case VK_S:
                case VK_W:
                case VK_X:
                case VK_Z:

                case VK_ESCAPE:
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                    return true;
            }
    }
    return false;
}

void ChatCommands::Update(const float delta)
{
    if (title_names.empty()) {
        const auto* titles = GetTitles();
        for (size_t i = 0; titles && i < titles->size(); i++) {
            switch (static_cast<GW::Constants::TitleID>(i)) {
                case GW::Constants::TitleID::Deprecated_SkillHunter:
                case GW::Constants::TitleID::Deprecated_TreasureHunter:
                case GW::Constants::TitleID::Deprecated_Wisdom:
                    continue;
            }
            auto dtn = new DecodedTitleName(static_cast<GW::Constants::TitleID>(i));
            title_names.push_back(dtn);
            dtn->name.string(); // Trigger decode for sorting.
        }
    }
    else if (!title_names_sorted) {
        const auto can_sort = std::ranges::all_of(title_names, [](const auto& title_name) {
            return !title_name->name.IsDecoding();
        });
        if (can_sort) {
            std::ranges::sort(title_names, [](DecodedTitleName* first, DecodedTitleName* second) {
                return first->name.string() < second->name.string();
            });
            title_names_sorted = true;
        }
    }

    if (delta == 0.f) {
        return;
    }

    if (GW::CameraMgr::GetCameraUnlock()
        && !GW::Chat::GetIsTyping()
        && !ImGui::GetIO().WantTextInput) {
        static bool keep_forward;

        float forward = 0;
        float vertical = 0;
        float rotate = 0;
        float side = 0;
        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow) || keep_forward) {
            forward += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
            forward -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            side += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E)) {
            side -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Z)) {
            vertical -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_X)) {
            vertical += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
            rotate += 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
            rotate -= 1.0f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_R)) {
            keep_forward = true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow) ||
            ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow) ||
            ImGui::IsKeyDown(ImGuiKey_Escape)) {
            keep_forward = false;
        }

        if (GWToolbox::Instance().right_mouse_down && rotate != 0.f) {
            side = rotate;
            rotate = 0.f;
        }

        GW::CameraMgr::ForwardMovement(forward * delta * cam_speed, !forward_fix_z);
        GW::CameraMgr::VerticalMovement(vertical * delta * cam_speed);
        GW::CameraMgr::RotateMovement(rotate * delta * ROTATION_SPEED);
        GW::CameraMgr::SideMovement(side * delta * cam_speed);
        GW::CameraMgr::UpdateCameraPos();
    }
    skill_to_use.Update();
    npc_to_find.Update();
    quest_ping.Update();
}

void ChatCommands::QuestPing::Init()
{
    const auto* quest = GW::QuestMgr::GetActiveQuest();
    if (quest) {
        quest_id = quest->quest_id;
        name.reset(quest->name);
        objectives.reset(quest->objectives);
    }
}

void ChatCommands::QuestPing::Update()
{
    if (!name.wstring().empty()) {
        wchar_t print_buf[128];
        swprintf(print_buf, _countof(print_buf), L"Current Quest: %s", name.wstring().c_str());
        GW::Chat::SendChat('#', print_buf);
    }
    if (!objectives.wstring().empty()) {
        // Find current objective using regex
        const std::wregex current_obj_regex(L"\\{s\\}([^\\{]+)");
        std::wsmatch m;
        if (std::regex_search(objectives.wstring(), m, current_obj_regex)) {
            wchar_t print_buf[128];
            swprintf(print_buf, _countof(print_buf), L" - %s", m[1].str().c_str());
            GW::Chat::SendChat('#', print_buf);
        }
        objectives.reset(nullptr);
    }
    if (!name.wstring().empty()) {
        wchar_t url_buf[64];
        swprintf(url_buf, _countof(url_buf), L"%SGame_link:Quest_%d", GuiUtils::WikiUrl(L"").c_str(), quest_id);
        GW::Chat::SendChat('#', url_buf);
        name.reset(nullptr);
    }
}

void ChatCommands::SearchAgent::Init(const wchar_t* _search, const TargetType type)
{
    started = 0;
    npc_names.clear();
    if (!_search || !_search[0]) {
        return;
    }
    search = GuiUtils::ToLower(_search);
    npc_names.clear();
    started = TIMER_INIT();
    GW::AgentArray* agents = GW::Agents::GetAgentArray();
    if (!agents) {
        return;
    }
    for (const GW::Agent* agent : *agents) {
        if (!agent) {
            continue;
        }
        if (!GW::Agents::GetIsAgentTargettable(agent)) {
            continue;
        }
        switch (type) {
            case Item:
                if (!agent->GetIsItemType()) {
                    continue;
                }
                break;
            case Gadget:
                if (!agent->GetIsGadgetType()) {
                    continue;
                }
                break;
            case Player:
                if (!agent->GetIsLivingType() || !agent->GetAsAgentLiving()->IsPlayer()) {
                    continue;
                }
                break;
            case Npc: {
                const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
                if (!agent_living || !agent_living->IsNPC() || !agent_living->GetIsAlive()) {
                    continue;
                }
            }
            break;
            case Living: {
                const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
                if (!agent_living || !agent_living->GetIsAlive()) {
                    continue;
                }
            }
            break;
            default:
                continue;
        }
        const wchar_t* enc_name = GW::Agents::GetAgentEncName(agent);
        if (!enc_name || !enc_name[0]) {
            continue;
        }
        npc_names.push_back({agent->agent_id, new GuiUtils::EncString(enc_name)});
    }
}

void ChatCommands::SearchAgent::Update()
{
    if (!started) {
        return;
    }
    if (TIMER_DIFF(started) > 3000) {
        Log::Error("Timeout getting NPC names");
        Init(nullptr);
        return;
    }
    for (const auto& str : npc_names | std::views::values) {
        if (str->wstring().empty()) {
            return; // Not all decoded yet
        }
    }
    // Do search
    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    const GW::Agent* me = GW::Agents::GetControlledCharacter();
    if (!me) {
        return;
    }
    for (const auto& enc_name : npc_names) {
        const size_t found = GuiUtils::ToLower(enc_name.second->wstring()).find(search.c_str());
        if (found == std::wstring::npos) {
            continue;
        }
        const GW::Agent* agent = GW::Agents::GetAgentByID(enc_name.first);
        if (!agent) {
            continue;
        }
        const auto dist = GetDistance(me->pos, agent->pos);
        if (dist < distance) {
            closest = agent->agent_id;
            distance = dist;
        }
    }
    if (closest) {
        GW::Agents::ChangeTarget(closest);
    }
    Init(nullptr);
}

void ChatCommands::SkillToUse::Update()
{
    if (!slot) {
        return;
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable || GW::Map::GetIsObserving()) {
        slot = 0;
        return;
    }
    if ((clock() - skill_timer) / 1000.0f < skill_usage_delay) {
        return;
    }
    const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        slot = 0;
        return;
    }
    const auto lslot = slot - 1;
    const GW::SkillbarSkill& skill = skillbar->skills[lslot];
    if (skill.skill_id == GW::Constants::SkillID::No_Skill
        || skill.skill_id == GW::Constants::SkillID::Mystic_Healing
        || skill.skill_id == GW::Constants::SkillID::Cautery_Signet) {
        slot = 0;
        return;
    }
    const GW::Skill& skilldata = *GW::SkillbarMgr::GetSkillConstantData(skill.skill_id);
    if ((skilldata.adrenaline == 0 && skill.GetRecharge() == 0) || (skilldata.adrenaline > 0 && skill.adrenaline_a == skilldata.adrenaline)) {
        GW::SkillbarMgr::UseSkill(lslot, GW::Agents::GetTargetId());
        skill_usage_delay = std::max(skilldata.activation + skilldata.aftercast, 0.25f); // a small flat delay of .3s for ping and to avoid spamming in case of bad target
        skill_timer = clock();
    }
}

bool ChatCommands::ReadTemplateFile(const std::wstring& path, char* buff, const size_t buffSize)
{
    const auto fileHandle = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        // We don't print that error, because we can /load [template]
        // Log::Error("Failed openning file '%S'", path.c_str());
        return false;
    }

    const auto fileSize = GetFileSize(fileHandle, nullptr);
    if (fileSize >= buffSize) {
        Log::Error("Buffer size too small, file size is %d", fileSize);
        CloseHandle(fileHandle);
        return false;
    }

    DWORD bytesReaded; // @Remark, necessary !!!!! failed on some Windows 7.
    if (ReadFile(fileHandle, buff, fileSize, &bytesReaded, nullptr) == FALSE) {
        Log::Error("ReadFile failed ! (%u)", GetLastError());
        CloseHandle(fileHandle);
        return false;
    }

    buff[fileSize] = 0;
    CloseHandle(fileHandle);
    return true;
}

void CHAT_CMD_FUNC(ChatCommands::CmdEnterMission)
{
    const auto error_use_from_outpost = "Use '/enter' to start a mission or elite area from an outpost";
    const auto error_fow_uw_syntax = "Use '/enter fow' or '/enter uw' to trigger entry";
    const auto error_no_scrolls = "Unable to enter elite area; no scroll found";
    const auto error_not_leading = "Unable to enter mission; you're not party leader";

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return Log::Error(error_use_from_outpost);
    }

    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::Temple_of_the_Ages:
        case GW::Constants::MapID::Embark_Beach: {
            if (argc < 2) {
                return Log::Error(error_fow_uw_syntax);
            }
            uint32_t item_id;
            const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
            if (arg1 == L"fow") {
                item_id = 22280;
            }
            else if (arg1 == L"uw") {
                item_id = 3746;
            }
            else {
                return Log::Error(error_fow_uw_syntax);
            }
            if (!GW::Items::UseItemByModelId(item_id, 1, 4) && !GW::Items::UseItemByModelId(item_id, 8, 16)) {
                return Log::Error(error_no_scrolls);
            }
        }
        break;
        default:
            const auto map_info = GW::Map::GetCurrentMapInfo();
            if (!map_info || !map_info->GetHasEnterButton()) {
                return Log::Error(error_use_from_outpost);
            }
            if (!GW::PartyMgr::GetIsLeader()) {
                return Log::Error(error_not_leading);
            }
            const auto p = GW::GetGameContext()->party;
            if (p && (p->flag & 0x8) != 0) {
                GW::Map::CancelEnterChallenge();
            }
            else {
                GW::Map::EnterChallenge();
            }
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdMorale)
{
    if (GW::GetGameContext()->world->morale == 100) {
        GW::Chat::SendChat('#', L"I have no Morale Boost or Death Penalty!");
    }
    else {
        auto packet = GW::UI::UIPacket::kSendCallTarget { GW::CallTargetType::Morale, GW::Agents::GetControlledCharacterId() };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &packet);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdAge2)
{
    TimerWidget::Instance().PrintTimer();
}

void CHAT_CMD_FUNC(ChatCommands::CmdDialog)
{
    if (!IsMapReady()) {
        return;
    }
    const auto syntax = "Syntax: '/dialog [dialog_id]' (e.g. '/dialog 0x184')\nSyntax: '/dialog take' (to take first available quest)";
    if (argc <= 1) {
        Log::Error(syntax);
        return;
    }
    uint32_t id = 0;
    const auto dialog_str = std::wstring{argv[1]};
    if (dialog_str == L"take" || dialog_str == L"0") {
        id = 0;
    }
    else if (!(GuiUtils::ParseUInt(argv[1], &id) && id)) {
        Log::Error(syntax);
        return;
    }
    if (!DialogModule::GetDialogAgent()) {
        const auto* target = GW::Agents::GetTargetAsAgentLiving();
        const auto* me = GW::Agents::GetControlledCharacter();
        if (target && target->allegiance == GW::Constants::Allegiance::Npc_Minipet
            && GetDistance(me->pos, target->pos) < GW::Constants::Range::Area) {
            GW::Agents::InteractAgent(target);
        }
    }
    DialogModule::SendDialog(id);
}

void CHAT_CMD_FUNC(ChatCommands::CmdChest)
{
    if (!IsMapReady()) {
        return;
    }
    GW::Items::OpenXunlaiWindow();
}

void CHAT_CMD_FUNC(ChatCommands::CmdTB)
{
    if (!ImGui::GetCurrentContext()) {
        return; // Don't process window manips until ImGui is ready
    }
    if (argc < 2) {
        // e.g. /tbs
        MainWindow::Instance().visible ^= 1;
        return;
    }

    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (argc < 3) {
        if (arg1 == L"hide") {
            // e.g. /tb hide
            MainWindow::Instance().visible = false;
        }
        else if (arg1 == L"show") {
            // e.g. /tb show
            MainWindow::Instance().visible = true;
        }
        else if (arg1 == L"toggle") {
            // e.g. /tb toggle
            MainWindow::Instance().visible = !MainWindow::Instance().visible;
        }
        else if (arg1 == L"save") {
            // e.g. /tb save
            GWToolbox::SetSettingsFolder({});
            const auto file_location = GWToolbox::SaveSettings();
            const auto dir = file_location.parent_path();
            const auto dirstr = dir.wstring();
            const auto printable = std::regex_replace(dirstr, std::wregex(L"\\\\"), L"/");
            Log::InfoW(L"Settings saved to %s", printable.c_str());
        }
        else if (arg1 == L"load") {
            // e.g. /tb load
            GWToolbox::SetSettingsFolder({});
            const auto file_location = GWToolbox::LoadSettings();
            const auto dir = file_location.parent_path();
            const auto dirstr = dir.wstring();
            const auto printable = std::regex_replace(dirstr, std::wregex(L"\\\\"), L"/");
            Log::InfoW(L"Settings loaded from %s", printable.c_str());
        }
        else if (arg1 == L"reset") {
            // e.g. /tb reset
            ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            MainWindow::Instance().visible = false;
            SettingsWindow::Instance().visible = true;
        }
        else if (arg1 == L"mini" || arg1 == L"minimize" || arg1 == L"collapse") {
            // e.g. /tb mini
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
        }
        else if (arg1 == L"maxi" || arg1 == L"maximize") {
            // e.g. /tb maxi
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
        }
        else if (arg1 == L"close" || arg1 == L"quit" || arg1 == L"exit") {
            // e.g. /tb close
            GWToolbox::SignalTerminate();
        }
        else {
            // e.g. /tb travel
            const std::vector<ToolboxUIElement*> windows = MatchingWindows(status, message, argc, argv);
            for (ToolboxUIElement* window : windows) {
                window->visible ^= 1;
            }
        }
        return;
    }
    const std::vector<ToolboxUIElement*> windows = MatchingWindows(status, message, argc, argv);
    const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
    if (arg2 == L"hide") {
        // e.g. /tb travel hide
        for (const auto& window : windows) {
            window->visible = false;
        }
    }
    else if (arg2 == L"show") {
        // e.g. /tb travel show
        for (const auto& window : windows) {
            window->visible = true;
        }
    }
    else if (arg2 == L"toggle") {
        // e.g. /tb travel show
        for (const auto& window : windows) {
            window->visible = !window->visible;
        }
    }
    else if (arg2 == L"mini" || arg2 == L"minimize" || arg2 == L"collapse") {
        // e.g. /tb travel mini
        for (const auto& window : windows) {
            ImGui::SetWindowCollapsed(window->Name(), true);
        }
    }
    else if (arg2 == L"maxi" || arg2 == L"maximize") {
        // e.g. /tb travel maxi
        for (const auto& window : windows) {
            ImGui::SetWindowCollapsed(window->Name(), false);
        }
    }
    else if (arg1 == L"save") {
        // e.g. /tb save pure
        const auto sanitised_foldername = GuiUtils::SanitiseFilename(arg2);
        GWToolbox::SetSettingsFolder(sanitised_foldername);
        const auto file_location = GWToolbox::SaveSettings();
        const auto dir = file_location.parent_path();
        const auto dirstr = dir.wstring();
        const auto printable = std::regex_replace(dirstr, std::wregex(L"\\\\"), L"/");
        Log::InfoW(L"Settings saved to %s", printable.c_str());
    }
    else if (arg1 == L"load") {
        // e.g. /tb load tas
        const auto sanitised_foldername = GuiUtils::SanitiseFilename(arg2);
        const auto old_settings_folder = Resources::GetSettingsFolderName();
        GWToolbox::SetSettingsFolder(sanitised_foldername);
        if (!std::filesystem::exists(Resources::GetSettingFile(GWTOOLBOX_INI_FILENAME))) {
            Log::ErrorW(L"Settings folder '%s' does not exist", arg2.c_str());
            GWToolbox::SetSettingsFolder(old_settings_folder);
            return;
        }
        const auto file_location = GWToolbox::LoadSettings();
        const auto dir = file_location.parent_path();
        const auto dirstr = dir.wstring();
        const auto printable = std::regex_replace(dirstr, std::wregex(L"\\\\"), L"/");
        Log::InfoW(L"Settings loaded from %s", printable.c_str());
    }
    else {
        // Invalid argument
        const auto text = std::format(L"Syntax: {} {} [hide|show|toggle|mini|maxi|load|save]", argv[0], argv[1]);
        Log::ErrorW(text.c_str());
    }
}

GW::UI::WindowID CHAT_CMD_FUNC(ChatCommands::MatchingGWWindow)
{
    const std::map<GW::UI::WindowID, const wchar_t*> gw_windows = {
        {GW::UI::WindowID_Compass, L"compass"},
        {GW::UI::WindowID_HealthBar, L"healthbar"},
        {GW::UI::WindowID_EnergyBar, L"energybar"},
        {GW::UI::WindowID_ExperienceBar, L"experiencebar"},
        {GW::UI::WindowID_Chat, L"chat"}
    };
    if (argc < 2) {
        return GW::UI::WindowID_Count;
    }
    const std::wstring arg = GuiUtils::ToLower(argv[1]);
    if (!arg.empty() && arg != L"all") {
        for (const auto& [window_id, window_name] : gw_windows) {
            if (wcscmp(window_name, arg.c_str()) == 0) {
                return window_id;
            }
        }
    }
    return GW::UI::WindowID_Count;
}

std::vector<ToolboxUIElement*> CHAT_CMD_FUNC(ChatCommands::MatchingWindows)
{
    std::vector<ToolboxUIElement*> ret;
    if (argc <= 1) {
        ret.push_back(&MainWindow::Instance());
    }
    else {
        const std::wstring arg = GuiUtils::ToLower(argv[1]);
        if (arg == L"all") {
            for (ToolboxUIElement* window : GWToolbox::GetUIElements()) {
                ret.push_back(window);
            }
        }
        else if (!arg.empty()) {
            const std::string name = GuiUtils::WStringToString(arg);
            for (ToolboxUIElement* window : GWToolbox::GetUIElements()) {
                if (GuiUtils::ToLower(window->Name()).find(name) == 0) {
                    ret.push_back(window);
                }
            }
        }
    }
    return ret;
}

void CHAT_CMD_FUNC(ChatCommands::CmdShow)
{
    std::wstring cmd = L"toggle ";
    cmd.append(GetRemainingArgsWstr(message, 1));
    cmd.append(L" on");
    GW::Chat::SendChat('/', cmd.c_str());
}

void CHAT_CMD_FUNC(ChatCommands::CmdHide)
{
    std::wstring cmd = L"toggle ";
    cmd.append(GetRemainingArgsWstr(message, 1));
    cmd.append(L" off");
    GW::Chat::SendChat('/', cmd.c_str());
}

void CHAT_CMD_FUNC(ChatCommands::CmdToggle)
{
    if (argc < 2) {
        Log::ErrorW(L"Invalid syntax: %s", message);
        return;
    }
    const std::wstring last_arg = GuiUtils::ToLower(argv[argc - 1]);
    bool ignore_last_arg = false;
    enum ActionType : uint8_t {
        Toggle,
        On,
        Off
    } action = Toggle;
    if (last_arg == L"on" || last_arg == L"1" || last_arg == L"show") {
        action = On;
        ignore_last_arg = true;
    }
    else if (last_arg == L"off" || last_arg == L"0" || last_arg == L"hide") {
        action = Off;
        ignore_last_arg = true;
    }
    const std::wstring second_arg = GuiUtils::ToLower(argv[1]);

    auto equipment_slot = GW::EquipmentType::Unknown;

    if (second_arg == L"cape") {
        equipment_slot = GW::EquipmentType::Cape;
    }
    else if (second_arg == L"head" || second_arg == L"helm") {
        equipment_slot = GW::EquipmentType::Helm;
    }
    else if (second_arg == L"costume_head") {
        equipment_slot = GW::EquipmentType::CostumeHeadpiece;
    }
    else if (second_arg == L"costume") {
        equipment_slot = GW::EquipmentType::CostumeBody;
    }
    if (equipment_slot != GW::EquipmentType::Unknown) {
        GW::EquipmentStatus state = GW::Items::GetEquipmentVisibility(equipment_slot);
        // Toggling visibility of equipment
        switch (action) {
            case On:
                state = GW::EquipmentStatus::AlwaysShow;
                break;
            case Off:
                state = GW::EquipmentStatus::AlwaysHide;
                break;
            default:
                state = state == GW::EquipmentStatus::AlwaysShow ? GW::EquipmentStatus::AlwaysHide : GW::EquipmentStatus::AlwaysShow;
                break;
        }
        ASSERT(GW::Items::SetEquipmentVisibility(equipment_slot, state));
        return;
    }
    const std::vector<ToolboxUIElement*> windows = MatchingWindows(status, message, ignore_last_arg ? argc - 1 : argc, argv);
    /*if (windows.empty()) {
        Log::Error("Cannot find window or command '%ls'", argc > 1 ? argv[1] : L"");
        return;
    }*/
    for (ToolboxUIElement* window : windows) {
        switch (action) {
            case On:
                window->visible = true;
                break;
            case Off:
                window->visible = false;
                break;
            default:
                window->visible = !window->visible;
                break;
        }
    }
    const GW::UI::WindowID gw_window = MatchingGWWindow(status, message, ignore_last_arg ? argc - 1 : argc, argv);
    if (gw_window < GW::UI::WindowID_Count) {
        bool set = true;
        switch (action) {
            case Off:
                set = false;
                break;
            case Toggle:
                set = !GetWindowPosition(gw_window)->visible();
                break;
        }
        SetWindowVisible(gw_window, set);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdZoom)
{
    if (argc <= 1) {
        GW::CameraMgr::SetMaxDist();
    }
    else {
        int distance;
        if (GuiUtils::ParseInt(argv[1], &distance)) {
            if (distance > 0) {
                GW::CameraMgr::SetMaxDist(static_cast<float>(distance));
            }
            else {
                Log::Error("Invalid argument '%ls', please use a positive integer value", argv[1]);
            }
        }
        else {
            Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
        }
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdCamera)
{
    if (argc == 1) {
        GW::CameraMgr::UnlockCam(false);
    }
    else {
        const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
        if (arg1 == L"lock") {
            GW::CameraMgr::UnlockCam(false);
        }
        else if (arg1 == L"unlock") {
            GW::CameraMgr::UnlockCam(true);
            Log::Info("Use Q/E, A/D, W/S, X/Z, R and arrows for camera movement");
        }
        else if (arg1 == L"fog") {
            if (argc == 3) {
                const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
                if (arg2 == L"on") {
                    GW::CameraMgr::SetFog(true);
                }
                else if (arg2 == L"off") {
                    GW::CameraMgr::SetFog(false);
                }
            }
        }
        else if (arg1 == L"speed") {
            if (argc < 3) {
                Instance().cam_speed = Instance().DEFAULT_CAM_SPEED;
            }
            else {
                const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
                if (arg2 == L"default") {
                    Instance().cam_speed = Instance().DEFAULT_CAM_SPEED;
                }
                else {
                    float speed = 0.0f;
                    if (!GuiUtils::ParseFloat(arg2.c_str(), &speed)) {
                        Log::Error(
                            "Invalid argument '%ls', please use a float value",
                            argv[2]);
                        return;
                    }
                    Instance().cam_speed = speed;
                    Log::Info("Camera speed is now %f", speed);
                }
            }
        }
        else {
            Log::Error("Invalid argument.");
        }
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdAfk)
{
    GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Away);
    if (argc > 1) {
        const wchar_t* afk_msg = next_word(message);
        ChatSettings::SetAfkMessage(afk_msg);
    }
    else {
        ChatSettings::SetAfkMessage(L"");
    }
}

const wchar_t* ChatCommands::GetRemainingArgsWstr(const wchar_t* message, const int argc_start)
{
    const wchar_t* out = message;
    for (auto i = 0; i < argc_start && out; i++) {
        out = wcschr(out, ' ');
        if (out) {
            out++;
        }
    }

    if(!out) {
        return L"";
    }
    return out;
};

void CHAT_CMD_FUNC(ChatCommands::CmdTarget)
{
    if (argc < 2) {
        return Log::ErrorW(L"Missing argument for /%s", argv[0]);
    }

    const auto zero_w = L"0";

    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"ee") // /target ee
    {
        return TargetEE();
    }
    if (arg1 == L"vipers" || arg1 == L"hos") // /target vipers or /target hos
    {
        return TargetVipers();
    }
    if (IsNearestStr(arg1.c_str())) {
        if (argc < 3) // /target nearest
        {
            return TargetNearest(zero_w, Living);
        }
        const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
        if (arg2 == L"item") {
            // /target nearest item [model_id|name]
            return TargetNearest(argc > 3 ? GetRemainingArgsWstr(message, 3) : zero_w, Item);
        }
        if (arg2 == L"npc") {
            // /target nearest npc [model_id|name]
            return TargetNearest(argc > 3 ? GetRemainingArgsWstr(message, 3) : zero_w, Npc);
        }
        if (arg2 == L"gadget") {
            // /target nearest gadget [model_id|name]
            return TargetNearest(argc > 3 ? GetRemainingArgsWstr(message, 3) : zero_w, Gadget);
        }
        if (arg2 == L"player") {
            // /target nearest player [model_id|name]
            return TargetNearest(argc > 3 ? GetRemainingArgsWstr(message, 3) : zero_w, Player);
        }
        if (arg2 == L"ally") {
            // /target nearest ally [model_id|name]
            return TargetNearest(argc > 3 ? GetRemainingArgsWstr(message, 3) : zero_w, Ally);
        }
        if (arg2 == L"enemy") {
            // /target nearest ally [model_id|name]
            return TargetNearest(argc > 3 ? GetRemainingArgsWstr(message, 3) : zero_w, Enemy);
        }
        // /target nearest 1234
        return TargetNearest(arg2.c_str(), Living);
    }
    if (arg1 == L"getid") {
        // /target getid
        const auto target = GW::Agents::GetTargetAsAgentLiving();
        if (target == nullptr) {
            Log::Error("No target selected!");
        }
        else {
            Log::Info("Target model id (PlayerNumber) is %d", target->player_number);
        }
        return;
    }
    if (arg1 == L"getpos") {
        // /target getpos
        const auto target = GW::Agents::GetTargetAsAgentLiving();
        if (target == nullptr) {
            Log::Error("No target selected!");
        }
        else {
            Log::Info("Target coordinates are (%f, %f)", target->pos.x, target->pos.y);
        }
        return;
    }
    if (arg1 == L"item") {
        // /target item [model_id|name]
        return TargetNearest(argc > 2 ? GetRemainingArgsWstr(message, 2) : zero_w, Item);
    }
    if (arg1 == L"npc") {
        // /target npc [model_id|name]
        return TargetNearest(argc > 2 ? GetRemainingArgsWstr(message, 2) : zero_w, Npc);
    }
    if (arg1 == L"gadget") {
        // /target gadget [model_id|name]
        return TargetNearest(argc > 2 ? GetRemainingArgsWstr(message, 2) : zero_w, Gadget);
    }
    if (arg1 == L"player") {
        // /target player [model_id|name]
        return TargetNearest(argc > 2 ? GetRemainingArgsWstr(message, 2) : zero_w, Player);
    }
    if (arg1 == L"ally") {
        // /target ally [model_id|name]
        return TargetNearest(argc > 2 ? GetRemainingArgsWstr(message, 2) : zero_w, Ally);
    }
    if (arg1 == L"enemy") {
        // /target nearest ally [model_id|name]
        return TargetNearest(argc > 2 ? GetRemainingArgsWstr(message, 2) : zero_w, Enemy);
    }
    if (arg1 == L"priority") {
        // /target priority [party_member_target]
        const GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        if (!party || !party->players.valid()) {
            return;
        }

        uint32_t calledTargetId = 0;

        if (argc == 2) {
            const GW::AgentLiving* meLiving = GW::Agents::GetControlledCharacter();
            if (!meLiving) {
                return;
            }
            for (size_t i = 0; i < party->players.size(); i++) {
                if (party->players[i].login_number != meLiving->login_number) {
                    continue;
                }
                calledTargetId = party->players[i].calledTargetId;
                break;
            }
        }
        else {
            uint32_t partyMemberNumber = 0;
            uint32_t partySize = party->players.size();
            if (party->heroes.valid()) {
                partySize += party->heroes.size();
            }

            if (!GuiUtils::ParseUInt(argv[2], &partyMemberNumber) || partyMemberNumber <= 0 ||
                partyMemberNumber > partySize) {
                Log::Error("Invalid argument '%ls', please use an integer value of 1 to %u", argv[2], partySize);
                return;
            }

            uint32_t count = 0;
            for (const GW::PlayerPartyMember& player : party->players) {
                count++;
                if (partyMemberNumber == count) {
                    calledTargetId = player.calledTargetId;
                    break;
                }
                for (const GW::HeroPartyMember& hero : party->heroes) {
                    if (hero.owner_player_id == player.login_number) {
                        count++;
                    }
                }
                if (count > partyMemberNumber) {
                    return;
                }
            }
        }
        if (calledTargetId == 0) {
            return;
        }
        const GW::Agent* agent = GW::Agents::GetAgentByID(calledTargetId);
        if (!agent) {
            return;
        }
        GW::Agents::ChangeTarget(agent);
    }
    return TargetNearest(GetRemainingArgsWstr(message, 1), Living);
}

void CHAT_CMD_FUNC(ChatCommands::CmdUseSkill)
{
    if (!IsMapReady()) {
        return;
    }
    SkillToUse& skill_to_use = Instance().skill_to_use;
    skill_to_use.slot = 0;
    if (argc < 2) {
        return;
    }
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"stop" || arg1 == L"off" || arg1 == L"0") {
        return; // do nothing, already cleared skills_to_use
    }
    uint32_t num = 0;
    if (!GuiUtils::ParseUInt(argv[1], &num) || num < 1 || num > 8) {
        Log::Error("Invalid argument '%ls', please use an integer value of 1 to 8", argv[1]);
        return;
    }
    skill_to_use.slot = num;
    skill_to_use.skill_usage_delay = .0f;
}

void CHAT_CMD_FUNC(ChatCommands::CmdSCWiki)
{
    const auto res = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (!SUCCEEDED(res)) {
        return;
    }
    if (argc == 1) {
        ShellExecuteW(nullptr, L"open", L"https://wiki.fbgmguild.com/wiki/Main_Page", nullptr, nullptr, SW_SHOWNORMAL);
    }
    else {
        // the buffer is large enough, because you can type only 120 characters at once in the chat.
        wchar_t link[256] = L"https://wiki.fbgmguild.com/index.php?search=";
        int i;
        for (i = 1; i < argc - 1; i++) {
            wcscat_s(link, argv[i]);
            wcscat_s(link, L"+");
        }
        wcscat_s(link, argv[i]);
        ShellExecuteW(nullptr, L"open", link, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdLoad)
{
    // We will & should move that to GWCA.
    static int (__cdecl *GetPersonalDir)(size_t size, wchar_t* dir) = nullptr;
    *(uintptr_t*)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
    if (argc == 1) {
        return;
    }

    constexpr size_t dir_size = 512;
    constexpr size_t temp_size = 64;

    const auto arg1 = argv[1];
    wchar_t dir[dir_size];
    GetPersonalDir(dir_size, dir); // @Fix, GetPersonalDir failed on Windows7, return path without slashes
    wcscat_s(dir, L"/GUILD WARS/Templates/Skills/");
    wcscat_s(dir, arg1);
    wcscat_s(dir, L".txt");

    char temp[temp_size];
    if (!ReadTemplateFile(dir, temp, temp_size)) {
        // If it failed, we will interpret the input as the code models.
        const size_t len = wcslen(arg1);
        if (len >= temp_size) {
            return;
        }
        for (size_t i = 0; i < len; i++) {
            temp[i] = static_cast<char>(arg1[i]);
        }
        temp[len] = 0;
    }
    if (argc == 2) {
        GW::SkillbarMgr::LoadSkillTemplate(temp);
    }
    else if (argc == 3) {
        uint32_t ihero_number;
        if (GuiUtils::ParseUInt(argv[2], &ihero_number)) {
            // @Robustness:
            // Check that the number is actually valid or make sure LoadSkillTemplate is safe
            if (0 < ihero_number && ihero_number <= 8) {
                GW::SkillbarMgr::LoadSkillTemplate(temp, ihero_number);
            }
        }
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdPingBuild)
{
    // We will & should move that to GWCA.
    static int (__cdecl *GetPersonalDir)(size_t size, wchar_t* dir) = nullptr;
    *(uintptr_t*)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
    if (argc < 2) {
        return;
    }

    constexpr size_t dir_size = 512;
    constexpr size_t temp_size = 64;
    constexpr size_t template_size = 256;

    for (auto arg_idx = 1; arg_idx < argc; arg_idx++) {
        const LPWSTR arg = argv[arg_idx];
        wchar_t dir[dir_size];
        GetPersonalDir(dir_size, dir); // @Fix, GetPersonalDir failed on Windows7, return path without slashes
        wcscat_s(dir, L"/GUILD WARS/Templates/Skills/");
        wcscat_s(dir, arg);
        wcscat_s(dir, L".txt");

        char temp[temp_size];
        if (!ReadTemplateFile(dir, temp, temp_size)) {
            // If it failed, we will interpret the input as the code models.
            const size_t len = wcslen(arg);
            if (len >= temp_size) {
                continue;
            }
            for (size_t i = 0; i < len; i++) {
                temp[i] = static_cast<char>(arg[i]);
            }
            temp[len] = 0;
        }

        // If template file does not exist, skip
        GW::SkillbarMgr::SkillTemplate skill_template{};
        if (!DecodeSkillTemplate(&skill_template, temp)) {
            continue;
        }

        char template_code[template_size];
        snprintf(template_code, template_size, "[%S;%s]", arg, temp);
        GW::Chat::SendChat('#', template_code);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdPingEquipment)
{
    if (!IsMapReady()) {
        return;
    }
    if (argc < 2) {
        Log::Error("Missing argument for /pingitem");
        return;
    }
    const auto equipped_items_bag = GW::Items::GetBag(GW::Constants::Bag::Equipped_Items);
    if (!equipped_items_bag)
        return;
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"weapon") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 1), 3);
    }
    else if (arg1 == L"offhand" || arg1 == L"shield") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 2), 3);
    }
    else if (arg1 == L"chest") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 3), 2);
    }
    else if (arg1 == L"legs") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 4), 2);
    }
    else if (arg1 == L"head") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 5), 2);
    }
    else if (arg1 == L"boots" || arg1 == L"feet") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 6), 2);
    }
    else if (arg1 == L"gloves" || arg1 == L"hands") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 7), 2);
    }
    else if (arg1 == L"weapons") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 1), 3);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 2), 3);
    }
    else if (arg1 == L"armor") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 5), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 3), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 7), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 4), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 6), 2);
    }
    else if (arg1 == L"costume") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 8), 1);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 9), 1);
    }
    else {
        Log::Error("Unrecognised /pingitem %ls", argv[1]);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdTransmoParty)
{
    GW::PartyInfo* pInfo = GW::PartyMgr::GetPartyInfo();
    if (!pInfo) {
        return;
    }
    PendingTransmo transmo;

    if (argc > 1) {
        int iscale;
        if (wcsncmp(argv[1], L"reset", 5) == 0) {
            transmo.npc_id = std::numeric_limits<int>::max();
        }
        else if (GuiUtils::ParseInt(argv[1], &iscale)) {
            if (!ParseScale(iscale, transmo)) {
                return;
            }
        }
        else if (!GetNPCInfoByName(argv[1], transmo)) {
            Log::Error("Unknown transmo '%ls'", argv[1]);
            return;
        }
        if (argc > 2 && GuiUtils::ParseInt(argv[2], &iscale)) {
            if (!ParseScale(iscale, transmo)) {
                return;
            }
        }
    }
    else {
        if (!GetTargetTransmoInfo(transmo)) {
            return;
        }
    }
    for (const GW::HeroPartyMember& p : pInfo->heroes) {
        TransmoAgent(p.agent_id, transmo);
    }
    for (const GW::HenchmanPartyMember& p : pInfo->henchmen) {
        TransmoAgent(p.agent_id, transmo);
    }
    for (const GW::PlayerPartyMember& p : pInfo->players) {
        const auto player = GW::PlayerMgr::GetPlayerByID(p.login_number);
        if (!player) {
            continue;
        }
        TransmoAgent(player->agent_id, transmo);
    }
}

bool ChatCommands::ParseScale(const int scale, PendingTransmo& transmo)
{
    if (scale < 6 || scale > 255) {
        Log::Error("scale must be between [6, 255]");
        return false;
    }
    transmo.scale = static_cast<DWORD>(scale) << 24;
    if (!transmo.npc_id) {
        transmo.npc_id = std::numeric_limits<int>::max() - 1;
    }
    return true;
}

void CHAT_CMD_FUNC(ChatCommands::CmdTransmoTarget)
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (argc < 2) {
        Log::Error("Missing /transmotarget argument");
        return;
    }
    if (!target) {
        Log::Error("Invalid /transmotarget target");
        return;
    }
    PendingTransmo transmo;
    int iscale;
    if (wcsncmp(argv[1], L"reset", 5) == 0) {
        transmo.npc_id = std::numeric_limits<int>::max();
    }
    else if (GuiUtils::ParseInt(argv[1], &iscale)) {
        if (!ParseScale(iscale, transmo)) {
            return;
        }
    }
    else if (!GetNPCInfoByName(argv[1], transmo)) {
        Log::Error("Unknown transmo '%ls'", argv[1]);
        return;
    }
    if (argc > 2 && GuiUtils::ParseInt(argv[2], &iscale)) {
        if (!ParseScale(iscale, transmo)) {
            return;
        }
    }
    TransmoAgent(target->agent_id, transmo);
}


void GetAchievements(const std::wstring& player_name)
{
    if (!(!player_name.empty() && player_name.size() < 20)) {
        return Log::Error("Invalid player name for hall of monuments command");
    }
    memset(&hom_achievements, 0, sizeof(hom_achievements));
    HallOfMonumentsModule::AsyncGetAccountAchievements(
        player_name, &hom_achievements, OnAchievementsLoaded);
}

void CHAT_CMD_FUNC(ChatCommands::CmdHom)
{
    std::wstring player_name;
    if (argc > 1) {
        player_name = GetRemainingArgsWstr(message, 1);

        if (player_name == L"me") {
            player_name = GW::PlayerMgr::GetPlayerName(0);
            return GetAchievements(player_name);
        }
        if (player_name.find(L" ") != std::wstring::npos && player_name.size() < 20) {
            return GetAchievements(player_name);
        }
    }

    const auto target = GW::Agents::GetTargetAsAgentLiving();
    const auto player = target && target->IsPlayer() ? GW::PlayerMgr::GetPlayerByID(target->player_number) : nullptr;
    if (player) {
        player_name = player->name;
        return GetAchievements(player_name);
    }
}

// /withdraw quantity model_id1 [model_id2 ...]
void CHAT_CMD_FUNC(ChatCommands::CmdWithdraw)
{
    const auto syntax_error = [] {
        Log::Error("Incorrect syntax:");
        Log::Error(withdraw_syntax);
    };
    if (argc < 2) {
        return syntax_error();
    }
    uint32_t wanted_quantity = 0;
    if (argc < 3) {
        std::wstring amount = argv[1];
        const auto platinum = amount.ends_with(L'k') || amount.ends_with(L'p');
        if (amount == L"max" || amount == L"all") {
            wanted_quantity = 0; // gwca withdraws maximum then
        }
        else {
            if (platinum) {
                amount.pop_back();
            }
            if (!(GuiUtils::ParseUInt(amount.c_str(), &wanted_quantity) && wanted_quantity <= 0xFFFF)) {
                return syntax_error();
            }
            if (platinum) {
                wanted_quantity *= 1000;
            }
        }
        GW::Items::WithdrawGold(wanted_quantity);
        return;
    }
    std::vector<uint32_t> model_ids;

    if (!(GuiUtils::ParseUInt(argv[1], &wanted_quantity) && wanted_quantity <= 0xFFFF)) {
        return syntax_error();
    }
    for (auto i = 2; i < argc; i++) {
        uint32_t model_id;
        if (!GuiUtils::ParseUInt(argv[i], &model_id)) {
            return syntax_error();
        }
        model_ids.push_back(model_id);
    }

    // NB: uint16_t used already throughout Inv manager, and can't possibly have move than 0xffff of any item anyway.
    const auto to_move = static_cast<uint16_t>(wanted_quantity);
    InventoryManager::RefillUpToQuantity(to_move, model_ids);
}

void CHAT_CMD_FUNC(ChatCommands::CmdDeposit)
{
    const auto syntax_error = [] {
        Log::Error("Incorrect syntax:");
        Log::Error(deposit_syntax);
    };
    if (argc < 2) {
        return syntax_error();
    }

    uint32_t wanted_quantity = 0;
    if (argc < 3) {
        std::wstring amount = argv[1];
        const auto platinum = amount.ends_with(L'k') || amount.ends_with(L'p');
        if (amount == L"max" || amount == L"all") {
            wanted_quantity = 0; // gwca deposits maximum then
        }
        else {
            if (platinum) {
                amount.pop_back();
            }
            if (!(GuiUtils::ParseUInt(amount.c_str(), &wanted_quantity) && wanted_quantity <= 0xFFFF)) {
                return syntax_error();
            }
            if (platinum) {
                wanted_quantity *= 1000;
            }
        }
        GW::Items::DepositGold(wanted_quantity);
        return;
    }

    std::vector<uint32_t> model_ids;

    if (!(GuiUtils::ParseUInt(argv[1], &wanted_quantity) && wanted_quantity <= 0xFFFF)) {
        return syntax_error();
    }
    for (auto i = 2; i < argc; i++) {
        uint32_t model_id;
        if (!GuiUtils::ParseUInt(argv[i], &model_id)) {
            return syntax_error();
        }
        model_ids.push_back(model_id);
    }

    // NB: uint16_t used already throughout Inv manager, and can't possibly have move than 0xffff of any item anyway.
    const auto to_move = static_cast<uint16_t>(wanted_quantity);
    InventoryManager::StoreItems(to_move, model_ids);
}

void CHAT_CMD_FUNC(ChatCommands::CmdTransmo)
{
    PendingTransmo transmo;

    if (argc > 1) {
        int iscale;
        if (wcsncmp(argv[1], L"reset", 5) == 0) {
            transmo.npc_id = std::numeric_limits<int>::max();
        }
        else if (GuiUtils::ParseInt(argv[1], &iscale)) {
            if (!ParseScale(iscale, transmo)) {
                return;
            }
        }
        else if (!GetNPCInfoByName(argv[1], transmo)) {
            Log::Error("unknown transmo '%ls'", argv[1]);
            return;
        }
        if (argc > 2 && GuiUtils::ParseInt(argv[2], &iscale)) {
            if (!ParseScale(iscale, transmo)) {
                return;
            }
        }
    }
    else {
        if (!GetTargetTransmoInfo(transmo)) {
            return;
        }
    }
    TransmoAgent(GW::Agents::GetControlledCharacterId(), transmo);
}

bool ChatCommands::GetTargetTransmoInfo(PendingTransmo& transmo)
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) {
        return false;
    }
    transmo.npc_id = target->player_number;
    if (target->transmog_npc_id & 0x20000000) {
        transmo.npc_id = target->transmog_npc_id ^ 0x20000000;
    }
    else if (target->IsPlayer()) {
        return false;
    }
    return true;
}

void ChatCommands::TargetNearest(const wchar_t* model_id_or_name, const TargetType type)
{
    uint32_t model_id = 0;
    uint32_t index = 0; // 0=nearest. 1=first by id, 2=second by id, etc.

    // Searching by name; offload this to decode agent names first.
    if (GuiUtils::ParseUInt(model_id_or_name, &model_id)) {
        // check if there's an index component
        if (const wchar_t* rest = GetRemainingArgsWstr(model_id_or_name, 1)) {
            GuiUtils::ParseUInt(rest, &index);
        }
    }
    else {
        if (!IsNearestStr(model_id_or_name)) {
            Instance().npc_to_find.Init(model_id_or_name, type);
            return;
        }
    }

    // target nearest agent
    const auto agents = GW::Agents::GetAgentArray();
    const auto me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
    if (me == nullptr) {
        return;
    }

    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    size_t count = 0;

    for (const GW::Agent* agent : *agents) {
        if (!agent || agent == me) {
            continue;
        }
        if (!GW::Agents::GetIsAgentTargettable(agent)) {
            continue;
        }
        switch (type) {
            case Gadget: {
                // Target gadget by gadget id
                const auto gadget = agent->GetAsAgentGadget();
                if (!gadget || (model_id && gadget->gadget_id != model_id)) {
                    continue;
                }
            }
            break;
            case Item: {
                // Target item by model id
                const auto item_agent = agent->GetAsAgentItem();
                if (!item_agent) {
                    continue;
                }
                const auto item = GW::Items::GetItemById(item_agent->item_id);
                if (!item || (model_id && item->model_id != model_id)) {
                    continue;
                }
            }
            break;
            case Npc: {
                // Target npc by model id
                const auto living_agent = agent->GetAsAgentLiving();
                if (!living_agent || !living_agent->IsNPC() || !living_agent->GetIsAlive() || (model_id && living_agent->player_number != model_id)) {
                    continue;
                }
            }
            break;
            case Player: {
                // Target player by player number
                const auto living_agent = agent->GetAsAgentLiving();
                if (!living_agent || !living_agent->IsPlayer() || (model_id && living_agent->player_number != model_id)) {
                    continue;
                }
            }
            break;
            case Ally: {
                // Target any living ally
                // NB: Not quite the same as the GW version;
                // GW targets nearest player if they're less than half the distance as the nearest agent.
                // Could be a little confusing if this is used instead of 'V' in-game.
                const auto living_agent = agent->GetAsAgentLiving();
                if (!living_agent
                    || living_agent->allegiance == GW::Constants::Allegiance::Enemy
                    || living_agent->allegiance == GW::Constants::Allegiance::Neutral
                    || !living_agent->GetIsAlive() || (model_id && living_agent->player_number != model_id)) {
                    continue;
                }
            }
            break;
            case Enemy: {
                // Target any living enemy
                const auto living_agent = agent->GetAsAgentLiving();
                if (!living_agent || living_agent->allegiance != GW::Constants::Allegiance::Enemy || !living_agent->GetIsAlive() || (model_id && living_agent->player_number != model_id)) {
                    continue;
                }
            }
            break;
            case Living: {
                // Target any living agent by model id
                const auto living_agent = agent->GetAsAgentLiving();
                if (!living_agent || !living_agent->GetIsAlive() || (model_id && living_agent->player_number != model_id)) {
                    continue;
                }
            }
            break;
            default:
                continue;
        }
        if (index == 0) {
            // target closest
            const float new_distance = GetSquareDistance(me->pos, agent->pos);
            if (new_distance < distance) {
                closest = agent->agent_id;
                distance = new_distance;
            }
        }
        else {
            // target based on id
            ++count;
            if (count == index) {
                closest = agent->agent_id;
                break;
            }
        }
    }
    if (closest) {
        GW::Agents::ChangeTarget(closest);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdTransmoAgent)
{
    if (argc < 3) {
        return Log::Error("Missing /transmoagent argument");
    }
    uint32_t agent_id;
    if (!GuiUtils::ParseUInt(argv[1], &agent_id)) {
        return Log::Error("Invalid /transmoagent agent_id");
    }
    PendingTransmo transmo;
    int iscale;
    if (wcsncmp(argv[2], L"reset", 5) == 0) {
        transmo.npc_id = std::numeric_limits<int>::max();
    }
    else if (GuiUtils::ParseInt(argv[2], &iscale)) {
        if (!ParseScale(iscale, transmo)) {
            return;
        }
    }
    else if (!GetNPCInfoByName(argv[2], transmo)) {
        Log::Error("unknown transmo '%s'", argv[1]);
        return;
    }
    if (argc > 4 && GuiUtils::ParseInt(argv[3], &iscale)) {
        if (!ParseScale(iscale, transmo)) {
            return;
        }
    }
    TransmoAgent(agent_id, transmo);
}

void CHAT_CMD_FUNC(ChatCommands::CmdResize)
{
    if (argc != 3) {
        Log::Error("The syntax is /resize width height");
        return;
    }
    int width, height;
    if (!(GuiUtils::ParseInt(argv[1], &width) && GuiUtils::ParseInt(argv[2], &height))) {
        Log::Error("The syntax is /resize width height");
        return;
    }
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    RECT rect;
    GetWindowRect(hwnd, &rect);
    MoveWindow(hwnd, rect.left, rect.top, width, height, TRUE);
}

void CHAT_CMD_FUNC(ChatCommands::CmdReapplyTitle)
{
    uint32_t title_id = Instance().default_title_id;
    if (argc > 1) {
        if (!GuiUtils::ParseUInt(argv[1], &title_id)) {
            Log::Error("Syntax: /title [title_id]");
            return;
        }
        goto apply;
    }
    if (!IsMapReady()) {
        return;
    }

    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::Alcazia_Tangle:
        case GW::Constants::MapID::Arbor_Bay:
        case GW::Constants::MapID::Gadds_Encampment_outpost:
        case GW::Constants::MapID::Magus_Stones:
        case GW::Constants::MapID::Rata_Sum_outpost:
        case GW::Constants::MapID::Riven_Earth:
        case GW::Constants::MapID::Sparkfly_Swamp:
        case GW::Constants::MapID::Tarnished_Haven_outpost:
        case GW::Constants::MapID::Umbral_Grotto_outpost:
        case GW::Constants::MapID::Verdant_Cascades:
        case GW::Constants::MapID::Vloxs_Falls:
            title_id = std::to_underlying(GW::Constants::TitleID::Asuran);
            break;
        case GW::Constants::MapID::A_Gate_Too_Far_Level_1:
        case GW::Constants::MapID::A_Gate_Too_Far_Level_2:
        case GW::Constants::MapID::A_Gate_Too_Far_Level_3:
        case GW::Constants::MapID::A_Time_for_Heroes:
        case GW::Constants::MapID::Central_Transfer_Chamber_outpost:
        case GW::Constants::MapID::Destructions_Depths_Level_1:
        case GW::Constants::MapID::Destructions_Depths_Level_2:
        case GW::Constants::MapID::Destructions_Depths_Level_3:
        case GW::Constants::MapID::Genius_Operated_Living_Enchanted_Manifestation:
        case GW::Constants::MapID::Glints_Challenge_mission:
        case GW::Constants::MapID::Ravens_Point_Level_1:
        case GW::Constants::MapID::Ravens_Point_Level_2:
        case GW::Constants::MapID::Ravens_Point_Level_3:
            title_id = std::to_underlying(GW::Constants::TitleID::Deldrimor);
            break;
        case GW::Constants::MapID::Attack_of_the_Nornbear:
        case GW::Constants::MapID::Bjora_Marches:
        case GW::Constants::MapID::Blood_Washes_Blood:
        case GW::Constants::MapID::Boreal_Station_outpost:
        case GW::Constants::MapID::Cold_as_Ice:
        case GW::Constants::MapID::Curse_of_the_Nornbear:
        case GW::Constants::MapID::Drakkar_Lake:
        case GW::Constants::MapID::Eye_of_the_North_outpost:
        case GW::Constants::MapID::Gunnars_Hold_outpost:
        case GW::Constants::MapID::Ice_Cliff_Chasms:
        case GW::Constants::MapID::Jaga_Moraine:
        case GW::Constants::MapID::Mano_a_Norn_o:
        case GW::Constants::MapID::Norrhart_Domains:
        case GW::Constants::MapID::Olafstead_outpost:
        case GW::Constants::MapID::Service_In_Defense_of_the_Eye:
        case GW::Constants::MapID::Sifhalla_outpost:
        case GW::Constants::MapID::The_Norn_Fighting_Tournament:
        case GW::Constants::MapID::Varajar_Fells:
            // @todo: case MapID for Bear Club for Women/Men
            title_id = std::to_underlying(GW::Constants::TitleID::Norn);
            break;
        case GW::Constants::MapID::Against_the_Charr:
        case GW::Constants::MapID::Ascalon_City_outpost:
        case GW::Constants::MapID::Assault_on_the_Stronghold:
        case GW::Constants::MapID::Cathedral_of_Flames_Level_1:
        case GW::Constants::MapID::Cathedral_of_Flames_Level_2:
        case GW::Constants::MapID::Cathedral_of_Flames_Level_3:
        case GW::Constants::MapID::Dalada_Uplands:
        case GW::Constants::MapID::Diessa_Lowlands:
        case GW::Constants::MapID::Doomlore_Shrine_outpost:
        case GW::Constants::MapID::Dragons_Gullet:
        case GW::Constants::MapID::Eastern_Frontier:
        case GW::Constants::MapID::Flame_Temple_Corridor:
        case GW::Constants::MapID::Fort_Ranik:
        case GW::Constants::MapID::Frontier_Gate_outpost:
        case GW::Constants::MapID::Grendich_Courthouse_outpost:
        case GW::Constants::MapID::Grothmar_Wardowns:
        case GW::Constants::MapID::Longeyes_Ledge_outpost:
        case GW::Constants::MapID::Nolani_Academy:
        case GW::Constants::MapID::Old_Ascalon:
        case GW::Constants::MapID::Piken_Square_outpost:
        case GW::Constants::MapID::Regent_Valley:
        case GW::Constants::MapID::Rragars_Menagerie_Level_1:
        case GW::Constants::MapID::Rragars_Menagerie_Level_2:
        case GW::Constants::MapID::Rragars_Menagerie_Level_3:
        case GW::Constants::MapID::Ruins_of_Surmia:
        case GW::Constants::MapID::Sacnoth_Valley:
        case GW::Constants::MapID::Sardelac_Sanitarium_outpost:
        case GW::Constants::MapID::The_Breach:
        case GW::Constants::MapID::The_Great_Northern_Wall:
        case GW::Constants::MapID::Warband_Training:
        case GW::Constants::MapID::Warband_of_Brothers_Level_1:
        case GW::Constants::MapID::Warband_of_Brothers_Level_2:
        case GW::Constants::MapID::Warband_of_Brothers_Level_3:
            title_id = std::to_underlying(GW::Constants::TitleID::Vanguard);
            break;
        case GW::Constants::MapID::Abaddons_Gate:
        case GW::Constants::MapID::Basalt_Grotto_outpost:
        case GW::Constants::MapID::Bone_Palace_outpost:
        case GW::Constants::MapID::Crystal_Overlook:
        case GW::Constants::MapID::Depths_of_Madness:
        case GW::Constants::MapID::Domain_of_Anguish:
        case GW::Constants::MapID::Domain_of_Fear:
        case GW::Constants::MapID::Domain_of_Pain:
        case GW::Constants::MapID::Domain_of_Secrets:
        case GW::Constants::MapID::Dzagonur_Bastion:
        case GW::Constants::MapID::Forum_Highlands:
        case GW::Constants::MapID::Gate_of_Desolation:
        case GW::Constants::MapID::Gate_of_Fear_outpost:
        case GW::Constants::MapID::Gate_of_Madness:
        case GW::Constants::MapID::Gate_of_Pain:
        case GW::Constants::MapID::Gate_of_Secrets_outpost:
        case GW::Constants::MapID::Gate_of_Torment_outpost:
        case GW::Constants::MapID::Gate_of_the_Nightfallen_Lands_outpost:
        case GW::Constants::MapID::Grand_Court_of_Sebelkeh:
        case GW::Constants::MapID::Heart_of_Abaddon:
        case GW::Constants::MapID::Jennurs_Horde:
        case GW::Constants::MapID::Jokos_Domain:
        case GW::Constants::MapID::Lair_of_the_Forgotten_outpost:
        case GW::Constants::MapID::Nightfallen_Coast:
        case GW::Constants::MapID::Nightfallen_Garden:
        case GW::Constants::MapID::Nightfallen_Jahai:
        case GW::Constants::MapID::Nundu_Bay:
        case GW::Constants::MapID::Poisoned_Outcrops:
        case GW::Constants::MapID::Remains_of_Sahlahja:
        case GW::Constants::MapID::Ruins_of_Morah:
        case GW::Constants::MapID::The_Alkali_Pan:
        case GW::Constants::MapID::The_Mirror_of_Lyss:
        case GW::Constants::MapID::The_Mouth_of_Torment_outpost:
        case GW::Constants::MapID::The_Ruptured_Heart:
        case GW::Constants::MapID::The_Shadow_Nexus:
        case GW::Constants::MapID::The_Shattered_Ravines:
        case GW::Constants::MapID::The_Sulfurous_Wastes:
        case GW::Constants::MapID::Throne_of_Secrets:
        case GW::Constants::MapID::Vehtendi_Valley:
        case GW::Constants::MapID::Yatendi_Canyons:
            title_id = std::to_underlying(GW::Constants::TitleID::Lightbringer);
            break;
    }
apply:
    GW::Constants::TitleID current_title = GW::PlayerMgr::GetActiveTitleId();
    if (title_id == CMDTITLE_KEEP_CURRENT && current_title != GW::Constants::TitleID::None) {
        title_id = std::to_underlying(current_title);
    }
    if (current_title != GW::Constants::TitleID::None) {
        GW::PlayerMgr::RemoveActiveTitle();
    }
    switch (title_id) {
        case CMDTITLE_REMOVE_CURRENT:
            break;
        default:
            if (title_id > std::to_underlying(GW::Constants::TitleID::Codex)) {
                Log::Error("Invalid title_id %d", title_id);
                return;
            }
            GW::PlayerMgr::SetActiveTitle(static_cast<GW::Constants::TitleID>(title_id));
            break;
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdHeroBehaviour)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }
    // Argument validation
    if (argc < 2) {
        return Log::Error("Invalid argument for /hero. It can be one of: avoid | guard | attack");
    }
    // set behavior based on command message
    auto behaviour = GW::HeroBehavior::Guard; // guard by default
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"avoid") {
        behaviour = GW::HeroBehavior::AvoidCombat; // avoid combat
    }
    else if (arg1 == L"guard") {
        behaviour = GW::HeroBehavior::Guard; // guard
    }
    else if (arg1 == L"attack") {
        behaviour = GW::HeroBehavior::Fight; // attack
    }
    else {
        return Log::Error("Invalid argument for /hero. It can be one of: avoid | guard | attack");
    }

    GW::WorldContext* w = GW::GetWorldContext();
    GW::HeroFlagArray* f = w ? &w->hero_flags : nullptr;
    if (!(f && f->size())) {
        return;
    }
    for (const auto& hero : *f) {
        GW::PartyMgr::SetHeroBehavior(hero.agent_id, behaviour);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdVolume)
{
    const auto syntax = "Syntax: '/volume [master|music|background|effects|dialog|ui] [amount (0-100)]'";
    wchar_t* value;
    GW::UI::NumberPreference pref;
    switch (argc) {
        case 2:
            pref = GW::UI::NumberPreference::MasterVolume;
            value = argv[1];
            break;
        case 3: {
            const wchar_t* pref_str = argv[1];
            if (wcscmp(pref_str, L"master") == 0) {
                pref = GW::UI::NumberPreference::MasterVolume;
            }
            else if (wcscmp(pref_str, L"music") == 0) {
                pref = GW::UI::NumberPreference::MusicVolume;
            }
            else if (wcscmp(pref_str, L"background") == 0) {
                pref = GW::UI::NumberPreference::BackgroundVolume;
            }
            else if (wcscmp(pref_str, L"effects") == 0) {
                pref = GW::UI::NumberPreference::EffectsVolume;
            }
            else if (wcscmp(pref_str, L"dialog") == 0) {
                pref = GW::UI::NumberPreference::DialogVolume;
            }
            else if (wcscmp(pref_str, L"ui") == 0) {
                pref = GW::UI::NumberPreference::UIVolume;
            }
            else {
                return Log::Error(syntax);
            }
            value = argv[2];
            break;
        }
        default:
            return Log::Error(syntax);
    }
    uint32_t value_dec;
    if (!GuiUtils::ParseUInt(value, &value_dec, 10)) {
        return Log::Error(syntax);
    }
    if (value_dec > 100) {
        return Log::Error(syntax);
    }
    SetPreference(pref, value_dec);
}

void CHAT_CMD_FUNC(ChatCommands::CmdSetHardMode)
{
    if (!GW::GetWorldContext()->is_hard_mode_unlocked) {
        return;
    }
    GW::PartyMgr::SetHardMode(true);
}

void CHAT_CMD_FUNC(ChatCommands::CmdSetNormalMode)
{
    GW::PartyMgr::SetHardMode(false);
}

void CHAT_CMD_FUNC(ChatCommands::CmdAnimation)
{
    const auto syntax = "Syntax: '/animation [me|target] [animation_id (1-2076)]'";

    if (argc < 3) {
        return Log::Error(syntax);
    }

    uint32_t agentid;
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);

    if (arg1 == L"me") {
        const GW::AgentLiving* agent = GW::Agents::GetControlledCharacter();
        agentid = agent->agent_id;
    }
    else if (arg1 == L"target") {
        const GW::AgentLiving* agent = GW::Agents::GetTargetAsAgentLiving();
        if (!agent) {
            return Log::Error("No target chosen");
        }
        agentid = agent->agent_id;
    }
    else {
        return Log::Error(syntax);
    }

    uint32_t animationid = 0;
    if (!GuiUtils::ParseUInt(argv[2], &animationid) || animationid < 1 || animationid > 2076) {
        return Log::Error(syntax);
    }

    GW::GameThread::Enqueue([animationid, agentid] {
        GW::Packet::StoC::GenericValue packet;
        packet.value_id = 20;
        packet.agent_id = agentid;
        packet.value = animationid;
        GW::StoC::EmulatePacket(&packet);
    });
}

void CHAT_CMD_FUNC(ChatCommands::CmdMute)
{
    if (SetMuted_Func) {
        SetMuted_Func(!*is_muted);
        PostMuted_Func(0);
    }
}
