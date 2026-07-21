#include "stdafx.h"

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <ImGuiAddons.h>
#include <Modules/CartographerModule.h>
#include <Modules/GwDatModule.h>
#include <Modules/QuestModule.h>
#include <Modules/Resources.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/SettingsWindow.h>
#include <Utils/ToolboxUtils.h>
#include <D3DContainers.h>

namespace {
    MissionMapWidget::Settings settings;

    IDirect3DPixelShader9* icon_tint_shader = nullptr;

    std::vector<MissionMapWidget::DrawCallback> draw_callbacks;
    std::vector<MissionMapWidget::ContextMenuCallback> context_menu_callbacks;
    std::vector<MissionMapWidget::OverlayCallback> overlay_callbacks;

    // Pixel-to-game-unit scale — converts pixel thickness to game units
    float cached_px_to_game = 1.f;

    bool render_ready = false;
    D3DMATRIX cached_game_to_screen = {};

    D3DTriangleBuffer minimap_lines;

    const D3DMATRIX IDENTITY_MATRIX = {{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f}};

    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords);
    void RenderMinimapLayers(IDirect3DDevice9* dx_device, const D3DMATRIX& game_to_screen, const D3DMATRIX& projection);

    struct GameToScreenBasis {
        float ox, oy;
        float ax, ay;
        float bx, by;
        bool valid = false;
        bool has_basis = false;

        float cached_basis_zoom = 0.f;
        void Rebuild();

        void Project(const float gx, const float gy, float& sx, float& sy) const
        {
            sx = ox + gx * ax + gy * bx;
            sy = oy + gx * ay + gy * by;
        }
    } g2s;

    GW::Vec2f mission_map_top_left;
    GW::Vec2f mission_map_bottom_right;
    GW::Vec2f mission_map_scale;
    float mission_map_zoom;

    GW::Vec2f mission_map_center_pos;
    GW::Vec2f mission_map_last_click_location;
    GW::Vec2f current_pan_offset;

    GW::Vec2f mission_map_screen_pos;
    GW::Vec2f world_map_click_pos;

    bool right_clicking = false;

    // Left-click-to-target: a press that doesn't drag (the game uses drag to pan) selects a target
    bool left_clicking = false;
    bool left_click_dragged = false;
    GW::Vec2f left_click_pos;
    constexpr float left_click_drag_threshold_sq = 25.f; // 5px

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

    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords)
    {
        GW::Vec2f world_map_pos;
        return WorldMapWidget::GamePosToWorldMap(game_map_position, world_map_pos) && WorldMapCoordsToMissionMapScreenPos(world_map_pos, screen_coords);
    }

    // Invert the game->screen basis so a screen click maps back to game coords.
    bool ScreenPosToGamePos(const GW::Vec2f& screen, GW::Vec2f& game)
    {
        const float det = g2s.ax * g2s.by - g2s.bx * g2s.ay;
        if (!g2s.valid || fabsf(det) < 1e-9f) return false;
        const float dx = screen.x - g2s.ox;
        const float dy = screen.y - g2s.oy;
        game = {(g2s.by * dx - g2s.bx * dy) / det, (-g2s.ay * dx + g2s.ax * dy) / det};
        return true;
    }

    void GameToScreenBasis::Rebuild()
    {
        valid = false;

        const auto* mm_ctx = GW::Map::GetMissionMapContext();
        if (!mm_ctx || !mm_ctx->h003c) return;
        const GW::Vec2f mm_pos = mm_ctx->h003c->player_mission_map_pos;

        // Use the controlled character's position when not spectating (works on
        // underground maps where WorldMapToGamePos returns wrong coordinates).
        // When spectating, mm_pos tracks the observed character so we must convert
        // it back to game coords to stay consistent with the origin calculation.
        float px, py;
        const auto* player = GW::Agents::GetControlledCharacter();
        const bool spectating = player && GW::Agents::GetObservingId() != player->agent_id;
        if (spectating) {
            GW::GamePos anchor_pos;
            if (!WorldMapWidget::WorldMapToGamePos(mm_pos, anchor_pos)) return;
            px = anchor_pos.x;
            py = anchor_pos.y;
        } else {
            if (!player) return;
            px = player->pos.x;
            py = player->pos.y;
        }

        if (mission_map_zoom != cached_basis_zoom || !has_basis) {
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
            has_basis = true;
        }

        const GW::Vec2f mm_offset = mm_pos - current_pan_offset;
        const GW::Vec2f mm_scaled = {mm_offset.x * mission_map_scale.x, mm_offset.y * mission_map_scale.y};
        const GW::Vec2f player_screen = {mm_scaled.x * mission_map_zoom + mission_map_screen_pos.x,
                                         mm_scaled.y * mission_map_zoom + mission_map_screen_pos.y};

        ox = roundf(player_screen.x - px * ax - py * bx);
        oy = roundf(player_screen.y - px * ay - py * by);
        valid = true;
    }

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
    bool mission_map_ready = false;
    bool InitializeMissionMapParameters()
    {
        const auto gameplay_context = GW::GetGameplayContext();
        const auto mission_map_context = GW::Map::GetMissionMapContext();
        if (!(gameplay_context && mission_map_context && mission_map_frame && mission_map_frame->IsVisible())) return false;

        const auto root = GW::UI::GetRootFrame();
        mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
        if (!std::isfinite(mission_map_top_left.x))
            return false;
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
            CartographerModule::OnUserMarkerAction();
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        if (QuestModule::GetCustomQuestMarker()) {
            if (ImGui::Button("Remove Marker")) {
                CartographerModule::OnUserMarkerAction();
                GW::GameThread::Enqueue([] { QuestModule::ClearCustomQuestMarker(); });
                return false;
            }
        }
        for (const auto& cb : context_menu_callbacks) {
            if (!cb()) return false;
        }

        return true;
    }

    // --- Walkable terrain overlay -----------------------------------------------
    // Shades non-walkable parts of the map grey and outlines walkable terrain.
    // Unlike the Vanquish overlay's map grid, this has no reachability/BFS, no
    // fog-of-war and no enemy tracking — it's a static rasterization of every
    // trapezoid in the pathing map, independent of the Vanquish widget entirely.
    constexpr float TERRAIN_CELL_SIZE = GW::Constants::Range::Adjacent / 2.f;

    struct TerrainTrapezoidSnapshot {
        float XTL, XTR, XBL, XBR, YB, YT;
    };

    struct TerrainBorderSegment {
        GW::Vec2f p1, p2;
    };

    bool IsCellWalkableInTerrainTrapezoid(int gx, int gy, const TerrainTrapezoidSnapshot& trap)
    {
        const float cx0 = gx * TERRAIN_CELL_SIZE;
        const float cy0 = gy * TERRAIN_CELL_SIZE;
        const float cx1 = cx0 + TERRAIN_CELL_SIZE;
        const float cy1 = cy0 + TERRAIN_CELL_SIZE;

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

    struct TerrainGridData {
        std::unique_ptr<bool[]> walkable;
        int x0 = 0, y0 = 0, w = 0, h = 0, size = 0;
        std::vector<TerrainBorderSegment> border_segments;
        std::vector<D3DVertex> static_vertices;

        int CellIndex(int gx, int gy) const
        {
            const int lx = gx - x0;
            const int ly = gy - y0;
            if (lx < 0 || lx >= w || ly < 0 || ly >= h) return -1;
            return ly * w + lx;
        }
        bool IsWalkable(int gx, int gy) const
        {
            const int idx = CellIndex(gx, gy);
            return idx >= 0 && walkable[idx];
        }
    };

    class TerrainOverlayBuffer : public D3DTriangleBuffer {
    public:
        void Adopt(std::vector<D3DVertex>&& verts)
        {
            vertices = std::move(verts);
            dirty = true;
        }
    };

    TerrainGridData terrain_grid;
    TerrainOverlayBuffer terrain_overlay_buffer;
    GW::Constants::MapID terrain_map_id = static_cast<GW::Constants::MapID>(0);
    float terrain_cached_zoom = 0.0f;
    bool terrain_rebuild_pending = false;

    template <size_t N>
    void AppendTerrainShape(std::vector<D3DVertex>& out, const D3DShape<N>& shape)
    {
        for (const auto& tri : shape.t)
            out.insert(out.end(), tri.v, tri.v + 3);
    }

    // Pure function, safe on a worker thread: build the grey shading + border vertices.
    void BuildTerrainOverlayVertices(const TerrainGridData& data, float px_to_game, std::vector<D3DVertex>& out)
    {
        out.clear();
        if (!data.walkable || data.size <= 0) return;

        const float border_thickness_game = px_to_game;
        const float gx0 = data.x0 * TERRAIN_CELL_SIZE;
        const float gy0 = data.y0 * TERRAIN_CELL_SIZE;
        const float gx1 = (data.x0 + data.w) * TERRAIN_CELL_SIZE;
        const float gy1 = (data.y0 + data.h) * TERRAIN_CELL_SIZE;
        const float ext = std::max(gx1 - gx0, gy1 - gy0) * 5.0f;
        const DWORD shade_color = settings.terrain_shade_color;

        AppendTerrainShape(out, D3DQuad({gx0 - ext, gy0 - ext}, {gx1 + ext, gy0}, shade_color));
        AppendTerrainShape(out, D3DQuad({gx0 - ext, gy1}, {gx1 + ext, gy1 + ext}, shade_color));
        AppendTerrainShape(out, D3DQuad({gx0 - ext, gy0}, {gx0, gy1}, shade_color));
        AppendTerrainShape(out, D3DQuad({gx1, gy0}, {gx1 + ext, gy1}, shade_color));

        for (int gy = data.y0; gy < data.y0 + data.h; gy++) {
            const float y0 = gy * TERRAIN_CELL_SIZE;
            const float y1 = y0 + TERRAIN_CELL_SIZE;
            int run_start = -1;
            for (int gx = data.x0; gx < data.x0 + data.w; gx++) {
                if (!data.IsWalkable(gx, gy)) {
                    if (run_start == -1) run_start = gx;
                }
                else if (run_start != -1) {
                    AppendTerrainShape(out, D3DQuad({run_start * TERRAIN_CELL_SIZE, y0}, {gx * TERRAIN_CELL_SIZE, y1}, shade_color));
                    run_start = -1;
                }
            }
            if (run_start != -1) {
                AppendTerrainShape(out, D3DQuad({run_start * TERRAIN_CELL_SIZE, y0}, {(data.x0 + data.w) * TERRAIN_CELL_SIZE, y1}, shade_color));
            }
        }

        const DWORD border_color = settings.terrain_border_color;
        for (const auto& seg : data.border_segments)
            AppendTerrainShape(out, D3DLine(seg.p1, seg.p2, border_thickness_game, border_color));
    }

    // Game thread only: copy trapezoid coords out so the worker thread can rasterize
    // without touching game memory.
    std::vector<TerrainTrapezoidSnapshot> SnapshotTerrainPathingMap()
    {
        std::vector<TerrainTrapezoidSnapshot> out;
        const auto pathing_map = GW::Map::GetPathingMap();
        if (!pathing_map) return out;
        for (size_t p = 0; p < pathing_map->size(); p++) {
            const auto& plane = pathing_map->at(p);
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                out.push_back({trap.XTL, trap.XTR, trap.XBL, trap.XBR, trap.YB, trap.YT});
            }
        }
        return out;
    }

    // Pure function, safe on a worker thread: rasterize the snapshot into a walkable
    // grid and compute border segments + vertex data.
    TerrainGridData ComputeTerrainGrid(const std::vector<TerrainTrapezoidSnapshot>& traps, float px_to_game)
    {
        TerrainGridData data;
        if (traps.empty()) return data;

        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
        for (const auto& trap : traps) {
            min_x = std::min({min_x, trap.XTL, trap.XTR, trap.XBL, trap.XBR});
            max_x = std::max({max_x, trap.XTL, trap.XTR, trap.XBL, trap.XBR});
            min_y = std::min(min_y, trap.YB);
            max_y = std::max(max_y, trap.YT);
        }

        data.x0 = static_cast<int>(floorf(min_x / TERRAIN_CELL_SIZE)) - 1;
        data.y0 = static_cast<int>(floorf(min_y / TERRAIN_CELL_SIZE)) - 1;
        data.w = static_cast<int>(ceilf(max_x / TERRAIN_CELL_SIZE)) + 1 - data.x0;
        data.h = static_cast<int>(ceilf(max_y / TERRAIN_CELL_SIZE)) + 1 - data.y0;
        data.size = data.w * data.h;
        data.walkable = std::make_unique<bool[]>(data.size);

        for (const auto& trap : traps) {
            const int ty0 = std::max(0, static_cast<int>(floorf(trap.YB / TERRAIN_CELL_SIZE)) - data.y0);
            const int ty1 = std::min(data.h - 1, static_cast<int>(ceilf(trap.YT / TERRAIN_CELL_SIZE)) - data.y0);
            const int tx0 = std::max(0, static_cast<int>(floorf(std::min(trap.XTL, trap.XBL) / TERRAIN_CELL_SIZE)) - data.x0);
            const int tx1 = std::min(data.w - 1, static_cast<int>(ceilf(std::max(trap.XTR, trap.XBR) / TERRAIN_CELL_SIZE)) - data.x0);
            for (int cy = ty0; cy <= ty1; cy++) {
                for (int cx = tx0; cx <= tx1; cx++) {
                    const int idx = cy * data.w + cx;
                    if (data.walkable[idx]) continue;
                    if (IsCellWalkableInTerrainTrapezoid(data.x0 + cx, data.y0 + cy, trap))
                        data.walkable[idx] = true;
                }
            }
        }

        for (int cy = 0; cy < data.h; cy++) {
            for (int cx = 0; cx < data.w; cx++) {
                if (!data.walkable[cy * data.w + cx]) continue;
                const int gx = data.x0 + cx, gy = data.y0 + cy;
                const float x0 = gx * TERRAIN_CELL_SIZE;
                const float y0 = gy * TERRAIN_CELL_SIZE;
                const float x1 = x0 + TERRAIN_CELL_SIZE;
                const float y1 = y0 + TERRAIN_CELL_SIZE;
                if (!data.IsWalkable(gx, gy - 1)) data.border_segments.push_back({{x0, y0}, {x1, y0}});
                if (!data.IsWalkable(gx, gy + 1)) data.border_segments.push_back({{x0, y1}, {x1, y1}});
                if (!data.IsWalkable(gx - 1, gy)) data.border_segments.push_back({{x0, y0}, {x0, y1}});
                if (!data.IsWalkable(gx + 1, gy)) data.border_segments.push_back({{x1, y0}, {x1, y1}});
            }
        }

        BuildTerrainOverlayVertices(data, px_to_game, data.static_vertices);
        return data;
    }

    // Async rebuild: rasterization runs on a worker thread; only the snapshot and the
    // final swap touch the game thread.
    void QueueRebuildTerrainGrid()
    {
        if (terrain_rebuild_pending) return;
        terrain_rebuild_pending = true;
        const auto snapshot = std::make_shared<std::vector<TerrainTrapezoidSnapshot>>(SnapshotTerrainPathingMap());
        const float px_to_game = cached_px_to_game;
        const auto map_id = GW::Map::GetMapID();
        Resources::EnqueueWorkerTask([snapshot, px_to_game, map_id] {
            const auto data = std::make_shared<TerrainGridData>(ComputeTerrainGrid(*snapshot, px_to_game));
            Resources::EnqueueMainTask([data, map_id] {
                terrain_rebuild_pending = false;
                if (map_id != GW::Map::GetMapID()) return;
                terrain_overlay_buffer.Adopt(std::move(data->static_vertices));
                terrain_grid = std::move(*data);
            });
        });
    }

    void UpdateTerrainOverlay()
    {
        if (!settings.show_walkable_terrain) return;

        const auto map_id = GW::Map::GetMapID();
        const bool map_changed = map_id != terrain_map_id;
        const bool zoom_changed = mission_map_zoom != terrain_cached_zoom;

        if (map_changed) {
            terrain_map_id = map_id;
            terrain_cached_zoom = mission_map_zoom;
            QueueRebuildTerrainGrid();
        }
        else if (zoom_changed && terrain_grid.walkable) {
            terrain_cached_zoom = mission_map_zoom;
            std::vector<D3DVertex> verts;
            BuildTerrainOverlayVertices(terrain_grid, cached_px_to_game, verts);
            terrain_overlay_buffer.Adopt(std::move(verts));
        }
    }

    void SubmitVertexBuffers(IDirect3DDevice9* dx_device)
    {
        const D3DStateGuard guard(dx_device);

        dx_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        dx_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dx_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dx_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dx_device->SetRenderState(D3DRS_LIGHTING, FALSE);
        dx_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        dx_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

        RECT scissorRect;
        scissorRect.left = static_cast<LONG>(floorf(mission_map_top_left.x));
        scissorRect.top = static_cast<LONG>(floorf(mission_map_top_left.y));
        scissorRect.right = static_cast<LONG>(ceilf(mission_map_bottom_right.x));
        scissorRect.bottom = static_cast<LONG>(ceilf(mission_map_bottom_right.y));
        dx_device->SetScissorRect(&scissorRect);

        D3DVIEWPORT9 vp;
        dx_device->GetViewport(&vp);
        const D3DMATRIX ortho = MakeOrthoProjection(static_cast<float>(vp.Width), static_cast<float>(vp.Height));
        const D3DMATRIX gameToScreen = {{g2s.ax, g2s.ay, 0.f, 0.f, g2s.bx, g2s.by, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, g2s.ox, g2s.oy, 0.f, 1.f}};

        dx_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
        dx_device->SetTransform(D3DTS_VIEW, &IDENTITY_MATRIX);
        dx_device->SetTransform(D3DTS_PROJECTION, &ortho);

        if (settings.show_walkable_terrain) terrain_overlay_buffer.Render(dx_device);

        for (const auto& cb : draw_callbacks) {
            cb(dx_device);
        }

        // When the overlay is off (default), draw the lines directly — a single DrawPrimitive with the
        // state already set above. Only the opt-in overlay pays for Minimap::Render (D3DSBT_ALL capture).
        if (settings.draw_minimap && Minimap::IsEnabled()) {
            RenderMinimapLayers(dx_device, gameToScreen, ortho);
        }
        else {
            dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
            minimap_lines.Render(dx_device);
        }
    }

    // Overlay the minimap layers on the mission map; mission-map lines feed ctx.lines (a context layer,
    // separate from the minimap's draw_on_minimap CustomRenderer). Only called when the overlay is on.
    void RenderMinimapLayers(IDirect3DDevice9* dx_device, const D3DMATRIX& game_to_screen, const D3DMATRIX& projection)
    {
        MinimapRenderContext ctx = Minimap::GetRenderContext(); // inherit user colours (symbols)
        ctx.top_left = {mission_map_top_left.x, mission_map_top_left.y};
        ctx.bottom_right = {mission_map_bottom_right.x, mission_map_bottom_right.y};
        ctx.view_override = &game_to_screen;
        ctx.projection_override = &projection;
        ctx.gwinches_per_pixel_override = 1.f / cached_px_to_game;
        ctx.lines = &minimap_lines;

        ctx.translation = {};
        ctx.zoom_scale = 1.f;
        ctx.circular_map = false;
        ctx.draw_center_marker = false;
        ctx.draw_custom = false; // lines come from ctx.lines; minimap markers/polygons are not wanted here

        ctx.draw_background = false;
        ctx.draw_cardinals = false; // mission map is always north-aligned
        ctx.draw_pmap = settings.draw_pmap;
        // The pmap shadow offset is applied in the view's output space, which for the mission map's
        // game->px view is raw pixels; express a small game-unit offset in px so it stays subtle and
        // scales with the mission-map zoom (the compass keeps the default, applied in its own space).
        constexpr float shadow_gwinches = 180.f;
        ctx.shadow_translation = shadow_gwinches / cached_px_to_game;
        ctx.draw_symbols = settings.draw_symbols;
        ctx.draw_ranges = settings.draw_ranges;
        ctx.draw_agents = settings.draw_agents;
        ctx.draw_pings = settings.draw_pings;
        ctx.draw_effects = settings.draw_effects;

        // Sub-renderers draw in game coords via the view override, so WORLD must be identity.
        dx_device->SetTransform(D3DTS_WORLD, &IDENTITY_MATRIX);
        Minimap::Render(dx_device, ctx);
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
            if (line->world_coords) continue; // world coords, not game coords; drawn by DrawWorldCoordRouteLines
            if (!line->draw_on_mission_map && !(settings.draw_all_minimap_lines && line->draw_on_minimap) && !(settings.draw_all_terrain_lines && line->draw_on_terrain)) continue;
            if (line->map != map_id) continue;
            if (line->from_player_pos && player_pos) {
                minimap_lines.push_back(D3DLine(*player_pos, line->p2, LINE_HALF_THICKNESS, static_cast<DWORD>(line->color)));
            }
            else {
                minimap_lines.push_back(D3DLine(line->p1, line->p2, LINE_HALF_THICKNESS, static_cast<DWORD>(line->color)));
            }
        }

    }

    // Cross-map route tails are stored in world-map coords (the only form that can place other
    // maps' positions). Draw them in an ImGui overlay via the world->mission-map transform,
    // clipped to the widget. These cover the whole route (incl. the current map's exit stretch
    // past the nearest portal, which the game-coord lines don't reach); the overlap with the
    // game-coord current-map lines is the same path, so it's harmless - matches the world map.
    void DrawWorldCoordRouteLines()
    {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        if (lines.empty()) return;

        auto* draw_list = ImGui::GetBackgroundDrawList();
        draw_list->PushClipRect({mission_map_top_left.x, mission_map_top_left.y}, {mission_map_bottom_right.x, mission_map_bottom_right.y}, true);
        const float thickness = 1.5f * mission_map_scale.x;
        for (const auto& line : lines) {
            if (!(line->visible && line->world_coords && line->draw_on_mission_map)) continue;
            GW::Vec2f s1, s2;
            WorldMapCoordsToMissionMapScreenPos({line->p1.x, line->p1.y}, s1);
            WorldMapCoordsToMissionMapScreenPos({line->p2.x, line->p2.y}, s2);
            draw_list->AddLine({s1.x, s1.y}, {s2.x, s2.y}, line->color, thickness);
        }
        draw_list->PopClipRect();
    }
} // namespace

void MissionMapWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
}

void MissionMapWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void MissionMapWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void MissionMapWidget::Draw(IDirect3DDevice9* dx_device)
{
    if (!visible || !mission_map_ready) return;

    SubmitVertexBuffers(dx_device);

    DrawWorldCoordRouteLines();

    if (!overlay_callbacks.empty()) {
        auto* draw_list = ImGui::GetBackgroundDrawList();
        draw_list->PushClipRect({mission_map_top_left.x, mission_map_top_left.y}, {mission_map_bottom_right.x, mission_map_bottom_right.y}, true);
        for (const auto cb : overlay_callbacks) {
            cb(draw_list);
        }
        draw_list->PopClipRect();
    }

    // POC: mission map icons, desaturated. Res shrine icon (maybe others) are wrong colour by default so we desaturate, but this impacts other icons!
    #if 0
    const auto* world_ctx = GW::GetWorldContext();
    if (!world_ctx) return;
    auto* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->PushClipRect({mission_map_top_left.x, mission_map_top_left.y}, {mission_map_bottom_right.x, mission_map_bottom_right.y});
    static constexpr ImU32 kAffiliationColors[] = {
        IM_COL32_WHITE,                   // 0: gray
        IM_COL32(0x20, 0x20, 0xFF, 0xFF), // 1: blue
        IM_COL32(0xFF, 0x20, 0x20, 0xFF), // 2: red
        IM_COL32(0xFF, 0xFF, 0x20, 0xFF), // 3: yellow
        IM_COL32(0x20, 0xFF, 0xFF, 0xFF), // 4: teal
        IM_COL32(0x80, 0x20, 0xFF, 0xFF), // 5: purple
        IM_COL32(0x20, 0xFF, 0x20, 0xFF), // 6: green
        IM_COL32_WHITE,                   // 7: gray (same as 0)
    };
    for (const auto& icon : world_ctx->mission_map_icons) {
        auto** tex = GwDatModule::LoadGreyscaleTextureFromFileId(icon.model_id);
        if (!tex || !*tex) continue;
        GW::Vec2f pos;
        GamePosToMissionMapScreenPos({icon.X, icon.Y}, pos);
        ImVec2 sz = {42.f, 42.f};
        const ImU32 tint = icon.option < std::size(kAffiliationColors) ? kAffiliationColors[icon.option] : IM_COL32_WHITE;
        draw_list->AddImage(*tex, {pos.x - sz.x / 2, pos.y - sz.y / 2}, {pos.x + sz.x / 2, pos.y + sz.y / 2}, {0, 0}, {1, 1}, tint);
    }
    draw_list->PopClipRect();
    #endif
}

void MissionMapWidget::Update(float)
{
    if (!HookMissionMapFrame()) return;
    mission_map_ready = InitializeMissionMapParameters();
    if (!mission_map_ready) return;

    g2s.Rebuild();
    if (!g2s.valid) { render_ready = false; return; }

    cached_px_to_game = 1.0f / sqrtf(g2s.ax * g2s.ax + g2s.ay * g2s.ay);
    cached_game_to_screen = {{g2s.ax, g2s.ay, 0.f, 0.f, g2s.bx, g2s.by, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, g2s.ox, g2s.oy, 0.f, 1.f}};
    render_ready = true;

    UpdateTerrainOverlay();

    // Frame rate check — only throttle minimap line updates
    static clock_t last_check = TIMER_INIT();
    if (!ToolboxUtils::FrameRateCheck(last_check, 30)) return;

    EnqueueMinimapLines(GW::Map::GetMapID());
}

void MissionMapWidget::DrawSettingsInternal()
{
    const bool minimap_enabled = Minimap::IsEnabled();
    const auto needs_minimap = [minimap_enabled] {
        if (!minimap_enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Enable the Minimap module to use this.");
    };

    ImGui::Checkbox("Show walkable terrain", &settings.show_walkable_terrain);
    ImGui::ShowHelp("Shades non-walkable parts of the map grey and outlines walkable terrain.\nIndependent of the Vanquish overlay — no enemy tracking, fog or navigation.");
    if (settings.show_walkable_terrain) {
        bool terrain_colors_changed = false;
        terrain_colors_changed |= Colors::DrawSettingHueWheel("Non-walkable shading", &settings.terrain_shade_color.value);
        terrain_colors_changed |= Colors::DrawSettingHueWheel("Walkable border", &settings.terrain_border_color.value);
        if (terrain_colors_changed && terrain_grid.walkable) {
            std::vector<D3DVertex> verts;
            BuildTerrainOverlayVertices(terrain_grid, cached_px_to_game, verts);
            terrain_overlay_buffer.Adopt(std::move(verts));
        }
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!minimap_enabled);
    ImGui::Checkbox("Draw all terrain lines", &settings.draw_all_terrain_lines);
    needs_minimap();
    ImGui::Checkbox("Draw all minimap lines", &settings.draw_all_minimap_lines);
    needs_minimap();

    ImGui::Separator();
    ImGui::Checkbox("Show minimap on mission map", &settings.draw_minimap);
    needs_minimap();
    ImGui::BeginDisabled(!settings.draw_minimap);
    ImGui::TextDisabled("Minimap layers drawn on the mission map");
    ImGui::Checkbox("Range rings", &settings.draw_ranges);
    needs_minimap();
    ImGui::Checkbox("Agents", &settings.draw_agents);
    needs_minimap();
    ImGui::Checkbox("Pings & drawings", &settings.draw_pings);
    needs_minimap();
    ImGui::Checkbox("Effects", &settings.draw_effects);
    needs_minimap();
    ImGui::Checkbox("Background", &settings.draw_background);
    needs_minimap();
    ImGui::Checkbox("Pathing map (terrain)", &settings.draw_pmap);
    needs_minimap();
    ImGui::Checkbox("Symbols", &settings.draw_symbols);
    needs_minimap();
    ImGui::Checkbox("Click to target", &settings.click_to_target);
    needs_minimap();
    ImGui::ShowHelp("Left-click (without dragging) on the mission map to target the nearest agent");
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (ImGui::Button("Customise minimap (colours, agents, ranges)...")) {
        SettingsWindow::Instance().NavigateToSection(Minimap::Instance().SettingsName());
    }
}

bool MissionMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    const GW::Vec2f cursor_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

    switch (Message) {
        case WM_GW_RBUTTONCLICK: {
            if (!IsScreenPosOnMissionMap(cursor_pos)) break;
            if (!mission_map_frame) break;
            if (GW::UI::GetCurrentTooltip()) break;
            world_map_click_pos = ScreenPosToMissionMapCoords(cursor_pos);
            ImGui::SetContextMenu(MissionMapContextMenu);
        } break;

        // A left click that doesn't drag selects a target; drags are left to the game (it pans the map), so never capture.
        case WM_LBUTTONDOWN: {
            left_clicking = settings.draw_minimap && Minimap::IsEnabled() && settings.click_to_target && IsScreenPosOnMissionMap(cursor_pos) && !GW::UI::GetCurrentTooltip();
            left_click_dragged = false;
            left_click_pos = cursor_pos;
        } break;
        case WM_MOUSEMOVE: {
            const float dx = cursor_pos.x - left_click_pos.x, dy = cursor_pos.y - left_click_pos.y;
            if (left_clicking && dx * dx + dy * dy > left_click_drag_threshold_sq) left_click_dragged = true;
        } break;
        case WM_LBUTTONUP: {
            if (left_clicking && !left_click_dragged && IsScreenPosOnMissionMap(cursor_pos)) {
                GW::Vec2f game_pos;
                if (ScreenPosToGamePos(left_click_pos, game_pos)) Minimap::SelectTarget(game_pos);
            }
            left_clicking = false;
        } break;
    }
    return false;
}

void MissionMapWidget::Terminate()
{
    ToolboxWidget::Terminate();
    if (OnMissionMap_UICallback_Func) GW::Hook::RemoveHook(OnMissionMap_UICallback_Func);
    OnMissionMap_UICallback_Func = nullptr;

    if (icon_tint_shader) {
        icon_tint_shader->Release();
        icon_tint_shader = nullptr;
    }

    minimap_lines.Terminate();
    terrain_overlay_buffer.Terminate();
    terrain_grid = {};
    terrain_map_id = static_cast<GW::Constants::MapID>(0);
    render_ready = false;
}

bool MissionMapWidget::IsRenderReady() { return render_ready && GW::Map::GetIsMapLoaded() && mission_map_frame && mission_map_frame->IsVisible(); }
const D3DMATRIX& MissionMapWidget::GetGameToScreenMatrix() { return cached_game_to_screen; }
GW::Vec2f MissionMapWidget::GetTopLeft() { return mission_map_top_left; }
GW::Vec2f MissionMapWidget::GetBottomRight() { return mission_map_bottom_right; }
GW::UI::Frame* MissionMapWidget::GetFrame() { return mission_map_frame; }
float MissionMapWidget::GetPxToGame() { return cached_px_to_game; }
float MissionMapWidget::GetZoom() { return mission_map_zoom; }

void MissionMapWidget::AddDrawCallback(DrawCallback cb) { draw_callbacks.push_back(cb); }
void MissionMapWidget::RemoveDrawCallback(DrawCallback cb) { std::erase(draw_callbacks, cb); }
void MissionMapWidget::AddContextMenuCallback(ContextMenuCallback cb) { context_menu_callbacks.push_back(cb); }
void MissionMapWidget::RemoveContextMenuCallback(ContextMenuCallback cb) { std::erase(context_menu_callbacks, cb); }
GW::Vec2f MissionMapWidget::GetContextMenuWorldMapPos() { return world_map_click_pos; }

void MissionMapWidget::AddOverlayCallback(OverlayCallback cb) { overlay_callbacks.push_back(cb); }
void MissionMapWidget::RemoveOverlayCallback(OverlayCallback cb) { std::erase(overlay_callbacks, cb); }

bool MissionMapWidget::WorldMapToScreen(const GW::Vec2f& world_map_pos, ImVec2& out)
{
    if (!mission_map_ready) return false;
    GW::Vec2f screen_pos;
    if (!WorldMapCoordsToMissionMapScreenPos(world_map_pos, screen_pos)) return false;
    out = {screen_pos.x, screen_pos.y};
    return true;
}

float MissionMapWidget::GetPxPerWorldMapUnit()
{
    return mission_map_ready ? mission_map_zoom * mission_map_scale.x : 0.f;
}
