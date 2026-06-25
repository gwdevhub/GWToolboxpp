#include "stdafx.h"

#include <chrono>
#include <unordered_map>

#include <GWCA/Context/MapContext.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <Utils/GameWorldCompositor.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <ImGuiAddons.h>

#include "GWCA/GameEntities/Agent.h"
#include "GWCA/Managers/AgentMgr.h"

namespace {
    unsigned lerp_steps_per_line = 10;
    float render_max_distance = 5000.f;
    float fog_factor = 0.5f;
    bool need_sync_markers = true;
    bool render_under_ui = true;
    // Stencil the round compass out of overlays so they don't bleed across the minimap.
    bool exclude_compass = true;

    // Test overlays against the scene depth buffer so world geometry hides them; z_near/z_far must match GW's projection (near clip ~47, valid 45-48; far insensitive 50k-200k).
    bool occlude_behind_terrain = true;
    float z_near = 47.0f;
    float z_far = 100000.f;


    GameWorldRenderer::RenderableVectors renderables;
    std::mutex renderables_mutex{};

    // Hash index over `renderables`, rebuilt per sync, so the three Sync* passes can reuse already-draped polys in
    // O(1). Without it, matching each incoming line against every existing renderable is O(N^2) — seconds for the
    // navmesh overlay's thousands of edges, which froze the compositor draw on every overlay rebuild.
    std::unordered_multimap<uint64_t, size_t> renderable_index;

    uint64_t PolyMatchKey(const GameWorldRenderer::GenericPolyRenderable& p)
    {
        uint64_t h = 1469598103934665603ull; // FNV-1a over the fields find_matching_poly compares
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

    // Token for our under-UI draw registered with the shared compositor (0 = not registered).
    int compositor_token = 0;

    // Screen-space circle of GW's compass terrain (inside the frame), or false when hidden. Mirrors Minimap's RepositionMinimapToCompass: inset by compass_padding, square off, inscribe.
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

    // Set/clear `bit` in a screen-space disc over the compass terrain (fixed-function) so the overlay can be punched out to the circular minimap; touches only `bit` to spare GW's stencil, leaves pipeline unset for the caller to restore.
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

    constexpr auto ALTITUDE_UNKNOWN = std::numeric_limits<float>::max();

    // Highest terrain surface at (x,y) among candidate planes (GW up is -z, so highest = min altitude). QueryAltitude returns 0.f out-of-bounds (ignored); ALTITUDE_UNKNOWN if no plane has data.
    float HighestSurfaceZ(const float x, const float y, const std::vector<uint32_t>& planes)
    {
        float best = ALTITUDE_UNKNOWN;
        for (const uint32_t zp : planes) {
            GW::GamePos p = {x, y, zp};
            const float a = GW::Map::QueryAltitude(&p);
            if (a != 0.f && (best == ALTITUDE_UNKNOWN || a < best))
                best = a;
        }
        return best;
    }

    // Surface altitude at (x,y) closest to `prev` over all planes (continuity tracking): the line follows whichever surface it walks (deck/ramp/ground), so a straight visgraph edge rides a bridge instead of cutting through, and unlike "highest wins" never floats a ground sample onto an overhead deck. 0.f is out-of-bounds (skipped); ALTITUDE_UNKNOWN if no data.
    float ClosestSurfaceZ(const float x, const float y, const uint32_t num_planes, const float prev)
    {
        float best = ALTITUDE_UNKNOWN, best_d = std::numeric_limits<float>::max();
        for (uint32_t zp = 0; zp < num_planes; ++zp) {
            GW::GamePos p = {x, y, zp};
            const float a = GW::Map::QueryAltitude(&p);
            if (a == 0.f) continue;
            const float d = std::abs(a - prev);
            if (d < best_d) { best_d = d; best = a; }
        }
        return best;
    }

    // ===== Batched navmesh-overlay line buffer =====
    // The navmesh debug overlay can emit tens of thousands of edges; funnelling them through per-line CustomLines
    // made every map-load rebuild O(N^2) (sync/find/remove) and re-draped them on every move. Instead we keep ONE
    // line-list buffer, terrain-drape it incrementally on the game thread (QueryAltitude can't run off-thread), and
    // draw it in a single call. Touched only on the render thread (DrawInWorld + Set/ClearNavmeshLines) — no mutex.
    struct NavmeshBatch {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;     // map the LIVE verts belong to
        GW::Constants::MapID pending_map = GW::Constants::MapID::None; // map `lines`/`staging` are being built for
        std::vector<GameWorldRenderer::BatchedLine> lines; // source segments (game coords + colour) being draped
        std::vector<D3DVertex> verts;                      // LIVE draped line-list vertices (drawn)
        std::vector<D3DVertex> staging;                    // next set, draped incrementally; swapped in when done
        size_t build_cursor = 0;                           // next source line to drape into staging
        bool building = false;                             // a staging build is in progress
        IDirect3DVertexBuffer9* vb = nullptr;
        size_t vb_cap = 0;                                 // capacity in vertices
        bool vb_dirty = false;                             // verts changed since last upload
    };
    NavmeshBatch navmesh_batch;

    // Full mesh (not draped) for the 2D top-down M-key world map. WorldMapWidget redraws these flat each frame.
    std::vector<GameWorldRenderer::BatchedLine> navmesh_worldmap_lines;
    GW::Constants::MapID navmesh_worldmap_map = GW::Constants::MapID::None;

    // Drape a per-frame, wall-clock-bounded slice into `staging`; the LIVE `verts` keep drawing until the new set is
    // complete, then we swap (double-buffer) so a rebuild-on-move never blanks the overlay (no flicker).
    void StepNavmeshBatchBuild()
    {
        auto& b = navmesh_batch;
        if (!b.building) return;
        const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
        const uint32_t num_planes = pm ? static_cast<uint32_t>(pm->size()) : 0;
        if (!num_planes) return; // pathing map not ready yet; retry next frame
        constexpr float sample_spacing = 50.f; // sample altitude every ~50gw so the edge hugs the floor, not a chord
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
        for (; b.build_cursor < b.lines.size(); ++b.build_cursor) {
            if (std::chrono::steady_clock::now() >= deadline) break; // budget spent; resume next frame
            const auto& ln = b.lines[b.build_cursor];
            const float dx = ln.b.x - ln.a.x, dy = ln.b.y - ln.a.y;
            const int steps = std::max(1, static_cast<int>(std::sqrt(dx * dx + dy * dy) / sample_spacing));
            // Drape by continuity: each sample follows whichever surface the previous one walked (ramp/bridge/floor),
            // and we emit a sub-segment per step so the edge follows the terrain instead of a straight chord. Seed
            // from the edge's OWN plane (ln.a carries its zplane), not the highest surface — else an edge under a
            // bridge gets pulled up onto the deck.
            GW::GamePos seed = ln.a;
            float prev = GW::Map::QueryAltitude(&seed);
            if (prev == 0.f) prev = ClosestSurfaceZ(ln.a.x, ln.a.y, num_planes, -1.0e9f);
            if (prev == ALTITUDE_UNKNOWN) prev = 0.f;
            float px = ln.a.x, py = ln.a.y, pz = prev;
            for (int s = 1; s <= steps; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(steps);
                const float x = ln.a.x + dx * t, y = ln.a.y + dy * t;
                float z = ClosestSurfaceZ(x, y, num_planes, prev);
                if (z == ALTITUDE_UNKNOWN) z = prev;
                else prev = z;
                b.staging.push_back({px, py, pz, ln.color}); // LINELIST: each consecutive pair is one sub-segment
                b.staging.push_back({x, y, z, ln.color});
                px = x; py = y; pz = z;
            }
        }
        if (b.build_cursor >= b.lines.size()) {
            // Staging complete: swap it in as the live, drawn set (atomic — no blank frame).
            b.verts.swap(b.staging);
            b.staging.clear();
            b.map_id = b.pending_map;
            b.building = false;
            b.vb_dirty = true;
        }
    }

    // (Re)create and upload the batch VB when it grew or its contents changed.
    bool EnsureNavmeshBatchVb(IDirect3DDevice9* device)
    {
        auto& b = navmesh_batch;
        const size_t need = b.verts.size();
        if (need < 2) return false;
        if (!b.vb || b.vb_cap < need) {
            if (b.vb) { b.vb->Release(); b.vb = nullptr; }
            const size_t cap = need + need / 2; // headroom so the growing build doesn't reallocate every frame
            if (device->CreateVertexBuffer(static_cast<UINT>(cap * sizeof(D3DVertex)), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &b.vb, nullptr) != D3D_OK) {
                b.vb_cap = 0;
                return false;
            }
            b.vb_cap = cap;
            b.vb_dirty = true;
        }
        if (b.vb_dirty) {
            void* mem = nullptr;
            // flags=0, not D3DLOCK_DISCARD: DISCARD is only valid on D3DUSAGE_DYNAMIC buffers, and this is MANAGED
            // (mirrors RiverModule's managed VB). The batch is rebuilt rarely (once per map), so no dynamic rename.
            if (b.vb->Lock(0, static_cast<UINT>(need * sizeof(D3DVertex)), &mem, 0) != D3D_OK || !mem) return false;
            memcpy(mem, b.verts.data(), need * sizeof(D3DVertex));
            b.vb->Unlock();
            b.vb_dirty = false;
        }
        return true;
    }

    // Draw the whole navmesh as one line list (called inside the shared world pipeline, with the compass stencil).
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
        points.push_back(points.at(0)); // close the loop
        return points;
    }

    GameWorldRenderer::GenericPolyRenderable* find_matching_poly(const GameWorldRenderer::GenericPolyRenderable& poly_to_find)
    {
        // Reuse an already-plotted poly (keeps its draped vertex buffer) via the prebuilt index — O(1) average
        // instead of an O(N) scan, so a full sync is O(N) not O(N^2).
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
                renderable_index.erase(it); // consume: the caller moves-from it, so it can't be claimed twice
                return &check;
            }
        }
        return nullptr;
    }

    // Compute vertex altitudes (once, requires the correct map) then upload to the device buffer.
    bool AddPolyToDevice(GameWorldRenderer::GenericPolyRenderable& poly, IDirect3DDevice9* device)
    {
        if (poly.vb)
            return true; // vb exists => altitudes already done
        auto& vertices = poly.vertices;
        if (poly.vertices_processed == vertices.size())
            return true;
        const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
        if (!pathing_map || pathing_map->size() == 0)
            return false;
        const uint32_t num_planes = static_cast<uint32_t>(pathing_map->size());

        if (poly.filled) {
            // Filled shapes are an earcut triangle soup (no vertex order), so no continuity: drape each vertex on the highest surface among the shape's own planes.
            std::vector<uint32_t> candidate_planes;
            for (const auto& pt : poly.points) {
                if (std::ranges::find(candidate_planes, pt.zplane) == candidate_planes.end())
                    candidate_planes.push_back(pt.zplane);
            }
            for (size_t i = poly.vertices_processed; i < vertices.size(); i++, poly.vertices_processed++)
                vertices[i].z = HighestSurfaceZ(vertices[i].x, vertices[i].y, candidate_planes);
        }
        else {
            // Ordered, densely-sampled path: drape by continuity (each sample follows the walked surface across bridges even with no waypoint on that plane). Seed from the first waypoint; the leading from_player_pos segment is re-seeded from live player height below.
            GW::GamePos seed_pos = poly.points.empty() ? GW::GamePos{} : poly.points.front();
            float prev = GW::Map::QueryAltitude(&seed_pos);
            if (prev == 0.f) prev = ClosestSurfaceZ(seed_pos.x, seed_pos.y, num_planes, -1.0e9f); // highest surface
            for (size_t i = poly.vertices_processed; i < vertices.size(); i++, poly.vertices_processed++) {
                const float z = ClosestSurfaceZ(vertices[i].x, vertices[i].y, num_planes, prev);
                if (z != ALTITUDE_UNKNOWN) prev = z;
                vertices[i].z = z;
            }
        }

        // Backfill no-data vertices by holding the last known altitude so the line never spikes off-screen.
        float fill = ALTITUDE_UNKNOWN;
        for (const auto& v : vertices)
            if (v.z != ALTITUDE_UNKNOWN) { fill = v.z; break; }
        if (fill != ALTITUDE_UNKNOWN) {
            float last = fill;
            for (auto& v : vertices) {
                if (v.z == ALTITUDE_UNKNOWN) v.z = last;
                else last = v.z;
            }
        }

        auto res = device->CreateVertexBuffer(vertices.size() * sizeof(D3DVertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &poly.vb, nullptr);
        if (res != S_OK) {
            poly.vb = nullptr;
            return false;
        }

        void* mem_loc = nullptr;
        res = poly.vb->Lock(0, vertices.size() * sizeof(D3DVertex), &mem_loc, D3DLOCK_DISCARD);
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
        if (filled && points.size() >= 3) { // need >= 3 points for one triangle
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
            // Sample each segment ~every 50 gwinches so a slope/stairs/bridge between hops follows the surface instead of a straight chord; lerp_steps_per_line is the floor. Plane draping is resolved in AddPolyToDevice.
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
        if (const auto player = GW::Agents::GetControlledCharacter()) {
            // Re-anchor the leading line to the player's live position each frame and drape by continuity from player->z, which is reliable even when the reported plane is 0 on a bridge (seeding from the plane sank the line to the ground beneath the bridge).
            const float ex = vertices.back().x, ey = vertices.back().y;
            const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
            const uint32_t num_planes = pathing_map ? static_cast<uint32_t>(pathing_map->size()) : 0;
            const size_t last = vertices.size() - 1;
            float prev = player->z;
            for (size_t j = 0; j <= last; ++j) {
                const float t = static_cast<float>(j) / static_cast<float>(last);
                const float sx = player->pos.x + (ex - player->pos.x) * t;
                const float sy = player->pos.y + (ey - player->pos.y) * t;
                float z = num_planes ? ClosestSurfaceZ(sx, sy, num_planes, prev) : ALTITUDE_UNKNOWN;
                if (z != ALTITUDE_UNKNOWN) prev = z; else z = prev;
                vertices[j].x = sx;
                vertices[j].y = sy;
                vertices[j].z = z;
            }

            void* mem_loc = nullptr;
            auto res = vb->Lock(0, vertices.size() * sizeof(D3DVertex), &mem_loc, D3DLOCK_DISCARD);
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
        Log::Error("GameWorldRenderer: unable to SetPixelShaderConstantF#3, aborting render.");
        return;
    }

    // Guard the counts: an empty line would underflow vertices.size()-1 (size_t) and crash DrawPrimitive.
    if (filled) {
        if (vertices.size() >= 3) device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, vertices.size() / 3);
    }
    else if (vertices.size() >= 2) {
        device->DrawPrimitive(D3DPT_LINESTRIP, 0, vertices.size() - 1);
    }
}

void GameWorldRenderer::UpdateCompositorRegistration()
{
    // Register our under-UI draw with the shared compositor only while that mode is wanted; the
    // module's enabled state is handled by Initialize()/SignalTerminate().
    if (render_under_ui && !compositor_token) {
        compositor_token = GameWorldCompositor::RegisterDraw(&GameWorldRenderer::DrawInWorld);
    }
    else if (!render_under_ui && compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
}

// The actual world draw: sync markers, set up the shared world pipeline, draw (compass punched out).
// Invoked either by the compositor (under the UI) or by Render() (on top), never both in one frame.
void GameWorldRenderer::DrawInWorld(IDirect3DDevice9* device)
{
    if (GW::UI::GetIsWorldMapShowing()) {
        return;
    }
    if (need_sync_markers) {
        // Sync on the render thread: creating vertex buffers needs the D3D device.
        SyncAllMarkers();
    }
    StepNavmeshBatchBuild(); // advance the incremental, terrain-draped navmesh line buffer (bounded per frame)
    if (renderables.empty() && navmesh_batch.verts.empty()) {
        return;
    }

    // Snapshot all device state; restored unconditionally on exit (incl. error paths) so GW's later rendering isn't corrupted.
    IDirect3DStateBlock9* state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &state_block) != D3D_OK) {
        return;
    }

    if (GameWorldCompositor::SetupPipeline(device, occlude_behind_terrain, z_near, z_far, render_max_distance, fog_factor)) {
        const auto map_id = GW::Map::GetMapID();
        renderables_mutex.lock();

        // Bound first-time terrain draping to a slice of the frame. AddPolyToDevice samples QueryAltitude (~once
        // per plane) along every new line, so a burst of fresh lines — e.g. the navmesh overlay's thousands of
        // edges — would freeze the game thread if draped at once. QueryAltitude can't run off-thread (it swaps the
        // global map context), so spread it across frames under a wall-clock budget; the geometry fills in over a
        // few frames and no single frame stalls. Already-draped renderables (cached vb) always draw — that's cheap.
        const auto drape_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
        bool drape_budget_spent = false;

        auto draw_renderables = [&] {
            for (auto& renderable : renderables) {
                if (renderable.map_id != map_id) continue;
                if (renderable.vb == nullptr) { // first draw computes altitudes (heavy)
                    if (drape_budget_spent) continue; // out of time this frame; drape it next frame
                    renderable.Draw(device);
                    if (std::chrono::steady_clock::now() >= drape_deadline) drape_budget_spent = true;
                }
                else {
                    renderable.Draw(device);
                }
            }
            DrawNavmeshBatch(device, map_id); // batched navmesh overlay: one draw call, same pipeline + compass stencil
        };

        // GW draws the compass disc (world pass) and its frame (later HUD pass) separately, so the overlay lands between and bleeds across the minimap; stencil the disc out.
        float compass_cx, compass_cy, compass_radius;
        if (exclude_compass && GetCompassTerrainCircle(compass_cx, compass_cy, compass_radius)) {
            constexpr DWORD compass_stencil_bit = 0x80;
            MarkCompassStencil(device, compass_cx, compass_cy, compass_radius, compass_stencil_bit, true);

            // restore the programmable pipeline + render state the mark pass changed
            device->SetVertexShader(GameWorldCompositor::VertexShader());
            device->SetPixelShader(GameWorldCompositor::PixelShader());
            device->SetVertexDeclaration(GameWorldCompositor::VertexDeclaration());
            device->SetRenderState(D3DRS_COLORWRITEENABLE,
                                   D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
            device->SetRenderState(D3DRS_ZENABLE, occlude_behind_terrain ? D3DZB_TRUE : D3DZB_FALSE);

            // draw the overlay only where our bit is clear, i.e. outside the compass disc
            device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            device->SetRenderState(D3DRS_STENCILMASK, compass_stencil_bit);
            device->SetRenderState(D3DRS_STENCILREF, 0);
            device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
            device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
            device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
            device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);

            draw_renderables();

            // clear our bit back so GW's shared stencil is left exactly as we found it
            MarkCompassStencil(device, compass_cx, compass_cy, compass_radius, compass_stencil_bit, false);
            device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        }
        else {
            draw_renderables();
        }
        renderables_mutex.unlock();
    }

    state_block->Apply();
    state_block->Release();
}

void GameWorldRenderer::Render(IDirect3DDevice9* device)
{
    // On-top (End Scene) path. When under-UI is wanted and the shared compositor is running, it
    // draws our markers between GW's world and HUD instead (and runs the marker sync itself), so
    // skip here. Otherwise draw on top - this is also the fallback if the compositor failed.
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
    SettingsRegistry::RegisterField(module, "depth_z_near", &z_near);
    SettingsRegistry::RegisterField(module, "depth_z_far", &z_far);
    SettingsRegistry::RegisterField(module, "render_under_ui", &render_under_ui);
    SettingsRegistry::RegisterField(module, "exclude_compass", &exclude_compass);
}

void GameWorldRenderer::OnSettingsLoaded()
{
    render_max_distance = std::max(render_max_distance, 10.0f);
    fog_factor = std::clamp(fog_factor, 0.0f, 1.0f);
    z_near = std::max(z_near, 0.01f);
    z_far = std::max(z_far, z_near + 1.0f);
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

    if (ImGui::Checkbox("Render under game UI", &render_under_ui)) {
        UpdateCompositorRegistration();
    }
    ImGui::ShowHelp("Draw overlays beneath the in-game UI (menus, party window, etc.) instead of on top.\n"
                    "Experimental: hooks GW's UI render pass. Turn off to restore the original on-top drawing.");
    if (render_under_ui) {
        if (GameWorldCompositor::HasFailed())
            ImGui::TextColored(red, "  under-UI hook FAILED to install - drawing on top.");
        else if (GameWorldCompositor::IsActive())
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Green()), "  under-UI hook active.");
        else
            ImGui::TextDisabled("  under-UI hook: not installed yet.");
    }

    ImGui::Checkbox("Keep clear of the compass", &exclude_compass);
    ImGui::ShowHelp("Don't draw in-world overlays over the in-game compass/minimap.\n"
                    "GW renders the compass terrain and its frame in separate passes, so overlays "
                    "would otherwise bleed across the inside of the minimap.");

    ImGui::Checkbox("Occlude behind terrain", &occlude_behind_terrain);
    ImGui::ShowHelp("Hide overlays behind walls, buildings and terrain using the game's depth buffer.\n"
                    "If overlays vanish or z-fight badly, disable this or tune the depth values below.");
    if (occlude_behind_terrain) {
        ImGui::DragFloat("Projection near plane", &z_near, 0.1f, 1.f, 100.f, "%.1f");
        ImGui::ShowHelp("Must match the game's near clip for occlusion to be accurate.\n"
                        "~47 is correct; too low hides visible overlays, too high lets them show through walls.");
        ImGui::DragFloat("Projection far plane", &z_far, 50.f, 1000.f, 200000.f, "%.0f");
        ImGui::ShowHelp("Insensitive anywhere from ~50000 to 200000.");
    }
}

void GameWorldRenderer::TriggerSyncAllMarkers()
{
    need_sync_markers = true;
}

void GameWorldRenderer::SetNavmeshLines(GW::Constants::MapID map_id, std::vector<BatchedLine> lines)
{
    // Render-thread only (called from PathfindingWindow::Draw, same thread as DrawInWorld) — no lock needed.
    // Start a NEW build into staging; the live `verts` keep drawing until it completes, so the swap is seamless.
    auto& b = navmesh_batch;
    b.pending_map = map_id;
    b.lines = std::move(lines);
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
    // Index the current renderables so the three Sync* passes match in O(1); find_matching_poly reads it.
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
            // don't draw normal lines in doa outpost
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
            // don't draw normal polys in doa outpost
            continue;
        }
        std::vector<GW::GamePos> pts{};
        std::ranges::transform(poly.points, std::back_inserter(pts), [](const GW::Vec2f& pt) { return GW::GamePos(pt); });

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
            // don't draw normal markers in doa outpost
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
// ToolboxModule lifecycle (own settings section / JSON file, separate from Minimap)
// ===========================================================================

void GameWorldRenderer::Initialize()
{
    ToolboxModule::Initialize(); // registers DrawSettingsInternal() under "In-game rendering"
    // Register fields against this module so they persist in their own section, not under the Minimap.
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
    // Drop our under-UI draw the instant the module is disabled; the shared compositor removes its
    // hook once no module is registered. Render() stops being called too (gated on module enabled).
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
}
