#include "stdafx.h"

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <D3DContainers.h>
#include <Defines.h>
#include <Timer.h>
#include <Modules/QuestModule.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/VanquishMapOverlayWidget.h>
#include <Widgets/WorldMapWidget.h>

namespace {
    VanquishMapOverlayWidget::Settings settings;
    // VQ overlay colours not exposed as settings
    Color vq_color_blocked = IM_COL32(60, 20, 20, 170);
    Color vq_color_blocked_border = IM_COL32(180, 100, 100, 140);

    // Enemy tracking
    enum class EnemyState { NotApplicable, Alive, Stale };
    struct TrackedEnemy {
        GW::Vec2f pos;
        float rotation = 0.f;
        EnemyState state = EnemyState::NotApplicable;
    };
    std::vector<TrackedEnemy> tracked_enemies_by_agent_id;
    size_t highest_trackable_agent_id = 0;

    GW::Constants::MapID tracked_enemies_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType tracked_enemies_instance_type = GW::Constants::InstanceType::Loading;

    constexpr float COMPASS_RANGE = GW::Constants::Range::Compass;
    constexpr float STALE_CHECK_RANGE = COMPASS_RANGE * 0.9f;

    bool should_draw = false;

    float cached_px_to_game = 1.f;

    // Fog buffer with pre-allocated quads per walkable cell
    class D3DFogBuffer : public D3DTriangleBuffer {
    public:
        static constexpr size_t VERTS_PER_CELL = sizeof(D3DQuad) / sizeof(D3DVertex);
        std::vector<int> walkable_cell_index;

        void Build(int grid_w, int grid_h, const bool* walkable)
        {
            clear();
            walkable_cell_index.assign(grid_w * grid_h, -1);
            int walkable_count = 0;
            for (int i = 0; i < grid_w * grid_h; i++) {
                if (!walkable[i]) continue;
                walkable_cell_index[i] = walkable_count++;
            }
            vertices.reserve(walkable_count * VERTS_PER_CELL);
            dirty = true;
        }

        void SetCellColor(int grid_idx, DWORD color)
        {
            if (grid_idx < 0 || grid_idx >= static_cast<int>(walkable_cell_index.size())) return;
            const int wi = walkable_cell_index[grid_idx];
            if (wi < 0) return;
            const size_t off = static_cast<size_t>(wi) * VERTS_PER_CELL;
            if (vertices[off].color == color) return;
            for (size_t v = 0; v < VERTS_PER_CELL; v++)
                vertices[off + v].color = color;
            dirty = true;
        }
    };

    // Thin wrapper to allow direct vertex/dirty access for bulk flush.
    class FrontierBuffer : public D3DTriangleBuffer {
    public:
        void FlushFrom(const std::unordered_map<int, std::vector<D3DVertex>>& cell_edges)
        {
            vertices.clear();
            for (const auto& [idx, verts] : cell_edges)
                vertices.insert(vertices.end(), verts.begin(), verts.end());
            dirty = true;
        }
    };

    D3DFogBuffer fog_buffer;
    FrontierBuffer frontier_border;
    D3DTriangleBuffer inaccessible_area_and_borders;
    D3DTriangleBuffer enemy_vertex_buffer;
    D3DCircle compass_circle;
    D3DVertexBuffer* vertex_buffers[] = {&fog_buffer, &frontier_border, &inaccessible_area_and_borders, &enemy_vertex_buffer, &compass_circle};

    constexpr float EXPLORE_CELL_SIZE = GW::Constants::Range::Adjacent / 2.f;

    bool* explored_cells = nullptr;
    GW::Constants::MapID explored_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType explored_instance_type = GW::Constants::InstanceType::Loading;

    struct BorderSegment {
        GW::Vec2f p1, p2;
    };
    std::vector<BorderSegment> cached_border_segments;

    constexpr int COMPASS_CIRCLE_SEGMENTS = 64;
    constexpr float COMPASS_CIRCLE_THICKNESS_PX = 0.5f;

    bool* cached_walkable_grid = nullptr;
    bool* cached_blocked_grid = nullptr; // cells with unreachable trapezoids
    int cached_walkable_grid_size = 0;
    int cached_grid_x0 = 0, cached_grid_y0 = 0, cached_grid_w = 0, cached_grid_h = 0;
    GW::Constants::MapID border_map_id = static_cast<GW::Constants::MapID>(0);
    float border_cached_zoom = 0.0f;

    // Snapshot of blockedPlanes to detect runtime changes (gates opening/closing)
    std::vector<uint32_t> cached_blocked_planes;
    bool force_exploration_update = false;

    bool HaveBlockedPlanesChanged()
    {
        auto* map_ctx = GW::GetMapContext();
        auto* path_ctx = map_ctx ? map_ctx->path : nullptr;
        if (!path_ctx) return false;

        const auto& bp = path_ctx->blockedPlanes;
        if (bp.size() != cached_blocked_planes.size()) {
            cached_blocked_planes.assign(bp.begin(), bp.begin() + bp.size());
            return true;
        }
        for (size_t i = 0; i < bp.size(); i++) {
            if (bp[i] != cached_blocked_planes[i]) {
                cached_blocked_planes.assign(bp.begin(), bp.begin() + bp.size());
                return true;
            }
        }
        return false;
    }

    bool IsFrontierEdge(int idx)
    {
        if (!explored_cells) return false;
        return explored_cells[idx];
    }

    int GetCellIndex(int gx, int gy)
    {
        const int lx = gx - cached_grid_x0;
        const int ly = gy - cached_grid_y0;
        if (lx < 0 || lx >= cached_grid_w || ly < 0 || ly >= cached_grid_h) return -1;
        return ly * cached_grid_w + lx;
    }

    std::vector<int> newly_explored_cells;

    bool UpdateExploration(const GW::GamePos* player_pos)
    {
        static GW::GamePos last_check = {};
        if (!player_pos) return false;
        if (!force_exploration_update && GW::GetSquareDistance(last_check, *player_pos) < GW::Constants::SqrRange::Adjacent) return false;
        force_exploration_update = false;
        last_check = *player_pos;

        const auto map_id = GW::Map::GetMapID();
        const auto instance_type = GW::Map::GetInstanceType();
        if (map_id != explored_map_id || instance_type != explored_instance_type) {
            delete[] explored_cells;
            explored_cells = nullptr;
            explored_map_id = map_id;
            explored_instance_type = instance_type;
        }

        if (!explored_cells && cached_walkable_grid_size > 0) explored_cells = new bool[cached_walkable_grid_size]();
        if (!explored_cells) return false;

        newly_explored_cells.clear();
        constexpr int range_cells = static_cast<int>(COMPASS_RANGE / EXPLORE_CELL_SIZE) + 1;
        const int player_cx = static_cast<int>(floorf(player_pos->x / EXPLORE_CELL_SIZE));
        const int player_cy = static_cast<int>(floorf(player_pos->y / EXPLORE_CELL_SIZE));
        constexpr float range_sq = COMPASS_RANGE * COMPASS_RANGE;

        for (int dy = -range_cells; dy <= range_cells; dy++) {
            for (int dx = -range_cells; dx <= range_cells; dx++) {
                const int gx = player_cx + dx;
                const int gy = player_cy + dy;
                const float cell_center_x = (gx + 0.5f) * EXPLORE_CELL_SIZE;
                const float cell_center_y = (gy + 0.5f) * EXPLORE_CELL_SIZE;
                const float ddx = cell_center_x - player_pos->x;
                const float ddy = cell_center_y - player_pos->y;
                if (ddx * ddx + ddy * ddy > range_sq) continue;
                const int idx = GetCellIndex(gx, gy);
                if (idx >= 0 && !explored_cells[idx]
                    && (cached_walkable_grid[idx] || (cached_blocked_grid && cached_blocked_grid[idx]))) {
                    explored_cells[idx] = true;
                    fog_buffer.SetCellColor(idx, 0x00000000);
                    newly_explored_cells.push_back(idx);
                }
            }
        }
        return !newly_explored_cells.empty();
    }

    void InitFogBuffer()
    {
        fog_buffer.clear();
        if (!cached_walkable_grid || cached_walkable_grid_size <= 0) return;

        const float grid_origin_x = cached_grid_x0 * EXPLORE_CELL_SIZE;
        const float grid_origin_y = cached_grid_y0 * EXPLORE_CELL_SIZE;

        // Include both walkable and blocked cells in the fog buffer so blocked
        // areas can also be "explored" (fog clears, revealing the blocked color).
        bool* fog_cells = new bool[cached_walkable_grid_size]();
        for (int i = 0; i < cached_walkable_grid_size; i++)
            fog_cells[i] = cached_walkable_grid[i] || (cached_blocked_grid && cached_blocked_grid[i]);
        fog_buffer.Build(cached_grid_w, cached_grid_h, fog_cells);
        delete[] fog_cells;

        for (int ly = 0; ly < cached_grid_h; ly++) {
            for (int lx = 0; lx < cached_grid_w; lx++) {
                const int idx = ly * cached_grid_w + lx;
                if (!cached_walkable_grid[idx] && !(cached_blocked_grid && cached_blocked_grid[idx])) continue;
                const float x0 = grid_origin_x + lx * EXPLORE_CELL_SIZE;
                const float y0 = grid_origin_y + ly * EXPLORE_CELL_SIZE;
                fog_buffer.push_back(D3DQuad({x0, y0}, {x0 + EXPLORE_CELL_SIZE, y0 + EXPLORE_CELL_SIZE}, settings.vq_color_fog_unexplored));
            }
        }
    }

    // Per-cell frontier edge storage for incremental updates.
    // Key = local cell index of an unexplored walkable cell that borders explored cells.
    std::unordered_map<int, std::vector<D3DVertex>> frontier_cell_edges;

    void ComputeCellFrontierEdges(int idx, std::vector<D3DVertex>& out)
    {
        out.clear();
        const bool is_map_cell = cached_walkable_grid[idx] || (cached_blocked_grid && cached_blocked_grid[idx]);
        if (!is_map_cell || explored_cells[idx]) return;

        const int ly = idx / cached_grid_w;
        const int lx = idx % cached_grid_w;
        const float FRONTIER_HALF_THICKNESS = cached_px_to_game;
        const float grid_origin_x = cached_grid_x0 * EXPLORE_CELL_SIZE;
        const float grid_origin_y = cached_grid_y0 * EXPLORE_CELL_SIZE;
        const float wx0 = grid_origin_x + lx * EXPLORE_CELL_SIZE;
        const float wx1 = wx0 + EXPLORE_CELL_SIZE;
        const float wy0 = grid_origin_y + ly * EXPLORE_CELL_SIZE;
        const float wy1 = wy0 + EXPLORE_CELL_SIZE;

        const auto edge = [&](bool cond, int neighbour_idx, float ax, float ay, float bx, float by) {
            if (cond && IsFrontierEdge(neighbour_idx)) {
                D3DLine line({ax, ay}, {bx, by}, FRONTIER_HALF_THICKNESS, settings.vq_color_frontier);
                for (const auto& tri : line.t)
                    out.insert(out.end(), tri.v, tri.v + 3);
            }
        };
        edge(ly > 0, idx - cached_grid_w, wx0, wy0, wx1, wy0);
        edge(ly < cached_grid_h - 1, idx + cached_grid_w, wx0, wy1, wx1, wy1);
        edge(lx > 0, idx - 1, wx0, wy0, wx0, wy1);
        edge(lx < cached_grid_w - 1, idx + 1, wx1, wy0, wx1, wy1);
    }

    void RebuildFrontierBorder()
    {
        frontier_cell_edges.clear();
        frontier_border.clear();
        if (!explored_cells || !cached_walkable_grid) return;

        std::vector<D3DVertex> cell_verts;
        for (int idx = 0; idx < cached_grid_w * cached_grid_h; idx++) {
            ComputeCellFrontierEdges(idx, cell_verts);
            if (!cell_verts.empty())
                frontier_cell_edges[idx] = cell_verts;
        }
        frontier_border.FlushFrom(frontier_cell_edges);
    }

    void UpdateFrontierIncremental()
    {
        if (!explored_cells || !cached_walkable_grid) return;

        // Collect cells that need re-evaluation: each newly explored cell
        // plus its 4 neighbors.
        std::unordered_set<int> dirty_cells;
        for (const int idx : newly_explored_cells) {
            const int ly = idx / cached_grid_w;
            const int lx = idx % cached_grid_w;
            dirty_cells.insert(idx);
            if (ly > 0) dirty_cells.insert(idx - cached_grid_w);
            if (ly < cached_grid_h - 1) dirty_cells.insert(idx + cached_grid_w);
            if (lx > 0) dirty_cells.insert(idx - 1);
            if (lx < cached_grid_w - 1) dirty_cells.insert(idx + 1);
        }

        std::vector<D3DVertex> cell_verts;
        for (const int idx : dirty_cells) {
            ComputeCellFrontierEdges(idx, cell_verts);
            if (cell_verts.empty())
                frontier_cell_edges.erase(idx);
            else
                frontier_cell_edges[idx] = cell_verts;
        }
        frontier_border.FlushFrom(frontier_cell_edges);
    }

    bool IsGridCellWalkable(int gx, int gy)
    {
        const int idx = GetCellIndex(gx, gy);
        return idx >= 0 && cached_walkable_grid[idx];
    }

    bool IsCellWalkableInTrapezoid(int gx, int gy, const GW::PathingTrapezoid& trap)
    {
        const float cx0 = gx * EXPLORE_CELL_SIZE;
        const float cy0 = gy * EXPLORE_CELL_SIZE;
        const float cx1 = cx0 + EXPLORE_CELL_SIZE;
        const float cy1 = cy0 + EXPLORE_CELL_SIZE;

        if (cy1 <= trap.YB || cy0 >= trap.YT) return false;
        const float y_lo = std::max(cy0, trap.YB);
        const float y_hi = std::min(cy1, trap.YT);
        const float height = trap.YT - trap.YB;
        if (height < 0.001f) return cx1 > std::min(trap.XBL, trap.XTL) && cx0 < std::max(trap.XBR, trap.XTR);

        const float t_lo = (y_lo - trap.YB) / height;
        const float t_hi = (y_hi - trap.YB) / height;
        const float left = std::min(trap.XBL + t_lo * (trap.XTL - trap.XBL), trap.XBL + t_hi * (trap.XTL - trap.XBL));
        const float right = std::max(trap.XBR + t_lo * (trap.XTR - trap.XBR), trap.XBR + t_hi * (trap.XTR - trap.XBR));
        return cx1 > left && cx0 < right;
    }

    const GW::PathingTrapezoid* FindTrapezoidInPlane(const GW::Vec2f& point, const GW::PathingMap& plane)
    {
        GW::Node* n = plane.root_node;
        int cnt = 50000;
        while (n && cnt--) {
            switch (n->type) {
                case 0: {
                    auto* xn = static_cast<GW::XNode*>(n);
                    float d = (point.y - xn->pos.y) * xn->dir.x - (point.x - xn->pos.x) * xn->dir.y;
                    n = d >= 0.0f ? xn->right : xn->left;
                    break;
                }
                case 1: {
                    auto* yn = static_cast<GW::YNode*>(n);
                    if (point.y > yn->pos.y) n = yn->above;
                    else if (point.y < yn->pos.y) n = yn->below;
                    else n = point.x >= yn->pos.x ? yn->above : yn->below;
                    break;
                }
                case 2: {
                    auto* sn = static_cast<GW::SinkNode*>(n);
                    return sn ? sn->trapezoid : nullptr;
                }
            }
        }
        return nullptr;
    }

    // BFS from the player's trapezoid through adjacency and unblocked portals.
    // Returns the set of reachable trapezoids, or empty if the player position is unknown.
    // Blocking checks match the game's native A* (IPath_ExpandPortalLeft/Right):
    //   - portal.flags & 0x04  (portal flagged as blocked)
    //   - blockedPlanes[neighbor_plane] & 1  (target plane blocked at runtime)
    std::unordered_set<const GW::PathingTrapezoid*> FindReachableTrapezoids(const GW::PathingMapArray* pathing_map)
    {
        std::unordered_set<const GW::PathingTrapezoid*> reachable;

        const auto* player = GW::Agents::GetControlledCharacter();
        if (!player) return reachable;
        const GW::Vec2f player_pos = {player->pos.x, player->pos.y};

        auto* map_ctx = GW::GetMapContext();
        auto* path_ctx = map_ctx ? map_ctx->path : nullptr;

        const GW::PathingTrapezoid* start_trap = nullptr;
        size_t start_plane = 0;

        if (player->pos.zplane < pathing_map->size()) {
            start_trap = FindTrapezoidInPlane(player_pos, pathing_map->at(player->pos.zplane));
            start_plane = player->pos.zplane;
        }
        if (!start_trap) {
            for (size_t p = 0; p < pathing_map->size(); p++) {
                start_trap = FindTrapezoidInPlane(player_pos, pathing_map->at(p));
                if (start_trap) { start_plane = p; break; }
            }
        }
        if (!start_trap) return reachable;

        struct TrapRef { const GW::PathingTrapezoid* trap; size_t plane; };
        std::vector<TrapRef> queue;
        queue.push_back({start_trap, start_plane});
        reachable.insert(start_trap);

        for (size_t head = 0; head < queue.size(); head++) {
            const auto [trap, plane_idx] = queue[head];

            for (int i = 0; i < 4; i++) {
                auto* adj = trap->adjacent[i];
                if (adj && !reachable.contains(adj)) {
                    reachable.insert(adj);
                    queue.push_back({adj, plane_idx});
                }
            }

            const auto& pm = pathing_map->at(plane_idx);
            auto check_portal = [&](uint16_t portal_idx) {
                if (portal_idx >= pm.portal_count) return;
                auto& portal = pm.portals[portal_idx];
                if (portal.flags & 0x04) return;
                auto* pair = portal.pair;
                if (!pair) return;
                size_t target_plane = portal.neighbor_plane;
                if (path_ctx && target_plane < path_ctx->blockedPlanes.size()
                    && (path_ctx->blockedPlanes[target_plane] & 1)) return;
                for (uint32_t i = 0; i < pair->count; i++) {
                    auto* t = pair->trapezoids[i];
                    if (t && !reachable.contains(t)) {
                        reachable.insert(t);
                        queue.push_back({t, target_plane});
                    }
                }
            };
            check_portal(trap->portal_left);
            check_portal(trap->portal_right);
        }

        return reachable;
    }

    void BuildStaticMapGeometry()
    {
        const float border_thickness_game = cached_px_to_game;
        const float gx0 = cached_grid_x0 * EXPLORE_CELL_SIZE;
        const float gy0 = cached_grid_y0 * EXPLORE_CELL_SIZE;
        const float gx1 = (cached_grid_x0 + cached_grid_w) * EXPLORE_CELL_SIZE;
        const float gy1 = (cached_grid_y0 + cached_grid_h) * EXPLORE_CELL_SIZE;
        const float ext = std::max(gx1 - gx0, gy1 - gy0) * 5.0f;

        inaccessible_area_and_borders.clear();

        inaccessible_area_and_borders.push_back(D3DQuad({gx0 - ext, gy0 - ext}, {gx1 + ext, gy0}, settings.vq_color_inaccessible));
        inaccessible_area_and_borders.push_back(D3DQuad({gx0 - ext, gy1}, {gx1 + ext, gy1 + ext}, settings.vq_color_inaccessible));
        inaccessible_area_and_borders.push_back(D3DQuad({gx0 - ext, gy0}, {gx0, gy1}, settings.vq_color_inaccessible));
        inaccessible_area_and_borders.push_back(D3DQuad({gx1, gy0}, {gx1 + ext, gy1}, settings.vq_color_inaccessible));

        std::vector<BorderSegment> blocked_border_segments;
        for (int gy = cached_grid_y0; gy < cached_grid_y0 + cached_grid_h; gy++) {
            const float y0 = gy * EXPLORE_CELL_SIZE;
            const float y1 = y0 + EXPLORE_CELL_SIZE;
            int run_start = -1;
            bool run_blocked = false;
            for (int gx = cached_grid_x0; gx < cached_grid_x0 + cached_grid_w; gx++) {
                if (!IsGridCellWalkable(gx, gy)) {
                    const int idx = GetCellIndex(gx, gy);
                    const bool is_blocked = idx >= 0 && cached_blocked_grid && cached_blocked_grid[idx];
                    if (run_start == -1) {
                        run_start = gx;
                        run_blocked = is_blocked;
                    } else if (is_blocked != run_blocked) {
                        inaccessible_area_and_borders.push_back(D3DQuad({run_start * EXPLORE_CELL_SIZE, y0}, {gx * EXPLORE_CELL_SIZE, y1}, run_blocked ? vq_color_blocked : settings.vq_color_inaccessible.value));
                        run_start = gx;
                        run_blocked = is_blocked;
                    }
                }
                else {
                    if (run_start != -1) {
                        inaccessible_area_and_borders.push_back(D3DQuad({run_start * EXPLORE_CELL_SIZE, y0}, {gx * EXPLORE_CELL_SIZE, y1}, run_blocked ? vq_color_blocked : settings.vq_color_inaccessible.value));
                        run_start = -1;
                    }
                }
            }
            if (run_start != -1) {
                inaccessible_area_and_borders.push_back(D3DQuad({run_start * EXPLORE_CELL_SIZE, y0}, {(cached_grid_x0 + cached_grid_w) * EXPLORE_CELL_SIZE, y1}, run_blocked ? vq_color_blocked : settings.vq_color_inaccessible.value));
            }
        }

        // Borders around walkable area
        for (const auto& seg : cached_border_segments)
            inaccessible_area_and_borders.push_back(D3DLine(seg.p1, seg.p2, border_thickness_game, settings.vq_color_border));

        // Borders around blocked (unreachable) areas
        if (cached_blocked_grid) {
            for (int cy = 0; cy < cached_grid_h; cy++) {
                for (int cx = 0; cx < cached_grid_w; cx++) {
                    const int idx = cy * cached_grid_w + cx;
                    if (!cached_blocked_grid[idx] || cached_walkable_grid[idx]) continue;
                    const int gx = cached_grid_x0 + cx, gy = cached_grid_y0 + cy;
                    const float x0 = gx * EXPLORE_CELL_SIZE, y0 = gy * EXPLORE_CELL_SIZE;
                    const float x1 = x0 + EXPLORE_CELL_SIZE, y1 = y0 + EXPLORE_CELL_SIZE;
                    auto is_blocked = [&](int nx, int ny) {
                        const int ni = GetCellIndex(nx, ny);
                        return ni >= 0 && cached_blocked_grid[ni] && !cached_walkable_grid[ni];
                    };
                    if (!is_blocked(gx, gy - 1)) inaccessible_area_and_borders.push_back(D3DLine({x0, y0}, {x1, y0}, border_thickness_game, vq_color_blocked_border));
                    if (!is_blocked(gx, gy + 1)) inaccessible_area_and_borders.push_back(D3DLine({x0, y1}, {x1, y1}, border_thickness_game, vq_color_blocked_border));
                    if (!is_blocked(gx - 1, gy)) inaccessible_area_and_borders.push_back(D3DLine({x0, y0}, {x0, y1}, border_thickness_game, vq_color_blocked_border));
                    if (!is_blocked(gx + 1, gy)) inaccessible_area_and_borders.push_back(D3DLine({x1, y0}, {x1, y1}, border_thickness_game, vq_color_blocked_border));
                }
            }
        }
    }

    void RebuildMapBorder()
    {
        // Save old exploration state so we can restore it after rebuilding the grid
        bool* old_explored = explored_cells;
        const int old_x0 = cached_grid_x0, old_y0 = cached_grid_y0;
        const int old_w = cached_grid_w, old_h = cached_grid_h;
        explored_cells = nullptr;

        cached_border_segments.clear();
        frontier_cell_edges.clear();
        frontier_border.clear();
        delete[] cached_walkable_grid;
        cached_walkable_grid = nullptr;
        delete[] cached_blocked_grid;
        cached_blocked_grid = nullptr;
        cached_walkable_grid_size = 0;

        const auto pathing_map = GW::Map::GetPathingMap();
        if (!pathing_map) return;

        const auto reachable = FindReachableTrapezoids(pathing_map);

        // Compute grid bounds from ALL trapezoids so the inaccessible overlay
        // covers the full map, but only rasterize reachable ones as walkable.
        std::vector<const GW::PathingTrapezoid*> traps;
        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
        for (size_t p = 0; p < pathing_map->size(); p++) {
            const auto& plane = pathing_map->at(p);
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                min_x = std::min({min_x, trap.XTL, trap.XTR, trap.XBL, trap.XBR});
                max_x = std::max({max_x, trap.XTL, trap.XTR, trap.XBL, trap.XBR});
                min_y = std::min(min_y, trap.YB);
                max_y = std::max(max_y, trap.YT);
                if (!reachable.empty() && !reachable.contains(&trap)) continue;
                traps.push_back(&trap);
            }
        }
        if (min_x == FLT_MAX) return;

        cached_grid_x0 = static_cast<int>(floorf(min_x / EXPLORE_CELL_SIZE)) - 1;
        cached_grid_y0 = static_cast<int>(floorf(min_y / EXPLORE_CELL_SIZE)) - 1;
        cached_grid_w = static_cast<int>(ceilf(max_x / EXPLORE_CELL_SIZE)) + 1 - cached_grid_x0;
        cached_grid_h = static_cast<int>(ceilf(max_y / EXPLORE_CELL_SIZE)) + 1 - cached_grid_y0;

        cached_walkable_grid_size = cached_grid_w * cached_grid_h;
        cached_walkable_grid = new bool[cached_walkable_grid_size]();
        cached_blocked_grid = new bool[cached_walkable_grid_size]();

        // Rasterize unreachable trapezoids into blocked grid
        for (size_t p = 0; p < pathing_map->size(); p++) {
            const auto& plane = pathing_map->at(p);
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                if (reachable.empty() || reachable.contains(&trap)) continue;
                const int ty0 = std::max(0, static_cast<int>(floorf(trap.YB / EXPLORE_CELL_SIZE)) - cached_grid_y0);
                const int ty1 = std::min(cached_grid_h - 1, static_cast<int>(ceilf(trap.YT / EXPLORE_CELL_SIZE)) - cached_grid_y0);
                const int tx0 = std::max(0, static_cast<int>(floorf(std::min({trap.XTL, trap.XBL}) / EXPLORE_CELL_SIZE)) - cached_grid_x0);
                const int tx1 = std::min(cached_grid_w - 1, static_cast<int>(ceilf(std::max({trap.XTR, trap.XBR}) / EXPLORE_CELL_SIZE)) - cached_grid_x0);
                for (int cy = ty0; cy <= ty1; cy++) {
                    for (int cx = tx0; cx <= tx1; cx++) {
                        const int idx = GetCellIndex(cached_grid_x0 + cx, cached_grid_y0 + cy);
                        if (idx >= 0 && IsCellWalkableInTrapezoid(cached_grid_x0 + cx, cached_grid_y0 + cy, trap))
                            cached_blocked_grid[idx] = true;
                    }
                }
            }
        }

        // Rasterize reachable trapezoids into walkable grid
        for (const auto* trap : traps) {
            const int ty0 = std::max(0, static_cast<int>(floorf(trap->YB / EXPLORE_CELL_SIZE)) - cached_grid_y0);
            const int ty1 = std::min(cached_grid_h - 1, static_cast<int>(ceilf(trap->YT / EXPLORE_CELL_SIZE)) - cached_grid_y0);
            const int tx0 = std::max(0, static_cast<int>(floorf(std::min({trap->XTL, trap->XBL}) / EXPLORE_CELL_SIZE)) - cached_grid_x0);
            const int tx1 = std::min(cached_grid_w - 1, static_cast<int>(ceilf(std::max({trap->XTR, trap->XBR}) / EXPLORE_CELL_SIZE)) - cached_grid_x0);
            for (int cy = ty0; cy <= ty1; cy++) {
                for (int cx = tx0; cx <= tx1; cx++) {
                    const int abs_gx = cached_grid_x0 + cx;
                    const int abs_gy = cached_grid_y0 + cy;
                    const int idx = GetCellIndex(abs_gx, abs_gy);
                    if (idx < 0 || cached_walkable_grid[idx]) continue;
                    if (IsCellWalkableInTrapezoid(abs_gx, abs_gy, *trap))
                        cached_walkable_grid[idx] = true;
                }
            }
        }

        for (int cy = 0; cy < cached_grid_h; cy++) {
            for (int cx = 0; cx < cached_grid_w; cx++) {
                const int idx = GetCellIndex(cached_grid_x0 + cx, cached_grid_y0 + cy);
                if (idx < 0 || !cached_walkable_grid[idx]) continue;

                const float x0 = (cached_grid_x0 + cx) * EXPLORE_CELL_SIZE;
                const float y0 = (cached_grid_y0 + cy) * EXPLORE_CELL_SIZE;
                const float x1 = x0 + EXPLORE_CELL_SIZE;
                const float y1 = y0 + EXPLORE_CELL_SIZE;

                if (!IsGridCellWalkable(cached_grid_x0 + cx, cached_grid_y0 + cy - 1)) cached_border_segments.push_back({{x0, y0}, {x1, y0}});
                if (!IsGridCellWalkable(cached_grid_x0 + cx, cached_grid_y0 + cy + 1)) cached_border_segments.push_back({{x0, y1}, {x1, y1}});
                if (!IsGridCellWalkable(cached_grid_x0 + cx - 1, cached_grid_y0 + cy)) cached_border_segments.push_back({{x0, y0}, {x0, y1}});
                if (!IsGridCellWalkable(cached_grid_x0 + cx + 1, cached_grid_y0 + cy)) cached_border_segments.push_back({{x1, y0}, {x1, y1}});
            }
        }
        BuildStaticMapGeometry();
        InitFogBuffer();

        // Restore exploration state from the old grid
        if (old_explored && cached_walkable_grid_size > 0) {
            if (!explored_cells) explored_cells = new bool[cached_walkable_grid_size]();
            explored_map_id = GW::Map::GetMapID();
            explored_instance_type = GW::Map::GetInstanceType();
            for (int old_ly = 0; old_ly < old_h; old_ly++) {
                for (int old_lx = 0; old_lx < old_w; old_lx++) {
                    if (!old_explored[old_ly * old_w + old_lx]) continue;
                    const int abs_gx = old_x0 + old_lx;
                    const int abs_gy = old_y0 + old_ly;
                    const int new_idx = GetCellIndex(abs_gx, abs_gy);
                    if (new_idx >= 0 && (cached_walkable_grid[new_idx]
                        || (cached_blocked_grid && cached_blocked_grid[new_idx]))) {
                        explored_cells[new_idx] = true;
                        fog_buffer.SetCellColor(new_idx, 0x00000000);
                    }
                }
            }
            RebuildFrontierBorder();
        }
        delete[] old_explored;
        force_exploration_update = true;
    }

    bool nav_active = false;
    GW::Vec2f nav_target_pos;
    GW::Vec2f nav_marker_pos;
    bool nav_marker_set = false;
    bool nav_marker_hidden = false;
    bool self_changing_marker = false;

    constexpr float MARKER_UPDATE_DIST = 500.0f * 500.f;

    void StopNavigating()
    {
        nav_active = false;
        nav_target_pos = {};
        nav_marker_pos = {};
        nav_marker_set = false;
        nav_marker_hidden = false;
        GW::GameThread::Enqueue([] {
            self_changing_marker = true;
            QuestModule::ClearCustomQuestMarker();
            self_changing_marker = false;
        });
    }

    void SetNavTarget(const GW::Vec2f& target)
    {
        nav_active = true;
        nav_target_pos = target;

        const auto player = GW::Agents::GetControlledCharacter();
        if (player) {
            constexpr float HIDE_RANGE_SQ = (COMPASS_RANGE * 0.5f) * (COMPASS_RANGE * 0.5f);
            const bool in_compass = GW::GetSquareDistance(target, player->pos) < HIDE_RANGE_SQ;

            if (in_compass && !nav_marker_hidden) {
                nav_marker_hidden = true;
                self_changing_marker = true;
                QuestModule::ClearCustomQuestMarker();
                self_changing_marker = false;
                return;
            }
            if (in_compass) return;

            if (nav_marker_hidden) {
                nav_marker_hidden = false;
                nav_marker_set = false;
            }
        }

        if (nav_marker_set && GW::GetSquareDistance(target, nav_marker_pos) < MARKER_UPDATE_DIST) return;

        nav_marker_pos = target;
        nav_marker_set = true;
        GW::Vec2f world_pos;
        if (WorldMapWidget::GamePosToWorldMap(target, world_pos)) {
            GW::GameThread::Enqueue([world_pos] {
                self_changing_marker = true;
                QuestModule::SetCustomQuestMarker(world_pos, true);
                self_changing_marker = false;
            });
        }
    }

    void NavigateToClosestEnemy()
    {
        const auto player = GW::Agents::GetControlledCharacter();
        if (!player) return;

        const auto& player_pos = player->pos;
        float best_dist_sq = FLT_MAX;
        GW::GamePos best_pos = {};
        bool found = false;

        for (size_t agent_id = 0, len = std::min(tracked_enemies_by_agent_id.size(), highest_trackable_agent_id + 1); agent_id < len; agent_id++) {
            const auto& enemy = tracked_enemies_by_agent_id[agent_id];
            if (enemy.state == EnemyState::NotApplicable) continue;
            const float dist_sq = GW::GetDistance(enemy.pos, player_pos);
            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                best_pos = enemy.pos;
                found = true;
            }
        }

        if (!found) {
            if (!nav_marker_hidden) {
                nav_marker_hidden = true;
                self_changing_marker = true;
                QuestModule::ClearCustomQuestMarker();
                self_changing_marker = false;
            }
            return;
        }

        SetNavTarget(best_pos);
    }

    constexpr float stale_range_sq = STALE_CHECK_RANGE * STALE_CHECK_RANGE;
    D3DTeardrop teardrop_outline;
    D3DTeardrop teardrop_fill;

    void UpdateEnemyTracking()
    {
        const auto map_id = GW::Map::GetMapID();
        const auto instance_type = GW::Map::GetInstanceType();
        if (map_id != tracked_enemies_map_id || instance_type != tracked_enemies_instance_type) {
            tracked_enemies_by_agent_id.assign(tracked_enemies_by_agent_id.size(), {});
            tracked_enemies_map_id = map_id;
            highest_trackable_agent_id = 0;
            tracked_enemies_instance_type = instance_type;
        }

        const auto player_pos = GW::PlayerMgr::GetPlayerPosition();
        const auto* agents = player_pos ? GW::Agents::GetAgentArray() : nullptr;
        if (!agents) return;

        static bool was_dead = false;
        const bool is_dead = !GW::Agents::GetAgentMatchesFlags(GW::Agents::GetControlledCharacter(), GW::TargetFilter::Allies);
        if (is_dead) { was_dead = true; return; }
        if (was_dead) { was_dead = false; return; }

        if (tracked_enemies_by_agent_id.size() < agents->size()) tracked_enemies_by_agent_id.resize(agents->size());

        for (size_t agent_id = 0, len = agents->size(); agent_id < len; agent_id++) {
            const auto agent = agents->at(agent_id);
            auto& tracked = tracked_enemies_by_agent_id[agent_id];
            if (!agent) {
                if (tracked.state == EnemyState::Alive) {
                    if (GW::GetSquareDistance(*player_pos, tracked.pos) < stale_range_sq) {
                        tracked.state = EnemyState::Stale;
                    }
                }
                if (tracked.state != EnemyState::NotApplicable) {
                    highest_trackable_agent_id = agent_id;
                }
                continue;
            }
            if (!GW::Agents::GetAgentMatchesFlags(agent, GW::TargetFilter::Enemies)) {
                tracked.state = EnemyState::NotApplicable;
                continue;
            }
            const auto* living = agent->GetAsAgentLiving();
            auto* npc = GW::Agents::GetNPCByID(living->player_number);
            if (npc && (npc->IsSpirit() || npc->IsMinion())) {
                tracked.state = EnemyState::NotApplicable;
                continue;
            }

            if (isnan(living->pos.x) || isnan(living->pos.y)) continue;
            tracked.pos = {living->pos.x, living->pos.y};
            tracked.rotation = living->rotation_angle;
            tracked.state = EnemyState::Alive;
            highest_trackable_agent_id = agent_id;
        }
        for (size_t agent_id = agents->size(), len = tracked_enemies_by_agent_id.size(); agent_id < len; agent_id++) {
            auto& tracked = tracked_enemies_by_agent_id[agent_id];
            if (tracked.state == EnemyState::Alive) {
                if (GW::GetSquareDistance(*player_pos, tracked.pos) < stale_range_sq) {
                    tracked.state = EnemyState::Stale;
                }
            }
            if (tracked.state != EnemyState::NotApplicable) {
                highest_trackable_agent_id = agent_id;
            }
        }

        if (nav_active) NavigateToClosestEnemy();
        enemy_vertex_buffer.clear();

        const float radius = settings.vq_enemy_marker_size * cached_px_to_game;
        const float radius_outer = radius + cached_px_to_game;
        teardrop_fill.SetRadius(radius);
        teardrop_outline.SetRadius(radius_outer);
        teardrop_outline.SetColor(settings.vq_color_enemy_outline);

        for (size_t i = 0, len = std::min(tracked_enemies_by_agent_id.size(), highest_trackable_agent_id + 1); i < len; i++) {
            auto& enemy = tracked_enemies_by_agent_id[i];
            if (enemy.state == EnemyState::NotApplicable) continue;
            const DWORD color = enemy.state == EnemyState::Stale ? settings.vq_color_enemy_stale : settings.vq_color_enemy_alive;
            teardrop_fill.SetColor(color);
            teardrop_fill.SetCenterColor(color);
            teardrop_fill.SetPosition(enemy.pos);
            teardrop_fill.SetRotation(enemy.rotation);
            teardrop_outline.SetPosition(enemy.pos);
            teardrop_outline.SetRotation(enemy.rotation);
            enemy_vertex_buffer.push_back(teardrop_outline);
            enemy_vertex_buffer.push_back(teardrop_fill);
        }
    }

    void RebuildCompassCircle()
    {
        compass_circle = D3DCircle({0.f, 0.f}, COMPASS_RANGE, COMPASS_CIRCLE_THICKNESS_PX * cached_px_to_game, (DWORD)settings.vq_color_compass, COMPASS_CIRCLE_SEGMENTS);
    }

    void DrawEnemyCountLabel()
    {
        if (!should_draw) return;

        const auto mm_top_left = MissionMapWidget::GetTopLeft();
        const auto mm_bottom_right = MissionMapWidget::GetBottomRight();

        int alive_count = 0, stale_count = 0;
        for (size_t i = 0, len = std::min(tracked_enemies_by_agent_id.size(), highest_trackable_agent_id + 1); i < len; i++) {
            const auto& enemy = tracked_enemies_by_agent_id[i];
            if (enemy.state == EnemyState::Alive)
                alive_count++;
            else if (enemy.state == EnemyState::Stale)
                stale_count++;
        }

        const uint32_t foes_remaining = GW::Map::GetFoesToKill();
        const bool has_vq_data = foes_remaining > 0 || GW::Map::GetFoesKilled() > 0;

        if (!alive_count && !stale_count && !(has_vq_data && foes_remaining > 0)) return;

        char label[128];
        int pos = 0;
        if (alive_count > 0 || stale_count > 0) {
            pos += snprintf(label + pos, sizeof(label) - pos, "%d", alive_count);
            if (stale_count > 0) pos += snprintf(label + pos, sizeof(label) - pos, " (+%d?)", stale_count);
            if (has_vq_data) pos += snprintf(label + pos, sizeof(label) - pos, " / %u remaining", foes_remaining);
        }
        else {
            pos += snprintf(label + pos, sizeof(label) - pos, "%u remaining", foes_remaining);
        }

        constexpr float PADDING = 8.0f;
        const float btn_size = ImGui::GetTextLineHeight() + 12.0f;
        const ImVec2 text_pos(mm_top_left.x + PADDING + btn_size, mm_bottom_right.y - ImGui::GetTextLineHeight() - PADDING);

        auto* draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddText({text_pos.x + 1, text_pos.y + 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText({text_pos.x - 1, text_pos.y - 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText({text_pos.x + 1, text_pos.y - 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText({text_pos.x - 1, text_pos.y + 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), label);
    }


    void OnCustomMarkerChanged()
    {
        if (self_changing_marker || !nav_active) return;
        // Another system changed the custom quest marker — stop navigating
        // but don't clear the marker (it belongs to whoever set it now)
        nav_active = false;
        nav_target_pos = {};
        nav_marker_pos = {};
        nav_marker_set = false;
        nav_marker_hidden = false;
    }

    void OnMissionMapDraw(IDirect3DDevice9* dx_device)
    {
        if (!should_draw) return;

        const auto& gameToScreen = MissionMapWidget::GetGameToScreenMatrix();

        inaccessible_area_and_borders.Render(dx_device);
        fog_buffer.Render(dx_device);
        frontier_border.Render(dx_device);
        enemy_vertex_buffer.Render(dx_device);

        if (!compass_circle.empty()) {
            if (const auto* player = GW::Agents::GetControlledCharacter()) {
                D3DMATRIX compassMatrix = gameToScreen;
                compassMatrix._41 = roundf(gameToScreen._41 + player->pos.x * gameToScreen._11 + player->pos.y * gameToScreen._21);
                compassMatrix._42 = roundf(gameToScreen._42 + player->pos.x * gameToScreen._12 + player->pos.y * gameToScreen._22);
                dx_device->SetTransform(D3DTS_WORLD, &compassMatrix);
                compass_circle.Render(dx_device);
                dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
            }
        }
    }
} // namespace

void VanquishMapOverlayWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
    MissionMapWidget::AddDrawCallback(&OnMissionMapDraw);
    MissionMapWidget::AddContextMenuCallback(&VanquishMapOverlayWidget::ContextMenuItems);
    QuestModule::AddCustomMarkerChangedCallback(&OnCustomMarkerChanged);
}

void VanquishMapOverlayWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void VanquishMapOverlayWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void VanquishMapOverlayWidget::Draw(IDirect3DDevice9*)
{
    DrawVanquishToggleButton();
    DrawEnemyCountLabel();
}

void VanquishMapOverlayWidget::DrawVanquishToggleButton()
{
    if (!MissionMapWidget::IsRenderReady() || !ToolboxUtils::IsExplorable()) return;

    const auto mm_top_left = MissionMapWidget::GetTopLeft();
    const auto mm_bottom_right = MissionMapWidget::GetBottomRight();

    constexpr float PADDING = 4.0f;
    const float btn_size = ImGui::GetTextLineHeight() + PADDING * 2;
    const ImVec2 btn_pos(mm_top_left.x + PADDING, mm_bottom_right.y - btn_size - PADDING);

    ImGui::SetNextWindowPos(btn_pos);
    ImGui::SetNextWindowSize({0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {0, 0});
    if (ImGui::Begin("##vq_toggle", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(visible ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        if (ImGui::Button(ICON_FA_SKULL "##vq_toggler")) visible = !visible;
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip(visible ? "VQ overlay active. Click to hide." : "VQ overlay hidden. Click to show.");
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void VanquishMapOverlayWidget::Update(float)
{
    const bool render_ready = MissionMapWidget::IsRenderReady();
    should_draw = render_ready && visible && ToolboxUtils::IsExplorable();

    if (render_ready) {
        cached_px_to_game = MissionMapWidget::GetPxToGame();
        const float zoom = MissionMapWidget::GetZoom();

        const auto map_id = GW::Map::GetMapID();
        const auto instance_type = GW::Map::GetInstanceType();
        static GW::Constants::InstanceType border_instance_type = GW::Constants::InstanceType::Loading;
        const bool map_changed = map_id != border_map_id || instance_type != border_instance_type;
        const bool zoom_changed = zoom != border_cached_zoom;

        if (map_changed || zoom_changed) {
            border_cached_zoom = zoom;
            RebuildCompassCircle();
            if (map_changed) {
                border_map_id = map_id;
                border_instance_type = instance_type;
                cached_blocked_planes.clear();
                RebuildMapBorder(); // calls BuildStaticMapGeometry + InitFogBuffer
            }
            else {
                BuildStaticMapGeometry(); // zoom changed, rebuild with new thickness
                RebuildFrontierBorder();
            }
        }
        else if (HaveBlockedPlanesChanged()) {
            RebuildMapBorder();
        }
        else if (cached_walkable_grid && cached_walkable_grid_size > 0) {
            // Detect teleport: if the player is standing on a cell that isn't
            // walkable, the reachability set is stale (e.g. UW teleports).
            const auto* player = GW::Agents::GetControlledCharacter();
            if (player) {
                const int px = static_cast<int>(floorf(player->pos.x / EXPLORE_CELL_SIZE));
                const int py = static_cast<int>(floorf(player->pos.y / EXPLORE_CELL_SIZE));
                const int idx = GetCellIndex(px, py);
                if (idx < 0 || !cached_walkable_grid[idx]) {
                    RebuildMapBorder();
                }
            }
        }
    }

    // Frame rate check for expensive updates
    static clock_t last_check = TIMER_INIT();
    if (!ToolboxUtils::FrameRateCheck(last_check, 30)) return;

    const auto player_pos = GW::PlayerMgr::GetPlayerPosition();
    if (ToolboxUtils::IsExplorable()) {
        UpdateEnemyTracking();
        if (UpdateExploration(player_pos))
            UpdateFrontierIncremental();
    } else {
        if (nav_active) StopNavigating();
        if (tracked_enemies_map_id != GW::Constants::MapID::None) {
            tracked_enemies_by_agent_id.assign(tracked_enemies_by_agent_id.size(), {});
            tracked_enemies_map_id = GW::Constants::MapID::None;
            tracked_enemies_instance_type = GW::Constants::InstanceType::Loading;
            highest_trackable_agent_id = 0;
            enemy_vertex_buffer.clear();
        }
        if (explored_map_id != GW::Constants::MapID::None) {
            delete[] explored_cells;
            explored_cells = nullptr;
            explored_map_id = GW::Constants::MapID::None;
            explored_instance_type = GW::Constants::InstanceType::Loading;
        }
    }
}

void VanquishMapOverlayWidget::Terminate()
{
    ToolboxWidget::Terminate();
    MissionMapWidget::RemoveDrawCallback(&OnMissionMapDraw);
    MissionMapWidget::RemoveContextMenuCallback(&VanquishMapOverlayWidget::ContextMenuItems);
    QuestModule::RemoveCustomMarkerChangedCallback(&OnCustomMarkerChanged);

    delete[] cached_walkable_grid;
    cached_walkable_grid = nullptr;
    delete[] explored_cells;
    explored_cells = nullptr;

    for (auto buffer : vertex_buffers) {
        buffer->Terminate();
    }
}

void VanquishMapOverlayWidget::DrawSettingsInternal()
{
    ImGui::TextDisabled("Tracks enemy positions as they enter compass range.\nBlue = alive, Orange = last known (moved away).\nArrows on orange markers show last movement direction.\nAlso highlights areas you've explored during this session on the mission map.");
    bool static_changed = false;
    bool fog_changed = false;

    static_changed |= Colors::DrawSettingHueWheel("Inaccessible area", &settings.vq_color_inaccessible.value);
    static_changed |= Colors::DrawSettingHueWheel("Map border", &settings.vq_color_border.value);
    fog_changed |= Colors::DrawSettingHueWheel("Unexplored fog", &settings.vq_color_fog_unexplored.value);
    fog_changed |= Colors::DrawSettingHueWheel("Frontier edge", &settings.vq_color_frontier.value);
    bool rebuild_compass = Colors::DrawSettingHueWheel("Compass range", &settings.vq_color_compass.value);
    Colors::DrawSettingHueWheel("Enemy (alive)", &settings.vq_color_enemy_alive.value);
    Colors::DrawSettingHueWheel("Enemy (last known position)", &settings.vq_color_enemy_stale.value);
    Colors::DrawSettingHueWheel("Enemy outline", &settings.vq_color_enemy_outline.value);
    ImGui::SliderFloat("Enemy marker size", &settings.vq_enemy_marker_size, 3.0f, 20.0f, "%.0f");
    ImGui::Unindent();

    if (static_changed) BuildStaticMapGeometry();
    if (fog_changed) {
        InitFogBuffer();
        RebuildFrontierBorder();
    }
    if (rebuild_compass) RebuildCompassCircle();
}

bool VanquishMapOverlayWidget::ContextMenuItems()
{
    if (!Instance().visible || !ToolboxUtils::IsExplorable()) return true;
    if (nav_active) {
        if (ImGui::Button("Stop navigating")) {
            StopNavigating();
            return false;
        }
    }
    else {
        if (ImGui::Button("Navigate to closest enemy")) {
            nav_active = true;
            NavigateToClosestEnemy();
            return false;
        }
    }
    return true;
}

void VanquishMapOverlayWidget::GetTrackedEnemyCounts(int& alive, int& stale)
{
    alive = 0;
    stale = 0;
    for (size_t i = 0, len = std::min(tracked_enemies_by_agent_id.size(), highest_trackable_agent_id + 1); i < len; i++) {
        const auto& enemy = tracked_enemies_by_agent_id[i];
        if (enemy.state == EnemyState::Alive) alive++;
        else if (enemy.state == EnemyState::Stale) stale++;
    }
}

bool VanquishMapOverlayWidget::IsOverlayActive() { return should_draw; }
bool VanquishMapOverlayWidget::IsNavigating() { return nav_active; }
void VanquishMapOverlayWidget::StopNavigating() { ::StopNavigating(); }
