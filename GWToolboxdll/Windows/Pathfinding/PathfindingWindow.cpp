#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Timer.h>
#include <ImGuiAddons.h>

#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/Resources.h>
#include <GWCA/Context/MapContext.h>
#include <Utils/ArenaNetFileParser.h>

#include "PathingMapData.h"
#include "PathingMapDataLoader.h"


namespace {
    std::unordered_map<uint64_t, Pathing::MilePath*> mile_paths_by_coords;
    // Returns milepath pointer for the current map, nullptr if we're not in a valid state
    Pathing::MilePath* GetMilepathForCurrentMap()
    {
        //todo: maybe use bool GetIsMapReady() from other modules?
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) return nullptr;
        const auto mc = GW::GetMapContext();
        if (!(mc && mc->path && mc->path->staticData)) return nullptr;

        auto hash = static_cast<uint64_t>(mc->path->staticData->map_id);
        hash |= static_cast<uint64_t>(mc->path->pathNodes.size()) << 32;

        if (mile_paths_by_coords.contains(hash))
            return mile_paths_by_coords[hash];

        const auto m = new Pathing::MilePath(mc);
        mile_paths_by_coords[hash] = m;
        return m;
    }

    Pathing::AStar* astar = nullptr;
    size_t draw_pos = 0;
    clock_t last_draw = 0;

    volatile bool pending_terminate = false;
    volatile bool pending_worker_task = false;

    bool pending_redraw = false;
    clock_t pending_undraw = 0;

    GW::HookEntry gw_ui_hookentry;

    std::vector<CustomRenderer::CustomLine*> minimap_lines;

 
    GW::GamePos* GetPlayerPos()
    {
        const auto p = GW::Agents::GetObservingAgent();
        return p ? &p->pos : nullptr;
    }

    // Returns false if our last AStar calculation matches what we're asking for.
    bool NeedsRecalculating(const GW::GamePos& from, const GW::GamePos& to)
    {
        if (!(astar && astar->m_path.ready() && astar->m_path.points().size()))
            return true;
        return from != astar->m_path.points().at(0)
               || to != astar->m_path.points().at(astar->m_path.points().size() - 1);
    }

    void RecalculatePath(const GW::GamePos& from, const GW::GamePos& to)
    {
        if (!NeedsRecalculating(from, to))
            return;
        delete astar;
        astar = nullptr;
        Resources::EnqueueWorkerTask([from, to] {
            const auto milepath = GetMilepathForCurrentMap();
            if (!milepath) {
                return;
            }
            const auto tmpAstar = new Pathing::AStar(milepath);
            const auto res = tmpAstar->Search(from, to);
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

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kLoadMapContext: {
                [[maybe_unused]] const auto packet = static_cast<GW::UI::UIPacket::kLoadMapContext*>(wParam);
                #ifdef _DEBUG
                // Load from map context, but also load from the DAT - save both to JSON to review and compare the data.
                if (packet->file_name && *packet->file_name) {
                    const uint32_t map_file_id = ArenaNetFileParser::FileHashToFileId(packet->file_name);

                    auto from_dat = new Pathing::PathingMapData();
                    if (!Pathing::LoadPathingMapDataFromDAT(map_file_id, from_dat)) {
                        delete from_dat;
                        return;
                    }
                    auto from_context = new Pathing::PathingMapData();
                    if (!Pathing::LoadFromMapContext(GW::GetMapContext(), map_file_id, from_context)) {
                        delete from_dat;
                        delete from_context;
                        return;
                    }
                    Resources::EnqueueWorkerTask([from_dat, from_context]() {
                        auto write_to = std::format(L"pathing_map_data_from_file_{:#}.json", from_dat->map_file_id);
                        auto fp = fopen(Resources::GetPath(write_to).string().c_str(), "wb");
                        if (fp) {
                            const nlohmann::json json = *from_dat;
                            const auto str = json.dump(2);
                            fwrite(str.data(), str.size(), 1, fp);
                            fclose(fp);
                        }
                        write_to = std::format(L"pathing_map_data_from_context_{:#}.json", from_context->map_file_id);
                        fp = fopen(Resources::GetPath(write_to).string().c_str(), "wb");
                        if (fp) {
                            const nlohmann::json json = *from_context;
                            const auto str = json.dump(2);
                            fwrite(str.data(), str.size(), 1, fp);
                            fclose(fp);
                        }
                        delete from_dat;
                        delete from_context;
                    });
                }
                #endif
                PathfindingWindow::ReadyForPathing();
            } break;
        }
    }

}

bool PathfindingWindow::ReadyForPathing()
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) 
        return false;
    const auto m = GetMilepathForCurrentMap();
    return m && m->ready();
}

void PathfindingWindow::Draw(IDirect3DDevice9*)
{
#ifndef _DEBUG
    return;
#endif
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

    const auto player = GW::Agents::GetObservingAgent();
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
    static GW::GamePos from;
    static GW::GamePos to;
    const auto pos = GetPlayerPos();
    ImGui::PushItemWidth(100.f);
    if (pos) {
        ImGui::TextUnformatted("Player: ");
        ImGui::SameLine();
        ImGui::InputFloat("##player_x", &pos->x, 1.f, 100.f, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::InputFloat("##player_y", &pos->y, 1.f, 100.f, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("From: ");
    ImGui::SameLine();
    ImGui::InputFloat("##from_x", &from.x, 1.f, 100.f, "%.3f");
    ImGui::SameLine();
    ImGui::InputFloat("##from_y", &from.y, 1.f, 100.f, "%.3f");
    ImGui::TextUnformatted("To: ");
    ImGui::SameLine();
    ImGui::InputFloat("##to_x", &to.x, 1.f, 100.f, "%.3f");
    ImGui::SameLine();
    ImGui::InputFloat("##to_y", &to.y, 1.f, 100.f, "%.3f");
    if (ImGui::Button("Find Path")) {
        RecalculatePath(from, to);
        pending_redraw = true;
    }
    if (!astar)
        return ImGui::End();
    ImGui::Text("Length: %.2f", astar->m_path.cost());
    const auto& points = astar->m_path.points();
    ImGui::Text("n points: %d", points.size());
    for (auto& p : points) {
        const auto& gp = static_cast<GW::GamePos>(p);
        ImGui::Text("%.2f, %.2f, %d", gp.x, gp.y, gp.zplane);
    }

    if (pending_redraw) {
        for (size_t i = 0; i < points.size() - 1; i++) {
            const auto& redraw_from = static_cast<GW::GamePos>(points[i]);
            const auto& redraw_to = static_cast<GW::GamePos>(points[i + 1]);
            const auto line = Minimap::Instance().custom_renderer.AddCustomLine(redraw_from, redraw_to);
            line->created_by_toolbox = true;
            minimap_lines.push_back(line);
        }
        pending_redraw = false;
        pending_undraw = TIMER_INIT() + 10000;
        Log::Flash("Path drawn on minimap");
    }
    if (pending_undraw && TIMER_INIT() > pending_undraw) {
        for (const auto line : minimap_lines) {
            Minimap::Instance().custom_renderer.RemoveCustomLine(line);
        }
        minimap_lines.clear();
        pending_undraw = 0;
    }
    ImGui::End();
}

void PathfindingWindow::SignalTerminate()
{
    ToolboxWindow::SignalTerminate();
    pending_terminate = true;
    GW::UI::RemoveUIMessageCallback(&gw_ui_hookentry);
    for (const auto mile_path : mile_paths_by_coords | std::views::values) {
        mile_path->stopProcessing();
    }
    mile_paths_by_coords.clear();
}

bool PathfindingWindow::CanTerminate()
{
    if (!pending_terminate && pending_worker_task)
        return false;
    for (const auto m : mile_paths_by_coords) {
        if (m.second->isProcessing())
            return false;
    }
    return true;
}

clock_t PathfindingWindow::CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args)
{
    if (pending_terminate)
        return 0;

    if (!ReadyForPathing())
        return 0;

    pending_worker_task = true;

    Resources::EnqueueWorkerTask([from, to, callback, args] {
        if (pending_terminate) {
            return;
        }

        const auto milepath = GetMilepathForCurrentMap();
        if (milepath && milepath->ready()) {
            auto astr = Pathing::AStar(milepath);

            const auto res = astr.Search(from, to);
            if (res != Pathing::Error::OK) {
                Log::Error("Pathing failed; Pathing::Error code %d", res);
                return;
            }
            if (!astr.m_path.ready()) {
                Log::Log("Pathing failed; astar.m_path not ready");
                return;
            }
            const auto& points = astr.m_path.points();
            auto waypoints = new std::vector<GW::GamePos>();
            waypoints->reserve(points.size());
            for (const auto& p : points) {
                waypoints->emplace_back(p);
            }

            Resources::EnqueueMainTask([waypoints, callback, args] {
                callback(*waypoints, args);
                delete waypoints;
            });
        }
        pending_worker_task = false;
    });
    return TIMER_INIT();
}

void PathfindingWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (const auto m : mile_paths_by_coords) {
        delete m.second; // Blocking
    }
    mile_paths_by_coords.clear();
    delete astar;
    astar = nullptr;
}

void PathfindingWindow::Initialize()
{
    ToolboxWindow::Initialize();

    GW::UI::RegisterUIMessageCallback(&gw_ui_hookentry, GW::UI::UIMessage::kLoadMapContext, OnUIMessage, 0x4000);
}
