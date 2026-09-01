#include "stdafx.h"

#include <unordered_map>

#include <GWCA/Context/MapContext.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <Timer.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/TerrainDrape.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <Windows/Pathfinding/NavMesh.h>           // 用于路径/覆盖层悬挂的每采样平面分辨率
#include <Windows/Pathfinding/PathfindingWindow.h> // GetResidentNavMesh
#include <ImGuiAddons.h>

#include "GWCA/GameEntities/Agent.h"
#include "GWCA/Managers/AgentMgr.h"

namespace {
    unsigned lerp_steps_per_line = 10;
    float render_max_distance = 5000.f;
    float fog_factor = 0.5f;
    bool need_sync_markers = true;
    bool render_under_ui = true;
    // 使用模板将圆形罗盘从小地图覆盖层中剔除，防止它们渗透到整个小地图上。
    bool exclude_compass = true;

    // 根据场景深度缓冲区测试覆盖层，使世界几何体遮挡它们。
    bool occlude_behind_terrain = false;

    float z_lift = 2.f; // raise above the surface so lines draw on top of terrain instead of z-fighting it (GW up is -z)

    GameWorldRenderer::RenderableVectors renderables;
    std::mutex renderables_mutex{};

    // 对 `renderables` 的哈希索引，每次同步时重建，以便 Sync* 传递以 O(1) 而非 O(N^2) 的方式重用已悬挂的多边形
    //（而 O(N^2) 匹配会导致导航网格覆盖层数千条边上的合成器绘制冻结）。
    std::unordered_multimap<uint64_t, size_t> renderable_index;

    uint64_t PolyMatchKey(const GameWorldRenderer::GenericPolyRenderable& p)
    {
        uint64_t h = 1469598103934665603ull; // FNV-1a 对 find_matching_poly 比较的字段
        const auto mix = [&h](uint32_t v) { h = (h ^ v) * 1099511628211ull; };
        mix(static_cast<uint32_t>(p.map_id));
        mix(p.col);
        mix(p.filled ? 1u : 0u);
        mix(static_cast<uint32_t>(p.points.size()));
        for (const auto& pt : p.points) {
            mix(*reinterpret_cast<const uint32_t*>(&pt.x));
            mix(*reinterpret_cast<const uint32_t*>(&pt.y));
            mix(pt.zplane);
        }
        return h;
    }

    // 我们注册到共享合成器的 UI 下层绘制令牌（0 = 未注册）。
    int compositor_token = 0;

    // 屏幕空间中的游戏罗盘地形圆圈（在框架内部），如果隐藏则返回 false。镜像 Minimap 的 RepositionMinimapToCompass：按 compass_padding 内缩，正方形化，内接。
    bool GetCompassTerrainCircle(float& cx, float& cy, float& radius)
    {
        const auto* frame = GW::UI::GetFrameByLabel(L"Compass");
        if (!frame || !frame->IsCreated() || !frame->IsVisible()) {
            return false;
        }
        constexpr float compass_padding = 1.05f;
        auto top_left = frame->position.GetTopLeftOnScreen(frame);
        auto bottom_right = frame->position.GetBottomRightOnScreen(frame);
        const float height = bottom_right.y - top_left.y;
        if (height <= 0.f) {
            return false;
        }
        const float diff = height - height / compass_padding;
        top_left.y += diff;
        top_left.x += diff;
        bottom_right.x -= diff;
        bottom_right.y = top_left.y + (bottom_right.x - top_left.x);
        cx = 0.5f * (top_left.x + bottom_right.x);
        cy = 0.5f * (top_left.y + bottom_right.y);
        radius = 0.5f * (bottom_right.x - top_left.x);
        return radius > 0.f;
    }

    // 在罗盘地形上的屏幕空间圆盘中设置/清除 `bit`（固定功能），以便将覆盖层挖空成圆形小地图；
    // 只触及 `bit` 以节省 GW 的模板，让调用者恢复管道。
    void MarkCompassStencil(IDirect3DDevice9* device, const float cx, const float cy, const float radius, const DWORD bit, const bool set)
    {
        struct ScreenVertex { float x, y, z, rhw; };
        constexpr unsigned segments = 64;
        ScreenVertex fan[segments + 2];
        fan[0] = {cx, cy, 0.f, 1.f};
        for (unsigned i = 0; i <= segments; i++) {
            const float a = static_cast<float>(i) / static_cast<float>(segments) * DirectX::XM_2PI;
            fan[i + 1] = {cx + radius * std::cos(a), cy + radius * std::sin(a), 0.f, 1.f};
        }

        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        device->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, bit);
        device->SetRenderState(D3DRS_STENCILREF, set ? bit : 0);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE);
        device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_REPLACE);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, segments, fan, sizeof(ScreenVertex));
    }

    constexpr GW::Vec2f lerp(const GW::Vec2f& a, const GW::Vec2f& b, const float t)
    {
        return a * t + b * (1.f - t);
    }

    constexpr auto ALTITUDE_UNKNOWN = TerrainDrape::kNoAltitude;

    // ===== 批处理导航网格覆盖层线缓冲 =====
    // 一个用于导航网格覆盖层数万条边的线列表缓冲（逐条 CustomLine 在每次地图加载时是 O(N^2) 且每次移动都重新悬挂）：
    // 在游戏线程上增量悬挂，单次绘制调用，仅渲染线程（无互斥锁）。
    struct NavmeshBatch {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;     // 实时顶点所属的地图
        GW::Constants::MapID pending_map = GW::Constants::MapID::None; // 正在为 `lines`/`staging` 构建的地图
        std::vector<GameWorldRenderer::BatchedLine> lines; // 源段（游戏坐标 + 颜色）正在悬挂
        std::vector<D3DVertex> verts;                      // 实时悬挂的线列表顶点（绘制）
        std::vector<D3DVertex> staging;                    // 下一组，增量悬挂；完成后交换
        size_t build_cursor = 0;                           // 下一个要悬挂到 staging 的源线
        bool building = false;                             // 正在进行 staging 构建
        IDirect3DVertexBuffer9* vb = nullptr;
        size_t vb_cap = 0;                                 // 顶点容量
        bool vb_dirty = false;                             // 自上次上传以来顶点已更改
    };
    NavmeshBatch navmesh_batch;
    float navmesh_sample_spacing = 5.f; // 悬挂覆盖层边时表面采样之间的游戏单位（用户可调）

    // 用于 2D 俯视图 M 键世界地图的完整网格（未悬挂）。WorldMapWidget 每帧重新绘制这些平面线条。
    std::vector<GameWorldRenderer::BatchedLine> navmesh_worldmap_lines;
    GW::Constants::MapID navmesh_worldmap_map = GW::Constants::MapID::None;

    // 将每帧的、受墙钟限制的切片悬挂到 `staging` 中；实时 `verts` 在完成前持续绘制，然后交换（双缓冲）因此重建不会使覆盖层空白。
    void StepNavmeshBatchBuild()
    {
        auto& b = navmesh_batch;
        if (!b.building) return;
        const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
        const uint32_t num_planes = pm ? static_cast<uint32_t>(pm->size()) : 0;
        if (!num_planes) return; // 路径图尚未就绪；下一帧重试
        const auto budget_timer = TIMER_INIT();
        const float spacing = std::max(1.f, navmesh_sample_spacing); // 用户可调：越小越贴合地面，顶点越多
        for (; b.build_cursor < b.lines.size(); ++b.build_cursor) {
            if (TIMER_DIFF(budget_timer) >= 2) break; // 预算用尽；下一帧继续
            const auto& ln = b.lines[b.build_cursor];
            const float dx = ln.b.x - ln.a.x, dy = ln.b.y - ln.a.y;
            const int steps = std::max(1, static_cast<int>(std::sqrt(dx * dx + dy * dy) / spacing));
            // Drape each sample on the edge's OWN plane (an edge lies on a single trapezoid, so that plane's heightfield
            // IS its surface): unlike a globally-closest query, an edge under a bridge stays on the ground.
            const uint32_t plane = ln.a.zplane; // == ln.b.zplane: both verts come from the same trapezoid
            auto surfaceZ = [&](float x, float y, float fallback) -> float {
                const float a = TerrainDrape::QueryAltAt(x, y, plane);
                if (a != 0.f) return a;                                      // edge's plane has floor here (the common case)
                const float c = TerrainDrape::ClosestZ(x, y, num_planes, fallback); // only at an edge lip with no plane surface
                return c == ALTITUDE_UNKNOWN ? fallback : c;
            };
            float prev = surfaceZ(ln.a.x, ln.a.y, 0.f);
            float px = ln.a.x, py = ln.a.y, pz = prev;
            for (int s = 1; s <= steps; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(steps);
                const float x = ln.a.x + dx * t, y = ln.a.y + dy * t;
                const float z = surfaceZ(x, y, prev);
                b.staging.push_back({px, py, pz, ln.color}); // LINELIST：每对连续顶点是一个子段
                b.staging.push_back({x, y, z, ln.color});
                px = x; py = y; pz = z; prev = z;
            }
        }
        if (b.build_cursor >= b.lines.size()) {
            // 暂存完成：交换为实时绘制集（原子操作 — 无空白帧）
            b.verts.swap(b.staging);
            b.staging.clear();
            b.map_id = b.pending_map;
            b.building = false;
            b.vb_dirty = true;
        }
    }

    // 当批处理 VB 增长或内容变化时（重新）创建并上传。
    bool EnsureNavmeshBatchVb(IDirect3DDevice9* device)
    {
        auto& b = navmesh_batch;
        const size_t need = b.verts.size();
        if (need < 2) return false;
        if (!b.vb || b.vb_cap < need) {
            if (b.vb) { b.vb->Release(); b.vb = nullptr; }
            const size_t cap = need + need / 2; // 预留空间，使增长构建不会每帧重新分配
            if (device->CreateVertexBuffer(static_cast<UINT>(cap * sizeof(D3DVertex)), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &b.vb, nullptr) != D3D_OK) {
                b.vb_cap = 0;
                return false;
            }
            b.vb_cap = cap;
            b.vb_dirty = true;
        }
        if (b.vb_dirty) {
            void* mem = nullptr;
            // flags=0，不是 D3DLOCK_DISCARD：DISCARD 需要 D3DUSAGE_DYNAMIC，但这是 MANAGED（如 RiverModule 的 VB），且批处理很少重建（每地图一次）。
            if (b.vb->Lock(0, static_cast<UINT>(need * sizeof(D3DVertex)), &mem, 0) != D3D_OK || !mem) return false;
            memcpy(mem, b.verts.data(), need * sizeof(D3DVertex));
            b.vb->Unlock();
            b.vb_dirty = false;
        }
        return true;
    }

    // 将整个导航网格绘制为一个线列表（在共享世界管道内调用，带罗盘模板）。
    void DrawNavmeshBatch(IDirect3DDevice9* device, GW::Constants::MapID map_id)
    {
        auto& b = navmesh_batch;
        if (b.map_id != map_id || b.verts.size() < 2) return;
        if (!EnsureNavmeshBatchVb(device)) return;
        const BOOL dotted_off[1] = {FALSE};
        device->SetPixelShaderConstantB(0, dotted_off, 1);
        if (device->SetStreamSource(0, b.vb, 0, sizeof(D3DVertex)) != D3D_OK) return;
        device->DrawPrimitive(D3DPT_LINELIST, 0, static_cast<UINT>(b.verts.size() / 2));
    }

    std::vector<GW::GamePos> circular_points_from_marker(const GW::GamePos& marker, const float size)
    {
        std::vector<GW::GamePos> points{};
        constexpr float pi = DirectX::XM_PI;
        constexpr size_t num_points_per_circle = 48;
        constexpr auto slice = 2.0f * pi / static_cast<float>(num_points_per_circle);
        for (auto i = 0u; i < num_points_per_circle; i++) {
            const auto angle = slice * static_cast<float>(i);
            points.emplace_back(marker.x + size * std::cos(angle), marker.y + size * std::sin(angle), marker.zplane);
        }
        points.push_back(points.at(0)); // 闭合环路
        return points;
    }

    GameWorldRenderer::GenericPolyRenderable* find_matching_poly(const GameWorldRenderer::GenericPolyRenderable& poly_to_find)
    {
        // 通过预构建索引重用已绘制的多边形（保持其悬挂的顶点缓冲）— 平均 O(1) 而非 O(N) 扫描，因此完整同步是 O(N) 而非 O(N^2)。
        const auto range = renderable_index.equal_range(PolyMatchKey(poly_to_find));
        for (auto it = range.first; it != range.second; ++it) {
            auto& check = renderables[it->second];
            if (!(check.map_id == poly_to_find.map_id
                  && check.col == poly_to_find.col
                  && check.filled == poly_to_find.filled
                  && check.points.size() == poly_to_find.points.size()))
                continue;
            bool same = true;
            for (size_t i = 0; i < check.points.size(); i++) {
                if (check.points[i] != poly_to_find.points[i]) { same = false; break; }
            }
            if (same) {
                renderable_index.erase(it); // 消费：调用者移走它，因此不能再次被认领
                return &check;
            }
        }
        return nullptr;
    }

    // 计算顶点海拔（一次，需要正确的地图）然后上传到设备缓冲区。
    bool AddPolyToDevice(GameWorldRenderer::GenericPolyRenderable& poly, IDirect3DDevice9* device)
    {
        if (poly.vb)
            return true; // vb 存在 => 海拔已计算
        auto& vertices = poly.vertices;
        if (poly.vertices_processed == vertices.size())
            return true;
        const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
        if (!pathing_map || pathing_map->size() == 0)
            return false;
        const uint32_t num_planes = static_cast<uint32_t>(pathing_map->size());

        if (poly.filled) {
            // 填充形状是耳切三角形汤（无顶点顺序），因此无连续性：将每个顶点悬挂在形状自身平面中的最高表面上。
            std::vector<uint32_t> candidate_planes;
            for (const auto& pt : poly.points) {
                if (std::ranges::find(candidate_planes, pt.zplane) == candidate_planes.end())
                    candidate_planes.push_back(pt.zplane);
            }
            for (size_t i = poly.vertices_processed; i < vertices.size(); i++, poly.vertices_processed++)
                vertices[i].z = TerrainDrape::HighestZOnPlanes(vertices[i].x, vertices[i].y, candidate_planes);
        }
        else {
            auto* nav = PathfindingWindow::GetResidentNavMesh();
            GW::GamePos seed_pos = poly.points.empty() ? GW::GamePos{} : poly.points.front();
            float prev = TerrainDrape::QueryAltAt(seed_pos.x, seed_pos.y, seed_pos.zplane);
            if (prev == 0.f) prev = TerrainDrape::ClosestZ(seed_pos.x, seed_pos.y, num_planes, -1.0e9f); // highest surface
            for (size_t i = poly.vertices_processed; i < vertices.size(); i++, poly.vertices_processed++) {
                float z;
                if (nav) {
                    z = nav->DrapeHeightAt(vertices[i].x, vertices[i].y, prev);
                    if (z == ALTITUDE_UNKNOWN) z = prev; // 在无可行走多边形的间隙中：保持高度，不沉到地面
                }
                else {
                    z = TerrainDrape::ClosestZ(vertices[i].x, vertices[i].y, num_planes, prev); // navmesh not built yet
                }
                if (z != ALTITUDE_UNKNOWN) prev = z;
                vertices[i].z = z;
            }
        }

        // 回填无数据顶点的海拔，保持最后已知海拔，使线条不会跳到屏幕外。
        float fill = ALTITUDE_UNKNOWN;
        for (const auto& v : vertices)
            if (v.z != ALTITUDE_UNKNOWN) { fill = v.z; break; }
        if (fill != ALTITUDE_UNKNOWN) {
            float last = fill;
            for (auto& v : vertices) {
                if (v.z == ALTITUDE_UNKNOWN) v.z = last;
                else last = v.z;
            }
            // After backfill so the sentinel is never lifted (the compare above is exact).
            for (auto& v : vertices) v.z -= z_lift;
        }

        auto res = device->CreateVertexBuffer(vertices.size() * sizeof(D3DVertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &poly.vb, nullptr);
        if (res != S_OK) {
            poly.vb = nullptr;
            return false;
        }

        void* mem_loc = nullptr;
        res = poly.vb->Lock(0, vertices.size() * sizeof(D3DVertex), &mem_loc, 0);
        if (res != S_OK || !mem_loc) {
            poly.vb->Release();
            poly.vb = nullptr;
            return false;
        }

        memcpy(mem_loc, vertices.data(), vertices.size() * sizeof(D3DVertex));
        poly.vb->Unlock();
        return true;
    }
} // namespace

GameWorldRenderer::GenericPolyRenderable::GenericPolyRenderable(
    const GW::Constants::MapID map_id,
    const std::vector<GW::GamePos>& points,
    const unsigned int col,
    const bool filled) noexcept
    : map_id(map_id),
      col(col),
      points(points),
      filled(filled) {}

GameWorldRenderer::GenericPolyRenderable::~GenericPolyRenderable() noexcept
{
    if (vb != nullptr) {
        vb->Release();
        vb = nullptr;
    }
}

void GameWorldRenderer::GenericPolyRenderable::Draw(IDirect3DDevice9* device)
{
    if (vertices.empty()) {
        if (filled && points.size() >= 3) { // 至少需要 3 个点才能形成一个三角形
            std::vector<GW::GamePos> lerp_points{};
            for (size_t i = 0; i < points.size(); i++) {
                if (!lerp_points.empty() && lerp_steps_per_line > 0) {
                    for (auto j = 1u; j < lerp_steps_per_line; j++) {
                        const float div = static_cast<float>(j) / static_cast<float>(lerp_steps_per_line);
                        auto split = lerp(points[i], points[i - 1], div);
                        lerp_points.emplace_back(split.x, split.y, points[i].zplane);
                    }
                }
                lerp_points.push_back(points[i]);
            }
            const std::vector<unsigned> indices = mapbox::earcut<unsigned>(std::vector{{lerp_points}});
            for (size_t i = 0; i < indices.size(); i++) {
                const auto& pt = lerp_points[indices[i]];
                vertices.emplace_back(pt.x, pt.y, ALTITUDE_UNKNOWN, col);
            }
        }
        else {
            // 大约每 50 游戏单位采样一个段，使斜坡/楼梯/桥梁在跳跃之间跟随表面而不是直线弦；lerp_steps_per_line 是下限。平面悬挂在 AddPolyToDevice 中解析。
            constexpr float sample_spacing = 50.f;
            for (size_t i = 0; i < points.size(); i++) {
                const auto& pt = points[i];
                if (!vertices.empty()) {
                    const float dx = points[i].x - points[i - 1].x, dy = points[i].y - points[i - 1].y;
                    const auto steps = std::max(lerp_steps_per_line, static_cast<unsigned>(std::sqrt(dx * dx + dy * dy) / sample_spacing));
                    for (auto j = 1u; j < steps; j++) {
                        const auto div = static_cast<float>(j) / static_cast<float>(steps);
                        const auto split = lerp(points[i], points[i - 1], div);
                        vertices.emplace_back(split.x, split.y, ALTITUDE_UNKNOWN, col);
                    }
                }
                vertices.emplace_back(pt.x, pt.y, ALTITUDE_UNKNOWN, col);
            }
        }
    }

    if (!AddPolyToDevice(*this, device))
        return;

    if (from_player_pos && vertices.size() > 1) {
        // Any movement re-anchors: a distance threshold leaves the line's start trailing the player and snapping forward, which reads as jitter.
        // Standing still still costs nothing, and a few dozen vertices against the Y-row drape index is cheap per-frame.
        const auto player = GW::Agents::GetControlledCharacter();
        if (player && (!anchored || player->pos.x != anchor_x || player->pos.y != anchor_y)) {
            anchor_x = player->pos.x;
            anchor_y = player->pos.y;
            anchored = true;
            const float ex = vertices.back().x, ey = vertices.back().y;
            const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
            const uint32_t num_planes = pathing_map ? static_cast<uint32_t>(pathing_map->size()) : 0;
            const size_t last = vertices.size() - 1;
            auto* nav = PathfindingWindow::GetResidentNavMesh();
            float prev = player->z;
            for (size_t j = 0; j <= last; ++j) {
                const float t = static_cast<float>(j) / static_cast<float>(last);
                const float sx = player->pos.x + (ex - player->pos.x) * t;
                const float sy = player->pos.y + (ey - player->pos.y) * t;
                float z;
                if (nav) {
                    z = nav->DrapeHeightAt(sx, sy, prev);
                    if (z == ALTITUDE_UNKNOWN) z = prev; // 无可行走多边形的间隙：保持高度，不沉到地面
                }
                else {
                    z = num_planes ? TerrainDrape::ClosestZ(sx, sy, num_planes, prev) : ALTITUDE_UNKNOWN; // navmesh not built yet
                }
                if (z != ALTITUDE_UNKNOWN) prev = z; else z = prev;
                vertices[j].x = sx;
                vertices[j].y = sy;
                vertices[j].z = z - z_lift; // `prev` stays unlifted: it seeds the next drape query
            }

            void* mem_loc = nullptr;
            // flags=0, not D3DLOCK_DISCARD: DISCARD needs D3DUSAGE_DYNAMIC but this is MANAGED, so it's ignored and the lock serialises against the GPU.
            auto res = vb->Lock(0, vertices.size() * sizeof(D3DVertex), &mem_loc, 0);
            if (res == S_OK) {
                memcpy(mem_loc, vertices.data(), vertices.size() * sizeof(D3DVertex));
                vb->Unlock();
            }
        }
    }

    if (device->SetStreamSource(0, vb, 0, sizeof(D3DVertex)) != D3D_OK) {
        return;
    }

    const BOOL dotted_effect_constant[1] = {static_cast<BOOL>(use_dotted_effect)};
    if (device->SetPixelShaderConstantB(0, dotted_effect_constant, 1) != D3D_OK) {
        Log::Error("GameWorldRenderer：无法设置像素着色器常量 B#3，中止渲染。");
        return;
    }

    // 保护计数：空行会使 vertices.size()-1（size_t）下溢并导致 DrawPrimitive 崩溃。
    if (filled) {
        if (vertices.size() >= 3) device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, vertices.size() / 3);
    }
    else if (vertices.size() >= 2) {
        device->DrawPrimitive(D3DPT_LINESTRIP, 0, vertices.size() - 1);
    }
}

void GameWorldRenderer::UpdateCompositorRegistration()
{
    // 仅在需要该模式时向共享合成器注册我们的 UI 下层绘制；
    // 模块的启用状态由 Initialize()/SignalTerminate() 处理。
    if (render_under_ui && !compositor_token) {
        compositor_token = GameWorldCompositor::RegisterDraw(&GameWorldRenderer::DrawInWorld);
    }
    else if (!render_under_ui && compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
}

// 实际的世界绘制：同步标记，设置共享世界管道，绘制（罗盘挖空）。
// 由合成器（在 UI 下层）或 Render()（在顶部）调用，同一帧中不会同时调用两者。
void GameWorldRenderer::DrawInWorld(IDirect3DDevice9* device)
{
    if (GW::UI::GetIsWorldMapShowing()) {
        return;
    }
    if (need_sync_markers) {
        // 在渲染线程上同步：创建顶点缓冲区需要 D3D 设备。
        SyncAllMarkers();
    }
    StepNavmeshBatchBuild(); // 推进增量、表面悬挂的导航网格线缓冲（每帧受预算限制）
    if (renderables.empty() && navmesh_batch.verts.empty()) {
        return;
    }

    // Snapshot the device state toolbox touches; restored unconditionally on exit (incl. error paths) so GW's later rendering isn't corrupted.
    const D3DStateGuard state_guard(device);

    if (GameWorldCompositor::SetupPipeline(device, occlude_behind_terrain, render_max_distance, fog_factor)) {
        const auto map_id = GW::Map::GetMapID();
        renderables_mutex.lock();

        // 将首次表面悬挂限制为每帧切片：AddPolyToDevice 为每条新线采样 QueryAltitude，
        // 因此新边的大量突发会冻结游戏线程；QueryAltitude 不能离线运行，因此跨帧分布悬挂。
        // 已悬挂的渲染对象（缓存的 vb）始终绘制。
        const auto drape_timer = TIMER_INIT();
        bool drape_budget_spent = false;

        auto draw_renderables = [&] {
            for (auto& renderable : renderables) {
                if (renderable.map_id != map_id) continue;
                if (renderable.vb == nullptr) { // 首次绘制计算海拔（繁重）
                    if (drape_budget_spent) continue; // 本帧时间用尽；下一帧悬挂
                    renderable.Draw(device);
                    if (TIMER_DIFF(drape_timer) >= 2) drape_budget_spent = true;
                }
                else {
                    renderable.Draw(device);
                }
            }
            DrawNavmeshBatch(device, map_id); // 批处理导航网格覆盖层：一次绘制调用，相同管道 + 罗盘模板
        };

        // GW 分别绘制罗盘圆盘（世界通道）和其框架（后续 HUD 通道），因此覆盖层落在两者之间并渗透到小地图上；
        // 用模板将圆盘挖空。
        float compass_cx, compass_cy, compass_radius;
        if (exclude_compass && GetCompassTerrainCircle(compass_cx, compass_cy, compass_radius)) {
            constexpr DWORD compass_stencil_bit = 0x80;
            MarkCompassStencil(device, compass_cx, compass_cy, compass_radius, compass_stencil_bit, true);

            // 恢复标记通道更改的可编程管道 + 渲染状态
            device->SetVertexShader(GameWorldCompositor::VertexShader());
            device->SetPixelShader(GameWorldCompositor::PixelShader());
            device->SetVertexDeclaration(GameWorldCompositor::VertexDeclaration());
            device->SetRenderState(D3DRS_COLORWRITEENABLE,
                                   D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
            device->SetRenderState(D3DRS_ZENABLE, occlude_behind_terrain ? D3DZB_TRUE : D3DZB_FALSE);

            // 仅在我们的位清零的地方绘制覆盖层，即罗盘圆盘外部
            device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            device->SetRenderState(D3DRS_STENCILMASK, compass_stencil_bit);
            device->SetRenderState(D3DRS_STENCILREF, 0);
            device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
            device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
            device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
            device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);

            draw_renderables();

            // 清除我们的位，使 GW 的共享模板恢复到我们找到时的状态
            MarkCompassStencil(device, compass_cx, compass_cy, compass_radius, compass_stencil_bit, false);
            device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        }
        else {
            draw_renderables();
        }
        renderables_mutex.unlock();
    }
}

void GameWorldRenderer::Render(IDirect3DDevice9* device)
{
    if (render_under_ui && GameWorldCompositor::IsActive()) {
        return;
    }
    DrawInWorld(device);
}

void GameWorldRenderer::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "render_max_distance", &render_max_distance);
    SettingsRegistry::RegisterField(module, "lerp_steps_per_line", &lerp_steps_per_line);
    SettingsRegistry::RegisterField(module, "fog_factor", &fog_factor);
    SettingsRegistry::RegisterField(module, "occlude_behind_terrain", &occlude_behind_terrain);
    SettingsRegistry::RegisterField(module, "render_under_ui", &render_under_ui);
    SettingsRegistry::RegisterField(module, "exclude_compass", &exclude_compass);
    SettingsRegistry::RegisterField(module, "z_lift", &z_lift);
}

void GameWorldRenderer::OnSettingsLoaded()
{
    render_max_distance = std::max(render_max_distance, 10.0f);
    fog_factor = std::clamp(fog_factor, 0.0f, 1.0f);
    z_lift = std::clamp(z_lift, 0.0f, 200.0f);
    need_sync_markers = true;
    UpdateCompositorRegistration();
}

void GameWorldRenderer::DrawSettings()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    ImGui::TextColored(red, "Warning: This is a beta feature.");
    ImGui::Text("Note: custom markers are only rendered in-game if the option is enabled for a particular marker (check settings).");
    ImGui::DragFloat("Maximum render distance", &render_max_distance, 5.f, 0.f, 10000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ShowHelp("Maximum distance to render custom markers on the in-game terrain.");
    need_sync_markers |= ImGui::DragInt("Interpolation granularity", reinterpret_cast<int*>(&lerp_steps_per_line), 1.0f, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ShowHelp("Number of points to interpolate. Affects smoothness of rendering.");
    ImGui::DragFloat("Fog factor", &fog_factor, 0.1f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ShowHelp("Scales from 0.0 (disabled) to 1.0");
    if (ImGui::DragFloat("Height lift", &z_lift, 0.5f, 0.f, 200.f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        // The lift is baked into each renderable's vertex buffer, so drop them and let the next sync re-drape.
        renderables_mutex.lock();
        renderables.clear();
        renderables_mutex.unlock();
        need_sync_markers = true;
    }
    ImGui::ShowHelp("Raise quest paths and other in-world overlays above the surface they're draped on, so they draw on top of the terrain instead of z-fighting it.");

    if (ImGui::Checkbox("在游戏 UI 下层渲染", &render_under_ui)) {
        UpdateCompositorRegistration();
    }
    ImGui::ShowHelp("在游戏内 UI（菜单、队伍窗口等）下层绘制覆盖层，而非顶层。\n"
                    "实验性：钩入 GW 的 UI 渲染通道。关闭以恢复原始的顶层绘制。");
    if (render_under_ui) {
        if (GameWorldCompositor::HasFailed())
            ImGui::TextColored(red, "  UI 下层钩子安装失败 — 在顶层绘制。");
        else if (GameWorldCompositor::IsActive())
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Green()), "  UI 下层钩子已激活。");
        else
            ImGui::TextDisabled("  UI 下层钩子：尚未安装。");
    }

    ImGui::Checkbox("避开罗盘区域", &exclude_compass);
    ImGui::ShowHelp("不在游戏内罗盘/小地图上方绘制世界覆盖层。\n"
                    "GW 在单独的通道中渲染罗盘地形及其框架，否则覆盖层会渗透到小地图内部。");

    ImGui::Checkbox("地形遮挡", &occlude_behind_terrain);
    ImGui::ShowHelp("使用游戏深度缓冲区隐藏墙壁、建筑物和地形后面的覆盖层。");
}

void GameWorldRenderer::TriggerSyncAllMarkers()
{
    need_sync_markers = true;
}

void GameWorldRenderer::SetNavmeshLines(GW::Constants::MapID map_id, std::vector<BatchedLine> lines)
{
    // 仅渲染线程（从 PathfindingWindow::Draw 调用，与 DrawInWorld 同一线程）— 无需锁定。
    // 开始一个新的构建到 staging 中；实时 `verts` 在完成前持续绘制，因此交换是无缝的。
    auto& b = navmesh_batch;
    b.pending_map = map_id;
    b.lines = std::move(lines);
    b.staging.clear();
    b.build_cursor = 0;
    b.building = true;
}

void GameWorldRenderer::SetNavmeshSampleSpacing(float gw)
{
    navmesh_sample_spacing = std::max(1.f, gw);
}

void GameWorldRenderer::RedrapeNavmesh()
{
    // 重新悬挂当前边集（例如在采样间距滑块更改后）而不重新裁剪：从现有源线重新开始增量构建。
    // 如果未加载任何内容则为空操作。
    auto& b = navmesh_batch;
    if (b.lines.empty()) return;
    b.pending_map = b.map_id;
    b.staging.clear();
    b.build_cursor = 0;
    b.building = true;
}

void GameWorldRenderer::SetNavmeshWorldMapLines(GW::Constants::MapID map_id, std::vector<BatchedLine> lines)
{
    navmesh_worldmap_lines = std::move(lines);
    navmesh_worldmap_map = map_id;
}

void GameWorldRenderer::ClearNavmeshLines()
{
    navmesh_batch.lines.clear();
    navmesh_batch.staging.clear();
    navmesh_batch.verts.clear();
    navmesh_batch.build_cursor = 0;
    navmesh_batch.building = false;
    navmesh_batch.map_id = GW::Constants::MapID::None;
    navmesh_batch.pending_map = GW::Constants::MapID::None;
    navmesh_batch.vb_dirty = true;
    navmesh_worldmap_lines.clear();
    navmesh_worldmap_map = GW::Constants::MapID::None;
}

const std::vector<GameWorldRenderer::BatchedLine>& GameWorldRenderer::GetNavmeshWorldMapLines() { return navmesh_worldmap_lines; }
GW::Constants::MapID GameWorldRenderer::GetNavmeshWorldMapMapId() { return navmesh_worldmap_map; }
float GameWorldRenderer::GetRenderMaxDistance() { return render_max_distance; }
bool GameWorldRenderer::GetOccludeBehindTerrain() { return occlude_behind_terrain; }

void GameWorldRenderer::Terminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
    renderables.clear();
    if (navmesh_batch.vb) {
        navmesh_batch.vb->Release();
        navmesh_batch.vb = nullptr;
    }
    navmesh_batch.lines.clear();
    navmesh_batch.verts.clear();
    ToolboxModule::Terminate();
}

void GameWorldRenderer::SyncAllMarkers()
{
    renderables_mutex.lock();
    // 索引当前渲染对象，使三个 Sync* 传递以 O(1) 匹配；find_matching_poly 读取它。
    renderable_index.clear();
    renderable_index.reserve(renderables.size());
    for (size_t i = 0; i < renderables.size(); i++)
        renderable_index.emplace(PolyMatchKey(renderables[i]), i);

    auto lines = SyncLines();
    auto polys = SyncPolys();
    auto markers = SyncMarkers();
    renderable_index.clear();

    renderables.clear();
    renderables.reserve(lines.size() + polys.size() + markers.size());

    for (auto& line : lines) {
        renderables.push_back(std::move(line));
    }
    for (auto& poly : polys) {
        renderables.push_back(std::move(poly));
    }
    for (auto& marker : markers) {
        renderables.push_back(std::move(marker));
    }
    renderables_mutex.unlock();
    need_sync_markers = false;
}

GameWorldRenderer::RenderableVectors GameWorldRenderer::SyncLines()
{
    const auto& lines = Minimap::Instance().custom_renderer.GetLines();

    const auto map_id = GW::Map::GetMapID();

    RenderableVectors out;
    out.reserve(lines.size());
    for (const auto line : lines) {
        if (!(line->draw_on_terrain && line->visible)) {
            continue;
        }
        if (!(line->map == map_id || line->map == GW::Constants::MapID::None))
            continue;
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && map_id == GW::Constants::MapID::Domain_of_Anguish && !line->draw_everywhere) {
            // 在痛苦领域前哨站不绘制普通线条
            continue;
        }
        std::vector points = {line->p1, line->p2};

        auto poly_to_add = GenericPolyRenderable(line->map, points, line->color, false);

        poly_to_add.from_player_pos = line->from_player_pos;
        poly_to_add.use_dotted_effect = line->created_by_toolbox && line->dotted;

        if (const auto found = find_matching_poly(poly_to_add)) {
            found->from_player_pos = poly_to_add.from_player_pos;
            found->use_dotted_effect = poly_to_add.use_dotted_effect;
            out.emplace_back(std::move(*found));
        }
        else {
            out.emplace_back(std::move(poly_to_add));
        }
    }
    return out;
}

GameWorldRenderer::RenderableVectors GameWorldRenderer::SyncPolys()
{
    const auto& polys = Minimap::Instance().custom_renderer.GetPolys();
    RenderableVectors out;

    const auto map_id = GW::Map::GetMapID();

    out.reserve(polys.size());
    for (const auto& poly : polys) {
        if (!(poly.draw_on_terrain && poly.visible && poly.points.size())) {
            continue;
        }
        if (!(poly.map == map_id || poly.map == GW::Constants::MapID::None))
            continue;
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && map_id == GW::Constants::MapID::Domain_of_Anguish) {
            // 在痛苦领域前哨站不绘制普通多边形
            continue;
        }
        const std::vector<GW::GamePos> pts(poly.points.begin(), poly.points.end());

        auto poly_to_add = GenericPolyRenderable(poly.map, pts, poly.color, poly.filled);

        if (const auto found = find_matching_poly(poly_to_add)) {
            out.emplace_back(std::move(*found));
        }
        else {
            out.emplace_back(std::move(poly_to_add));
        }
    }
    return out;
}

GameWorldRenderer::RenderableVectors GameWorldRenderer::SyncMarkers()
{
    const auto& markers = Minimap::Instance().custom_renderer.GetMarkers();

    const auto map_id = GW::Map::GetMapID();

    RenderableVectors out;
    out.reserve(markers.size());
    for (const auto& marker : markers) {
        if (!(marker.draw_on_terrain && marker.visible)) {
            continue;
        }
        if (!(marker.map == map_id || marker.map == GW::Constants::MapID::None)) {
            continue;
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && map_id == GW::Constants::MapID::Domain_of_Anguish) {
            // 在痛苦领域前哨站不绘制普通标记
            continue;
        }

        auto points = circular_points_from_marker(marker.pos, marker.size);

        const auto color = (marker.color & IM_COL32_A_MASK) == 0 ? CustomRenderer::color : marker.color;

        auto poly_to_add = GenericPolyRenderable(marker.map, points, color, marker.IsFilled());

        auto found = find_matching_poly(poly_to_add);

        if (found) {
            out.emplace_back(std::move(*found));
        }
        else {
            out.emplace_back(std::move(poly_to_add));
        }
    }
    return out;
}

// ===========================================================================
// ToolboxModule 生命周期（自有设置部分 / JSON 文件，独立于 Minimap）
// ===========================================================================

void GameWorldRenderer::Initialize()
{
    ToolboxModule::Initialize(); // 在“游戏内渲染”下注册 DrawSettingsInternal()
    // 针对此模块注册字段，使其持久化到自己的部分，而非 Minimap 下。
    RegisterSettings(this);
    UpdateCompositorRegistration();
}

void GameWorldRenderer::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    OnSettingsLoaded();
}

void GameWorldRenderer::DrawSettingsInternal()
{
    DrawSettings();
}

void GameWorldRenderer::SignalTerminate()
{
    // 在模块禁用时立即移除我们的 UI 下层绘制；一旦没有模块注册，共享合成器会移除其钩子。
    // Render() 也不再被调用（由模块启用状态控制）。
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
}
