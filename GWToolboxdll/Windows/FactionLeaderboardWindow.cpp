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
    const float tiny_text_width = 50.0f * ImGui::FontScale();
    const float short_text_width = 80.0f * ImGui::FontScale();
    const float avail_width = ImGui::GetContentRegionAvail().x;
    const float long_text_width = 200.0f * ImGui::FontScale();
    ImGui::Text("排名");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("阵营");
    ImGui::SameLine(offset += short_text_width);
    ImGui::Text("阵营点数");
    ImGui::SameLine(offset += short_text_width);
    ImGui::Text("前哨站");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("公会");
    ImGui::Separator();
    bool has_entries = false;

    if (const auto g = GW::GetGuildContext()) {
        const auto& leaderboard = g->factions_outpost_guilds;
        for (const auto& e : leaderboard) {
            has_entries = true;
            offset = 0.0f;
            ImGui::Text("%d", e.rank);
            ImGui::SameLine(offset += tiny_text_width);
            ImGui::Text(e.allegiance == 1 ? "路克森" : "库兹柯");
            ImGui::SameLine(offset += short_text_width);
            ImGui::Text("%d", e.faction);
            ImGui::SameLine(offset += short_text_width);
            ImGui::Text(Resources::GetMapName(e.map_id)->string().c_str());
            ImGui::SameLine(offset += long_text_width);
            const auto name_s = TextUtils::WStringToString(e.name);
            ImGui::Text("%s [%s]", name_s.c_str(), TextUtils::WStringToString(e.tag).c_str());
            ImGui::PushID(name_s.c_str());
            ImGui::SameLine(offset = avail_width - tiny_text_width);
            if (ImGui::Button("维基", ImVec2(tiny_text_width, 0))) {
                GuiUtils::OpenWiki(std::format(L"Guild:{}", e.name));
            }
            ImGui::PopID();
        }
    }
    if (!has_entries) {
        const ImVec2 w = ImGui::CalcTextSize("进入凯珊前哨站以查看数据");
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
        ImGui::SetCursorPosX(avail_width / 2 - w.x / 2);
        ImGui::Text("进入凯珊前哨站以查看数据");
    }
    return ImGui::End();
}
