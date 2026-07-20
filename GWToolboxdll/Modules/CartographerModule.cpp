#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Timer.h>
#include <Modules/CartographerModule.h>
#include <Modules/QuestModule.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/PathfindingWindow.h>

#ifdef _DEBUG

namespace {
    bool enabled = false;

    // The client stores world-map fog as one flat bitfield spanning the whole world map:
    // each cell is 32 world-map units (= 3072 gwinches) square, bit index = cell_y * width + cell_x.
    // Bits live in WorldContext::cartographed_areas; grid dims are the two dwords right after it
    // (Gw.exe Cartography_IsWorldMapPosExplored / Cartography_Sample3x3Flags, labelled in the Ghidra project).
    constexpr float kWorldMapUnitsPerCell = 32.f;

    struct CartoGrid {
        const uint32_t* bits = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t dword_count = 0;

        // Strict bit semantics matching the client's fog mesh builder: anything without a set
        // bit (out of grid, beyond the synced array, empty array) counts as UNexplored — the
        // game draws fog there, so must we. Target eligibility is gated separately.
        bool IsExplored(const int cx, const int cy) const
        {
            if (cx < 0 || cy < 0 || static_cast<uint32_t>(cx) >= width || static_cast<uint32_t>(cy) >= height) return false;
            const uint32_t bit = static_cast<uint32_t>(cy) * width + static_cast<uint32_t>(cx);
            if (!bits || bit / 32 >= dword_count) return false;
            return (bits[bit / 32] >> (bit % 32)) & 1;
        }
    };

    bool GetCartoGrid(CartoGrid& out)
    {
        const auto* world = GW::GetWorldContext();
        if (!world) return false;
        out.width = world->h05B4[0];
        out.height = world->h05B4[1];
        out.bits = reinterpret_cast<const uint32_t*>(world->cartographed_areas.m_buffer);
        out.dword_count = world->cartographed_areas.size();
        return out.width && out.height;
    }

    GW::Vec2f CellCenterWorldMap(const int cx, const int cy)
    {
        return {(cx + .5f) * kWorldMapUnitsPerCell, (cy + .5f) * kWorldMapUnitsPerCell};
    }

    float Dist2(const GW::Vec2f& a, const GW::Vec2f& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    constexpr float kGwinchesPerWorldMapUnit = 96.f;
    constexpr ImU32 kFogPointColor = IM_COL32(64, 220, 255, 255);
    constexpr ImU32 kTargetColor = IM_COL32(255, 190, 64, 255);

    ImU32 WithAlpha(const ImU32 color, const int alpha)
    {
        return (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha) << 24);
    }

    float Pulse()
    {
        const float t = static_cast<float>(TIMER_INIT()) / CLOCKS_PER_SEC;
        return 0.5f + 0.5f * sinf(t * (2.f * IM_PI) / 1.6f);
    }

    // World map coords have north at -y.
    const char* CompassDir(const GW::Vec2f& from, const GW::Vec2f& to)
    {
        static constexpr const char* dirs[] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
        const int idx = static_cast<int>(roundf(atan2f(to.y - from.y, to.x - from.x) / (IM_PI / 4.f)));
        return dirs[(idx + 8) % 8];
    }

    std::set<std::pair<int, int>> skipped_cells;
    std::set<std::pair<int, int>> declined_cells;
    std::vector<GW::Vec2f> custom_points;
    std::string declined_cells_str;
    std::string custom_points_str;

    void SerializeDeclined()
    {
        declined_cells_str.clear();
        for (const auto& [cx, cy] : declined_cells) {
            if (!declined_cells_str.empty()) declined_cells_str += ",";
            declined_cells_str += std::format("{}:{}", cx, cy);
        }
    }

    void ParseDeclined()
    {
        declined_cells.clear();
        std::istringstream is(declined_cells_str);
        std::string tok;
        while (std::getline(is, tok, ',')) {
            int cx, cy;
            if (sscanf_s(tok.c_str(), "%d:%d", &cx, &cy) == 2) declined_cells.insert({cx, cy});
        }
    }

    void SerializePoints()
    {
        custom_points_str.clear();
        for (const auto& p : custom_points) {
            if (!custom_points_str.empty()) custom_points_str += ",";
            custom_points_str += std::format("{:.1f}:{:.1f}", p.x, p.y);
        }
    }

    void ParsePoints()
    {
        custom_points.clear();
        std::istringstream is(custom_points_str);
        std::string tok;
        while (std::getline(is, tok, ',')) {
            float x, y;
            if (sscanf_s(tok.c_str(), "%f:%f", &x, &y) == 2) custom_points.push_back({x, y});
        }
    }

    bool FindNearestUnexploredCell(const CartoGrid& grid, const GW::Vec2f& player_wm, int& out_cx, int& out_cy, float& out_d2, bool& out_on_current_map)
    {
        float best_d2 = FLT_MAX;
        const auto consider = [&](const int cx, const int cy) {
            if (cx < 0 || cy < 0 || static_cast<uint32_t>(cx) >= grid.width || static_cast<uint32_t>(cy) >= grid.height) return;
            if (grid.IsExplored(cx, cy)) return;
            if (skipped_cells.contains({cx, cy})) return;
            if (declined_cells.contains({cx, cy})) return;
            const float d2 = Dist2(CellCenterWorldMap(cx, cy), player_wm);
            if (d2 < best_d2) {
                best_d2 = d2;
                out_cx = cx;
                out_cy = cy;
            }
        };

        ImRect bounds;
        const auto map_info = GW::Map::GetMapInfo(GW::Map::GetMapID());
        if (map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f) {
            const int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
            const int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
            const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
            const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
            for (int cy = y0; cy < y1; cy++) {
                for (int cx = x0; cx < x1; cx++) {
                    consider(cx, cy);
                }
            }
            if (best_d2 != FLT_MAX) {
                out_d2 = best_d2;
                out_on_current_map = true;
                return true;
            }
        }

        out_on_current_map = false;
        const int pcx = static_cast<int>(floorf(player_wm.x / kWorldMapUnitsPerCell));
        const int pcy = static_cast<int>(floorf(player_wm.y / kWorldMapUnitsPerCell));
        constexpr int max_radius = 256;
        const auto ring = [&](const int r) {
            for (int i = -r; i <= r; i++) {
                consider(pcx + i, pcy - r);
                consider(pcx + i, pcy + r);
                consider(pcx - r, pcy + i);
                consider(pcx + r, pcy + i);
            }
        };
        for (int r = 1; r <= max_radius; r++) {
            ring(r);
            if (best_d2 == FLT_MAX) continue;
            // Chebyshev rings aren't euclidean-ordered; expand until no farther ring can hold a closer center.
            for (int k = r + 1; k <= max_radius; k++) {
                const float min_d = (k - 1) * kWorldMapUnitsPerCell;
                if (min_d * min_d >= best_d2) break;
                ring(k);
            }
            out_d2 = best_d2;
            return true;
        }
        return false;
    }

    struct Target {
        bool valid = false;
        bool custom = false;
        int cx = 0;
        int cy = 0;
        GW::Vec2f wm{};
        bool on_map = true;
    };

    GW::Constants::MapID state_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType state_instance_type = GW::Constants::InstanceType::Loading;
    Target target;
    GW::GamePos goal_game{};
    GW::Vec2f player_wm_cached{};
    int map_fog_cells = -1;
    clock_t last_scan = 0;
    clock_t arrived_at = 0;
    clock_t map_settled_at = 0;
    clock_t marker_recheck_at = 0;
    bool arrived = false;
    bool self_changing_marker = false;
    bool marker_owned = false;
    GW::Vec2f last_marker_wm{};
    bool warned_no_data = false;
    bool warned_no_fog = false;
    bool warned_pathing_disabled = false;

    void SetMarkerAt(const GW::Vec2f& wm)
    {
        self_changing_marker = true;
        QuestModule::SetCustomQuestMarker(wm, true);
        self_changing_marker = false;
        marker_owned = true;
        last_marker_wm = wm;
    }

    void ClearMarker()
    {
        if (!marker_owned) return;
        marker_owned = false;
        self_changing_marker = true;
        QuestModule::ClearCustomQuestMarker();
        self_changing_marker = false;
    }

    void ClearTarget()
    {
        target = {};
        goal_game = {};
        arrived = false;
        arrived_at = 0;
        ClearMarker();
    }

    void ResetState()
    {
        target = {};
        goal_game = {};
        arrived_at = 0;
        arrived = false;
        warned_no_data = false;
        warned_no_fog = false;
        warned_pathing_disabled = false;
        map_fog_cells = -1;
        marker_recheck_at = 0;
        skipped_cells.clear();
        ClearMarker();
    }

    void UpdateMapFogCount(const CartoGrid& grid)
    {
        map_fog_cells = -1;
        ImRect bounds;
        const auto map_info = GW::Map::GetMapInfo(GW::Map::GetMapID());
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f)) return;
        const int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
        const int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
        const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        int count = 0;
        for (int cy = y0; cy < y1; cy++) {
            for (int cx = x0; cx < x1; cx++) {
                if (!grid.IsExplored(cx, cy)) count++;
            }
        }
        map_fog_cells = count;
    }

    void RemoveCustomPointAt(const GW::Vec2f& wm)
    {
        std::erase_if(custom_points, [&wm](const GW::Vec2f& p) { return Dist2(p, wm) < 1.f; });
        SerializePoints();
    }

    void SkipTargetImpl(const bool forever)
    {
        if (!target.valid) return;
        if (target.custom) {
            Log::Log("[cartographer] custom point (%.0f, %.0f) removed", target.wm.x, target.wm.y);
            RemoveCustomPointAt(target.wm);
        }
        else if (forever) {
            declined_cells.insert({target.cx, target.cy});
            SerializeDeclined();
            Log::Log("[cartographer] cell (%d, %d) declined forever", target.cx, target.cy);
        }
        else {
            skipped_cells.insert({target.cx, target.cy});
            Log::Log("[cartographer] cell (%d, %d) declined for this map", target.cx, target.cy);
        }
        ClearTarget();
    }

    void AddCustomPointImpl(const GW::Vec2f& wm)
    {
        custom_points.push_back(wm);
        SerializePoints();
        Log::Log("[cartographer] custom fog point added at wm(%.0f, %.0f)", wm.x, wm.y);
    }

    void OnCustomMarkerChanged()
    {
        if (self_changing_marker) return;
        if (!enabled) {
            marker_owned = false;
            return;
        }
        // Map transitions re-emit or drop the marker; only an in-map change by someone else is a takeover.
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded() || !GW::Agents::GetControlledCharacter()) return;
        if (marker_owned) {
            const auto quest = QuestModule::GetCustomQuestMarker();
            GW::Vec2f pos{};
            if (quest && QuestModule::GetCustomQuestMarkerWorldPos(quest->quest_id, pos) && Dist2(pos, last_marker_wm) < 1.f) {
                marker_recheck_at = 0;
                return;
            }
            // The marker gets re-emitted as clear+set around us (map loads, world map interactions);
            // judge ownership once the dust settles instead of yielding on the transient.
            if (!marker_recheck_at) marker_recheck_at = TIMER_INIT();
        }
        // Markers we never owned (e.g. a stale persisted marker re-emitted on map load) are
        // not a takeover — the next scan overwrites them with our target.
    }

    void BuildStatusText(char* buf, const size_t len)
    {
        if (!PathfindingWindow::IsPathingEnabled()) {
            snprintf(buf, len, "idle, the pathfinding module is disabled");
            return;
        }
        if (!target.valid || marker_recheck_at) {
            snprintf(buf, len, "%s", map_fog_cells == 0 ? "nothing left to uncover on this map" : "looking for unexplored areas...");
            return;
        }
        if (arrived && !target.custom) {
            snprintf(buf, len, "arrived near the fog - explore around, or skip this suggestion");
            return;
        }
        const float dist_k = sqrtf(Dist2(player_wm_cached, target.wm)) * kGwinchesPerWorldMapUnit / 1000.f;
        snprintf(buf, len, "heading to %s %.1fk units %s of you%s",
                 target.custom ? "your fog point," : "an unexplored area", dist_k, CompassDir(player_wm_cached, target.wm),
                 target.on_map ? "" : " (another map - follow the route)");
    }

    int FindCustomPointNear(const GW::Vec2f& wm, const float max_dist)
    {
        int best = -1;
        float best_d2 = max_dist * max_dist;
        for (size_t i = 0; i < custom_points.size(); i++) {
            const float d2 = Dist2(custom_points[i], wm);
            if (d2 <= best_d2) {
                best_d2 = d2;
                best = static_cast<int>(i);
            }
        }
        return best;
    }

    bool ContextMenuItems(const GW::Vec2f& click_wm, const float px_per_wm_unit)
    {
        bool keep_open = true;
        ImGui::PushID("carto_ctx");
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        const ImVec2 item_size = {250.f * ImGui::FontScale(), 0.f};
        if (!enabled) {
            if (ImGui::Button(ICON_FA_MAP_MARKED_ALT " Enable cartographer helper", item_size)) {
                GW::GameThread::Enqueue([] { CartographerModule::SetEnabled(true); });
                keep_open = false;
            }
        }
        else {
            ImGui::TextColored(ImColor(kTargetColor).Value, ICON_FA_MAP_MARKED_ALT " Cartographer");
            char status[160];
            BuildStatusText(status, sizeof(status));
            ImGui::TextDisabled("%s", status);
            if (map_fog_cells > 0) ImGui::TextDisabled("%d cappable fog cells left on this map", map_fog_cells);

            const float near_dist = px_per_wm_unit > 0.f ? 12.f / px_per_wm_unit : 8.f;
            if (FindCustomPointNear(click_wm, near_dist) >= 0) {
                if (ImGui::Button("Remove fog point", item_size)) {
                    CartographerModule::RemoveCustomPointNear(click_wm, near_dist);
                    keep_open = false;
                }
            }
            else if (ImGui::Button("Add fog point here", item_size)) {
                CartographerModule::AddCustomPoint(click_wm);
                keep_open = false;
            }
            if (target.valid) {
                if (ImGui::Button(target.custom ? "Remove current target point" : "Skip this suggestion", item_size)) {
                    CartographerModule::SkipCurrentTarget(false);
                    keep_open = false;
                }
                if (!target.custom && ImGui::Button("Never suggest this spot again", item_size)) {
                    CartographerModule::SkipCurrentTarget(true);
                    keep_open = false;
                }
            }
            if (custom_points.size() > 1) {
                char label[48];
                snprintf(label, sizeof(label), "Clear all %u fog points", static_cast<unsigned>(custom_points.size()));
                if (ImGui::Button(label, item_size)) {
                    CartographerModule::ClearCustomPoints();
                    keep_open = false;
                }
            }
            if (ImGui::Button("Disable cartographer helper", item_size)) {
                GW::GameThread::Enqueue([] { CartographerModule::SetEnabled(false); });
                keep_open = false;
            }
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();
        return keep_open;
    }

    bool OnMissionMapContextMenu()
    {
        return ContextMenuItems(MissionMapWidget::GetContextMenuWorldMapPos(), MissionMapWidget::GetPxPerWorldMapUnit());
    }

    bool OnWorldMapContextMenu()
    {
        return ContextMenuItems(WorldMapWidget::GetContextMenuWorldMapPos(), WorldMapWidget::GetPxPerWorldMapUnit());
    }

    void DrawFogPointMarker(ImDrawList* dl, const ImVec2& c, const bool is_current_target)
    {
        constexpr float r = 6.f;
        const ImVec2 pts[4] = {{c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}};
        dl->AddConvexPolyFilled(pts, 4, kFogPointColor);
        dl->AddPolyline(pts, 4, IM_COL32(10, 30, 40, 230), ImDrawFlags_Closed, 1.5f);
        if (is_current_target) {
            dl->AddCircle(c, r + 3.f + 3.f * Pulse(), WithAlpha(kFogPointColor, 200), 0, 2.f);
        }
    }

    using ProjectToScreen = bool(*)(const GW::Vec2f&, ImVec2&);

    void DrawMapOverlay(ImDrawList* dl, const ProjectToScreen project, const bool cell_tooltip)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const char* tooltip = nullptr;
        // While marker ownership is in question (user removed/moved it; yield pending) the
        // target visuals hide immediately so the removal feels instant.
        const bool target_active = target.valid && !marker_recheck_at;
        if (target_active && !target.custom) {
            ImVec2 cell_min, cell_max;
            if (project({target.cx * kWorldMapUnitsPerCell, target.cy * kWorldMapUnitsPerCell}, cell_min) &&
                project({(target.cx + 1) * kWorldMapUnitsPerCell, (target.cy + 1) * kWorldMapUnitsPerCell}, cell_max)) {
                const float pulse = Pulse();
                dl->AddRectFilled(cell_min, cell_max, WithAlpha(kTargetColor, 16 + static_cast<int>(24.f * pulse)));
                dl->AddRect(cell_min, cell_max, WithAlpha(kTargetColor, 210), 0.f, 0, 1.5f + pulse);
                if (cell_tooltip && ImRect(cell_min, cell_max).Contains(mouse)) {
                    tooltip = "Cartographer: nearest unexplored area\nRight-click the map for options";
                }
            }
        }
        for (const auto& p : custom_points) {
            ImVec2 c;
            if (!project(p, c)) continue;
            const bool is_current = target_active && target.custom && Dist2(target.wm, p) < 1.f;
            DrawFogPointMarker(dl, c, is_current);
            const float mdx = mouse.x - c.x;
            const float mdy = mouse.y - c.y;
            if (mdx * mdx + mdy * mdy < 12.f * 12.f) {
                tooltip = "Cartographer fog point\nRight-click nearby to remove it";
            }
        }
        if (tooltip) ImGui::SetTooltip("%s", tooltip);
    }

    void OnWorldMapOverlayDraw(ImDrawList* dl)
    {
        if (!enabled) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return WorldMapWidget::WorldMapToScreen(wm, out); }, true);
        char status[160];
        BuildStatusText(status, sizeof(status));
        char line[192];
        snprintf(line, sizeof(line), ICON_FA_MAP_MARKED_ALT " Cartographer: %s", status);
        dl->AddText({16.f, dl->GetClipRectMax().y - 68.f}, ImGui::GetColorU32(ImGuiCol_Text), line);
    }

    void OnMissionMapOverlayDraw(ImDrawList* dl)
    {
        if (!enabled) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return MissionMapWidget::WorldMapToScreen(wm, out); }, false);
    }
} // namespace

void CartographerModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::RegisterField(this, "enabled", &enabled);
    SettingsRegistry::RegisterField(this, "declined_cells", &declined_cells_str);
    SettingsRegistry::RegisterField(this, "custom_points", &custom_points_str);
    MissionMapWidget::AddContextMenuCallback(&OnMissionMapContextMenu);
    WorldMapWidget::AddContextMenuCallback(&OnWorldMapContextMenu);
    MissionMapWidget::AddOverlayCallback(&OnMissionMapOverlayDraw);
    WorldMapWidget::AddOverlayCallback(&OnWorldMapOverlayDraw);
    QuestModule::AddCustomMarkerChangedCallback(&OnCustomMarkerChanged);
}

void CartographerModule::SignalTerminate()
{
    MissionMapWidget::RemoveContextMenuCallback(&OnMissionMapContextMenu);
    WorldMapWidget::RemoveContextMenuCallback(&OnWorldMapContextMenu);
    MissionMapWidget::RemoveOverlayCallback(&OnMissionMapOverlayDraw);
    WorldMapWidget::RemoveOverlayCallback(&OnWorldMapOverlayDraw);
    QuestModule::RemoveCustomMarkerChangedCallback(&OnCustomMarkerChanged);
    enabled = false;
    ResetState();
}

void CartographerModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    ParseDeclined();
    ParsePoints();
}

void CartographerModule::Update(float)
{
    if (!enabled) {
        if (target.valid) ResetState();
        return;
    }
    const auto map_id = GW::Map::GetMapID();
    const auto instance_type = GW::Map::GetInstanceType();
    if (instance_type == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) {
        if (state_instance_type != GW::Constants::InstanceType::Loading) {
            ResetState();
            state_instance_type = GW::Constants::InstanceType::Loading;
        }
        return;
    }
    if (map_id != state_map_id || instance_type != state_instance_type) {
        ResetState();
        state_map_id = map_id;
        state_instance_type = instance_type;
        map_settled_at = TIMER_INIT();
    }
    // Coordinate anchors can be transitional right after a map change; let them settle.
    if (TIMER_DIFF(map_settled_at) < 2000) return;

    if (marker_recheck_at) {
        // Ownership in question: freeze scanning/re-placing until resolved, so a user removing
        // or moving the marker never has it snapped back by a racing target change.
        if (TIMER_DIFF(marker_recheck_at) <= 1500) return;
        marker_recheck_at = 0;
        const auto quest = QuestModule::GetCustomQuestMarker();
        GW::Vec2f pos{};
        const bool still_ours = marker_owned && quest && QuestModule::GetCustomQuestMarkerWorldPos(quest->quest_id, pos) && Dist2(pos, last_marker_wm) < 1.f;
        if (!still_ours) {
            Log::Log("[cartographer] quest marker changed externally (quest=%d wm=%.0f,%.0f vs ours %.0f,%.0f); cartographer disabled",
                     quest ? 1 : 0, pos.x, pos.y, last_marker_wm.x, last_marker_wm.y);
            marker_owned = false;
            enabled = false;
            ResetState();
            return;
        }
    }

    const auto map_info = GW::Map::GetMapInfo(map_id);
    if (!map_info || !map_info->GetIsOnWorldMap()) return;

    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) return;

    if (TIMER_DIFF(last_scan) < 1000) return;
    last_scan = TIMER_INIT();

    if (!PathfindingWindow::IsPathingEnabled()) {
        if (!warned_pathing_disabled) {
            warned_pathing_disabled = true;
            Log::Log("[cartographer] pathfinding module is disabled; cartographer idle");
        }
        return;
    }
    warned_pathing_disabled = false;
    if (!PathfindingWindow::ReadyForPathing()) return;

    CartoGrid grid;
    if (!GetCartoGrid(grid)) {
        if (!warned_no_data) {
            warned_no_data = true;
            Log::Log("[cartographer] no cartography data available");
        }
        return;
    }

    GW::Vec2f player_wm;
    if (!WorldMapWidget::GamePosToWorldMap(player->pos, player_wm)) return;
    player_wm_cached = player_wm;
    UpdateMapFogCount(grid);

    if (target.valid && target.on_map && GW::GetSquareDistance(player->pos, goal_game) < 150.f * 150.f) {
        if (target.custom) {
            Log::Log("[cartographer] reached custom point wm(%.0f, %.0f)", target.wm.x, target.wm.y);
            RemoveCustomPointAt(target.wm);
            ClearTarget();
        }
        else if (!arrived) {
            arrived = true;
            arrived_at = TIMER_INIT();
            Log::Log("[cartographer] reached closest walkable point toward cell (%d, %d)", target.cx, target.cy);
        }
    }

    if (arrived && target.valid && !target.custom && TIMER_DIFF(arrived_at) > 8000 && !grid.IsExplored(target.cx, target.cy)) {
        Log::Log("[cartographer] cell (%d, %d) still foggy after arrival; skipping it for this map", target.cx, target.cy);
        skipped_cells.insert({target.cx, target.cy});
        ClearTarget();
    }

    Target cand{};
    float cand_d2 = FLT_MAX;
    for (const auto& p : custom_points) {
        const float d2 = Dist2(p, player_wm);
        if (d2 < cand_d2) {
            cand = {true, true, 0, 0, p};
            cand_d2 = d2;
        }
    }
    bool on_current_map = true;
    if (cand.valid) {
        ImRect bounds;
        on_current_map = GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.Contains({cand.wm.x, cand.wm.y});
    }
    else {
        int cx = 0, cy = 0;
        float d2 = FLT_MAX;
        if (FindNearestUnexploredCell(grid, player_wm, cx, cy, d2, on_current_map)) {
            cand = {true, false, cx, cy, CellCenterWorldMap(cx, cy)};
            cand_d2 = d2;
        }
    }
    if (!cand.valid) {
        if (!warned_no_fog) {
            warned_no_fog = true;
            Log::Log("[cartographer] no unexplored cells within search radius");
        }
        if (target.valid) ClearTarget();
        return;
    }
    warned_no_fog = false;

    bool same = target.valid && target.custom == cand.custom
        && (cand.custom ? Dist2(target.wm, cand.wm) < 1.f : (target.cx == cand.cx && target.cy == cand.cy));
    if (target.valid && !same && !(cand.custom && !target.custom)) {
        // Hysteresis: keep the current target unless it became ineligible or the candidate is meaningfully closer.
        const bool current_eligible = target.custom
            ? std::ranges::any_of(custom_points, [&](const GW::Vec2f& p) { return Dist2(p, target.wm) < 1.f; })
            : !grid.IsExplored(target.cx, target.cy) && !skipped_cells.contains({target.cx, target.cy}) && !declined_cells.contains({target.cx, target.cy});
        if (current_eligible && cand_d2 >= 0.7f * Dist2(target.wm, player_wm)) same = true;
    }
    if (same) return;

    GW::GamePos gp{};
    GW::Vec2f marker_wm = cand.wm;
    if (on_current_map) {
        if (!WorldMapWidget::WorldMapToGamePos(cand.wm, gp)) return;
        // The marker goes on the closest walkable spot toward the fog, not into the void.
        Pathing::FindClosestPositionOnTrapezoid(gp);
        if (!WorldMapWidget::GamePosToWorldMap(gp, marker_wm)) return;
    }
    // Off-map targets keep the raw position: QuestModule resolves the destination map from it
    // and plots the cross-map route on the world map, same as a manually placed marker.
    cand.on_map = on_current_map;
    target = cand;
    goal_game = gp;
    arrived = false;
    SetMarkerAt(marker_wm);
    if (target.custom) {
        if (on_current_map) Log::Log("[cartographer] target: custom point wm(%.0f, %.0f), marker at game(%.0f, %.0f)", target.wm.x, target.wm.y, gp.x, gp.y);
        else Log::Log("[cartographer] target: custom point wm(%.0f, %.0f) on another map; marker set there for cross-map routing", target.wm.x, target.wm.y);
    }
    else if (on_current_map) {
        Log::Log("[cartographer] fog target: cell (%d, %d) wm(%.0f, %.0f), marker at game(%.0f, %.0f)", target.cx, target.cy, target.wm.x, target.wm.y, gp.x, gp.y);
    }
    else {
        Log::Log("[cartographer] fog target: cell (%d, %d) wm(%.0f, %.0f) on another map; marker set there for cross-map routing", target.cx, target.cy, target.wm.x, target.wm.y);
    }
}

void CartographerModule::DrawSettingsInternal()
{
    ImGui::TextDisabled("Debug tool: every second, suggests the nearest unexplored (foggy) world-map cell and\nplaces the custom quest marker on the closest walkable spot toward it - the regular\nquest path leads you there. The suggestion, your queued fog points and a status line\nare drawn on the world map and mission map; right-click either map to manage the\nhelper (toggle, add/remove fog points, skip suggestions).");
    bool on = enabled;
    if (ImGui::Checkbox("Enabled", &on)) {
        GW::GameThread::Enqueue([on] { SetEnabled(on); });
    }
    ImGui::Text("Declined forever: %u cells", static_cast<unsigned>(declined_cells.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##declined")) ClearDeclined();
    ImGui::Text("Custom fog points: %u", static_cast<unsigned>(custom_points.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##points")) ClearCustomPoints();
}

void CartographerModule::SetEnabled(const bool on)
{
    if (enabled == on) return;
    enabled = on;
    if (!on) ResetState();
    Log::Log("[cartographer] %s", on ? "enabled" : "disabled");
}

void CartographerModule::OnUserMarkerAction()
{
    if (!enabled) return;
    enabled = false;
    marker_owned = false;
    GW::GameThread::Enqueue([] {
        ResetState();
        Log::Log("[cartographer] user placed/removed the quest marker; cartographer disabled");
    });
}

bool CartographerModule::GetCurrentTargetWorldPos(GW::Vec2f& out)
{
    if (!target.valid) return false;
    out = target.wm;
    return true;
}





void CartographerModule::SkipCurrentTarget(const bool forever)
{
    GW::GameThread::Enqueue([forever] {
        SkipTargetImpl(forever);
    });
}

void CartographerModule::AddCustomPoint(const GW::Vec2f& world_map_pos)
{
    GW::GameThread::Enqueue([world_map_pos] {
        AddCustomPointImpl(world_map_pos);
    });
}

void CartographerModule::RemoveCustomPointNear(const GW::Vec2f& world_map_pos, const float max_dist_wm)
{
    GW::GameThread::Enqueue([world_map_pos, max_dist_wm] {
        const int idx = FindCustomPointNear(world_map_pos, max_dist_wm);
        if (idx < 0) return;
        const GW::Vec2f p = custom_points[idx];
        const bool was_target = target.valid && target.custom && Dist2(target.wm, p) < 1.f;
        custom_points.erase(custom_points.begin() + idx);
        SerializePoints();
        if (was_target) ClearTarget();
        Log::Log("[cartographer] fog point (%.0f, %.0f) removed", p.x, p.y);
    });
}

void CartographerModule::ClearCustomPoints()
{
    GW::GameThread::Enqueue([] {
        custom_points.clear();
        SerializePoints();
        if (target.valid && target.custom) ClearTarget();
        Log::Log("[cartographer] custom fog points cleared");
    });
}

void CartographerModule::ClearDeclined()
{
    GW::GameThread::Enqueue([] {
        declined_cells.clear();
        skipped_cells.clear();
        SerializeDeclined();
        if (target.valid) ClearTarget();
        Log::Log("[cartographer] declined cells cleared");
    });
}

void CartographerModule::GetStatus(char* buf, const size_t len)
{
    const char* pathing = !PathfindingWindow::IsPathingEnabled() ? "off" : PathfindingWindow::ReadyForPathing() ? "ready" : "prewarming";
    char target_desc[64];
    if (!target.valid) snprintf(target_desc, sizeof(target_desc), "none");
    else if (target.custom) snprintf(target_desc, sizeof(target_desc), "point(%.0f,%.0f)", target.wm.x, target.wm.y);
    else snprintf(target_desc, sizeof(target_desc), "cell(%d,%d)", target.cx, target.cy);
    snprintf(buf, len, "carto: enabled=%d pathing=%s target=%s marker=%d arrived=%d skipped=%u declined=%u points=%u fogcells=%d",
             enabled, pathing, target_desc, marker_owned, arrived,
             static_cast<unsigned>(skipped_cells.size()), static_cast<unsigned>(declined_cells.size()), static_cast<unsigned>(custom_points.size()), map_fog_cells);
}

#else

void CartographerModule::Initialize() { ToolboxModule::Initialize(); }
void CartographerModule::SignalTerminate() {}
void CartographerModule::Update(float) {}
void CartographerModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) { ToolboxModule::LoadSettings(doc, legacy); }
void CartographerModule::DrawSettingsInternal() {}
void CartographerModule::SetEnabled(bool) {}
void CartographerModule::OnUserMarkerAction() {}
bool CartographerModule::GetCurrentTargetWorldPos(GW::Vec2f&) { return false; }

#endif
