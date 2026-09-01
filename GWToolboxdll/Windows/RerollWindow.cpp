#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Friendslist.h>

#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Modules/GwDatModule.h>
#include <Modules/Resources.h>
#include <Windows/RerollWindow.h>
#include <Timer.h>

#include <ImGuiAddons.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>
#include <GWCA/GameEntities/Frame.h>

namespace {

    GW::HookEntry ChatCmd_HookEntry;

    RerollWindow::Settings settings;

    // GW 文件 0x5e700：256x64 精灵表，包含 4 个 64px 图标：无、重铸、杜姆契约、梅兰朵协定
    IDirect3DTexture9** covenant_sprite = nullptr;

    bool check_available_chars = true;

    clock_t reroll_timeout = 0;
    clock_t reroll_stage_set = 0;
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
    bool reverting_reroll = false;

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

    bool GetIsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Map::GetIsMapLoaded() && GW::Agents::GetControlledCharacter();
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
        return TextUtils::ToLower(TextUtils::RemovePunctuation(in));
    }

    std::vector<std::wstring> exclude_charnames_from_reroll_cmd;
    char excluded_char_add_buf[20] = {0};

    // 映射 账号UUID字符串 → (职业ID → 首选角色名称)。
    // 空的职业条目表示该职业“无偏好”。
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::wstring>> preferred_chars_per_account;

    std::string GetCurrentAccountUuidStr()
    {
        const auto uuid = GW::AccountMgr::GetAccountUuid();
        const GUID empty{};
        if (memcmp(&uuid, &empty, sizeof(uuid)) == 0) return {};
        return TextUtils::GuidToString(&uuid);
    }

    std::unordered_map<uint32_t, std::wstring>& GetCurrentAccountPrefs()
    {
        return preferred_chars_per_account[GetCurrentAccountUuidStr()];
    }

    GW::HookEntry OnGoToCharSelect_Entry;

    bool IsExcludedFromReroll(const wchar_t* player_name)
    {
        return std::ranges::contains(exclude_charnames_from_reroll_cmd, LowerCaseRemovePunct(player_name));
    }

    void DrawExcludedCharacters()
    {
        ImGui::Spacing();
        if (ImGui::TreeNodeEx("/reroll 命令排除角色", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            for (size_t i = 0; i < exclude_charnames_from_reroll_cmd.size(); i++) {
                auto& excluded = exclude_charnames_from_reroll_cmd[i];
                ImGui::PushID(i);
                ImGui::TextUnformatted(TextUtils::WStringToString(excluded).c_str());
                ImGui::SameLine();
                const bool clicked = ImGui::SmallButton("X");
                ImGui::PopID();
                if (clicked) {
                    exclude_charnames_from_reroll_cmd.erase(exclude_charnames_from_reroll_cmd.begin() + i);
                    break; // 下一轮循环
                }
            }
            if (ImGui::InputText("###add_character_to_exclude", excluded_char_add_buf, _countof(excluded_char_add_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                const auto charname_w = TextUtils::StringToWString(excluded_char_add_buf);
                if (charname_w.length() && !IsExcludedFromReroll(charname_w.c_str())) {
                    exclude_charnames_from_reroll_cmd.push_back(LowerCaseRemovePunct(charname_w));
                }
                excluded_char_add_buf[0] = 0;
            }
            ImGui::TreePop();
        }
    }

    void DrawPreferredCharacters()
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("按职业首选角色");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Indent();
        ImGui::TextDisabled("选择每个职业要切换到的首选角色。留空则使用第一个可用角色。");

        const auto available_chars_ptr = GW::AccountMgr::GetAvailableChars();

        auto& account_prefs = GetCurrentAccountPrefs();
        const auto img_w = ImGui::CalcTextSize(" ").y;
        const ImVec2 img_s = {img_w, img_w};
        const auto dropdown_w = 200.f * ImGui::FontScale();

        size_t i = 1;
        for (i = 1; i <= (size_t)GW::Constants::Profession::Dervish; i++) {
            if ((i % 2) == 0) ImGui::SameLine(0, img_w);
            const auto prof = static_cast<GW::Constants::Profession>(i);
            const auto prof_key = static_cast<uint32_t>(prof);
            auto& pref = account_prefs[prof_key];

            ImGui::PushID(static_cast<int>(i));

            std::vector<const wchar_t*> candidates;
            if (available_chars_ptr && available_chars_ptr->valid()) {
                for (const auto& c : *available_chars_ptr) {
                    if (c.primary() == prof) {
                        candidates.push_back(c.player_name);
                    }
                }
            }

            ImGui::ImageFit(*Resources::GetProfessionIcon(prof), img_s);
            ImGui::SameLine();

            const auto current_str = TextUtils::WStringToString(pref);
            const auto preview = pref.empty() ? "(任意)" : current_str.c_str();

            ImGui::SetNextItemWidth(dropdown_w);
            if (ImGui::BeginCombo("##pref", preview)) {
                if (ImGui::Selectable("(any)", pref.empty())) {
                    pref.clear();
                }
                for (const auto* cname : candidates) {
                    const auto cname_str = TextUtils::WStringToString(cname);
                    const bool selected = !pref.empty() && wcscmp(cname, pref.c_str()) == 0;
                    if (ImGui::Selectable(cname_str.c_str(), selected)) {
                        pref = cname;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        ImGui::Unindent();
        ImGui::Spacing();
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

    void RerollFailed(const wchar_t* reason)
    {
        if (reroll_stage < WaitingForCharSelect) {
            reroll_stage = None;
            return;
        }
        reroll_stage = None;
        if (reverting_reroll) {
            return; // 无法处理
        }
        failed_message = reason;
        if (!settings.return_on_fail) {
            return;
        }
        reverting_reroll = true;
        wcscpy(reroll_to_player_name, initial_player_name);
        same_map = false;
        same_party = true;
        reroll_timeout = TIMER_INIT() + 1000;
        reroll_stage = PendingLogout;
    }

    bool Reroll(const wchar_t* character_name, bool _same_map, const bool _same_party = true, const bool _ignore_current_character = false, const bool _do_not_prompt = false)
    {
        reroll_stage = None;
        reverting_reroll = false;
        failed_message = nullptr;
        if (!GW::AccountMgr::GetAvailableCharacter(character_name)) {
            return false;
        }
        wcscpy(reroll_to_player_name, character_name);
        const wchar_t* player_name = GW::AccountMgr::GetCurrentPlayerName();
        if (!player_name) {
            return false;
        }
        if (!_ignore_current_character && wcscmp(player_name, character_name) == 0) {
            return false;
        }
        wcscpy(initial_player_name, player_name);
        const wchar_t* party_leader_name = GetNextPartyLeader();
        if (party_leader_name) {
            wcscpy(party_leader, party_leader_name);
            if (!_same_map && _same_party) {
                _same_map = true; // 如果要加入同一队伍，确保前往同一地图
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
        reroll_stage = _do_not_prompt ? PendingLogout : PromptPendingLogout;
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

    // 查找给定职业的最佳可用角色并发起切换。
    // 首选角色（如果配置了）优先尝试，然后选择第一个非排除的匹配角色。
    bool RerollToProfession(const GW::Constants::Profession profession, const bool _same_map, const bool _same_party)
    {
        const auto available_characters = GW::AccountMgr::GetAvailableChars();
        if (!available_characters || !available_characters->valid()) {
            return false;
        }

        const auto account_it = preferred_chars_per_account.find(GetCurrentAccountUuidStr());
        if (account_it != preferred_chars_per_account.end()) {
            const auto pref_it = account_it->second.find(static_cast<uint32_t>(profession));
            if (pref_it != account_it->second.end() && !pref_it->second.empty()) {
                const auto pref_char = GW::AccountMgr::GetAvailableCharacter(pref_it->second.c_str());
                if (pref_char && pref_char->primary() == profession) {
                    return Reroll(pref_it->second.c_str(), _same_map, _same_party);
                }
            }
        }

        for (const auto& available_char : *available_characters) {
            if (IsExcludedFromReroll(available_char.player_name)) {
                continue;
            }
            if (available_char.primary() == profession) {
                return Reroll(available_char.player_name, _same_map, _same_party);
            }
        }
        return false;
    }

    // 返回 RerollToProfession 将使用的角色名称，若无可用则返回 nullptr
    const wchar_t* FindAvailableCharForProfession(const GW::Constants::Profession profession)
    {
        const auto available_characters = GW::AccountMgr::GetAvailableChars();
        if (!available_characters || !available_characters->valid()) {
            return nullptr;
        }
        const auto account_it = preferred_chars_per_account.find(GetCurrentAccountUuidStr());
        if (account_it != preferred_chars_per_account.end()) {
            const auto pref_it = account_it->second.find(static_cast<uint32_t>(profession));
            if (pref_it != account_it->second.end() && !pref_it->second.empty()) {
                const auto pref_char = GW::AccountMgr::GetAvailableCharacter(pref_it->second.c_str());
                if (pref_char && pref_char->primary() == profession) {
                    return pref_char->player_name;
                }
            }
        }
        for (const auto& available_char : *available_characters) {
            if (available_char.primary() == profession) {
                return available_char.player_name;
            }
        }
        return nullptr;
    }

    void CHAT_CMD_FUNC(CmdReroll)
    {
        if (argc < 2) {
            Log::Error("语法错误：/reroll [职业|角色名称]");
            return;
        }
        auto available_characters = GW::AccountMgr::GetAvailableChars();
        if (!available_characters || !available_characters->valid()) {
            Log::Error("获取可用角色失败");
            return;
        }
        const auto character_or_profession = TextUtils::ToLower(GetRemainingArgsWstr(message, 1));
        constexpr std::array to_find = {
            L"",
            L"战士",
            L"游侠",
            L"僧侣",
            L"死灵法师",
            L"幻术师",
            L"元素使",
            L"刺客",
            L"祭祀",
            L"圣言者",
            L"神唤使"
        };

        // 按职业名称搜索 → 使用 RerollToProfession（遵循首选角色设置）
        for (size_t i = 1; i < to_find.size(); i++) {
            if (wcsstr(to_find.at(i), character_or_profession.c_str())) {
                const auto prof = static_cast<GW::Constants::Profession>(i);
                if (!RerollToProfession(prof, settings.travel_to_same_location_after_rerolling, settings.rejoin_party_after_rerolling)) {
                    Log::Error("未找到该职业的可用角色");
                }
                return;
            }
        }

        // 按角色名称搜索（先精确匹配，再子串匹配）
        const wchar_t* substring_match = nullptr;
        for (const auto& available_char : *available_characters) {
            const auto player_name = available_char.player_name;
            if (IsExcludedFromReroll(player_name)) {
                continue;
            }
            const auto lower_name = TextUtils::ToLower(player_name);
            if (!substring_match && wcsstr(lower_name.c_str(), character_or_profession.c_str())) {
                substring_match = player_name;
                if (lower_name == character_or_profession) break;
            }
        }
        if (substring_match) {
            Reroll(substring_match, settings.travel_to_same_location_after_rerolling, settings.rejoin_party_after_rerolling);
            return;
        }
        Log::Error("未匹配到职业或角色名称");
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
            ImGui::ConfirmDialog("你当前在探索区域中。\n确定要切换角色吗？", OnRerollPromptReply);
            reroll_stage = PromptPendingReply;
            return;
        }
        const auto char_select_info = GW::AccountMgr::GetAvailableCharacter(reroll_to_player_name);
        if (!char_select_info) {
            RerollFailed(L"从角色选择列表中找到可用角色失败");
            return;
        }
        const auto reroll_to_player_current_map = char_select_info->map_id();
        if (GWToolbox::ShouldDisableToolbox(reroll_to_player_current_map)) {
            const auto charname_str = TextUtils::WStringToString(char_select_info->player_name);
            const auto msg = std::format("{} 当前在 {} 中。\n"
                "这是一个工具箱无法工作的前哨站。\n"
                "你仍然可以切换到该角色，但不会自动旅行。\n\n"
                "继续？",
                charname_str, Resources::GetMapName(reroll_to_player_current_map)->string());
            ImGui::ConfirmDialog(msg.c_str(), OnRerollPromptReply);
            reroll_stage = PromptPendingReply;
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
        ImGui::TextDisabled("前往角色选择界面以记录可用角色");
    }
    else {
        ImGui::Text("点击角色名称即可切换到该角色。");
        ImGui::Checkbox("切换后前往相同地点", &settings.travel_to_same_location_after_rerolling);
        ImGui::Checkbox("切换后重新加入队伍", &settings.rejoin_party_after_rerolling);
        ImGui::Checkbox("失败时返回原角色", &settings.return_on_fail);
        const float btnw = ImGui::GetContentRegionAvail().x / 2.f - ImGui::GetStyle().ItemSpacing.x;
        const ImVec2 btn_dim = {btnw, 0.f};
        std::string buf;
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
        static std::vector<GW::AvailableCharacterInfo> available_chars_snapshot;
        static std::vector<GW::AvailableCharacterInfo> available_chars_vector;
        const auto available_chars_size = available_chars_ptr->size();
        if (available_chars_snapshot.size() != available_chars_size
            || (available_chars_size && memcmp(available_chars_snapshot.data(), available_chars_ptr->begin(), available_chars_size * sizeof(GW::AvailableCharacterInfo)) != 0)) {
            available_chars_snapshot.assign(available_chars_ptr->begin(), available_chars_ptr->end());
            available_chars_vector = available_chars_snapshot;
            std::ranges::sort(available_chars_vector, [](const auto& a, const auto& b) {
                return std::wstring_view(a.player_name) < std::wstring_view(b.player_name);
            });
        }
        for (const auto& [idx, character] : available_chars_vector | std::views::enumerate) {
            const wchar_t* player_name = character.player_name;
            const auto profession = character.primary();
            buf = TextUtils::WStringToString(player_name);
            if (idx % 2 != 0) {
                ImGui::SameLine();
            }
            const auto is_current_char = wcscmp(character.player_name, GW::AccountMgr::GetCurrentPlayerName()) == 0;
            if (is_current_char) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
            }
            if (ImGui::IconButton(buf.c_str(), *Resources::GetProfessionIcon(profession), btn_dim)) {
                const bool _same_map = settings.travel_to_same_location_after_rerolling;
                bool _same_party = settings.travel_to_same_location_after_rerolling && settings.rejoin_party_after_rerolling;
                if (settings.rejoin_party_after_rerolling && !_same_party) {
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
            if (covenant_sprite && *covenant_sprite) {
                float uv_x0 = -1.f;
                if (character.is_melandrus_accord())
                    uv_x0 = 0.75f;
                else if (character.is_dhuums_covenant())
                    uv_x0 = 0.50f;
                else if (character.is_reforged())
                    uv_x0 = 0.25f;
                if (uv_x0 >= 0.f) {
                    const ImVec2 item_min = ImGui::GetItemRectMin();
                    const ImVec2 item_max = ImGui::GetItemRectMax();
                    const float icon_h = ImGui::GetTextLineHeight();
                    const float icon_y = item_min.y + (item_max.y - item_min.y - icon_h) * 0.5f;
                    ImGui::GetWindowDrawList()->AddImage(
                        reinterpret_cast<ImTextureID>(*covenant_sprite),
                        {item_max.x - icon_h, icon_y}, {item_max.x, icon_y + icon_h},
                        {uv_x0, 0.f}, {uv_x0 + 0.25f, 1.f}
                    );
                }
            }
        }
        ImGui::PopStyleVar();
    }
    DrawExcludedCharacters();

    ImGui::End();
}

void RerollWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    DrawPreferredCharacters();
}

void RerollWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    reroll_stage = RerollStage::None;

    covenant_sprite = GwDatModule::LoadTextureFromFileId(0x5e700);

    // 在登录界面添加检查可用角色的条目
    RegisterUIMessageCallback(&OnGoToCharSelect_Entry, GW::UI::UIMessage::kCheckUIState, OnUIMessage, 0x4000);

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"reroll", CmdReroll);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"rr", CmdReroll);
}

void RerollWindow::Terminate() {
    ToolboxWindow::Terminate();

    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnGoToCharSelect_Entry);

    guild_hall_uuid = {};
}

void RerollWindow::Update(float)
{
    if (reroll_stage != None && TIMER_INIT() > reroll_timeout) {
        RerollFailed(L"切换角色超时");
        return;
    }
    if (GWToolbox::ShouldDisableToolbox()) {
        RerollSuccess();
        return;
    }

    switch (reroll_stage) {
        case PendingLogout: {
            const auto char_select_info = GW::AccountMgr::GetAvailableCharacter(reroll_to_player_name);
            if (!char_select_info) {
                RerollFailed(L"从角色选择列表中找到可用角色失败");
                return;
            }
            if (GWToolbox::ShouldDisableToolbox(char_select_info->map_id())) {
                // 如果工具箱在下一张地图不可用，确保切换后不尝试做任何事
                same_map = same_party = false;
            }
            auto packet = GW::UI::UIPacket::kLogout{.unknown = 0, .character_select = 1u};
            GW::UI::SendUIMessage(GW::UI::UIMessage::kLogout, &packet);
            reroll_stage = WaitingForCharSelect;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 10000;
            return;
        }
        case WaitingForCharSelect: {
            if (!GW::LoginMgr::IsCharSelectReady()) {
                return;
            }
            reroll_stage = NavigateToCharname;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 5000;
            return;
        }
        case NavigateToCharname: {
            if (!GW::LoginMgr::IsCharSelectReady()) {
                return;
            }

            GW::FriendListMgr::SetFriendListStatus(online_status);
            if (!GW::LoginMgr::SelectCharacterToPlay(reroll_to_player_name, true)) {
                RerollFailed(L"选择要游玩的角色失败");
                return;
            }
            reroll_stage = WaitForCharacterLoad;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
        } break;
        case WaitForCharacterLoad: {
            if (GW::LoginMgr::IsCharSelectReady()) {
                return;
            }
            if (!GetIsMapReady() || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
                return;
            }
            const wchar_t* player_name = GW::AccountMgr::GetCurrentPlayerName();
            if (!player_name || wcscmp(player_name, reroll_to_player_name) != 0) {
                RerollFailed(L"加载了错误的角色");
                return;
            }
            if (same_map) {
                if (!IsInMap()) {
                    if (guild_hall_uuid) {
                        // 之前在公会大厅中
                        GW::GuildMgr::TravelGH(guild_hall_uuid);
                    }
                    else {
                        if (!GW::Map::GetIsMapUnlocked(map_id)) {
                            RerollFailed(L"地图未解锁");
                            return;
                        }
                        reroll_scroll_from_map_id = GetScrollableOutpostForEliteArea(map_id);
                        if (reroll_scroll_from_map_id != GW::Constants::MapID::None) {
                            if (!GW::Map::GetIsMapUnlocked(reroll_scroll_from_map_id)) {
                                RerollFailed(L"没有可传送的前哨站已解锁");
                                return;
                            }
                            if (!GetScrollItemForEliteArea(map_id)) {
                                RerollFailed(L"精英区域没有可用卷轴");
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
                RerollFailed(L"精英区域没有可用卷轴");
                return;
            }
            if (!GW::Items::UseItem(scroll)) {
                RerollFailed(L"使用卷轴失败");
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
                // 相同地图，错误的区域
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
            ASSERT(swprintf(msg_buf, _countof(msg_buf), L"邀请 %s", party_leader) != -1);
            GW::Chat::SendChat('/', msg_buf);
            RerollSuccess();
        }
    }
}

bool RerollWindow::Reroll(const wchar_t* character_name, const GW::Constants::MapID _map_id)
{
    return ::Reroll(character_name, _map_id);
}

bool RerollWindow::Reroll(const wchar_t* character_name, bool _same_map, const bool _same_party, const bool _ignore_current_character, const bool _do_not_prompt)
{
    return ::Reroll(character_name, _same_map, _same_party, _ignore_current_character, _do_not_prompt);
}

bool RerollWindow::RerollToProfession(const GW::Constants::Profession profession, const bool _same_map, const bool _same_party)
{
    return ::RerollToProfession(profession, _same_map, _same_party);
}

const wchar_t* RerollWindow::FindAvailableCharForProfession(const GW::Constants::Profession profession)
{
    return ::FindAvailableCharForProfession(profession);
}

bool RerollWindow::IsRerolling()
{
    return reroll_stage != RerollStage::None;
}

void RerollWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    std::vector<std::string> excluded_charnames_strings;
    if (!doc.Get(Name(), "exclude_charnames_from_reroll_cmd", excluded_charnames_strings) && legacy) {
        GuiUtils::IniToArray(legacy->GetValue(Name(), "exclude_charnames_from_reroll_cmd", ""), excluded_charnames_strings, ',');
    }
    for (auto& cstring : excluded_charnames_strings) {
        auto charname_w = TextUtils::StringToWString(cstring);
        if (charname_w.length() && !IsExcludedFromReroll(charname_w.c_str())) {
            exclude_charnames_from_reroll_cmd.push_back(LowerCaseRemovePunct(charname_w));
        }
    }

    preferred_chars_per_account.clear();
    std::map<std::string, std::map<uint32_t, std::string>> stored_prefs;
    if (doc.Get(Name(), "preferred_chars_per_account", stored_prefs)) {
        for (const auto& [uuid_str, prof_map] : stored_prefs) {
            for (const auto& [prof, charname] : prof_map) {
                preferred_chars_per_account[uuid_str][prof] = TextUtils::StringToWString(charname);
            }
        }
    }
    else if (legacy) {
        // 旧版按账号首选角色。键为 "pref_{uuid}_{n}"（n = 1-10）。
        TNamesDepend keys;
        legacy->GetAllKeys(Name(), keys);
        for (const auto& entry : keys) {
            const std::string_view k = entry.pItem;
            if (!k.starts_with("pref_") || k.size() < 43) continue;
            const auto uuid_str = std::string(k.substr(5, 36));
            if (k.size() <= 42 || k[41] != '_') continue;
            const auto n_str = k.substr(42);
            const auto n = static_cast<uint32_t>(std::strtoul(n_str.data(), nullptr, 10));
            if (n < 1 || n > 10) continue;
            const char* val = legacy->GetValue(Name(), k.data(), "");
            if (val && *val) {
                preferred_chars_per_account[uuid_str][n] = TextUtils::StringToWString(val);
            }
        }
    }
}

void RerollWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    std::vector<std::string> excluded_charnames_strings;
    excluded_charnames_strings.reserve(exclude_charnames_from_reroll_cmd.size());
    for (const auto& excluded : exclude_charnames_from_reroll_cmd) {
        excluded_charnames_strings.push_back(TextUtils::WStringToString(excluded));
    }
    doc.Set(Name(), "exclude_charnames_from_reroll_cmd", excluded_charnames_strings);

    std::map<std::string, std::map<uint32_t, std::string>> stored_prefs;
    for (const auto& [uuid_str, prof_map] : preferred_chars_per_account) {
        for (const auto& [prof, charname] : prof_map) {
            if (charname.empty()) continue;
            stored_prefs[uuid_str][prof] = TextUtils::WStringToString(charname);
        }
    }
    doc.Set(Name(), "preferred_chars_per_account", stored_prefs);
}
