#include "stdafx.h"
#include <GWCA/Context/GuildContext.h>
#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include <Windows/FactionLeaderboardWindow.h>
#include <Utils/TextUtils.h>

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

    if (const auto g = GW::GetGuildContext()) {
        const auto& leaderboard = g->factions_outpost_guilds;
        for (const auto& e : leaderboard) {
            has_entries = true;
            offset = 0.0f;
            ImGui::Text("%d", e.rank);
            ImGui::SameLine(offset += tiny_text_width);
            ImGui::Text(e.allegiance == 1 ? "Luxon" : "Kurzick");
            ImGui::SameLine(offset += short_text_width);
            ImGui::Text("%d", e.faction);
            ImGui::SameLine(offset += short_text_width);
            ImGui::Text(Resources::GetMapName(e.map_id)->string().c_str());
            ImGui::SameLine(offset += long_text_width);
            ImGui::Text("%s [%s]", TextUtils::WStringToString(e.name).c_str(), TextUtils::WStringToString(e.tag).c_str());
            ImGui::PushID(&e);
            ImGui::SameLine(offset = avail_width - tiny_text_width);
            if (ImGui::Button("Wiki", ImVec2(tiny_text_width, 0))) {
                GuiUtils::OpenWiki(std::format(L"Guild:{}", e.name));
            }
            ImGui::PopID();
        }
    }
    if (!has_entries) {
        const ImVec2 w = ImGui::CalcTextSize("Enter a Canthan outpost to see data");
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::SetCursorPosX(avail_width / 2 - w.x / 2);
        ImGui::Text("Enter a Canthan outpost to see data");
    }
    return ImGui::End();
}
