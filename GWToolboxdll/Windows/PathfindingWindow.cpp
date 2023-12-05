#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/MapContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <Windows/PathfindingWindow.h>
#include <ImGuiAddons.h>

#include <Modules/Resources.h>
#include <Timer.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>

namespace {
    std::unordered_map<uint32_t, Pathing::MilePath*> mile_paths_by_map_file_id;
    // Returns milepath pointer for the current map, nullptr if we're not in a valid state
    Pathing::MilePath* GetMilepathForCurrentMap() {
        const auto pathing_map = GW::Map::GetPathingMap();
        const auto info = pathing_map ? GW::Map::GetCurrentMapInfo() : nullptr;
        if (!info)
            return nullptr;
        if (mile_paths_by_map_file_id.contains(info->name_id))
            return mile_paths_by_map_file_id[info->name_id];
        auto m = new Pathing::MilePath();
        mile_paths_by_map_file_id[info->name_id] = m;
        return m;
    }

    Pathing::AStar* astar = nullptr;
    size_t draw_pos = 0;
    clock_t last_draw = 0;

    GW::HookEntry gw_ui_hookentry;

    void OnMapLoaded(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        GetMilepathForCurrentMap();
    }
}
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
    const auto current_milepath = GetMilepathForCurrentMap();
    if (!current_milepath) {
        ImGui::TextUnformatted("No milepath object");
        return ImGui::End();
    }
    if (current_milepath->progress() < 100) {
        ImGui::ProgressBar(static_cast<float>(current_milepath->progress()) * 0.01f, ImVec2(-1.0f, 0.0f));
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
        delete astar;
        astar = nullptr;
        Resources::EnqueueWorkerTask([from = player->pos, to = m_saved_pos ]() {
            auto tmpAstar = new Pathing::AStar(GetMilepathForCurrentMap());
            const auto res = tmpAstar->search(from, to);
            if (res != Pathing::Error::OK) {
                Log::Error("Pathing failed; Pathing::Error code %d", res);
                delete tmpAstar;
                return;
            }
            if (!tmpAstar->m_path.ready()) {
                Log::Error("Pathing failed; tmpAstar->m_path not ready");
                delete tmpAstar;
                return;
            }

            draw_pos = 0;
            last_draw = 0;
            astar = tmpAstar;
            });

    }
    if (!astar)
        return ImGui::End();
    ImGui::Text("Length: %.2f", astar->m_path.cost());
    ImGui::Text("n points: %d", astar->m_path.points().size());

    if (TIMER_DIFF(last_draw) > 1000
        && draw_pos + 1 < astar->m_path.points().size()) {
        GW::Vec2f points[2] = { 
            astar->m_path.points()[draw_pos].pos,
            astar->m_path.points()[draw_pos + 1].pos
        };
        GW::UI::DrawOnCompass(draw_pos, _countof(points), points);

        GW::Packet::StoC::CompassEvent event;
        event.NumberPts = _countof(points);
        event.Player = GW::Agents::GetPlayerAsAgentLiving()->player_number;
        for (size_t i = 0; i < event.NumberPts; i++) {
            event.points[i].x = (short)points[i].x;
            event.points[i].y = (short)points[i].y;
        }
        GW::StoC::EmulatePacket(&event);
        last_draw = TIMER_INIT();
        draw_pos++;
    }


    for (uint32_t i = 1; i < astar->m_path.points().size(); ++i) {
        //TODO draw path:
        //DrawLine(points[i - 1], points[i]);
    }

    ImGui::End();
}

void PathfindingWindow::SignalTerminate()
{
    ToolboxWindow::SignalTerminate();
    GW::UI::RemoveUIMessageCallback(&gw_ui_hookentry);
    for (const auto m : mile_paths_by_map_file_id) {
        m.second->stopProcessing();
    }
}
bool PathfindingWindow::CanTerminate()
{
    for (const auto m : mile_paths_by_map_file_id) {
        if (m.second->isProcessing())
            return false;
    }
    return true;
}
void PathfindingWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (const auto m : mile_paths_by_map_file_id) {
        delete m.second; // Blocking
    }
    mile_paths_by_map_file_id.clear();
    delete astar;
    astar = nullptr;
}

void PathfindingWindow::Initialize()
{
    ToolboxWindow::Initialize();

    GW::UI::RegisterUIMessageCallback(&gw_ui_hookentry, GW::UI::UIMessage::kMapLoaded, OnMapLoaded, 0x4000);
}
