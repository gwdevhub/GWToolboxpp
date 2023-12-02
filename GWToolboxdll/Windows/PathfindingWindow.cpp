#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/GameEntities/Agent.h>

#include <Windows/PathfindingWindow.h>
#include <ImGuiAddons.h>

void PathfindingWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(512, 256), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    // Check on first load
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return ImGui::End();
    }

    auto mp = Pathing::MilePath::instance();

    if (mp->progress() < 100) {
        ImGui::ProgressBar(static_cast<float>(mp->progress()) * 0.01f, ImVec2(-1.0f, 0.0f));
        return ImGui::End();
    }

    auto player = GW::Agents::GetPlayer();
    if (!player) {
        return ImGui::End();
    }

    if (ImGui::Button("Save Pos")) {
        m_saved_pos = player->pos;
    }
    if (m_saved_pos.x != 0.0f && m_saved_pos.y != 0.0f) {
        ImGui::SameLine();
        ImGui::Text("Saved: %.2f %.2f", m_saved_pos.x, m_saved_pos.y);
    }

    if (ImGui::Button("Find Path")) {
        Pathing::AStar astar(mp);
        m_path = astar.search(player->pos, m_saved_pos);        
    }

    auto points = m_path.points();
    ImGui::Text("Length: %.2f", m_path.cost());
    ImGui::Text("n points: %d", points.size());

    for (auto i = 1; points.size(); ++i) {
        //TODO draw path:
        //DrawLine(points[i - 1], points[i]);
    }

    ImGui::End();
}

void PathfindingWindow::SignalTerminate()
{
    auto mp = Pathing::MilePath::instance();
    if (mp) {
        mp->stopProcessing();
    }
}

void PathfindingWindow::Initialize()
{
    ToolboxWindow::Initialize();
}
