#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Modules/Resources.h>
#include <Windows/RerollWindow.h>
#include <Timer.h>

#include <ImGuiAddons.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Utils/ToolboxUtils.h>


namespace {

    bool travel_to_same_location_after_rerolling = true;
    bool rejoin_party_after_rerolling = true;

    bool check_available_chars = true;

    clock_t reroll_timeout = 0;
    uint32_t char_sort_order = std::numeric_limits<uint32_t>::max();
    clock_t reroll_stage_set = 0;
    uint32_t reroll_index_needed = 0;
    uint32_t reroll_index_current = 0xffffffdd;
    GW::FriendStatus online_status = GW::FriendStatus::Online;
    GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0);
    int district_id = 0;
    GW::Constants::ServerRegion region_id = (GW::Constants::ServerRegion)0;
    GW::Constants::Language language_id = (GW::Constants::Language)0;
    GW::GHKey guild_hall_uuid{};
    wchar_t initial_player_name[20] = { 0 };
    wchar_t reroll_to_player_name[20] = { 0 };
    wchar_t party_leader[20] = { 0 };
    bool same_map = false;
    bool same_party = false;
    const wchar_t* failed_message = nullptr;
    bool return_on_fail = false;
    bool reverting_reroll = false;

    std::map<std::wstring, std::vector<std::wstring>*> account_characters{};

    enum RerollStage {
        None,
        PendingLogout,
        PromptPendingLogout,
        PromptPendingReply,
        WaitingForCharSelect,
        CheckForCharname,
        NavigateToCharname,
        WaitForCharacterLoad,
        WaitForScrollableOutpost,
        WaitForActiveDistrict,
        WaitForMapLoad,
        WaitForEmptyParty,
        Done
    };

    RerollStage reroll_stage = RerollStage::None;

    GW::Constants::MapID reroll_scroll_from_map_id = (GW::Constants::MapID)0;

    GW::PartyInfo* GetPlayerParty()
    {
        const auto c = GW::GetPartyContext();
        return c ? c->player_party : nullptr;
    }

    uint32_t GetPlayerNumber()
    {
        const auto c = GW::GetCharContext();
        return c ? c->player_number : 0;
    }

    const wchar_t* GetNextPartyLeader()
    {
        const auto player_party = GetPlayerParty();
        if (!player_party || !player_party->players.valid() || player_party->players.size() < 2) {
            return nullptr;
        }
        const uint32_t player_number = GetPlayerNumber();
        for (size_t i = 0; i < player_party->players.size(); i++) {
            if (player_party->players[i].login_number == player_number) {
                continue;
            }
            const auto player = GW::PlayerMgr::GetPlayerByID(player_party->players[i].login_number);
            if (!player) {
                continue;
            }
            return player->name;
        }
        return nullptr;
    }

    using SetOnlineStatus_pt = void(__cdecl*)(GW::FriendStatus status);
    SetOnlineStatus_pt SetOnlineStatus_Func;
    SetOnlineStatus_pt RetSetOnlineStatus;

    bool GetIsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Map::GetIsMapLoaded() && GW::Agents::GetControlledCharacter();
    }

    bool GetIsCharSelectReady()
    {
        const GW::PreGameContext* pgc = GW::GetPreGameContext();
        if (!pgc || !pgc->chars.valid()) {
            return false;
        }
        uint32_t ui_state = 10;
        SendUIMessage(GW::UI::UIMessage::kCheckUIState, nullptr, &ui_state);
        return ui_state == 2;
    }

    GW::Constants::MapID GetScrollableOutpostForEliteArea(const GW::Constants::MapID elite_area)
    {
        GW::Constants::MapID scrollable_map_id;
        switch (elite_area) {
            case GW::Constants::MapID::The_Deep:
                scrollable_map_id = GW::Constants::MapID::Cavalon_outpost;
                break;
            case GW::Constants::MapID::Urgozs_Warren:
                scrollable_map_id = GW::Constants::MapID::House_zu_Heltzer_outpost;
                break;
            default:
                return GW::Constants::MapID::None;
        }
        if (!GW::Map::GetIsMapUnlocked(scrollable_map_id)) {
            scrollable_map_id = GW::Constants::MapID::Embark_Beach;
        }
        return scrollable_map_id;
    }

    GW::Item* GetScrollItemForEliteArea(const GW::Constants::MapID elite_area)
    {
        uint32_t scroll_model_id = 0;
        switch (elite_area) {
            case GW::Constants::MapID::The_Deep:
                scroll_model_id = 22279;
                break;
            case GW::Constants::MapID::Urgozs_Warren:
                scroll_model_id = 3256;
                break;
        }
        if (!scroll_model_id) {
            return nullptr;
        }

        return GW::Items::GetItemByModelId(
            scroll_model_id,
            static_cast<int>(GW::Constants::Bag::Backpack),
            static_cast<int>(GW::Constants::Bag::Storage_14));
    }

    const wchar_t* GetRemainingArgsWstr(const wchar_t* message, const int argc_start)
    {
        const wchar_t* out = message;
        for (auto i = 0; i < argc_start && out; i++) {
            out = wcschr(out, ' ');
            if (out) {
                out++;
            }
        }
        return out;
    };

    std::wstring LowerCaseRemovePunct(const std::wstring& in)
    {
        return GuiUtils::ToLower(GuiUtils::RemovePunctuation(in));
    }

    std::vector<std::wstring> exclude_charnames_from_reroll_cmd;
    char excluded_char_add_buf[20] = {0};

    GW::HookEntry OnGoToCharSelect_Entry;

    bool IsExcludedFromReroll(const wchar_t* player_name)
    {
        return std::ranges::contains(exclude_charnames_from_reroll_cmd, LowerCaseRemovePunct(player_name));
    }

    void DrawExcludedCharacters()
    {
        ImGui::Spacing();
        if (ImGui::TreeNodeEx("Excluded Characters from /reroll command", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            for (size_t i = 0; i < exclude_charnames_from_reroll_cmd.size(); i++) {
                auto& excluded = exclude_charnames_from_reroll_cmd[i];
                ImGui::PushID(i);
                ImGui::TextUnformatted(GuiUtils::WStringToString(excluded).c_str());
                ImGui::SameLine();
                const bool clicked = ImGui::SmallButton("X");
                ImGui::PopID();
                if (clicked) {
                    exclude_charnames_from_reroll_cmd.erase(exclude_charnames_from_reroll_cmd.begin() + i);
                    break; // next loop
                }
            }
            if (ImGui::InputText("###add_character_to_exclude", excluded_char_add_buf, _countof(excluded_char_add_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                const auto charname_w = GuiUtils::StringToWString(excluded_char_add_buf);
                if (charname_w.length() && !IsExcludedFromReroll(charname_w.c_str())) {
                    exclude_charnames_from_reroll_cmd.push_back(LowerCaseRemovePunct(charname_w));
                }
                excluded_char_add_buf[0] = 0;
            }
            ImGui::TreePop();
        }
    }

    bool IsInMap(const bool include_district = true)
    {
        if (guild_hall_uuid) {
            const GW::Guild* current_location = GW::GuildMgr::GetCurrentGH();
            return current_location && memcmp(&current_location->key, &guild_hall_uuid, sizeof(guild_hall_uuid)) == 0;
        }
        return GW::Map::GetMapID() == map_id && (!include_district || district_id == 0 || GW::Map::GetDistrict() == district_id) && GW::Map::GetRegion() == region_id && GW::Map::GetLanguage() == language_id;
    }

    void RerollSuccess()
    {
        reroll_stage = None;
        if (reverting_reroll && failed_message) {
            Log::ErrorW(failed_message);
        }
    }

    bool IsCharSelectReady()
    {
        const GW::PreGameContext* pgc = GW::GetPreGameContext();
        if (!pgc || !pgc->chars.valid()) {
            return false;
        }
        uint32_t ui_state = 10;
        SendUIMessage(GW::UI::UIMessage::kCheckUIState, nullptr, &ui_state);
        return ui_state == 2;
    }

    void RerollFailed(const wchar_t* reason)
    {
        if (reroll_stage < WaitingForCharSelect) {
            reroll_stage = None;
            return;
        }
        reroll_stage = None;
        if (reverting_reroll) {
            return; // Can't do anything.
        }
        failed_message = reason;
        if (!return_on_fail) {
            return;
        }
        reverting_reroll = true;
        wcscpy(reroll_to_player_name, initial_player_name);
        same_map = false;
        same_party = true;
        reroll_timeout = TIMER_INIT() + 1000;
        reroll_stage = PendingLogout;
    }

    bool Reroll(const wchar_t* character_name, bool _same_map, const bool _same_party = true)
    {
        reroll_stage = None;
        reverting_reroll = false;
        failed_message = nullptr;
        char_sort_order = GetPreference(GW::UI::EnumPreference::CharSortOrder);
        SetPreference(GW::UI::EnumPreference::CharSortOrder, std::to_underlying(GW::Constants::Preference::CharSortOrder::Alphabetize));
        if (!GW::AccountMgr::GetAvailableCharacter(character_name)) {
            return false;
        }
        wcscpy(reroll_to_player_name, character_name);
        const wchar_t* player_name = GW::AccountMgr::GetCurrentPlayerName();
        if (!player_name || wcscmp(player_name, character_name) == 0) {
            return false;
        }
        wcscpy(initial_player_name, player_name);
        const wchar_t* party_leader_name = GetNextPartyLeader();
        if (party_leader_name) {
            wcscpy(party_leader, party_leader_name);
            if (!_same_map && _same_party) {
                _same_map = true; // Ensure we go to same map if we want to join the same party
            }
        }
        else {
            party_leader[0] = 0;
        }
        map_id = GW::Map::GetMapID();
        district_id = GW::Map::GetDistrict();
        region_id = GW::Map::GetRegion();
        language_id = GW::Map::GetLanguage();
        online_status = GW::FriendListMgr::GetMyStatus();
        guild_hall_uuid = {};
        if (const auto current_guild_hall = GW::GuildMgr::GetCurrentGH()) {
            memcpy(&guild_hall_uuid, &current_guild_hall->key, sizeof(current_guild_hall->key));
        }
        same_map = _same_map;
        same_party = _same_party;
        reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
        reroll_stage = PromptPendingLogout;
        return true;
    }

    bool Reroll(const wchar_t* character_name, const GW::Constants::MapID _map_id)
    {
        if (!Reroll(character_name, true, false)) {
            return false;
        }
        map_id = _map_id;
        guild_hall_uuid = {};
        district_id = 0;
        return true;
    }

    void CHAT_CMD_FUNC(CmdReroll)
    {
        if (argc < 2) {
            Log::Error("Incorrect syntax: /reroll [profession|character_name]");
            return;
        }
        auto available_characters = GW::AccountMgr::GetAvailableChars();
        if (!available_characters || !available_characters->valid()) {
            Log::Error("Failed to get available characters");
            return;
        }
        const std::wstring character_or_profession = GuiUtils::ToLower(GetRemainingArgsWstr(message, 1));
        constexpr std::array to_find = {
            L"",
            L"warrior",
            L"ranger",
            L"monk",
            L"necromancer",
            L"mesmer",
            L"elementalist",
            L"assassin",
            L"ritualist",
            L"paragon",
            L"dervish"
        };

        // Search by profession
        for (size_t i = 0; i < to_find.size(); i++) {
            if (!wcsstr(to_find.at(i), character_or_profession.c_str())) {
                continue;
            }
            for (auto& available_char : *available_characters) {
                if (available_char.primary() != i) {
                    continue;
                }
                const auto player_name = available_char.player_name;
                if (IsExcludedFromReroll(player_name)) {
                    continue;
                }
                Reroll(player_name, travel_to_same_location_after_rerolling, rejoin_party_after_rerolling);
                return;
            }
        }

        // Search by character name
        for (const auto& available_char : *available_characters) {
            const auto player_name = available_char.player_name;
            if (IsExcludedFromReroll(player_name)) {
                continue;
            }
            if (!wcsstr(GuiUtils::ToLower(player_name).c_str(), character_or_profession.c_str())) {
                continue;
            }
            Reroll(available_char.player_name, travel_to_same_location_after_rerolling, rejoin_party_after_rerolling);
            return;
        }
        Log::Error("Failed to match profession or character name for command");
    }

    // Hook to override status on login - allows us to keep FL status across rerolls without messing with UI
    void OnSetStatus(GW::FriendStatus status)
    {
        GW::Hook::EnterHook();
        if (reroll_stage == WaitForCharacterLoad) {
            status = online_status;
        }
        RetSetOnlineStatus(status);
        GW::Hook::LeaveHook();
    }

    void OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage msg_id, void*, void*)
    {
        if (msg_id == GW::UI::UIMessage::kCheckUIState) {
            check_available_chars = true;
        }
    }

    void OnRerollPromptReply(bool result, void*) {
        if (result && reroll_stage == PromptPendingReply) {
            reroll_stage = PendingLogout;
        }
        else {
            reroll_stage = None;
        }
    }

}

void RerollWindow::Draw(IDirect3DDevice9*)
{
    if (reroll_stage == PromptPendingLogout) {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
            ImGui::ConfirmDialog("You're currently in an explorable area.\nAre you sure you want to change character?", OnRerollPromptReply);
            reroll_stage = PromptPendingReply;
            return;
        }
        const auto char_select_info = GW::AccountMgr::GetAvailableCharacter(reroll_to_player_name);
        if (!char_select_info) {
            RerollFailed(L"Failed to find available character from char select list");
            return;
        }
        const auto reroll_to_player_current_map = char_select_info->map_id();
        if (GWToolbox::ShouldDisableToolbox(reroll_to_player_current_map)) {
            const auto charname_str = GuiUtils::WStringToString(char_select_info->player_name);
            const auto msg = std::format("{} is currently in {}.\n"
                "This is an outpost that toolbox won't work in.\n"
                "You can still swap to this character, but won't automatically travel.\n\n"
                "Continue?",
                charname_str, Resources::GetMapName(reroll_to_player_current_map)->string());
            ImGui::ConfirmDialog(msg.c_str(), OnRerollPromptReply);
            return;
        }
        reroll_stage = PendingLogout;
    }
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    const auto available_chars_ptr = GW::AccountMgr::GetAvailableChars();
    if (!available_chars_ptr || !available_chars_ptr->valid()) {
        ImGui::TextDisabled("Go to character select screen to record available characters");
    }
    else {
        ImGui::Text("Click on a character name to switch to that character.");
        ImGui::Checkbox("Travel to same location after rerolling", &travel_to_same_location_after_rerolling);
        ImGui::Checkbox("Re-join your party after rerolling", &rejoin_party_after_rerolling);
        ImGui::Checkbox("Return to original character on fail", &return_on_fail);
        const float btnw = ImGui::GetContentRegionAvail().x / 2.f;
        const ImVec2 btn_dim = {btnw, 0.f};
        std::string buf;
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
        for (size_t i = 0; i < available_chars_ptr->size(); i++) {
            auto& character = available_chars_ptr->at(i);
            const wchar_t* player_name = character.player_name;
            uint32_t profession = character.primary();
            buf = GuiUtils::WStringToString(player_name);
            if (i % 2 != 0) {
                ImGui::SameLine();
            }
            const auto is_current_char = wcscmp(character.player_name, GW::AccountMgr::GetCurrentPlayerName()) == 0;
            if (is_current_char) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
            }
            if (ImGui::IconButton(buf.c_str(), *Resources::GetProfessionIcon(static_cast<GW::Constants::Profession>(profession)), btn_dim)) {
                const bool _same_map = travel_to_same_location_after_rerolling;
                bool _same_party = travel_to_same_location_after_rerolling && rejoin_party_after_rerolling;
                if (rejoin_party_after_rerolling && !_same_party) {
                    const GW::PartyInfo* p = GetPlayerParty();
                    if (p && p->players.size() > 1) {
                        _same_party = true;
                    }
                }
                Reroll(player_name, _same_map || _same_party, _same_party);
            }
            if (is_current_char) {
                ImGui::PopItemFlag();
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopStyleVar();
    }
    DrawExcludedCharacters();

    ImGui::End();
}

void RerollWindow::Initialize()
{
    ToolboxWindow::Initialize();

    reroll_stage = RerollStage::None;

    // Add an entry to check available characters at login screen
    RegisterUIMessageCallback(&OnGoToCharSelect_Entry, GW::UI::UIMessage::kCheckUIState, OnUIMessage, 0x4000);
    // Hook to override status on login - allows us to keep FL status across rerolls without messing with UI
    SetOnlineStatus_Func = (SetOnlineStatus_pt)GW::Scanner::FindAssertion(R"(p:\code\gw\friend\friendapi.cpp)", "status < FRIEND_STATUSES", -0x11);
    if (SetOnlineStatus_Func) {
        GW::Hook::CreateHook((void**)&SetOnlineStatus_Func, OnSetStatus, (void**)&RetSetOnlineStatus);
        GW::Hook::EnableHooks(SetOnlineStatus_Func);
    }


    GW::Chat::CreateCommand(L"reroll", CmdReroll);
    GW::Chat::CreateCommand(L"rr", CmdReroll);


}

void RerollWindow::Terminate() {

    GW::Chat::DeleteCommand(L"reroll");
    GW::Chat::DeleteCommand(L"rr");

    GW::Hook::RemoveHook(SetOnlineStatus_Func);
    GW::UI::RemoveUIMessageCallback(&OnGoToCharSelect_Entry);

    for (const auto& char_name : account_characters | std::views::values) {
        delete char_name;
    }
    account_characters.clear();
    guild_hall_uuid = {};
}

void RerollWindow::Update(float)
{
    if (reroll_stage != None && TIMER_INIT() > reroll_timeout) {
        if (char_sort_order != std::numeric_limits<uint32_t>::max()) {
            SetPreference(GW::UI::EnumPreference::CharSortOrder, char_sort_order);
        }
        RerollFailed(L"Reroll timed out");
        return;
    }
    GW::PreGameContext* pgc = GW::GetPreGameContext();
    switch (reroll_stage) {
        case PendingLogout: {
            const auto char_select_info = GW::AccountMgr::GetAvailableCharacter(reroll_to_player_name);
            if (!char_select_info) {
                RerollFailed(L"Failed to find available character from char select list");
                return;
            }
            if (GWToolbox::ShouldDisableToolbox(char_select_info->map_id())) {
                // If toolbox isn't going to be available in the next map, make sure we don't try to do anything after reroll.
                same_map = same_party = false;
            }
            auto packet = GW::UI::UIPacket::kLogout{
                .unknown = 0, 
                .character_select = 0
            };
            SendUIMessage(GW::UI::UIMessage::kLogout, &packet);
            reroll_stage = WaitingForCharSelect;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 10000;
            return;
        }
        case WaitingForCharSelect: {
            if (!GetIsCharSelectReady()) {
                return;
            }
            reroll_stage = CheckForCharname;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 5000;
            return;
        }
        case CheckForCharname: {
            if (!GetIsCharSelectReady()) {
                return;
            }
            for (size_t i = 0; i < pgc->chars.size(); i++) {
                if (wcscmp(pgc->chars[i].character_name, reroll_to_player_name) == 0) {
                    reroll_index_needed = i;
                    reroll_index_current = 0xffffffdd;
                    reroll_stage = NavigateToCharname;
                    reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 3000;
                    return;
                }
            }
            return;
        }
        case NavigateToCharname: {
            if (!GetIsCharSelectReady()) {
                return;
            }
            if (pgc->index_1 == reroll_index_current) {
                return; // Not moved yet
            }
            const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
            if (pgc->index_1 == reroll_index_needed) {
                // Play
                SendMessage(hwnd, WM_KEYDOWN, 0x50, 0x00190001);
                SendMessage(hwnd, WM_CHAR, 'p', 0x00190001);
                SendMessage(hwnd, WM_KEYUP, 0x50, 0xC0190001);
                reroll_stage = WaitForCharacterLoad;
                reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                return;
            }
            reroll_index_current = pgc->index_1;
            SendMessage(hwnd, WM_KEYDOWN, VK_RIGHT, 0x014D0001);
            SendMessage(hwnd, WM_KEYUP, VK_RIGHT, 0xC14D0001);
            return;
        }
        case WaitForCharacterLoad: {
            if (GetIsCharSelectReady()) {
                return;
            }
            if (!GetIsMapReady() || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
                return;
            }
            const wchar_t* player_name = GW::AccountMgr::GetCurrentPlayerName();
            if (!player_name || wcscmp(player_name, reroll_to_player_name) != 0) {
                RerollFailed(L"Wrong character was loaded");
                return;
            }
            if (char_sort_order != std::numeric_limits<uint32_t>::max()) {
                SetPreference(GW::UI::EnumPreference::CharSortOrder, char_sort_order);
            }
            if (same_map) {
                if (!IsInMap()) {
                    if (guild_hall_uuid) {
                        // Was previously in a guild hall
                        GW::GuildMgr::TravelGH(guild_hall_uuid);
                    }
                    else {
                        if (!GW::Map::GetIsMapUnlocked(map_id)) {
                            RerollFailed(L"Map isn't unlocked");
                            return;
                        }
                        reroll_scroll_from_map_id = GetScrollableOutpostForEliteArea(map_id);
                        if (reroll_scroll_from_map_id != GW::Constants::MapID::None) {
                            if (!GW::Map::GetIsMapUnlocked(reroll_scroll_from_map_id)) {
                                RerollFailed(L"No scrollable outpost unlocked");
                                return;
                            }
                            if (!GetScrollItemForEliteArea(map_id)) {
                                RerollFailed(L"No scroll available for elite area");
                                return;
                            }
                            if (GW::Map::GetMapID() != reroll_scroll_from_map_id) {
                                GW::Map::Travel(reroll_scroll_from_map_id, region_id, 0, language_id);
                            }

                            reroll_stage = WaitForScrollableOutpost;
                            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                            return;
                        }
                        GW::Map::Travel(map_id, region_id, 0, language_id);
                        reroll_stage = WaitForActiveDistrict;
                        reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                        return;
                    }
                }
                reroll_stage = WaitForMapLoad;
                reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                return;
            }
            RerollSuccess();
            return;
        }
        case WaitForScrollableOutpost: {
            if (!GetIsMapReady() || GW::Map::GetMapID() != static_cast<GW::Constants::MapID>(reroll_scroll_from_map_id)) {
                return;
            }
            const GW::Item* scroll = GetScrollItemForEliteArea(map_id);
            if (!scroll) {
                RerollFailed(L"No scroll available for elite area");
                return;
            }
            if (!GW::Items::UseItem(scroll)) {
                RerollFailed(L"Failed to use scroll item");
                return;
            }
            reroll_stage = WaitForActiveDistrict;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
        }
        break;

        case WaitForActiveDistrict: {
            if (!GetIsMapReady() || !IsInMap(false)) {
                return;
            }
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
                return;
            }
            if (!IsInMap()) {
                // Same map, wrong district
                GW::Map::Travel(map_id, region_id, district_id, language_id);
            }
            reroll_stage = WaitForMapLoad;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
            return;
        }
        case WaitForMapLoad: {
            if (!GetIsMapReady() || !IsInMap()) {
                return;
            }
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
                return;
            }

            if (same_party && party_leader[0]) {
                GW::PartyInfo* player_party = GetPlayerParty();
                if (player_party && player_party->GetPartySize() > 1) {
                    GW::PartyMgr::LeaveParty();
                    GW::PartyMgr::KickAllHeroes();
                }
                reroll_stage = WaitForEmptyParty;
                reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 3000;
                return;
            }
            RerollSuccess();
            return;
        case WaitForEmptyParty:
            GW::PartyInfo* player_party = GetPlayerParty();
            if (player_party && player_party->GetPartySize() > 1) {
                return;
            }
            wchar_t msg_buf[32];
            ASSERT(same_party && party_leader[0]);
            ASSERT(swprintf(msg_buf, _countof(msg_buf), L"invite %s", party_leader) != -1);
            GW::Chat::SendChat('/', msg_buf);
            RerollSuccess();
        }
    }
}

bool RerollWindow::Reroll(const wchar_t* character_name, const GW::Constants::MapID _map_id)
{
    return ::Reroll(character_name, _map_id);
}

bool RerollWindow::Reroll(const wchar_t* character_name, bool _same_map, const bool _same_party)
{
    return ::Reroll(character_name, _same_map, _same_party);
}

void RerollWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    ToolboxIni::TNamesDepend keys;
    LOAD_BOOL(travel_to_same_location_after_rerolling);
    LOAD_BOOL(rejoin_party_after_rerolling);
    LOAD_BOOL(return_on_fail);

    std::vector<std::string> excluded_charnames_strings;
    GuiUtils::IniToArray(ini->GetValue(Name(), "exclude_charnames_from_reroll_cmd", ""), excluded_charnames_strings, ',');
    for (auto& cstring : excluded_charnames_strings) {
        auto charname_w = GuiUtils::StringToWString(cstring);
        if (charname_w.length() && !IsExcludedFromReroll(charname_w.c_str())) {
            exclude_charnames_from_reroll_cmd.push_back(LowerCaseRemovePunct(charname_w));
        }
    }
}

void RerollWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    for (const auto& it : account_characters) {
        std::string email_s = GuiUtils::WStringToString(it.first);
        const auto chars = it.second;
        for (auto it2 = chars->begin(); it2 != chars->end(); ++it2) {
            std::string charname_s = GuiUtils::WStringToString(*it2);
            if (charname_s.empty()) {
                continue;
            }
            ini->SetValue("RerollWindow_AvailableChars", charname_s.c_str(), email_s.c_str());
        }
    }
    SAVE_BOOL(travel_to_same_location_after_rerolling);
    SAVE_BOOL(rejoin_party_after_rerolling);
    SAVE_BOOL(return_on_fail);

    std::string excluded_charnames_ini = "";
    for (auto& excluded : exclude_charnames_from_reroll_cmd) {
        if (excluded_charnames_ini.length()) {
            excluded_charnames_ini += ',';
        }
        excluded_charnames_ini += GuiUtils::WStringToString(excluded);
    }
    ini->SetValue(Name(), "exclude_charnames_from_reroll_cmd", excluded_charnames_ini.c_str());
}
