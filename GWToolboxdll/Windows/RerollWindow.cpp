#include "stdafx.h"

#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Guild.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Modules/Resources.h>
#include <Windows/RerollWindow.h>
#include <Timer.h>
#include <GWCA/Context/PreGameContext.h>
#include <ImGuiAddons.h>
#include <GuiUtils.h>

namespace {
    GW::CharContext* GetCharContext() {
        auto g = GW::GameContext::instance();
        return g ? g->character : 0;
    }
    GW::PartyInfo* GetPlayerParty() {
        auto g = GW::GameContext::instance();
        if (!g) return 0;
        auto c = g->party;
        if (!c) return 0;
        return c->player_party;
    }
    uint32_t GetPlayerNumber() {
        auto c = GetCharContext();
        return c ? c->player_number : 0;
    }
    const wchar_t* GetPlayerName() {
        auto c = GetCharContext();
        return c ? c->player_name : 0;
    }
    const wchar_t* GetAccountEmail() {
        auto c = GetCharContext();
        wchar_t* email = c ? c->player_email : 0;
        if (email && !email[0])
            email = 0;
        return email;
    }
    GW::Guild* GetCurrentGH()
    {
        GW::AreaInfo* m = GW::Map::GetCurrentMapInfo();
        if (!m || m->type != GW::RegionType::RegionType_GuildHall)
            return nullptr;
        const GW::Array<GW::Guild*>& guilds = GW::GuildMgr::GetGuildArray();
        if (!guilds.valid())
            return nullptr;
        for (size_t i = 0; i < guilds.size(); i++) {
            if (!guilds[i])
                continue;
            return guilds[i];
        }
        return nullptr;
    }
    const wchar_t* GetNextPartyLeader() {
        GW::PartyInfo* player_party = GetPlayerParty();
        if (!player_party || !player_party->players.valid() || player_party->players.size() < 2)
            return 0;
        uint32_t player_number = GetPlayerNumber();
        for (size_t i = 0; i < player_party->players.size(); i++) {
            if (player_party->players[i].login_number == player_number)
                continue;
            auto player = GW::PlayerMgr::GetPlayerByID(player_party->players[i].login_number);
            if (!player)
                continue;
            return player->name;
        }
        return 0;
    }
}

RerollWindow::~RerollWindow() {
    for (auto it : account_characters) {
        delete it.second;
    }
    account_characters.clear();
    if (guild_hall_uuid) {
        delete guild_hall_uuid;
        guild_hall_uuid = 0;
    }
    if (OnGoToCharSelect_Entry) {
        GW::UI::RemoveUIMessageCallback(OnGoToCharSelect_Entry);
        delete OnGoToCharSelect_Entry;
        OnGoToCharSelect_Entry = 0;
    }
}
void RerollWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    auto available_chars = GetAvailableChars();
    if (!available_chars) {
        ImGui::TextDisabled("Go to character select screen to record available characters");
    }
    else {
        ImGui::Text("Click on a character name to switch to that character.");
        ImGui::Checkbox("Travel to same location after rerolling",&travel_to_same_location_after_rerolling);
        ImGui::Checkbox("Re-join your party after rerolling", &rejoin_party_after_rerolling);
        const float btnw = ImGui::GetContentRegionAvail().x / 2.f;
        const ImVec2 btn_dim = { btnw,0.f };
        char buf[40];
        for (size_t i = 0; i < available_chars->size(); i++) {
            wchar_t* player_name = available_chars->at(i).data();
            snprintf(buf, _countof(buf), "%ls", player_name);
            if ((i % 2) != 0)
                ImGui::SameLine();
            if (ImGui::Button(buf, btn_dim)) {
                bool _same_map = travel_to_same_location_after_rerolling;
                bool _same_party = travel_to_same_location_after_rerolling && rejoin_party_after_rerolling;
                if (rejoin_party_after_rerolling && !_same_party) {
                    GW::PartyInfo* p = GetPlayerParty();
                    if (p && p->players.size() > 1)
                        _same_party = true;
                }
                Reroll(player_name, _same_map || _same_party, _same_party);
            }
        }
    }

    ImGui::End();

}
std::vector<std::wstring>* RerollWindow::GetAvailableChars() {
    const wchar_t* email = GetAccountEmail();
    return email ? account_characters[email] : 0;
}
void RerollWindow::Update(float) {
    if (!OnGoToCharSelect_Entry) {
        // Add an entry to check available characters at login screen
        OnGoToCharSelect_Entry = new GW::HookEntry;
        GW::UI::RegisterUIMessageCallback(OnGoToCharSelect_Entry, [&](GW::HookStatus*, uint32_t msg_id, void*, void*) {
            if(msg_id == 0x10000170)
                check_available_chars = true;
            },0x4000);
    }
    if (check_available_chars && IsCharSelectReady()) {
        auto chars = GW::PreGameContext::instance()->chars;
        const wchar_t* email = GetAccountEmail();
        if (email) {
            auto found = account_characters.find(email);
            if (found != account_characters.end() && found->second) {
                found->second->clear();
            }
            for (auto& p : chars) {
                AddAvailableCharacter(email, p.character_name);
            }
            check_available_chars = false;
        }
    }
    if (reroll_stage != None && TIMER_INIT() > reroll_timeout) {
        RerollFailed(L"Reroll timed out");
        return;
    }
    GW::PreGameContext* pgc = GW::PreGameContext::instance();
    switch (reroll_stage) {
        case PendingLogout: {
            uint32_t logout = 1;
            GW::UI::SendUIMessage(0x1000009b, &logout);
            reroll_stage = WaitingForCharSelect;
            reroll_timeout = TIMER_INIT() + 10000;
            return;
        }
        case WaitingForCharSelect: {
            if (!pgc || !pgc->chars.valid())
                return;
            uint32_t ui_state = 10;
            GW::UI::SendUIMessage(0x10000170, 0, &ui_state);
            if (ui_state != 2)
                return; // Not at char select screen yet.
            reroll_stage = CheckForCharname;
            reroll_timeout = TIMER_INIT() + 5000;
            return;
        }
        case CheckForCharname: {
            if (!pgc || !pgc->chars.valid())
                return;
            for (size_t i = 0; i < pgc->chars.size(); i++) {
                if (wcscmp(pgc->chars[i].character_name, reroll_to_player_name) == 0) {
                    reroll_index_needed = i;
                    reroll_index_current = 0xffffffdd;
                    reroll_stage = NavigateToCharname;
                    reroll_timeout = TIMER_INIT() + 3000;
                    return;
                }
            }
            return;
        }
        case NavigateToCharname: {
            if (!pgc || !pgc->chars.valid())
                return;
            if (pgc->index_1 == reroll_index_current)
                return; // Not moved yet
            HWND h = GW::MemoryMgr::GetGWWindowHandle();
            if (pgc->index_1 == reroll_index_needed) {
                // Play
                SendMessage(h, WM_KEYDOWN, 0x50, 0x00190001);
                SendMessage(h, WM_CHAR, 'p', 0x00190001);
                SendMessage(h, WM_KEYUP, 0x50, 0xC0190001);
                reroll_stage = WaitForCharacterLoad;
                reroll_timeout = TIMER_INIT() + 20000;
                return;
            }
            reroll_index_current = pgc->index_1;
            SendMessage(h, WM_KEYDOWN, VK_LEFT, 0x014B0001);
            SendMessage(h, WM_KEYUP, VK_LEFT, 0xC14B0001);
            return;
        }
        case WaitForCharacterLoad: {
            if (pgc)
                return;
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
                return;
            const wchar_t* player_name = GetPlayerName();
            if (!player_name || wcscmp(player_name, reroll_to_player_name) != 0) {
                RerollFailed(L"Wrong character was loaded");
                return;
            }
            if (same_map) {
                if (!IsInMap()) {
                    if (guild_hall_uuid) {
                        // Was previously in a guild hall
                        GW::GuildMgr::TravelGH(*(GW::GHKey*)guild_hall_uuid);
                    }
                    else {
                        if (!GW::Map::GetIsMapUnlocked(map_id)) {
                            const GW::AreaInfo* map = GW::Map::GetMapInfo(map_id);
                            wchar_t map_name_buf[8];
                            wchar_t err_message_buf[256] = L"[Error] Your character does not have that map unlocked";
                            if (map && map->name_id && GW::UI::UInt32ToEncStr(map->name_id, map_name_buf, 8))
                                Log::ErrorW(L"[Error] Your character does not have \x1\x2%s\x2\x108\x107 unlocked", map_name_buf);
                            else
                                Log::ErrorW(err_message_buf);
                            RerollFailed(L"Map load failed");
                            return;
                        }
                        GW::Map::Travel(map_id, 0, region_id, language_id);
                        reroll_stage = WaitForActiveDistrict;
                        reroll_timeout = TIMER_INIT() + 20000;
                        return;
                    }
                }
                reroll_stage = WaitForMapLoad;
                reroll_timeout = TIMER_INIT() + 20000;
                return;
            }
            RerollSuccess();
            return;
        }
        case WaitForActiveDistrict: {
            if (!IsInMap(false))
                return;
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
                return;
            if (!IsInMap()) {
                // Same map, wrong district
                GW::Map::Travel(map_id, district_id, region_id, language_id);
                reroll_stage = WaitForMapLoad;
                reroll_timeout = TIMER_INIT() + 20000;
                return;
            }
            RerollSuccess();
            return;
        }
        case WaitForMapLoad: {
            if (!IsInMap())
                return;
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
                return;
            
            if (same_party && party_leader[0]) {
                GW::PartyInfo* player_party = GetPlayerParty();
                if (player_party && player_party->GetPartySize() > 1)
                    GW::PartyMgr::LeaveParty();
                wchar_t msg_buf[32];
                ASSERT(swprintf(msg_buf, _countof(msg_buf), L"invite %s", party_leader) != -1);
                GW::Chat::SendChat('/', msg_buf);
            }
            RerollSuccess();
            return;
        }
    }
}
void RerollWindow::AddAvailableCharacter(const wchar_t* email, const wchar_t* charname) {
    auto found = account_characters.find(email);
    if (found == account_characters.end() || !found->second) {
        account_characters[email] = new std::vector<std::wstring>();
    }
    account_characters[email]->push_back(charname);
}
bool RerollWindow::IsInMap(bool include_district) {
    if (guild_hall_uuid) {
        GW::Guild* current_location = GetCurrentGH();
        return current_location && memcmp(&current_location->key, guild_hall_uuid, sizeof(*guild_hall_uuid)) == 0;
    }
    return GW::Map::GetMapID() == map_id && (!include_district || GW::Map::GetDistrict() == district_id) && GW::Map::GetRegion() == region_id && GW::Map::GetLanguage() == language_id;
}
void RerollWindow::RerollSuccess() {
    reroll_stage = None;
    if (reverting_reroll && failed_message) {
        Log::ErrorW(failed_message);
    }
}
bool RerollWindow::IsCharSelectReady() {
    GW::PreGameContext* pgc = GW::PreGameContext::instance();
    if (!pgc || !pgc->chars.valid())
        return false;
    uint32_t ui_state = 10;
    GW::UI::SendUIMessage(0x10000170, 0, &ui_state);
    return ui_state == 2;
}
void RerollWindow::RerollFailed(wchar_t* reason) {
    reroll_stage = None;
    if (reverting_reroll)
        return; // Can't do anything.
    failed_message = reason;
    reverting_reroll = true;
    wcscpy(reroll_to_player_name, initial_player_name);
    same_map = true;
    same_party = true;
    reroll_timeout = TIMER_INIT() + 1000;
    reroll_stage = PendingLogout;
}
void RerollWindow::Reroll(wchar_t* character_name, bool _same_map, bool _same_party) {
    reroll_stage = None;
    reverting_reroll = false;
    failed_message = 0;
    if (!character_name)
        return;
    wcscpy(reroll_to_player_name, character_name);
    const wchar_t* player_name = GetPlayerName();
    if (!player_name)
        return;
    wcscpy(initial_player_name, player_name);
    const wchar_t* party_leader_name = GetNextPartyLeader();
    if (party_leader_name) {
        wcscpy(party_leader, party_leader_name);
    }
    else {
        party_leader[0] = 0;
    }
    map_id = GW::Map::GetMapID();
    district_id = GW::Map::GetDistrict();
    region_id = GW::Map::GetRegion();
    language_id = GW::Map::GetLanguage();
    if (guild_hall_uuid) {
        delete[] guild_hall_uuid;
        guild_hall_uuid = 0;
    }
    GW::Guild* current_guild_hall = GetCurrentGH();
    if (current_guild_hall) {
        guild_hall_uuid = new uint32_t[4];
        memcpy(guild_hall_uuid, &current_guild_hall->key, sizeof(current_guild_hall->key));
    }
    same_map = _same_map;
    same_party = _same_party;
    reroll_timeout = TIMER_INIT() + 1000;
    reroll_stage = PendingLogout;
}
void RerollWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    CSimpleIni::TNamesDepend keys;
    if (ini->GetAllKeys("RerollWindow_AvailableChars", keys)) {
        for (auto& it : keys) {
            std::wstring charname_ws = GuiUtils::StringToWString(it.pItem);
            std::string email_s = ini->GetValue("RerollWindow_AvailableChars", it.pItem, "");
            if (email_s.empty()) continue;
            std::wstring email_ws = GuiUtils::StringToWString(email_s);
            AddAvailableCharacter(email_ws.c_str(), charname_ws.c_str());
        }
    }
    travel_to_same_location_after_rerolling = ini->GetBoolValue(Name(), VAR_NAME(travel_to_same_location_after_rerolling), travel_to_same_location_after_rerolling);
    rejoin_party_after_rerolling = ini->GetBoolValue(Name(), VAR_NAME(rejoin_party_after_rerolling), rejoin_party_after_rerolling);
}
void RerollWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    for (auto it : account_characters) {
        std::string email_s = GuiUtils::WStringToString(it.first);
        auto chars = it.second;
        for (auto it2 = chars->begin(); it2 != chars->end(); it2++) {
            std::string charname_s = GuiUtils::WStringToString(*it2);
            if (charname_s.empty()) continue;
            ini->SetValue("RerollWindow_AvailableChars", charname_s.c_str(), email_s.c_str());
        }
    }
    ini->SetBoolValue(Name(), VAR_NAME(travel_to_same_location_after_rerolling), travel_to_same_location_after_rerolling);
    ini->SetBoolValue(Name(), VAR_NAME(rejoin_party_after_rerolling), rejoin_party_after_rerolling);
}
