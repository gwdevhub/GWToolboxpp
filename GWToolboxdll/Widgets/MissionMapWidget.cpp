#include "stdafx.h"

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <ImGuiAddons.h>
#include <Modules/QuestModule.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Utils/ToolboxUtils.h>
#include <GWCA/Context/WorldContext.h>
#include <D3DContainers.h>



namespace {
    bool draw_all_terrain_lines = false;
    bool draw_all_minimap_lines = true;
    bool show_vq_overlay = false; // master toggle for all VQ features on mission map
    bool show_vq_toggle_button = true;

    // VQ overlay colours — configurable via settings
    Color vq_color_inaccessible = IM_COL32(0, 0, 0, 190);
    Color vq_color_fog_unexplored = IM_COL32(0, 0, 0, 140);
    Color vq_color_border = IM_COL32(200, 220, 255, 160);
    Color vq_color_frontier = IM_COL32(255, 200, 50, 200);
    Color vq_color_compass = IM_COL32(180, 220, 255, 100);
    Color vq_color_enemy_alive = IM_COL32(70, 130, 255, 255);
    Color vq_color_enemy_stale = IM_COL32(255, 180, 50, 180);
    Color vq_color_enemy_outline = IM_COL32(0, 0, 0, 200);
    float vq_enemy_marker_size = 9.0f;

    // Enemy tracking for VQ assistance

    enum class EnemyState { NotApplicable, Alive, Stale };
    struct TrackedEnemy {
        GW::Vec2f pos;
        float rotation = 0.f; // last known movement direction
        EnemyState state = EnemyState::NotApplicable;
    };
    std::vector<TrackedEnemy> tracked_enemies_by_agent_id; // agent_id -> tracked enemy
    size_t highest_trackable_agent_id = 0;

    GW::Constants::MapID tracked_enemies_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType tracked_enemies_instance_type = GW::Constants::InstanceType::Loading;

    constexpr float TWO_PI = 6.2831853f;
    constexpr float COMPASS_RANGE = GW::Constants::Range::Compass;
    constexpr float STALE_CHECK_RANGE = COMPASS_RANGE * 0.9f;

    bool should_draw_vq_overlay = false;

    // Pixel-to-game-unit scale — converts pixel thickness to game units
    float cached_px_to_game = 1.f;


    D3DTriangleBuffer unexplored_area;
    D3DTriangleBuffer frontier_border;
    D3DTriangleBuffer minimap_lines;
    D3DTriangleBuffer inaccessible_area_and_borders;
    D3DTriangleBuffer enemy_vertex_buffer;
    D3DCircle compass_circle;
    // Used to easily terminate hanging buffers later
    D3DVertexBuffer* vertex_buffers[] = {&unexplored_area, &frontier_border, &minimap_lines, &inaccessible_area_and_borders, &enemy_vertex_buffer, &compass_circle};

    constexpr float EXPLORE_CELL_SIZE = GW::Constants::Range::Adjacent / 2.f;
    constexpr size_t MAX_MAP_WIDTH = 50000;
    constexpr size_t MAX_CELLS_PER_AXIS = (size_t)(MAX_MAP_WIDTH / EXPLORE_CELL_SIZE);

    // Exploration tracking (fog of war)
    
    bool* explored_cells = nullptr; // parallel to cached_walkable_grid, same dimensions
    GW::Constants::MapID explored_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType explored_instance_type = GW::Constants::InstanceType::Loading;

    struct BorderSegment {
        GW::Vec2f p1, p2;
    };

    std::vector<BorderSegment> cached_border_segments;

    // Static cached circle — built once per map/zoom change, centred on game origin
    constexpr int COMPASS_CIRCLE_SEGMENTS = 64;
    constexpr float COMPASS_CIRCLE_THICKNESS_PX = 0.5f;


    bool* cached_walkable_grid = nullptr;
    int cached_walkable_grid_size = 0;

    int cached_grid_x0 = 0, cached_grid_y0 = 0, cached_grid_w = 0, cached_grid_h = 0;
    GW::Constants::MapID border_map_id = static_cast<GW::Constants::MapID>(0);
    float border_cached_zoom = 0.0f; // zoom level used when static geometry was last built




    const D3DMATRIX IDENTITY_MATRIX = {{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f}};

    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords);

    struct GameToScreenBasis {
        float ox, oy;
        float ax, ay;
        float bx, by;
        bool valid = false;
        clock_t last_rebuild = TIMER_INIT();

        float cached_basis_zoom = 0.f;
        void Rebuild(); // defined after namespace variables

        void Project(float gx, float gy, float& sx, float& sy) const
        {
            sx = ox + gx * ax + gy * bx;
            sy = oy + gx * ay + gy * by;
        }
    } g2s;


    // Returns the flat index into cached_walkable_grid / explored_cells for
    // a given absolute grid coord, or -1 if out of bounds.
    int GetCellIndex(int gx, int gy)
    {
        const int lx = gx - cached_grid_x0;
        const int ly = gy - cached_grid_y0;
        if (lx < 0 || lx >= cached_grid_w || ly < 0 || ly >= cached_grid_h) return -1;
        return ly * cached_grid_w + lx;
    }

    bool IsCellExplored(int gx, int gy)
    {
        if (!explored_cells) return false;
        const int idx = GetCellIndex(gx, gy);
        return idx >= 0 && explored_cells[idx];
    }

    void UpdateExploration(const GW::GamePos& player_pos)
    {
        // Movement range check
        static GW::GamePos last_check = {};
        if (GW::GetSquareDistance(last_check, player_pos) < GW::Constants::SqrRange::Adjacent) return;
        last_check = player_pos;

        const auto map_id = GW::Map::GetMapID();
        const auto instance_type = GW::Map::GetInstanceType();
        if (map_id != explored_map_id || instance_type != explored_instance_type) {
            delete[] explored_cells;
            explored_cells = nullptr;
            explored_map_id = map_id;
            explored_instance_type = instance_type;
        }

        // Allocate parallel to walkable grid when first entering an explorable area
        if (!explored_cells && cached_walkable_grid_size > 0) {
            explored_cells = new bool[cached_walkable_grid_size](); // zero-init
        }
        if (!explored_cells) return;

        constexpr int range_cells = static_cast<int>(COMPASS_RANGE / EXPLORE_CELL_SIZE) + 1;
        const int player_cx = static_cast<int>(floorf(player_pos.x / EXPLORE_CELL_SIZE));
        const int player_cy = static_cast<int>(floorf(player_pos.y / EXPLORE_CELL_SIZE));
        constexpr float range_sq = COMPASS_RANGE * COMPASS_RANGE;

        for (int dy = -range_cells; dy <= range_cells; dy++) {
            for (int dx = -range_cells; dx <= range_cells; dx++) {
                const int gx = player_cx + dx;
                const int gy = player_cy + dy;
                const float cell_center_x = (gx + 0.5f) * EXPLORE_CELL_SIZE;
                const float cell_center_y = (gy + 0.5f) * EXPLORE_CELL_SIZE;
                const float ddx = cell_center_x - player_pos.x;
                const float ddy = cell_center_y - player_pos.y;
                if (ddx * ddx + ddy * ddy > range_sq) continue;
                const int idx = GetCellIndex(gx, gy);
                if (idx >= 0) explored_cells[idx] = true;
            }
        }
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

    void OnMinimapZoomChanged() {
        
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

        // 4 strips covering everything outside the grid
        inaccessible_area_and_borders.push_back(D3DQuad({gx0 - ext, gy0 - ext}, {gx1 + ext, gy0}, vq_color_inaccessible));
        inaccessible_area_and_borders.push_back(D3DQuad({gx0 - ext, gy1}, {gx1 + ext, gy1 + ext}, vq_color_inaccessible));
        inaccessible_area_and_borders.push_back(D3DQuad({gx0 - ext, gy0}, {gx0, gy1}, vq_color_inaccessible));
        inaccessible_area_and_borders.push_back(D3DQuad({gx1, gy0}, {gx1 + ext, gy1}, vq_color_inaccessible));

        // merge horizontally-adjacent inaccessible cells into single quads per row
        for (int gy = cached_grid_y0; gy < cached_grid_y0 + cached_grid_h; gy++) {
            const float y0 = gy * EXPLORE_CELL_SIZE;
            const float y1 = y0 + EXPLORE_CELL_SIZE;
            int run_start = -1;
            for (int gx = cached_grid_x0; gx < cached_grid_x0 + cached_grid_w; gx++) {
                if (!IsGridCellWalkable(gx, gy)) {
                    if (run_start == -1) run_start = gx; // start a new run
                }
                else {
                    if (run_start != -1) {
                        // flush run
                        inaccessible_area_and_borders.push_back(D3DQuad({run_start * EXPLORE_CELL_SIZE, y0}, {gx * EXPLORE_CELL_SIZE, y1}, vq_color_inaccessible));
                        run_start = -1;
                    }
                }
            }
            if (run_start != -1) {
                // flush run at end of row
                inaccessible_area_and_borders.push_back(D3DQuad({run_start * EXPLORE_CELL_SIZE, y0}, {(cached_grid_x0 + cached_grid_w) * EXPLORE_CELL_SIZE, y1}, vq_color_inaccessible));
            }
        }

        for (const auto& seg : cached_border_segments)
            inaccessible_area_and_borders.push_back(D3DLine(seg.p1, seg.p2, border_thickness_game, vq_color_border));
    }

    void RebuildMapBorder()
    {
        cached_border_segments.clear();
        delete[] cached_walkable_grid;
        cached_walkable_grid = nullptr;
        cached_walkable_grid_size = 0;
        delete[] explored_cells;
        explored_cells = nullptr;
        explored_map_id = static_cast<GW::Constants::MapID>(0);

        const auto pathing_map = GW::Map::GetPathingMap();
        if (!pathing_map) return;

        std::vector<const GW::PathingTrapezoid*> traps;
        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
        for (size_t p = 0; p < pathing_map->size(); p++) {
            const auto& plane = pathing_map->at(p);
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                traps.push_back(&trap);
                min_x = std::min({min_x, trap.XTL, trap.XTR, trap.XBL, trap.XBR});
                max_x = std::max({max_x, trap.XTL, trap.XTR, trap.XBL, trap.XBR});
                min_y = std::min(min_y, trap.YB);
                max_y = std::max(max_y, trap.YT);
            }
        }
        if (traps.empty()) return;

        cached_grid_x0 = static_cast<int>(floorf(min_x / EXPLORE_CELL_SIZE)) - 1;
        cached_grid_y0 = static_cast<int>(floorf(min_y / EXPLORE_CELL_SIZE)) - 1;
        cached_grid_w = static_cast<int>(ceilf(max_x / EXPLORE_CELL_SIZE)) + 1 - cached_grid_x0;
        cached_grid_h = static_cast<int>(ceilf(max_y / EXPLORE_CELL_SIZE)) + 1 - cached_grid_y0;

        cached_walkable_grid_size = cached_grid_w * cached_grid_h;
        cached_walkable_grid = new bool[cached_walkable_grid_size]();

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
    }

    void ClearQuestMarker()
    {
        GW::GameThread::Enqueue([] {
            QuestModule::SetCustomQuestMarker({0, 0});
        });
    }

    bool nav_active = false;
    GW::Vec2f nav_target_pos;
    GW::Vec2f nav_marker_pos;
    bool nav_marker_set = false;
    bool nav_marker_hidden = false;

    constexpr float MARKER_UPDATE_DIST = 500.0f * 500.f;

    void StopNavigating()
    {
        nav_active = false;
        nav_target_pos = {};
        nav_marker_pos = {};
        nav_marker_set = false;
        nav_marker_hidden = false;
        ClearQuestMarker();
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
                ClearQuestMarker();
                return;
            }
            if (in_compass) return;

            if (nav_marker_hidden) {
                nav_marker_hidden = false;
                nav_marker_set = false;
            }
        }

        if (nav_marker_set && GW::GetSquareDistance(target,nav_marker_pos) < MARKER_UPDATE_DIST) return;

        nav_marker_pos = target;
        nav_marker_set = true;
        GW::Vec2f world_pos;
        if (WorldMapWidget::GamePosToWorldMap(target, world_pos)) {
            GW::GameThread::Enqueue([world_pos] {
                QuestModule::SetCustomQuestMarker(world_pos, true);
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

        for (size_t agent_id = 0, len = highest_trackable_agent_id; agent_id <= len; agent_id++) {
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
                ClearQuestMarker();
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

        const auto* player = GW::Agents::GetControlledCharacter();
        const auto player_pos = GW::PlayerMgr::GetPlayerPosition();
        const auto* agents = player_pos ? GW::Agents::GetAgentArray() : nullptr;
        if (!agents) return;

        // Skip stale detection when dead or on the first frame after respawn —
        // agents near the death location vanish from the array during teleport
        static bool was_dead = false;
        const auto* player_living = player ? player->GetAsAgentLiving() : nullptr;
        const bool is_dead = !player_living || !player_living->GetIsAlive();
        if (is_dead) { was_dead = true; return; }
        if (was_dead) { was_dead = false; return; }

        if (tracked_enemies_by_agent_id.size() < agents->size()) tracked_enemies_by_agent_id.resize(agents->size());

        for (size_t agent_id = 0, len = agents->size(); agent_id < len; agent_id++) {
            const auto agent = agents->at(agent_id);
            auto& tracked = tracked_enemies_by_agent_id[agent_id];
            if (!agent) {
                // Agent not in radar — only mark stale if we're close enough to
                // their last known position that we should be able to see them
                if (tracked.state == EnemyState::Alive) {
                    if (GW::GetSquareDistance(*player_pos,tracked.pos) < stale_range_sq) {
                        tracked.state = EnemyState::Stale;
                    }
                }
                if (tracked.state != EnemyState::NotApplicable) {
                    highest_trackable_agent_id = agent_id;
                }
                continue;
            }
            const auto* living = agent->GetAsAgentLiving();

            if (!living || living->allegiance != GW::Constants::Allegiance::Enemy || !living->GetIsAlive()) {
                tracked.state = EnemyState::NotApplicable;
                continue;
            }
            auto* npc = GW::Agents::GetNPCByID(living->player_number);
            if (npc && (npc->IsSpirit() || npc->IsMinion())) {
                tracked.state = EnemyState::NotApplicable;
                continue;
            }

            tracked.pos = {living->pos.x, living->pos.y};
            tracked.rotation = living->rotation_angle;
            tracked.state = EnemyState::Alive;
            highest_trackable_agent_id = agent_id;
        }
        // Handle entries beyond current agent array size (previously tracked, now out of range)
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

        const float radius = vq_enemy_marker_size * cached_px_to_game;
        const float radius_outer = radius + cached_px_to_game;
        teardrop_fill.SetRadius(radius);
        teardrop_outline.SetRadius(radius_outer);
        teardrop_outline.SetColor(vq_color_enemy_outline);

        for (size_t i = 0, len = std::min(tracked_enemies_by_agent_id.size(),highest_trackable_agent_id + 1); i < len; i++) {
            auto& enemy = tracked_enemies_by_agent_id[i];
            if (enemy.state == EnemyState::NotApplicable) continue;
            const DWORD color = enemy.state == EnemyState::Stale ? vq_color_enemy_stale : vq_color_enemy_alive;
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

    GW::Vec2f mission_map_top_left;
    GW::Vec2f mission_map_bottom_right;
    GW::Vec2f mission_map_scale;
    float mission_map_zoom;

    GW::Vec2f mission_map_center_pos;
    GW::Vec2f mission_map_last_click_location;
    GW::Vec2f current_pan_offset;

    GW::Vec2f mission_map_screen_pos;
    GW::Vec2f world_map_click_pos;

    MinimapRenderContext mission_map_render_context;

    bool right_clicking = false;

    GW::UI::Frame* mission_map_frame = nullptr;

    bool IsScreenPosOnMissionMap(const GW::Vec2f& screen_pos)
    {
        if (!(mission_map_frame && mission_map_frame->IsVisible())) return false;
        return (screen_pos.x >= mission_map_top_left.x && screen_pos.x <= mission_map_bottom_right.x && screen_pos.y >= mission_map_top_left.y && screen_pos.y <= mission_map_bottom_right.y);
    }

    GW::Vec2f GetMissionMapScreenCenterPos()
    {
        return mission_map_top_left + (mission_map_bottom_right - mission_map_top_left) / 2;
    }

    bool WorldMapCoordsToMissionMapScreenPos(const GW::Vec2f& world_map_position, GW::Vec2f& screen_coords)
    {
        const auto offset = (world_map_position - current_pan_offset);
        const auto scaled_offset = GW::Vec2f(offset.x * mission_map_scale.x, offset.y * mission_map_scale.y);
        screen_coords = (scaled_offset * mission_map_zoom + mission_map_screen_pos);
        return true;
    }

    GW::Vec2f ScreenPosToMissionMapCoords(const GW::Vec2f screen_position)
    {
        GW::Vec2f unscaled_offset = (screen_position - mission_map_screen_pos) / mission_map_zoom;
        GW::Vec2f offset(unscaled_offset.x / mission_map_scale.x, unscaled_offset.y / mission_map_scale.y);
        return offset + current_pan_offset;
    }

    // Used for screen-space geometry (lines, enemy markers, viewport culling)
    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords)
    {
        GW::Vec2f world_map_pos;
        return WorldMapWidget::GamePosToWorldMap(game_map_position, world_map_pos) && WorldMapCoordsToMissionMapScreenPos(world_map_pos, screen_coords);
    }

    void GameToScreenBasis::Rebuild()
    {
        valid = false;

        const auto* mm_ctx = GW::Map::GetMissionMapContext();
        if (!mm_ctx || !mm_ctx->h003c) return;

        const auto player_pos = GW::PlayerMgr::GetPlayerPosition();
        if (!player_pos) return;
        const float px = player_pos->x;
        const float py = player_pos->y;

        // Only recompute basis vectors when zoom changes — they're stable
        // across panning. Recomputing every frame causes floating-point jitter.
        if (mission_map_zoom != cached_basis_zoom || !valid) {
            cached_basis_zoom = mission_map_zoom;

            constexpr float STEP = 1000.f;
            GW::Vec2f s00, s10, s01;
            if (!GamePosToMissionMapScreenPos({px, py, 0}, s00) ||
                !GamePosToMissionMapScreenPos({px + STEP, py, 0}, s10) ||
                !GamePosToMissionMapScreenPos({px, py + STEP, 0}, s01)) return;

            ax = (s10.x - s00.x) / STEP;
            ay = (s10.y - s00.y) / STEP;
            bx = (s01.x - s00.x) / STEP;
            by = (s01.y - s00.y) / STEP;
        }

        // Compute origin from the player's known mission map position,
        // not from the world-map-based s00 which is wrong for underground maps.
        const GW::Vec2f mm_pos = mm_ctx->h003c->player_mission_map_pos;
        const GW::Vec2f mm_offset = mm_pos - current_pan_offset;
        const GW::Vec2f mm_scaled = {mm_offset.x * mission_map_scale.x, mm_offset.y * mission_map_scale.y};
        const GW::Vec2f player_screen = {mm_scaled.x * mission_map_zoom + mission_map_screen_pos.x,
                                         mm_scaled.y * mission_map_zoom + mission_map_screen_pos.y};

        // Snap origin to nearest pixel to reduce sub-pixel flicker on thin lines
        ox = roundf(player_screen.x - px * ax - py * bx);
        oy = roundf(player_screen.y - px * ay - py * by);
        valid = true;
    }

    void Draw(IDirect3DDevice9*);

    std::vector<GW::UI::UIMessage> messages_hit;
    GW::UI::UIInteractionCallback OnMissionMap_UICallback_Func = nullptr, OnMissionMap_UICallback_Ret = nullptr;

    void OnMissionMap_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) mission_map_frame = nullptr;
        OnMissionMap_UICallback_Ret(message, wparam, lparam);
        if (message->message_id == GW::UI::UIMessage::kInitFrame) mission_map_frame = GW::UI::GetFrameById(message->frame_id);
        GW::Hook::LeaveHook();
    }

    bool HookMissionMapFrame()
    {
        if (OnMissionMap_UICallback_Func) return true;

        const auto mission_map_context = GW::Map::GetMissionMapContext();
        mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
        if (!(mission_map_frame && mission_map_frame->frame_callbacks[0].callback)) return false;
        OnMissionMap_UICallback_Func = mission_map_frame->frame_callbacks[0].callback;
        GW::Hook::CreateHook((void**)&OnMissionMap_UICallback_Func, OnMissionMap_UICallback, (void**)&OnMissionMap_UICallback_Ret);
        GW::Hook::EnableHooks(OnMissionMap_UICallback_Func);
        return true;
    }

    bool InitializeMissionMapParameters()
    {
        const auto gameplay_context = GW::GetGameplayContext();
        const auto mission_map_context = GW::Map::GetMissionMapContext();
        if (!(gameplay_context && mission_map_frame && mission_map_frame->IsVisible())) return false;

        const auto root = GW::UI::GetRootFrame();
        mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
        mission_map_bottom_right = mission_map_frame->position.GetContentBottomRight(root);
        mission_map_scale = mission_map_frame->position.GetViewportScale(root);
        mission_map_zoom = gameplay_context->mission_map_zoom;
        mission_map_center_pos = mission_map_context->player_mission_map_pos;
        mission_map_last_click_location = mission_map_context->last_mouse_location;
        current_pan_offset = mission_map_context->h003c->mission_map_pan_offset;
        mission_map_screen_pos = GetMissionMapScreenCenterPos();
        return true;
    }

    bool MissionMapContextMenu(void*)
    {
        if (!(mission_map_frame && mission_map_frame->IsVisible())) return false;
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_click_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        if (ImGui::Button("Place Marker")) {
            nav_active = false;
            nav_target_pos = {};
            nav_marker_pos = {};
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        if (QuestModule::GetCustomQuestMarker() && !nav_active) {
            if (ImGui::Button("Remove Marker")) {
                ClearQuestMarker();
                return false;
            }
        }
        if (show_vq_overlay) {
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
        }

        return true;
    }

    // ---------------------------------------------------------------------------
    // Matrix helpers
    // ---------------------------------------------------------------------------

    // Screen-space ortho: maps pixel coords [0,w]x[0,h] -> NDC [-1,1]x[1,-1]
    D3DMATRIX MakeOrthoProjection(float w, float h)
    {
        // clang-format off
        D3DMATRIX m = {{
             2.f/w,   0.f,  0.f, 0.f,
              0.f,  -2.f/h, 0.f, 0.f,
              0.f,   0.f,  1.f, 0.f,
             -1.f,   1.f,  0.f, 1.f
        }};
        // clang-format on
        return m;
    }

    // -----------------------------------------------------------------------
    // DrawEnemyCountLabel — ImGui overlay showing VQ kill counts
    // -----------------------------------------------------------------------
    void DrawEnemyCountLabel()
    {
        if (!show_vq_overlay) return;

        int alive_count = 0, stale_count = 0;
        for (size_t i = 0, len = std::min(tracked_enemies_by_agent_id.size(),highest_trackable_agent_id + 1); i < len; i++) {
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
        const ImVec2 text_pos(mission_map_top_left.x + PADDING + btn_size, mission_map_bottom_right.y - ImGui::GetTextLineHeight() - PADDING);

        auto* draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddText({text_pos.x + 1, text_pos.y + 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText({text_pos.x - 1, text_pos.y - 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText({text_pos.x + 1, text_pos.y - 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText({text_pos.x - 1, text_pos.y + 1}, IM_COL32(0, 0, 0, 220), label);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), label);
    }

    struct D3DStateGuard {
        IDirect3DStateBlock9* block = nullptr;

        explicit D3DStateGuard(IDirect3DDevice9* dev) { dev->CreateStateBlock(D3DSBT_ALL, &block); }

        ~D3DStateGuard()
        {
            if (block) {
                block->Apply();
                block->Release();
            }
        }
    };

    void SubmitVertexBuffers(IDirect3DDevice9* dx_device)
    {
        const D3DStateGuard guard(dx_device);

        dx_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        dx_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dx_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dx_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dx_device->SetRenderState(D3DRS_LIGHTING, FALSE);
        dx_device->SetRenderState(D3DRS_ZENABLE, FALSE);

        RECT scissorRect;
        scissorRect.left = static_cast<LONG>(mission_map_top_left.x);
        scissorRect.top = static_cast<LONG>(mission_map_top_left.y);
        scissorRect.right = static_cast<LONG>(mission_map_bottom_right.x);
        scissorRect.bottom = static_cast<LONG>(mission_map_bottom_right.y);
        dx_device->SetScissorRect(&scissorRect);

        D3DVIEWPORT9 vp;
        dx_device->GetViewport(&vp);
        const D3DMATRIX ortho = MakeOrthoProjection(static_cast<float>(vp.Width), static_cast<float>(vp.Height));
        const D3DMATRIX gameToScreen = {{g2s.ax, g2s.ay, 0.f, 0.f, g2s.bx, g2s.by, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, g2s.ox, g2s.oy, 0.f, 1.f}};

        dx_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
        dx_device->SetTransform(D3DTS_VIEW, &IDENTITY_MATRIX);
        dx_device->SetTransform(D3DTS_PROJECTION, &ortho);

        if (should_draw_vq_overlay) {
            inaccessible_area_and_borders.Render(dx_device);
            unexplored_area.Render(dx_device); // NB: Includes frontier border!
            enemy_vertex_buffer.Render(dx_device);

            if (!compass_circle.empty()) {
                if (const auto* player = GW::Agents::GetControlledCharacter()) {
                    D3DMATRIX compassMatrix = gameToScreen;
                    compassMatrix._41 = roundf(g2s.ox + player->pos.x * g2s.ax + player->pos.y * g2s.bx);
                    compassMatrix._42 = roundf(g2s.oy + player->pos.x * g2s.ay + player->pos.y * g2s.by);
                    dx_device->SetTransform(D3DTS_WORLD, &compassMatrix);
                    compass_circle.Render(dx_device);
                    dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
                }
            }
        }
        minimap_lines.Render(dx_device);
    }

    // Returns true if the cell at flat local grid index `idx` is walkable and explored,
    // making the shared edge a frontier border. Caller is responsible for bounds checking.
    bool IsFrontierEdge(int idx)
    {
        if (!explored_cells) return false;
        return explored_cells[idx];
    }

    void BuildFogGeometry()
    {
        unexplored_area.clear();
        frontier_border.clear();
        if (!explored_cells || !cached_walkable_grid) return;
        const float FRONTIER_HALF_THICKNESS = cached_px_to_game;
        const float grid_origin_x = cached_grid_x0 * EXPLORE_CELL_SIZE;
        const float grid_origin_y = cached_grid_y0 * EXPLORE_CELL_SIZE;

        for (int gy = 0; gy < cached_grid_h; gy++) {
            const float y0 = grid_origin_y + gy * EXPLORE_CELL_SIZE;
            const float y1 = y0 + EXPLORE_CELL_SIZE;
            const int row_base = gy * cached_grid_w;
            int run_start = -1;

            for (int gx = 0; gx <= cached_grid_w; gx++) {
                const int idx = row_base + gx;
                const bool unexplored = gx < cached_grid_w && cached_walkable_grid[idx] && !explored_cells[idx];

                if (unexplored) {
                    if (run_start == -1) run_start = gx;

                    // frontier edges still need to be per-cell
                    const float x0 = grid_origin_x + gx * EXPLORE_CELL_SIZE;
                    const float x1 = x0 + EXPLORE_CELL_SIZE;
                    const auto edge = [&](bool cond, int neighbour_idx, float ax, float ay, float bx, float by) {
                        if (cond && IsFrontierEdge(neighbour_idx)) frontier_border.push_back(D3DLine({ax, ay}, {bx, by}, FRONTIER_HALF_THICKNESS, vq_color_frontier));
                    };
                    edge(gy > 0, idx - cached_grid_w, x0, y0, x1, y0);
                    edge(gy < cached_grid_h - 1, idx + cached_grid_w, x0, y1, x1, y1);
                    edge(gx > 0, idx - 1, x0, y0, x0, y1);
                    edge(gx < cached_grid_w - 1, idx + 1, x1, y0, x1, y1);
                }
                else {
                    if (run_start != -1) {
                        const float rx0 = grid_origin_x + run_start * EXPLORE_CELL_SIZE;
                        const float rx1 = grid_origin_x + gx * EXPLORE_CELL_SIZE;
                        unexplored_area.push_back(D3DQuad({rx0, y0}, {rx1, y1}, vq_color_fog_unexplored));
                        run_start = -1;
                    }
                }
            }
        }
        unexplored_area.push_back(frontier_border);
    }

    void DrawVanquishToggleButton() {
        constexpr float PADDING = 4.0f;
        const float btn_size = ImGui::GetTextLineHeight() + PADDING * 2;
        const ImVec2 btn_pos(mission_map_top_left.x + PADDING, mission_map_bottom_right.y - btn_size - PADDING);

        ImGui::SetNextWindowPos(btn_pos);
        ImGui::SetNextWindowSize({0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {0, 0});
        if (ImGui::Begin("##vq_toggle", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            if (show_vq_overlay) {
                if (ImGui::Button(ICON_FA_SKULL "##vq_off")) show_vq_overlay = false;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("VQ overlay active. Click to hide.");
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                if (ImGui::Button(ICON_FA_SKULL "##vq_on")) show_vq_overlay = true;
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("VQ overlay hidden. Click to show.");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void EnqueueMinimapLines(GW::Constants::MapID map_id)
    {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        minimap_lines.clear();
        minimap_lines.reserve(lines.size());
        if (lines.empty()) return;
        const auto player_pos = GW::PlayerMgr::GetPlayerPosition();
        const float LINE_HALF_THICKNESS = 1.f * cached_px_to_game;
        for (const auto& line : lines) {
            if (!line->visible) continue;
            if (!line->draw_on_mission_map && !(draw_all_minimap_lines && line->draw_on_minimap) && !(draw_all_terrain_lines && line->draw_on_terrain)) continue;
            if (line->map != map_id) continue;
            if (line->from_player_pos && player_pos) {
                minimap_lines.push_back(D3DLine(*player_pos, line->p2, LINE_HALF_THICKNESS, static_cast<DWORD>(line->color)));
            }
            else {
                minimap_lines.push_back(D3DLine(line->p1, line->p2, LINE_HALF_THICKNESS, static_cast<DWORD>(line->color)));
            }
            
        }
    }

    void RebuildCompassCircle() {
        compass_circle = D3DCircle({0.f, 0.f}, COMPASS_RANGE, COMPASS_CIRCLE_THICKNESS_PX * cached_px_to_game, (DWORD)vq_color_compass, COMPASS_CIRCLE_SEGMENTS);
    }
} // namespace

void MissionMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(draw_all_terrain_lines);
    LOAD_BOOL(draw_all_minimap_lines);
    LOAD_BOOL(show_vq_overlay);
    LOAD_BOOL(show_vq_toggle_button);

    LOAD_COLOR(vq_color_inaccessible);
    LOAD_COLOR(vq_color_fog_unexplored);
    LOAD_COLOR(vq_color_border);
    LOAD_COLOR(vq_color_frontier);
    LOAD_COLOR(vq_color_compass);
    LOAD_COLOR(vq_color_enemy_alive);
    LOAD_COLOR(vq_color_enemy_stale);
    LOAD_COLOR(vq_color_enemy_outline);
    LOAD_FLOAT(vq_enemy_marker_size);
}

void MissionMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(draw_all_terrain_lines);
    SAVE_BOOL(draw_all_minimap_lines);
    SAVE_BOOL(show_vq_overlay);
    SAVE_BOOL(show_vq_toggle_button);

    SAVE_COLOR(vq_color_inaccessible);
    SAVE_COLOR(vq_color_fog_unexplored);
    SAVE_COLOR(vq_color_border);
    SAVE_COLOR(vq_color_frontier);
    SAVE_COLOR(vq_color_compass);
    SAVE_COLOR(vq_color_enemy_alive);
    SAVE_COLOR(vq_color_enemy_stale);
    SAVE_COLOR(vq_color_enemy_outline);
    SAVE_FLOAT(vq_enemy_marker_size);
}

void MissionMapWidget::Draw(IDirect3DDevice9* dx_device)
{
    if (!visible || !GW::Map::GetIsMapLoaded() || !mission_map_frame || !mission_map_frame->IsVisible()) return;

    SubmitVertexBuffers(dx_device);
    if (should_draw_vq_overlay) 
        DrawEnemyCountLabel();
    if (show_vq_toggle_button && ToolboxUtils::IsExplorable())
        DrawVanquishToggleButton();
}
void MissionMapWidget::Update(float)
{
    if (!HookMissionMapFrame()) return;
    if (!InitializeMissionMapParameters()) return;
    g2s.Rebuild();
    if (!g2s.valid) return;
    // Frame rate check
    static clock_t last_check = TIMER_INIT();
    if (!ToolboxUtils::FrameRateCheck(last_check, 30)) return;

    should_draw_vq_overlay = show_vq_overlay && ToolboxUtils::IsExplorable();

    // -----------------------------------------------------------------------
    // Custom renderer lines — screen space
    // -----------------------------------------------------------------------

    const auto map_id = GW::Map::GetMapID();

    const auto instance_type = GW::Map::GetInstanceType();
    static GW::Constants::InstanceType border_instance_type = GW::Constants::InstanceType::Loading;
    const bool map_changed = map_id != border_map_id || instance_type != border_instance_type;
    const bool zoom_changed = mission_map_zoom != border_cached_zoom;

    if (map_changed || zoom_changed) {
        border_cached_zoom = mission_map_zoom;
        cached_px_to_game = g2s.valid ? 1.0f / sqrtf(g2s.ax * g2s.ax + g2s.ay * g2s.ay) : EXPLORE_CELL_SIZE / 600.0f;
        BuildStaticMapGeometry();
        BuildFogGeometry();
        RebuildCompassCircle();
        if (map_changed) {
            border_map_id = map_id;
            border_instance_type = instance_type;
            RebuildMapBorder();
        }
    }

    EnqueueMinimapLines(map_id);    

    if (ToolboxUtils::IsExplorable()) {
        UpdateEnemyTracking();
        if (const auto player = GW::Agents::GetControlledCharacter()) {
            UpdateExploration(player->pos);
            BuildFogGeometry();
        }
    } else {
        if (nav_active) StopNavigating();
        // Reset tracking state so re-entering the same map triggers a fresh start
        if (tracked_enemies_map_id != GW::Constants::MapID::None) {
            tracked_enemies_by_agent_id.assign(tracked_enemies_by_agent_id.size(), {});
            tracked_enemies_map_id = GW::Constants::MapID::None;
            tracked_enemies_instance_type = GW::Constants::InstanceType::Loading;
            highest_trackable_agent_id = 0;
            enemy_vertex_buffer.clear();
        }
        // Reset exploration so fog of war restarts on the next explorable instance
        if (explored_map_id != GW::Constants::MapID::None) {
            delete[] explored_cells;
            explored_cells = nullptr;
            explored_map_id = GW::Constants::MapID::None;
            explored_instance_type = GW::Constants::InstanceType::Loading;
        }
    }

}

void MissionMapWidget::DrawSettingsInternal()
{
    ImGui::Checkbox("Draw all terrain lines", &draw_all_terrain_lines);
    ImGui::Checkbox("Draw all minimap lines", &draw_all_minimap_lines);
    ImGui::Checkbox("Show VQ toggle button on mission map", &show_vq_toggle_button);
    ImGui::Checkbox("Vanquish Overlay", &show_vq_overlay);
    ImGui::ShowHelp("Tracks enemy positions as they enter compass range.\nBlue = alive, Orange = last known (moved away).\nArrows on orange markers show last movement direction.\nAlso highlights areas you've explored during this session on the mission map.");

    if (show_vq_overlay) {
        ImGui::Indent();
        // Flag that triggers a static geometry rebuild if colour changes
        bool static_changed = false;
        bool fog_changed = false;

        static_changed |= Colors::DrawSettingHueWheel("Inaccessible area", &vq_color_inaccessible);
        static_changed |= Colors::DrawSettingHueWheel("Map border", &vq_color_border);
        fog_changed |= Colors::DrawSettingHueWheel("Unexplored fog", &vq_color_fog_unexplored);
        fog_changed |= Colors::DrawSettingHueWheel("Frontier edge", &vq_color_frontier);
        bool rebuild_compass = Colors::DrawSettingHueWheel("Compass range", &vq_color_compass);
        Colors::DrawSettingHueWheel("Enemy (alive)", &vq_color_enemy_alive);
        Colors::DrawSettingHueWheel("Enemy (last known position)", &vq_color_enemy_stale);
        Colors::DrawSettingHueWheel("Enemy outline", &vq_color_enemy_outline);
        ImGui::SliderFloat("Enemy marker size", &vq_enemy_marker_size, 3.0f, 20.0f, "%.0f");
        ImGui::Unindent();

        if (static_changed) BuildStaticMapGeometry();
        if (fog_changed) BuildFogGeometry();
        if (rebuild_compass) RebuildCompassCircle();
    }
}

bool MissionMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    switch (Message) {
        case WM_GW_RBUTTONCLICK:
            GW::Vec2f cursor_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (!IsScreenPosOnMissionMap(cursor_pos)) break;
            if (GW::UI::GetCurrentTooltip()) break;
            world_map_click_pos = ScreenPosToMissionMapCoords(cursor_pos);
            ImGui::SetContextMenu(MissionMapContextMenu);
            break;
    }
    return false;
}

void MissionMapWidget::Terminate()
{
    ToolboxWidget::Terminate();
    if (OnMissionMap_UICallback_Func) GW::Hook::RemoveHook(OnMissionMap_UICallback_Func);
    OnMissionMap_UICallback_Func = nullptr;
    delete[] cached_walkable_grid;
    cached_walkable_grid = nullptr;
    delete[] explored_cells;
    explored_cells = nullptr;

    for (auto buffer : vertex_buffers) {
        buffer->Terminate();
    }
}

void MissionMapWidget::GetTrackedEnemyCounts(int& alive, int& stale)
{
    alive = 0;
    stale = 0;
    for (size_t i = 0, len = highest_trackable_agent_id; i <= len && i < tracked_enemies_by_agent_id.size(); i++) {
        const auto& enemy = tracked_enemies_by_agent_id[i];
        if (enemy.state == EnemyState::Alive) alive++;
        else if (enemy.state == EnemyState::Stale) stale++;
    }
}
