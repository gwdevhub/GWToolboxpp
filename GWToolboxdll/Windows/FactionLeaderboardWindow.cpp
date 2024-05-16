#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>

#include <Windows/FactionLeaderboardWindow.h>

namespace {


    struct LeaderboardEntry {
        LeaderboardEntry() = default;

        LeaderboardEntry(const uint32_t m, const uint32_t r, const uint32_t a, const uint32_t f, const wchar_t* n, const wchar_t* t)
            : map_id(m), rank(r), allegiance(a), faction(f)
        {
            wcscpy(guild_wstr, n); // Copy the string to avoid read errors later.
            wcscpy(tag_wstr, t);   // Copy the string to avoid read errors later.
            map_name[0] = 0;
            strcpy(guild_str, GuiUtils::WStringToString(guild_wstr).c_str());
            strcpy(tag_str, GuiUtils::WStringToString(tag_wstr).c_str());
            guild_wiki_url = guild_wstr;
            std::ranges::transform(guild_wiki_url, guild_wiki_url.begin(),
                [](const wchar_t ch) -> wchar_t {
                    return ch == ' ' ? L'_' : ch;
                });
            guild_wiki_url = L"https://wiki.guildwars.com/wiki/Guild:" + guild_wiki_url;
            initialised = true;
        }

        uint32_t map_id = 0;
        uint32_t rank = 0;
        uint32_t allegiance = 0;
        uint32_t faction = 0;
        wchar_t guild_wstr[32]{};
        wchar_t tag_wstr[5]{};
        char guild_str[128]{}; // unicode char can be up to 4 bytes
        char tag_str[20]{};    // unicode char can be up to 4 bytes
        wchar_t map_name_enc[16]{};
        char map_name[256]{};
        std::wstring guild_wiki_url;
        bool initialised = false;
    };

    std::vector<LeaderboardEntry> leaderboard{};
    std::vector<LeaderboardEntry>::iterator lit{};

    GW::HookEntry TownAlliance_Entry;

    void OnStoC_TownAllianceObject(const GW::HookStatus*, GW::Packet::StoC::TownAllianceObject* pak) {
        const LeaderboardEntry leaderboardEntry = {
            pak->map_id,
            pak->rank,
            pak->allegiance,
            pak->faction,
            pak->name,
            pak->tag
        };
        if (leaderboard.size() <= leaderboardEntry.rank) {
            leaderboard.resize(leaderboardEntry.rank + 1);
        }
        leaderboard.at(leaderboardEntry.rank) = leaderboardEntry;
    }
}


void FactionLeaderboardWindow::Initialize()
{
    ToolboxWindow::Initialize();
    leaderboard.resize(15);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TownAllianceObject>(&TownAlliance_Entry, OnStoC_TownAllianceObject);
}
void FactionLeaderboardWindow::Terminate() {
    GW::StoC::RemoveCallbacks(&TownAlliance_Entry);
}

void FactionLeaderboardWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }
    float offset = 0.0f;
    const float tiny_text_width = 50.0f * ImGui::GetIO().FontGlobalScale;
    const float short_text_width = 80.0f * ImGui::GetIO().FontGlobalScale;
    const float avail_width = ImGui::GetContentRegionAvail().x;
    const float long_text_width = 200.0f * ImGui::GetIO().FontGlobalScale;
    ImGui::Text("Rank");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("Allegiance");
    ImGui::SameLine(offset += short_text_width);
    ImGui::Text("Faction");
    ImGui::SameLine(offset += short_text_width);
    ImGui::Text("Outpost");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Guild");
    ImGui::Separator();
    bool has_entries = false;
    for (size_t i = 0; i < leaderboard.size(); i++) {
        LeaderboardEntry* e = &leaderboard[i];
        if (!e->initialised) {
            continue;
        }
        has_entries = true;
        offset = 0.0f;
        if (e->map_name[0] == 0) {
            // Try to load map name in.
            const GW::AreaInfo* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(e->map_id));
            if (info && GW::UI::UInt32ToEncStr(info->name_id, e->map_name_enc, 256)) {
                GW::UI::AsyncDecodeStr(e->map_name_enc, e->map_name, 256);
            }
        }
        ImGui::Text("%d", e->rank);
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text(e->allegiance == 1 ? "Luxon" : "Kurzick");
        ImGui::SameLine(offset += short_text_width);
        ImGui::Text("%d", e->faction);
        ImGui::SameLine(offset += short_text_width);
        ImGui::Text(e->map_name);
        ImGui::SameLine(offset += long_text_width);
        ImGui::Text("%s [%s]", e->guild_str, e->tag_str);
        ImGui::PushID(static_cast<int>(e->map_id));
        ImGui::SameLine(offset = avail_width - tiny_text_width);
        if (ImGui::Button("Wiki", ImVec2(tiny_text_width, 0))) {
            ShellExecuteW(nullptr, L"open", e->guild_wiki_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::PopID();
    }
    if (!has_entries) {
        const ImVec2 w = ImGui::CalcTextSize("Enter a Canthan outpost to see data");
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::SetCursorPosX(avail_width / 2 - w.x / 2);
        ImGui::Text("Enter a Canthan outpost to see data");
    }
    return ImGui::End();
}
