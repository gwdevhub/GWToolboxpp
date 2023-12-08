#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/MapContext.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>

#include <Timer.h>
#include <ImGuiAddons.h>

#include <Windows/PathfindingWindow.h>
#include <Modules/Resources.h>
#include <Widgets/Minimap/Minimap.h>




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

    volatile bool pending_terminate = false;
    volatile bool pending_worker_task = false;

    bool pending_redraw = false;
    clock_t pending_undraw = 0;

    GW::HookEntry gw_ui_hookentry;

    std::vector<CustomRenderer::CustomLine*> minimap_lines;

    void OnMapLoaded(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        GetMilepathForCurrentMap();
    }

    GW::GamePos* GetPlayerPos() {
        const auto p = GW::Agents::GetPlayer();
        return p ? &p->pos : nullptr;
    }


    // Returns false if our last AStar calculation matches what we're asking for.
    bool NeedsRecalculating(GW::GamePos& from, GW::GamePos& to) {
        if (!(astar && astar->m_path.ready() && astar->m_path.points().size()))
            return true;
        return from != astar->m_path.points().at(0)
            || to != astar->m_path.points().at(astar->m_path.points().size() - 1);
    }



    void RecalculatePath(GW::GamePos& from, GW::GamePos& to) {
        if (!NeedsRecalculating(from, to))
            return;
        delete astar;
        astar = nullptr;
        Resources::EnqueueWorkerTask([from_cpy = from, to_cpy = to ]() {
            auto tmpAstar = new Pathing::AStar(GetMilepathForCurrentMap());
            const auto res = tmpAstar->search(from_cpy, to_cpy);
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
    void RecalculatePathTo(GW::GamePos& to) {
        const auto from = GetPlayerPos();
        if (!from) {
            return; // Invalid player position
        }
        RecalculatePath(*from, to);
    }
    clock_t last_recalculate = 0;
    void RecalculatePathToQuest() {
        last_recalculate = TIMER_INIT();
        const auto q = GW::QuestMgr::GetActiveQuest();
        if (!(q && q->marker.x)) return;

        RecalculatePathTo(q->marker);
    }

    void DrawPathOnMinimap() {
        if (!astar)
            return;

    }
}
bool PathfindingWindow::ReadyForPathing() {
    const auto m = GetMilepathForCurrentMap();
    return m && m->ready();
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
    static GW::GamePos from;
    static GW::GamePos to;
    const auto pos = GetPlayerPos();
    ImGui::PushItemWidth(100.f);
    if (pos) {
        ImGui::TextUnformatted("Player: ");
        ImGui::SameLine();
        ImGui::InputFloat("##player_x", &pos->x, 1.f, 100.f, "%.3f",ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::InputFloat("##player_y", &pos->y, 1.f, 100.f, "%.3f",ImGuiInputTextFlags_ReadOnly);
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
        RecalculatePath(from,to);
        pending_redraw = true;
    }
    if (!astar)
        return ImGui::End();
    ImGui::Text("Length: %.2f", astar->m_path.cost());
    const auto& points = astar->m_path.points();
    ImGui::Text("n points: %d", points.size());
    for (auto& p : points) {
        const auto& gp = static_cast<GW::GamePos>(p);
        ImGui::Text("%.2f, %.2f, %d", gp.x, gp.y,gp.zplane);
    }

    if (pending_redraw) {
        for (size_t i = 0; i < points.size() - 1; i++) {
            const auto& redraw_from = static_cast<GW::GamePos>(points[i]);
            const auto& redraw_to = static_cast<GW::GamePos>(points[i + 1]);
            const auto line = Minimap::Instance().custom_renderer.AddCustomLine(redraw_from, redraw_to);

            minimap_lines.push_back(line);
        }
        pending_redraw = false;
        pending_undraw = TIMER_INIT() + 10000;
        Log::Info("Path drawn on minimap");
    }
    if (pending_undraw && TIMER_INIT() > pending_undraw) {
        for (auto line : minimap_lines) {
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
    for (const auto m : mile_paths_by_map_file_id) {
        m.second->stopProcessing();
    }
}
bool PathfindingWindow::CanTerminate()
{
    if (pending_worker_task)
        return false;
    for (const auto m : mile_paths_by_map_file_id) {
        if (m.second->isProcessing())
            return false;
    }
    return true;
}
bool PathfindingWindow::CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args)
{
    if (pending_terminate)
        return false;

    if (!ReadyForPathing())
        return false;
    GW::GamePos* from_cpy = new GW::GamePos();
    memcpy(from_cpy, &from, sizeof(from));
    GW::GamePos* to_cpy = new GW::GamePos();
    memcpy(to_cpy, &to, sizeof(to));

    pending_worker_task = true;

    Resources::EnqueueWorkerTask([from_cpy, to_cpy, callback, args ]() {
        Pathing::MilePath* milepath = nullptr;
        Pathing::AStar* tmpAstar = nullptr;
        Pathing::Error res = Pathing::Error::OK;
        std::vector<GW::GamePos>* waypoints = new std::vector<GW::GamePos>();
        if (pending_terminate) {
            goto trigger_callback;
        }

        milepath = GetMilepathForCurrentMap();
        if (!(milepath && milepath->ready())) {
            goto trigger_callback;
        }
        tmpAstar = new Pathing::AStar(milepath);
        res = tmpAstar->search(*from_cpy, *to_cpy);
        if (res != Pathing::Error::OK) {
            Log::Error("Pathing failed; Pathing::Error code %d", res);
            goto trigger_callback;
        }
        if (!tmpAstar->m_path.ready()) {
            Log::Error("Pathing failed; tmpAstar->m_path not ready");
            goto trigger_callback;
        }
        for (auto& p : tmpAstar->m_path.points()) {
            waypoints->push_back(static_cast<GW::GamePos>(p));
        }

    trigger_callback:
        delete tmpAstar;
        delete from_cpy;
        delete to_cpy;
        Resources::EnqueueMainTask([waypoints, callback, args]() {
            callback(*waypoints,args);
            delete waypoints;

            });
        pending_worker_task = false;
        });
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
