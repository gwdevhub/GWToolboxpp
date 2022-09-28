#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Windows/FactionLeaderboardWindow.h>


void FactionLeaderboardWindow::Initialize() {
    ToolboxWindow::Initialize();
    leaderboard.resize(15);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TownAllianceObject>(&TownAlliance_Entry, 
        [this](GW::HookStatus* status, GW::Packet::StoC::TownAllianceObject *pak) -> bool {
            UNREFERENCED_PARAMETER(status);
            LeaderboardEntry leaderboardEntry = {
                pak->map_id,
                pak->rank,
                pak->allegiance,
                pak->faction,
                pak->name,
                pak->tag
            };
            if (leaderboard.size() <= leaderboardEntry.rank)
                leaderboard.resize(leaderboardEntry.rank + 1);
            leaderboard.at(leaderboardEntry.rank) = leaderboardEntry;
            return false;
        });
}
void FactionLeaderboardWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible)
        return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
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
    bool has_entries = 0;
    for (size_t i = 0; i < leaderboard.size(); i++) {
        LeaderboardEntry* e = &leaderboard[i];
        if (!e->initialised)
            continue;
        has_entries = 1;
        offset = 0.0f;
        if (e->map_name[0] == 0) {
            // Try to load map name in.
            GW::AreaInfo* info = GW::Map::GetMapInfo((GW::Constants::MapID)e->map_id);
            if (info && GW::UI::UInt32ToEncStr(info->name_id, e->map_name_enc, 256))
                GW::UI::AsyncDecodeStr(e->map_name_enc, e->map_name, 256);
        }
        ImGui::Text("%d",e->rank);
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text(e->allegiance == 1 ? "Luxon" : "Kurzick");
        ImGui::SameLine(offset += short_text_width);
        ImGui::Text("%d",e->faction);
        ImGui::SameLine(offset += short_text_width);
        ImGui::Text(e->map_name);
        ImGui::SameLine(offset += long_text_width);
        ImGui::Text("%s [%s]",e->guild_str,e->tag_str);
        ImGui::PushID(static_cast<int>(e->map_id));
        ImGui::SameLine(offset = avail_width - tiny_text_width);
        if (ImGui::Button("Wiki",ImVec2(tiny_text_width,0))) {
            ShellExecuteW(NULL, L"open", e->guild_wiki_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::PopID();
    }
    if (!has_entries) {
        ImVec2 w = ImGui::CalcTextSize("Enter a Canthan outpost to see data");
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::SetCursorPosX(avail_width / 2 - (w.x / 2));
        ImGui::Text("Enter a Canthan outpost to see data");
    }
    return ImGui::End();
}
