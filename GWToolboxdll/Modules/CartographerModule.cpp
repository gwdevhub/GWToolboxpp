#include "stdafx.h"

#include <map>

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
#define CARTO_LOG(...) Log::Log(__VA_ARGS__)
#else
#define CARTO_LOG(...) ((void)0)
#endif

namespace {
    bool enabled = true;

    // Gw.exe's fog mesh builder is handed WorldContext::cartographed_areas (+0x5A4) and h05B4
    // (+0x5B4, the grid dims): one bit per 32x32-world-map-unit cell, addressed as below.
    constexpr float kWorldMapUnitsPerCell = 32.f;

    // The fog mesh builder strides rows by (width >> 5) words while the explored-query indexes
    // bits flat as cy * width + cx; the client uses both interchangeably, so width is always a
    // multiple of 32 and either form works.
    uint32_t RowWords(const uint32_t width)
    {
        return width >> 5;
    }

    struct CartoGrid {
        const uint32_t* bits = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t dword_count = 0;

        bool InGrid(const int cx, const int cy) const
        {
            return cx >= 0 && cy >= 0 && static_cast<uint32_t>(cx) < width && static_cast<uint32_t>(cy) < height;
        }

        // Anything without a set bit - off-grid, past the synced array - is unexplored, because
        // that is what the game fogs.
        bool IsExplored(const int cx, const int cy) const
        {
            if (!InGrid(cx, cy)) return false;
            const uint32_t word = static_cast<uint32_t>(cy) * RowWords(width) + (static_cast<uint32_t>(cx) >> 5);
            if (!bits || word >= dword_count) return false;
            return (bits[word] >> (static_cast<uint32_t>(cx) & 31)) & 1;
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
    constexpr ImU32 kStandColor = IM_COL32(255, 236, 170, 255);
    constexpr ImU32 kFogColor = IM_COL32(0x50, 0xFF, 0x78, 255);
    constexpr ImU32 kGridColor = IM_COL32(255, 255, 255, 40);

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

    bool show_fog = true;
    bool show_stand_cells = true;
    bool show_grid = false;
    bool using_bec = false;

    // Standing in a tile credits it plus the ring around it; a Bird's Eye Compass widens that to
    // three rings. Chebyshev throughout, and where in the tile you stand makes no difference.
    constexpr int kRevealRadius = 1;
    constexpr int kRevealRadiusBec = 3;

    int RevealRadius()
    {
        return using_bec ? kRevealRadiusBec : kRevealRadius;
    }

    // Slivers that a wide-range visit failed to credit. BEC range misses a few tiles that only
    // normal range uncovers, so these stop counting for anything further than one tile away.
    std::set<std::pair<int, int>> strict_fog_cells;

    bool CellCreditableFrom(const int dx, const int dy, const int fx, const int fy)
    {
        if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) return true;
        return !strict_fog_cells.contains({fx, fy});
    }

    struct StandCell {
        bool walkable = false;
        GW::GamePos pos{}; // somewhere inside the cell you can actually stand
        int reveals = 0;   // still-foggy cells this spot would credit
    };
    std::map<std::pair<int, int>, StandCell> stand_cells;
    bool sweep_complete = false;
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

    // Fog nothing on this map can reach; excluded from the overlay so it only shows actionable fog.
    int unreachable_fog_cells = 0;

    // The client's fog texture is this many texels per cartography cell, so the visible fog is
    // four times finer than the 32-unit grid the bits live on.
    constexpr int kFogSubdivisions = 4;

    // Corner alphas are baked on the game thread so the overlay never reads the live bitmap.
    struct FogCell {
        int cx = 0, cy = 0;
        uint8_t corner_alpha[kFogSubdivisions + 1][kFogSubdivisions + 1] = {};
    };
    std::vector<FogCell> fog_cells;
    int map_fog_cells = -1;
    std::pair<int, int> map_cell_min{}, map_cell_max{};

    // Everything counts as coverable until the sweep finishes, so the overlay does not blink
    // cells out and back in as probing progresses.
    bool FogCellCoverable(const int cx, const int cy)
    {
        if (!sweep_complete) return true;
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (!CellCreditableFrom(dx, dy, cx, cy)) continue;
                const auto it = stand_cells.find({cx + dx, cy + dy});
                if (it != stand_cells.end() && it->second.walkable) return true;
            }
        }
        return false;
    }

    constexpr float kFogMaxAlpha = 135.f;

    float ExploredAtCorner(const CartoGrid& grid, const int cx, const int cy)
    {
        return (static_cast<float>(grid.IsExplored(cx - 1, cy - 1)) + static_cast<float>(grid.IsExplored(cx, cy - 1)) +
                static_cast<float>(grid.IsExplored(cx - 1, cy)) + static_cast<float>(grid.IsExplored(cx, cy))) * 0.25f;
    }

    // The client averages the four cells meeting at a corner, then bakes that field into a fog
    // texture at kFogSubdivisions texels per cell, 4 bits each - which is why the fog on screen
    // steps at a quarter of a cell and bands rather than ramping smoothly.
    void BakeFogCell(const CartoGrid& grid, FogCell& out)
    {
        const float tl = ExploredAtCorner(grid, out.cx, out.cy);
        const float tr = ExploredAtCorner(grid, out.cx + 1, out.cy);
        const float bl = ExploredAtCorner(grid, out.cx, out.cy + 1);
        const float br = ExploredAtCorner(grid, out.cx + 1, out.cy + 1);
        for (int j = 0; j <= kFogSubdivisions; j++) {
            const float v = static_cast<float>(j) / kFogSubdivisions;
            for (int i = 0; i <= kFogSubdivisions; i++) {
                const float u = static_cast<float>(i) / kFogSubdivisions;
                const float explored = (tl + (tr - tl) * u) * (1.f - v) + (bl + (br - bl) * u) * v;
                const float quantised = roundf((1.f - explored) * 15.f) / 15.f;
                out.corner_alpha[j][i] = static_cast<uint8_t>(kFogMaxAlpha * quantised);
            }
        }
    }


    // Coastlines ignore the cell grid, so sample the whole cell: one that is mostly cliff but
    // clips a walkable ledge is still somewhere you can go.
    bool ProbeStandCell(const int cx, const int cy, GW::GamePos& out)
    {
        // Fine enough to catch the shoreline slivers that are often the only footing near fog.
        constexpr int kSamples = 6;
        bool found = false;
        float best_d2 = FLT_MAX;
        const GW::Vec2f centre = CellCenterWorldMap(cx, cy);
        for (int sy = 0; sy < kSamples; sy++) {
            for (int sx = 0; sx < kSamples; sx++) {
                const GW::Vec2f wm{
                    (cx + (sx + 0.5f) / kSamples) * kWorldMapUnitsPerCell,
                    (cy + (sy + 0.5f) / kSamples) * kWorldMapUnitsPerCell,
                };
                GW::GamePos gp{};
                if (!WorldMapWidget::WorldMapToGamePos(wm, gp) || !Pathing::IsPositionWalkable(gp)) continue;
                // Deepest footing wins; near a border the server may credit the neighbour instead.
                const float d2 = Dist2(wm, centre);
                if (!found || d2 < best_d2) {
                    best_d2 = d2;
                    out = gp;
                    found = true;
                }
            }
        }
        return found;
    }

    // Keeps the sweep to the fog's fringe instead of probing every cell on the map.
    bool CellWorthProbing(const CartoGrid& grid, const int cx, const int cy)
    {
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (grid.InGrid(cx + dx, cy + dy) && !grid.IsExplored(cx + dx, cy + dy)) return true;
            }
        }
        return false;
    }

    // Whether a cell is standable never changes within a map, so probe it once and keep it.
    void SweepStandCells(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds))) return;
        // Only this map's squares are standable; the fog they credit may still be the next map's.
        const int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
        const int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
        const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        int budget = 6;
        for (int cy = y0; cy < y1 && budget > 0; cy++) {
            for (int cx = x0; cx < x1 && budget > 0; cx++) {
                if (stand_cells.contains({cx, cy})) continue;
                if (!CellWorthProbing(grid, cx, cy)) continue;
                StandCell sc;
                sc.walkable = ProbeStandCell(cx, cy, sc.pos);
                stand_cells[{cx, cy}] = sc;
                budget--;
            }
        }
        sweep_complete = budget > 0;
    }

    void RecomputeCoverage(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        const int r = RevealRadius();
        for (auto& [cell, sc] : stand_cells) {
            sc.reveals = 0;
            if (!sc.walkable) continue;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    const int fx = cell.first + dx;
                    const int fy = cell.second + dy;
                    if (!grid.InGrid(fx, fy) || grid.IsExplored(fx, fy)) continue;
                    if (!CellCreditableFrom(dx, dy, fx, fy)) continue;
                    sc.reveals++;
                }
            }
        }

        unreachable_fog_cells = 0;
        fog_cells.clear();
        map_fog_cells = -1;
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f)) return;
        const int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
        const int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
        const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        map_cell_min = {x0, y0};
        map_cell_max = {x1, y1};
        for (int cy = y0; cy < y1; cy++) {
            for (int cx = x0; cx < x1; cx++) {
                if (!grid.InGrid(cx, cy) || grid.IsExplored(cx, cy)) continue;
                if (!FogCellCoverable(cx, cy)) {
                    unreachable_fog_cells++;
                    continue;
                }
                FogCell f;
                f.cx = cx;
                f.cy = cy;
                BakeFogCell(grid, f);
                fog_cells.push_back(f);
            }
        }
        map_fog_cells = static_cast<int>(fog_cells.size());
    }

    struct Target {
        bool valid = false;
        bool custom = false;
        // For fog targets these are the cell to go and stand in, not the cell being uncovered.
        int cx = 0;
        int cy = 0;
        int reveals = 0;
        GW::Vec2f wm{};
        bool on_map = true;
    };

    GW::Constants::MapID state_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType state_instance_type = GW::Constants::InstanceType::Loading;
    Target target;
    GW::GamePos goal_game{};
    GW::Vec2f player_wm_cached{};
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
        stand_cells.clear();
        fog_cells.clear();
        unreachable_fog_cells = 0;
        strict_fog_cells.clear();
        map_cell_min = map_cell_max = {};
        sweep_complete = false;
        ClearMarker();
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
            CARTO_LOG("[cartographer] custom point (%.0f, %.0f) removed", target.wm.x, target.wm.y);
            RemoveCustomPointAt(target.wm);
        }
        else if (forever) {
            declined_cells.insert({target.cx, target.cy});
            SerializeDeclined();
            CARTO_LOG("[cartographer] stand cell (%d, %d) declined forever", target.cx, target.cy);
        }
        else {
            skipped_cells.insert({target.cx, target.cy});
            CARTO_LOG("[cartographer] stand cell (%d, %d) declined for this map", target.cx, target.cy);
        }
        ClearTarget();
    }

    void AddCustomPointImpl(const GW::Vec2f& wm)
    {
        custom_points.push_back(wm);
        SerializePoints();
        CARTO_LOG("[cartographer] custom fog point added at wm(%.0f, %.0f)", wm.x, wm.y);
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
            const char* idle = map_fog_cells == 0 ? "nothing left to uncover on this map"
                : !sweep_complete ? "scanning this map's fog..."
                : "nothing reachable left here - travel on, or add a fog point";
            snprintf(buf, len, "%s", idle);
            return;
        }
        if (arrived && !target.custom) {
            snprintf(buf, len, "standing in the right tile - if it does not register, take a step or click-walk");
            return;
        }
        const float dist_k = sqrtf(Dist2(player_wm_cached, target.wm)) * kGwinchesPerWorldMapUnit / 1000.f;
        if (target.custom) {
            snprintf(buf, len, "heading to your fog point, %.1fk units %s of you%s", dist_k, CompassDir(player_wm_cached, target.wm),
                     target.on_map ? "" : " (another map - follow the route)");
            return;
        }
        snprintf(buf, len, "stand in the square %.1fk units %s of you to uncover %d %s%s", dist_k,
                 CompassDir(player_wm_cached, target.wm), target.reveals, target.reveals == 1 ? "square" : "squares",
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
        if (!enabled) return true;
        bool keep_open = true;
        ImGui::PushID("carto_ctx");
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        const ImVec2 item_size = {250.f * ImGui::FontScale(), 0.f};
        {
            ImGui::TextColored(ImColor(kTargetColor).Value, ICON_FA_MAP_MARKED_ALT " Cartographer");
            char status[160];
            BuildStatusText(status, sizeof(status));
            ImGui::TextDisabled("%s", status);
            if (map_fog_cells > 0) ImGui::TextDisabled("%d squares left to uncover on this map", map_fog_cells);

            const float near_dist = px_per_wm_unit > 0.f ? 12.f / px_per_wm_unit : 8.f;
            const int point_here = FindCustomPointNear(click_wm, near_dist);
            // Clicking the suggestion itself means acting on it — offering to drop a fog point
            // on the very spot already being suggested is nonsense.
            const bool on_suggestion = target.valid && !target.custom
                && static_cast<int>(floorf(click_wm.x / kWorldMapUnitsPerCell)) == target.cx
                && static_cast<int>(floorf(click_wm.y / kWorldMapUnitsPerCell)) == target.cy;
            if (target.valid && (on_suggestion || (target.custom && point_here >= 0))) {
                if (ImGui::Button(target.custom ? "Remove this fog point" : "Skip this suggestion", item_size)) {
                    CartographerModule::SkipCurrentTarget(false);
                    keep_open = false;
                }
                if (!target.custom && ImGui::Button("Never suggest this spot again", item_size)) {
                    CartographerModule::SkipCurrentTarget(true);
                    keep_open = false;
                }
            }
            else if (point_here >= 0) {
                if (ImGui::Button("Remove fog point", item_size)) {
                    CartographerModule::RemoveCustomPointNear(click_wm, near_dist);
                    keep_open = false;
                }
            }
            else {
                if (ImGui::Button("Add fog point here", item_size)) {
                    CartographerModule::AddCustomPoint(click_wm);
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
#ifdef _DEBUG
            if (ImGui::Button("Log what the helper sees here", item_size)) {
                const GW::Vec2f at = click_wm;
                GW::GameThread::Enqueue([at] {
                    CartoGrid g;
                    if (!GetCartoGrid(g)) {
                        Log::Log("[cartographer] probe: no cartography data");
                        return;
                    }
                    const int cx = static_cast<int>(floorf(at.x / kWorldMapUnitsPerCell));
                    const int cy = static_cast<int>(floorf(at.y / kWorldMapUnitsPerCell));
                    GW::GamePos gp{};
                    const bool converted = WorldMapWidget::WorldMapToGamePos(at, gp);
                    const auto stand = stand_cells.find({cx, cy});
                    Log::Log("[cartographer] probe wm(%.0f,%.0f) cell(%d,%d): explored=%d, grid %ux%u (%u words/row), game(%.0f,%.0f), standable here=%d, probed=%d walkable=%d reveals=%d, coverable=%d, radius=%d",
                             at.x, at.y, cx, cy, static_cast<int>(g.IsExplored(cx, cy)),
                             g.width, g.height, RowWords(g.width),
                             gp.x, gp.y, converted && Pathing::IsPositionWalkable(gp),
                             static_cast<int>(stand != stand_cells.end()),
                             stand != stand_cells.end() ? static_cast<int>(stand->second.walkable) : 0,
                             stand != stand_cells.end() ? stand->second.reveals : 0,
                             static_cast<int>(FogCellCoverable(cx, cy)), RevealRadius());
                    Log::FlushFile();
                });
                keep_open = false;
            }
#endif
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

    bool ProjectCell(const ProjectToScreen project, const int cx, const int cy, ImVec2& min_out, ImVec2& max_out)
    {
        return project({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, min_out) &&
            project({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, max_out);
    }

    // One quad per fog texel, so ImGui interpolates them as the GPU does when it samples the
    // client's texture.
    void DrawFog(ImDrawList* dl, const ProjectToScreen project)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        for (const auto& f : fog_cells) {
            ImVec2 cell_min, cell_max;
            if (!ProjectCell(project, f.cx, f.cy, cell_min, cell_max)) continue;
            if (!clip.Overlaps(ImRect(cell_min, cell_max))) continue;
            const float w = (cell_max.x - cell_min.x) / kFogSubdivisions;
            const float h = (cell_max.y - cell_min.y) / kFogSubdivisions;
            for (int j = 0; j < kFogSubdivisions; j++) {
                for (int i = 0; i < kFogSubdivisions; i++) {
                    const ImVec2 t_min{cell_min.x + i * w, cell_min.y + j * h};
                    const ImVec2 t_max{t_min.x + w, t_min.y + h};
                    dl->AddRectFilledMultiColor(t_min, t_max,
                                                WithAlpha(kFogColor, f.corner_alpha[j][i]), WithAlpha(kFogColor, f.corner_alpha[j][i + 1]),
                                                WithAlpha(kFogColor, f.corner_alpha[j + 1][i + 1]), WithAlpha(kFogColor, f.corner_alpha[j + 1][i]));
                }
            }
        }
    }

    // The cartography grid itself. Every tile is credited as a unit, so seeing the boundaries is
    // what makes "stand in that tile" actionable.
    void DrawGrid(ImDrawList* dl, const ProjectToScreen project)
    {
        const auto [x0, y0] = map_cell_min;
        const auto [x1, y1] = map_cell_max;
        if (x1 <= x0 || y1 <= y0) return;
        ImVec2 origin, corner;
        if (!ProjectCell(project, x0, y0, origin, corner)) return;
        const float step_x = corner.x - origin.x;
        const float step_y = corner.y - origin.y;
        if (step_x < 3.f || step_y < 3.f) return; // denser than this is a smear, not a grid
        // Both projections are affine, so the far edge follows from the step rather than a second
        // projection that could fail on its own.
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        const float top = std::max(origin.y, clip.Min.y);
        const float bottom = std::min(origin.y + (y1 - y0) * step_y, clip.Max.y);
        const float left = std::max(origin.x, clip.Min.x);
        const float right = std::min(origin.x + (x1 - x0) * step_x, clip.Max.x);
        for (int cx = x0; cx <= x1; cx++) {
            const float x = origin.x + (cx - x0) * step_x;
            if (x >= clip.Min.x && x <= clip.Max.x) dl->AddLine({x, top}, {x, bottom}, kGridColor);
        }
        for (int cy = y0; cy <= y1; cy++) {
            const float y = origin.y + (cy - y0) * step_y;
            if (y >= clip.Min.y && y <= clip.Max.y) dl->AddLine({left, y}, {right, y}, kGridColor);
        }
    }

    // Drawn at true 32x32 size, shaded by how much fog the spot would credit.
    void DrawStandCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, const char*& tooltip)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        for (const auto& [cell, sc] : stand_cells) {
            if (!sc.walkable || sc.reveals <= 0) continue;
            if (declined_cells.contains(cell)) continue;
            // Skipped only while the suggestion is actually drawn on top, else a pending ownership
            // recheck blanks the square entirely.
            if (target.valid && !marker_recheck_at && !target.custom && target.cx == cell.first && target.cy == cell.second) continue;
            ImVec2 cell_min, cell_max;
            if (!ProjectCell(project, cell.first, cell.second, cell_min, cell_max)) continue;
            if (!clip.Overlaps(ImRect(cell_min, cell_max))) continue;
            const int strength = std::min(sc.reveals, 9);
            dl->AddRectFilled(cell_min, cell_max, WithAlpha(kStandColor, 10 + 6 * strength));
            dl->AddRect(cell_min, cell_max, WithAlpha(kStandColor, 60 + 12 * strength), 0.f, 0, 1.f);
            if (ImRect(cell_min, cell_max).Contains(mouse)) {
                tooltip = "Cartographer: stand here to uncover nearby squares";
            }
        }
    }

    void DrawMapOverlay(ImDrawList* dl, const ProjectToScreen project, const bool cell_tooltip)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const char* tooltip = nullptr;
        if (show_fog) {
            DrawFog(dl, project);
        }
        if (show_grid) {
            DrawGrid(dl, project);
        }
        if (show_stand_cells) {
            const char* stand_tooltip = nullptr;
            DrawStandCells(dl, project, mouse, stand_tooltip);
            if (cell_tooltip) tooltip = stand_tooltip;
        }
        // While marker ownership is in question (user removed/moved it; yield pending) the
        // target visuals hide immediately so the removal feels instant.
        const bool target_active = target.valid && !marker_recheck_at;
        if (target_active && !target.custom) {
            ImVec2 cell_min, cell_max;
            if (ProjectCell(project, target.cx, target.cy, cell_min, cell_max)) {
                const float pulse = Pulse();
                dl->AddRectFilled(cell_min, cell_max, WithAlpha(kTargetColor, 16 + static_cast<int>(24.f * pulse)));
                dl->AddRect(cell_min, cell_max, WithAlpha(kTargetColor, 210), 0.f, 0, 1.5f + pulse);
                if (cell_tooltip && ImRect(cell_min, cell_max).Contains(mouse)) {
                    tooltip = "Cartographer: stand in this square to uncover the fog around it\nRight-click the map for options";
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
    SettingsRegistry::RegisterField(this, "show_fog", &show_fog);
    SettingsRegistry::RegisterField(this, "show_stand_cells", &show_stand_cells);
    SettingsRegistry::RegisterField(this, "show_grid", &show_grid);
    SettingsRegistry::RegisterField(this, "using_bec", &using_bec);
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
            CARTO_LOG("[cartographer] quest marker changed externally (quest=%d wm=%.0f,%.0f vs ours %.0f,%.0f); cartographer disabled",
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
            CARTO_LOG("[cartographer] pathfinding module is disabled; cartographer idle");
        }
        return;
    }
    warned_pathing_disabled = false;
    if (!PathfindingWindow::ReadyForPathing()) return;

    CartoGrid grid;
    if (!GetCartoGrid(grid)) {
        if (!warned_no_data) {
            warned_no_data = true;
            CARTO_LOG("[cartographer] no cartography data available");
        }
        return;
    }

    GW::Vec2f player_wm;
    if (!WorldMapWidget::GamePosToWorldMap(player->pos, player_wm)) return;
    player_wm_cached = player_wm;
    SweepStandCells(grid, map_info);
    RecomputeCoverage(grid, map_info);

    // Arrival is being inside the square, not near the goal - on a ledge those are a square apart.
    const int player_cx = static_cast<int>(floorf(player_wm.x / kWorldMapUnitsPerCell));
    const int player_cy = static_cast<int>(floorf(player_wm.y / kWorldMapUnitsPerCell));
    if (target.valid && target.on_map) {
        if (target.custom) {
            if (GW::GetSquareDistance(player->pos, goal_game) < 150.f * 150.f) {
                CARTO_LOG("[cartographer] reached custom point wm(%.0f, %.0f)", target.wm.x, target.wm.y);
                RemoveCustomPointAt(target.wm);
                ClearTarget();
            }
        }
        else if (!arrived && player_cx == target.cx && player_cy == target.cy) {
            arrived = true;
            arrived_at = TIMER_INIT();
            CARTO_LOG("[cartographer] standing in cell (%d, %d), which should credit %d cells", target.cx, target.cy, target.reveals);
        }
    }

    // Credit is not always instant - it can need a step or a click-walk first - so give the square
    // a fair while before concluding anything.
    if (arrived && target.valid && !target.custom && TIMER_DIFF(arrived_at) > 15000) {
        const auto it = stand_cells.find({target.cx, target.cy});
        if (it != stand_cells.end() && it->second.reveals > 0) {
            // A wide visit that credits nothing usually means the tiles it was reaching for are the
            // ones only normal range uncovers; demote those rather than writing the square off.
            const int r = RevealRadius();
            int demoted = 0;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) continue;
                    const std::pair cell{target.cx + dx, target.cy + dy};
                    if (!grid.InGrid(cell.first, cell.second) || grid.IsExplored(cell.first, cell.second)) continue;
                    if (strict_fog_cells.insert(cell).second) demoted++;
                }
            }
            if (!demoted) skipped_cells.insert({target.cx, target.cy});
            CARTO_LOG("[cartographer] cell (%d, %d) credited nothing; %d tiles demoted to normal range%s",
                      target.cx, target.cy, demoted, demoted ? "" : ", square skipped for this map");
            ClearTarget();
        }
    }

    Target cand{};
    float cand_d2 = FLT_MAX;
    for (const auto& p : custom_points) {
        const float d2 = Dist2(p, player_wm);
        if (d2 < cand_d2) {
            cand = {true, true, 0, 0, 0, p};
            cand_d2 = d2;
        }
    }
    bool on_current_map = true;
    if (cand.valid) {
        ImRect bounds;
        on_current_map = GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.Contains({cand.wm.x, cand.wm.y});
    }
    else {
        // Ranked by cells-credited-per-square-walked: a spot crediting several is worth extra steps.
        float best_value = 0.f;
        for (const auto& [cell, sc] : stand_cells) {
            if (!sc.walkable || sc.reveals <= 0) continue;
            if (skipped_cells.contains(cell) || declined_cells.contains(cell)) continue;
            const auto centre = CellCenterWorldMap(cell.first, cell.second);
            const float d2 = Dist2(centre, player_wm);
            const float dist_cells = sqrtf(d2) / kWorldMapUnitsPerCell;
            const float value = static_cast<float>(sc.reveals) / (dist_cells + 2.f);
            if (value > best_value) {
                best_value = value;
                cand_d2 = d2;
                cand = {true, false, cell.first, cell.second, sc.reveals, centre};
            }
        }
    }
    if (!cand.valid) {
        if (!warned_no_fog) {
            warned_no_fog = true;
            CARTO_LOG("[cartographer] no fogged walkable ground left on this map");
        }
        if (target.valid) ClearTarget();
        return;
    }
    warned_no_fog = false;

    bool same = target.valid && target.custom == cand.custom
        && (cand.custom ? Dist2(target.wm, cand.wm) < 1.f : (target.cx == cand.cx && target.cy == cand.cy));
    if (target.valid && !same && !(cand.custom && !target.custom)) {
        // Hysteresis: keep the current target unless it became ineligible or the candidate is meaningfully closer.
        const auto current = stand_cells.find({target.cx, target.cy});
        const bool current_eligible = target.custom
            ? std::ranges::any_of(custom_points, [&](const GW::Vec2f& p) { return Dist2(p, target.wm) < 1.f; })
            : current != stand_cells.end() && current->second.walkable && current->second.reveals > 0
            && !skipped_cells.contains({target.cx, target.cy}) && !declined_cells.contains({target.cx, target.cy});
        if (current_eligible && cand_d2 >= 0.7f * Dist2(target.wm, player_wm)) same = true;
    }
    if (same) {
        // Same square, but the fog around it may have shrunk and the status line quotes it.
        if (target.valid && !target.custom) {
            const auto it = stand_cells.find({target.cx, target.cy});
            if (it != stand_cells.end()) target.reveals = it->second.reveals;
        }
        return;
    }

    GW::GamePos gp{};
    GW::Vec2f marker_wm = cand.wm;
    if (on_current_map) {
        if (cand.custom) {
            if (!WorldMapWidget::WorldMapToGamePos(cand.wm, gp)) return;
            // The marker goes on the closest walkable spot toward the point, not into the void.
            Pathing::FindClosestPositionOnTrapezoid(gp);
        }
        else {
            // Route to the footing the probe found, not the square's centre, which may be cliff.
            const auto it = stand_cells.find({cand.cx, cand.cy});
            if (it == stand_cells.end() || !it->second.walkable) return;
            gp = it->second.pos;
        }
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
        if (on_current_map) CARTO_LOG("[cartographer] target: custom point wm(%.0f, %.0f), marker at game(%.0f, %.0f)", target.wm.x, target.wm.y, gp.x, gp.y);
        else CARTO_LOG("[cartographer] target: custom point wm(%.0f, %.0f) on another map; marker set there for cross-map routing", target.wm.x, target.wm.y);
    }
    else if (on_current_map) {
        CARTO_LOG("[cartographer] stand target: cell (%d, %d) wm(%.0f, %.0f) credits %d cells at radius %d, marker at game(%.0f, %.0f)",
                 target.cx, target.cy, target.wm.x, target.wm.y, target.reveals, RevealRadius(), gp.x, gp.y);
    }
    else {
        CARTO_LOG("[cartographer] stand target: cell (%d, %d) wm(%.0f, %.0f) on another map; marker set there for cross-map routing", target.cx, target.cy, target.wm.x, target.wm.y);
    }
}

void CartographerModule::DrawSettingsInternal()
{
    ImGui::TextDisabled("Debug tool. Exploration is credited by 32x32 world-map square: standing inside a\nsquare credits it and the ring of squares around it. So instead of routing you at the\nfog itself, this works out which squares you could stand in, which of them would\ncredit something still foggy, draws those on the world map and mission map, and puts\nthe custom quest marker in the best one - the regular quest path leads you there.\nRight-click either map to manage the helper.");
    bool on = enabled;
    if (ImGui::Checkbox("Enabled", &on)) {
        GW::GameThread::Enqueue([on] { SetEnabled(on); });
    }
    ImGui::Checkbox("Show remaining fog on the maps", &show_fog);
    ImGui::ShowHelp("Green: everything still unexplored that some square on this map can credit. Fog nothing here can reach draws nothing.");
    ImGui::Checkbox("Show squares to stand in", &show_stand_cells);
    ImGui::ShowHelp("Draws every 32x32 square worth walking into, shaded by how many foggy squares standing there would credit. The current suggestion is outlined and pulses.");
    ImGui::Checkbox("Show the cartography grid", &show_grid);
    ImGui::ShowHelp("Draws the 32x32 tile boundaries over this map. Exploration is credited a whole tile at a time, so this is what tells you which tile you are actually standing in. Hidden when zoomed out far enough that the lines would smear together.");
    if (ImGui::Checkbox("Using a Bird's Eye Compass", &using_bec)) {
        GW::GameThread::Enqueue([] {
            // The radius decides which cells are worth probing at all, so the sweep restarts.
            stand_cells.clear();
            fog_cells.clear();
            unreachable_fog_cells = 0;
            strict_fog_cells.clear();
            sweep_complete = false;
        });
    }
    ImGui::ShowHelp("Standing in a tile credits it and the 8 tiles around it (Chebyshev distance, so a square block - not a circle, which is why the nearest-looking spot often is not the right one). A Bird's Eye Compass widens that to 3 tiles in each direction. Where in the tile you stand makes no difference. Rescans the map.");
    unsigned standable = 0;
    unsigned useful = 0;
    for (const auto& [cell, sc] : stand_cells) {
        if (!sc.walkable) continue;
        standable++;
        if (sc.reveals > 0) useful++;
    }
    ImGui::Text("Squares: %u probed, %u standable, %u worth visiting", static_cast<unsigned>(stand_cells.size()), standable, useful);
    ImGui::Text("Foggy squares: %d reachable here, %d out of reach", map_fog_cells, unreachable_fog_cells);
    ImGui::Text("Declined forever: %u squares", static_cast<unsigned>(declined_cells.size()));
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
    CARTO_LOG("[cartographer] %s", on ? "enabled" : "disabled");
}

bool CartographerModule::GetEnabled()
{
    return enabled;
}

void CartographerModule::OnUserMarkerAction()
{
    if (!enabled) return;
    enabled = false;
    marker_owned = false;
    GW::GameThread::Enqueue([] {
        ResetState();
        CARTO_LOG("[cartographer] user placed/removed the quest marker; cartographer disabled");
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
        CARTO_LOG("[cartographer] fog point (%.0f, %.0f) removed", p.x, p.y);
    });
}

void CartographerModule::ClearCustomPoints()
{
    GW::GameThread::Enqueue([] {
        custom_points.clear();
        SerializePoints();
        if (target.valid && target.custom) ClearTarget();
        CARTO_LOG("[cartographer] custom fog points cleared");
    });
}

void CartographerModule::ClearDeclined()
{
    GW::GameThread::Enqueue([] {
        declined_cells.clear();
        skipped_cells.clear();
        stand_cells.clear();
        fog_cells.clear();
        unreachable_fog_cells = 0;
        strict_fog_cells.clear();
        sweep_complete = false;
        SerializeDeclined();
        if (target.valid) ClearTarget();
        CARTO_LOG("[cartographer] declined cells cleared");
    });
}

void CartographerModule::GetStatus(char* buf, const size_t len)
{
    const char* pathing = !PathfindingWindow::IsPathingEnabled() ? "off" : PathfindingWindow::ReadyForPathing() ? "ready" : "prewarming";
    char target_desc[64];
    if (!target.valid) snprintf(target_desc, sizeof(target_desc), "none");
    else if (target.custom) snprintf(target_desc, sizeof(target_desc), "point(%.0f,%.0f)", target.wm.x, target.wm.y);
    else snprintf(target_desc, sizeof(target_desc), "stand(%d,%d)+%d", target.cx, target.cy, target.reveals);
    snprintf(buf, len, "carto: enabled=%d pathing=%s target=%s marker=%d arrived=%d radius=%d skipped=%u probed=%u declined=%u points=%u fogcells=%d",
             enabled, pathing, target_desc, marker_owned, arrived, RevealRadius(),
             static_cast<unsigned>(skipped_cells.size()), static_cast<unsigned>(stand_cells.size()),
             static_cast<unsigned>(declined_cells.size()), static_cast<unsigned>(custom_points.size()), map_fog_cells);
}

