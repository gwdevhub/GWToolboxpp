#include "stdafx.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>

#include <Logger.h>
#include <Modules/Resources.h>
#include <Utils/PropSurfaceIndex.h>

namespace {
    // ---- Live-memory layout of a prop's collision model, decoded from Gw.exe FUN_0073b960 /
    // FUN_00776b20 / FUN_007835d0 (prop-planes RE report; validated in-game by the harness `propverify`
    // verb against QueryAltitude ground truth).
    //
    // RTL handle (what GWCA declares as RecObject* MapProp::interactive_model):
    //   +0x00 object pointer (getObject's return), +0x08 accessKey tag; tag 0 passes untagged handles.
    // 'mdl ' object: +0x08 ModelData*, +0x10..0x18 world translation, +0x1c..0x30 two rotation basis
    //   vectors (only when flags +0xe4 bit 0x20000), +0x34 uniform scale (applied when != 1).
    // ModelData: +0xac submodel count, +0xb0 handle array of 'mgrg' collision geometries.
    // 'mgrg' object: +0x10 Mesh*. Mesh: +0x08 u16* indices, +0x10 index count, +0x18 vertex count,
    //   +0x1c vertex base (model-space float3 at the start of each element), +0x54 vertex stride.
    //
    // NON-walkable props (railings, fences) have NO collision submodels; their only triangles are the
    // RENDER records at ModelData +0xa0 (descriptor pointer array) / +0xa4 (count), one CPU-resident heap
    // blob per record (loader FUN_00794020, consumer FUN_00779d00): 0x24-byte header {+0x04 LOD0 index
    // count, +0x08 LOD1, +0x0c LOD2 (equal counts share the previous buffer), +0x10 vertex count, +0x14
    // runtime vertex format (bit0 = float3 position at vertex offset 0, mandatory)}, payload at +0x24 =
    // u16 index lists, then FVF-strided vertices. Stride tables from GrVertexFormatSize FUN_00687d30.
    constexpr uint32_t kTagMdl = 0x6d646c20;  // 'mdl '
    constexpr uint32_t kTagMgrg = 0x6772676d; // 'mgrg'

    constexpr uint32_t kFvfT1[16] = {0, 0xC, 4, 0x10, 0xC, 0x18, 0x10, 0x1C, 4, 0x10, 8, 0x14, 0x10, 0x1C, 0x14, 0x20};
    constexpr uint32_t kFvfT2[8] = {0, 0xC, 0xC, 0x18, 0xC, 0x18, 0x18, 0x24};
    constexpr uint32_t kFvfT3[16] = {0, 8, 8, 0x10, 8, 0x10, 0x10, 0x18, 8, 0x10, 0x10, 0x18, 0x10, 0x18, 0x18, 0x20};

    uint32_t VertexStride(const uint32_t fmt)
    {
        return kFvfT1[fmt & 0xf] + kFvfT2[(fmt >> 4) & 7] + kFvfT3[(fmt >> 8) & 0xf] + kFvfT3[(fmt >> 12) & 0xf];
    }

    constexpr uint32_t kMaxSubmodels = 256;
    constexpr uint32_t kMaxIndices = 3u * 1024 * 1024;
    constexpr uint32_t kMaxVerts = 2u * 1024 * 1024;
    constexpr uint32_t kMaxTotalTris = 4u * 1024 * 1024;
    constexpr float kMinTriArea2 = 1e-4f; // twice-area below this in XY = never contains a strict point
    // Steeper than ~78 deg from horizontal can never be a useful drape surface (its XY footprint is a
    // sliver) but dominates the cell lists of walls/columns/railing sides; nz is 2x the signed XY area.
    constexpr float kMinNzFrac2 = 0.04f; // keep when nz^2 >= 4% of |n|^2, i.e. |cos(slope)| >= 0.2
    constexpr float kMaxTriSpan = 10000.f; // gwinches; no real prop surface triangle is wider

    template <typename T>
    const T Read(const void* base, const size_t offset)
    {
        return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + offset);
    }

    const void* ResolveHandle(const void* handle, const uint32_t tag)
    {
        if (!handle) return nullptr;
        const auto key = Read<uint32_t>(handle, 0x08);
        if (key != 0 && key != tag) return nullptr;
        return Read<const void*>(handle, 0x00);
    }

    struct MapKey {
        const void* props = nullptr;
        const void* terrain = nullptr;
        uint32_t prop_count = 0;
        uint32_t map_id = 0;

        bool operator==(const MapKey&) const = default;
    };

    MapKey LiveKey()
    {
        const GW::MapContext* mc = GW::GetMapContext();
        if (!mc || !mc->props) return {};
        return {mc->props, mc->terrain, mc->props->propArray.size(), static_cast<uint32_t>(GW::Map::GetMapID())};
    }

    struct MeshView {
        const uint16_t* indices;
        const uint8_t* verts;
        uint32_t index_count, vert_count, stride;
    };

    struct SnapMesh {
        std::vector<float> verts; // model-space xyz triples
        std::vector<uint16_t> indices;
    };

    struct SnapProp {
        float r[9]; // model->world rotation (rows) with uniform scale folded in
        float t[3];
        std::vector<SnapMesh> meshes;
    };

    struct Snapshot {
        MapKey key;
        std::vector<SnapProp> props;
        uint32_t walked_props = 0, skipped_props = 0, meshes = 0, render_props = 0;
        float snapshot_ms = 0.f;
    };

    void CopyMesh(const MeshView& mv, SnapProp* sp)
    {
        SnapMesh sm;
        // u16 indices can never reference past 65536 verts; a larger header count is padding or a lie.
        const uint32_t nv = std::min(mv.vert_count, 65536u);
        sm.verts.resize(static_cast<size_t>(nv) * 3);
        for (uint32_t v = 0; v < nv; ++v) {
            const auto* src = reinterpret_cast<const float*>(mv.verts + static_cast<size_t>(v) * mv.stride);
            sm.verts[v * 3 + 0] = src[0];
            sm.verts[v * 3 + 1] = src[1];
            sm.verts[v * 3 + 2] = src[2];
        }
        sm.indices.assign(mv.indices, mv.indices + mv.index_count);
        sp->meshes.push_back(std::move(sm));
    }

    // Model->world transform of a prop's 'mdl ' object, mirroring PropModelPushWorldTransform
    // (FUN_007835d0): translation-only unless flag 0x20000 selects the two-basis-vector rotation, whose
    // rows are (-cross(A,B).x, +.y, -.z) / (-A.x, +A.y, -A.z) / (-B.x, +B.y, -B.z).
    void BuildTransform(const void* mdl, SnapProp* out)
    {
        out->t[0] = Read<float>(mdl, 0x10);
        out->t[1] = Read<float>(mdl, 0x14);
        out->t[2] = Read<float>(mdl, 0x18);
        const auto flags = Read<uint32_t>(mdl, 0xe4);
        if (flags & 0x20000) {
            const float ax = Read<float>(mdl, 0x1c), ay = Read<float>(mdl, 0x20), az = Read<float>(mdl, 0x24);
            const float bx = Read<float>(mdl, 0x28), by = Read<float>(mdl, 0x2c), bz = Read<float>(mdl, 0x30);
            const float cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
            const float rows[9] = {-cx, cy, -cz, -ax, ay, -az, -bx, by, -bz};
            memcpy(out->r, rows, sizeof(rows));
        }
        else {
            const float rows[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
            memcpy(out->r, rows, sizeof(rows));
        }
        const float s = Read<float>(mdl, 0x34);
        if (s != 0.f && s != 1.f) {
            for (float& v : out->r) v *= s;
        }
    }

    bool ReadMeshView(const void* geo_handle, MeshView* out)
    {
        const void* geo = ResolveHandle(geo_handle, kTagMgrg);
        if (!geo) return false;
        const auto* mesh = Read<const uint8_t*>(geo, 0x10);
        if (!mesh) return false;
        out->indices = Read<const uint16_t*>(mesh, 0x08);
        out->index_count = Read<uint32_t>(mesh, 0x10);
        out->vert_count = Read<uint32_t>(mesh, 0x18);
        out->verts = Read<const uint8_t*>(mesh, 0x1c);
        out->stride = Read<uint32_t>(mesh, 0x54);
        if (!out->indices || !out->verts) return false;
        if (out->index_count < 3 || out->index_count % 3 != 0 || out->index_count > kMaxIndices) return false;
        if (out->vert_count < 3 || out->vert_count > kMaxVerts) return false;
        if (out->stride < 12 || out->stride > 512) return false;
        return true;
    }
    // One render-geometry record (LOD0 slice). Returns false when the descriptor fails sanity checks.
    bool ReadRenderRecord(const uint8_t* g, MeshView* out)
    {
        if (!g) return false;
        const uint32_t i0 = Read<uint32_t>(g, 0x04), i1 = Read<uint32_t>(g, 0x08), i2 = Read<uint32_t>(g, 0x0c);
        const uint32_t nv = Read<uint32_t>(g, 0x10), fmt = Read<uint32_t>(g, 0x14);
        if (!(fmt & 1)) return false;
        const uint32_t stride = VertexStride(fmt);
        if (stride < 12 || stride > 512) return false;
        if (i0 < 3 || i0 % 3 != 0 || i0 > kMaxIndices) return false;
        if (i1 > kMaxIndices || i2 > kMaxIndices) return false; // a lying LOD count would displace the verts pointer
        if (nv < 3 || nv > kMaxVerts) return false;
        const uint32_t idx_total = i0 + (i1 != i0 ? i1 : 0) + (i2 != i1 ? i2 : 0);
        out->indices = reinterpret_cast<const uint16_t*>(g + 0x24);
        out->index_count = i0;
        out->verts = g + 0x24 + static_cast<size_t>(idx_total) * 2;
        out->vert_count = nv;
        out->stride = stride;
        return true;
    }

    // Main thread only: copies every prop's collision geometry out of live game memory so the worker
    // never touches structures the engine could tear down.
    void TakeSnapshot(const MapKey& key, Snapshot* out)
    {
        const auto t0 = std::chrono::steady_clock::now();
        out->key = key;
        const auto& props = static_cast<const GW::PropsContext*>(key.props)->propArray;
        out->props.reserve(props.size());
        for (uint32_t i = 0; i < props.size(); ++i) {
            const GW::MapProp* p = props[i];
            const void* mdl = p ? ResolveHandle(p->interactive_model, kTagMdl) : nullptr;
            const auto* md = mdl ? Read<const uint8_t*>(mdl, 0x08) : nullptr;
            if (!md) {
                ++out->skipped_props;
                continue;
            }
            const uint32_t submodels = std::min(Read<uint32_t>(md, 0xac), kMaxSubmodels);
            SnapProp sp;
            BuildTransform(mdl, &sp);
            const auto* handles = Read<const void* const*>(md, 0xb0);
            for (uint32_t s = 0; handles && s < submodels; ++s) {
                MeshView mv;
                if (!ReadMeshView(handles[s], &mv)) continue;
                CopyMesh(mv, &sp);
                ++out->meshes;
            }
            // Render records are needed even on props WITH collision: e.g. the Temple staircases'
            // collision is just the walkable ramp, while the flanking balustrades exist only in the
            // render mesh. Collision stays in as the game's canonical (smooth) walkable surface.
            const bool had_collision = !sp.meshes.empty();
            const uint32_t render_n = Read<uint32_t>(md, 0xa4);
            const auto* recs = render_n && render_n <= 4096 ? Read<const uint8_t* const*>(md, 0xa0) : nullptr;
            const size_t before_render = sp.meshes.size();
            for (uint32_t s = 0; recs && s < render_n; ++s) {
                MeshView mv;
                if (!ReadRenderRecord(recs[s], &mv)) continue;
                CopyMesh(mv, &sp);
                ++out->meshes;
            }
            if (!had_collision && sp.meshes.size() > before_render) ++out->render_props;
            if (sp.meshes.empty()) {
                ++out->skipped_props;
                continue;
            }
            ++out->walked_props;
            out->props.push_back(std::move(sp));
        }
        out->snapshot_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }

    struct Tri {
        float ax, ay, az, bx, by, bz, cx, cy, cz;
    };

    struct PropIndex {
        MapKey key;
        float min_x = 0.f, min_y = 0.f, cell = 64.f;
        uint32_t nx = 0, ny = 0;
        std::vector<uint32_t> cell_start; // nx*ny + 1 prefix sums into refs
        std::vector<uint32_t> refs;       // per cell, sorted by tri zmin ascending (highest first)
        std::vector<float> refs_zmin;     // tri zmin per ref, for the early-out in the queries
        std::vector<Tri> tris;
        PropSurface::Stats stats{};
    };

    std::atomic<const PropIndex*> g_index{nullptr};
    std::atomic<bool> g_bake_in_flight{false};
    std::atomic<uint32_t> g_generation{0};
    std::atomic<bool> g_enabled{false}; // prop draping off by default; see PropSurface::SetEnabled

    // Superseded indices parked by the worker until the next main-thread BeginFrame: a query somewhere
    // in the current frame may still be reading the old pointer, but nothing can hold it across the
    // frame boundary (queries never cache it). Owned here so shutdown ordering never leaks one.
    std::mutex g_retired_mutex;
    std::vector<const PropIndex*> g_retired;

    void RetireIndex(const PropIndex* old)
    {
        if (!old) return;
        std::scoped_lock lock(g_retired_mutex);
        g_retired.push_back(old);
    }

    void DrainRetired()
    {
        std::vector<const PropIndex*> dead;
        {
            std::scoped_lock lock(g_retired_mutex);
            dead.swap(g_retired);
        }
        for (const PropIndex* d : dead) delete d;
    }

    // Main-thread-only memo: the index needs its map key checked against the live context only once per
    // frame, not per query (the context can only change between frames, on the game thread).
    uint32_t g_frame = 0;
    uint32_t g_validated_frame = ~0u;
    const PropIndex* g_validated = nullptr;

    PropIndex* BuildIndex(const Snapshot& snap)
    {
        const auto t0 = std::chrono::steady_clock::now();
        auto* idx = new PropIndex();
        idx->key = snap.key;

        std::vector<float> world;
        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
        size_t candidate = 0;
        for (const SnapProp& p : snap.props) {
            for (const SnapMesh& m : p.meshes) candidate += m.indices.size() / 3;
        }
        idx->tris.reserve(std::min<size_t>(candidate, kMaxTotalTris));
        bool full = false;
        for (const SnapProp& p : snap.props) {
            if (full) break;
            for (const SnapMesh& m : p.meshes) {
                if (full) break;
                const size_t nv = m.verts.size() / 3;
                world.resize(nv * 3);
                for (size_t v = 0; v < nv; ++v) {
                    const float mx = m.verts[v * 3], my = m.verts[v * 3 + 1], mz = m.verts[v * 3 + 2];
                    world[v * 3 + 0] = p.r[0] * mx + p.r[1] * my + p.r[2] * mz + p.t[0];
                    world[v * 3 + 1] = p.r[3] * mx + p.r[4] * my + p.r[5] * mz + p.t[1];
                    world[v * 3 + 2] = p.r[6] * mx + p.r[7] * my + p.r[8] * mz + p.t[2];
                }
                for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                    const uint32_t a = m.indices[i], b = m.indices[i + 1], c = m.indices[i + 2];
                    if (a >= nv || b >= nv || c >= nv) continue;
                    Tri t{world[a * 3], world[a * 3 + 1], world[a * 3 + 2],
                          world[b * 3], world[b * 3 + 1], world[b * 3 + 2],
                          world[c * 3], world[c * 3 + 1], world[c * 3 + 2]};
                    // A non-finite or absurdly large triangle (garbage payload) would poison the grid
                    // math and the zmin sort; real prop surfaces are well under kMaxTriSpan across.
                    const float txmin = std::min({t.ax, t.bx, t.cx}), txmax = std::max({t.ax, t.bx, t.cx});
                    const float tymin = std::min({t.ay, t.by, t.cy}), tymax = std::max({t.ay, t.by, t.cy});
                    const float tzmin = std::min({t.az, t.bz, t.cz}), tzmax = std::max({t.az, t.bz, t.cz});
                    if (!(std::isfinite(txmin) && std::isfinite(txmax) && std::isfinite(tymin) &&
                          std::isfinite(tymax) && std::isfinite(tzmin) && std::isfinite(tzmax))) continue;
                    if (txmax - txmin > kMaxTriSpan || tymax - tymin > kMaxTriSpan) continue;
                    const float ux = t.bx - t.ax, uy = t.by - t.ay, uz = t.bz - t.az;
                    const float vx = t.cx - t.ax, vy = t.cy - t.ay, vz = t.cz - t.az;
                    const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
                    if (std::fabs(nz) < kMinTriArea2) continue;
                    if (nz * nz < kMinNzFrac2 * (nx * nx + ny * ny + nz * nz)) continue;
                    min_x = std::min(min_x, txmin);
                    max_x = std::max(max_x, txmax);
                    min_y = std::min(min_y, tymin);
                    max_y = std::max(max_y, tymax);
                    idx->tris.push_back(t);
                    if (idx->tris.size() >= kMaxTotalTris) {
                        full = true;
                        break;
                    }
                }
            }
        }
        if (full) Log::Log("[propsurface] triangle cap hit (%u), index truncated", kMaxTotalTris);

        idx->stats.props = snap.walked_props;
        idx->stats.render_props = snap.render_props;
        idx->stats.skipped_props = snap.skipped_props;
        idx->stats.meshes = snap.meshes;
        idx->stats.triangles = static_cast<uint32_t>(idx->tris.size());
        idx->stats.snapshot_ms = snap.snapshot_ms;

        if (!idx->tris.empty()) {
            const float w = max_x - min_x, h = max_y - min_y;
            float cell = 64.f;
            // Cap the grid around a million cells; huge maps get proportionally coarser cells.
            const float target = std::sqrt(std::max(1.f, w * h / 1.0e6f));
            if (target > cell) cell = target;
            const auto nx = std::clamp(static_cast<uint32_t>(w / cell) + 1, 1u, 65536u);
            const auto ny = std::clamp(static_cast<uint32_t>(h / cell) + 1, 1u, 65536u);
            cell = std::max({cell, w / static_cast<float>(nx), h / static_cast<float>(ny)});
            idx->min_x = min_x;
            idx->min_y = min_y;
            idx->cell = cell;
            idx->nx = nx;
            idx->ny = ny;

            const auto cell_of = [&](const float x, const float y) -> std::pair<uint32_t, uint32_t> {
                return {static_cast<uint32_t>((x - min_x) / cell), static_cast<uint32_t>((y - min_y) / cell)};
            };
            std::vector<uint32_t> counts(static_cast<size_t>(nx) * ny + 1, 0);
            const auto for_cells = [&](const Tri& t, auto&& fn) {
                const auto [cx0, cy0] = cell_of(std::min({t.ax, t.bx, t.cx}), std::min({t.ay, t.by, t.cy}));
                const auto [cx1, cy1] = cell_of(std::max({t.ax, t.bx, t.cx}), std::max({t.ay, t.by, t.cy}));
                for (uint32_t cy = cy0; cy <= std::min(cy1, ny - 1); ++cy) {
                    for (uint32_t cx = cx0; cx <= std::min(cx1, nx - 1); ++cx) fn(static_cast<size_t>(cy) * nx + cx);
                }
            };
            for (const Tri& t : idx->tris) for_cells(t, [&](const size_t c) { ++counts[c + 1]; });
            for (size_t c = 1; c < counts.size(); ++c) counts[c] += counts[c - 1];
            idx->cell_start = counts;
            idx->refs.resize(counts.back());
            std::vector<uint32_t> cursor(counts.begin(), counts.end() - 1);
            for (uint32_t ti = 0; ti < idx->tris.size(); ++ti) {
                for_cells(idx->tris[ti], [&](const size_t c) { idx->refs[cursor[c]++] = ti; });
            }
            // Highest surface = min z, so with each cell sorted by tri zmin the query can stop as soon
            // as the next triangle's whole z-range is below the best hit.
            const auto tri_zmin = [&](const uint32_t ti) {
                const Tri& t = idx->tris[ti];
                return std::min({t.az, t.bz, t.cz});
            };
            for (size_t c = 0; c + 1 < idx->cell_start.size(); ++c) {
                std::sort(idx->refs.begin() + idx->cell_start[c], idx->refs.begin() + idx->cell_start[c + 1],
                          [&](const uint32_t a, const uint32_t b) { return tri_zmin(a) < tri_zmin(b); });
            }
            idx->refs_zmin.resize(idx->refs.size());
            for (size_t i = 0; i < idx->refs.size(); ++i) idx->refs_zmin[i] = tri_zmin(idx->refs[i]);
            idx->stats.cells_x = nx;
            idx->stats.cells_y = ny;
            idx->stats.refs = static_cast<uint32_t>(idx->refs.size());
        }
        idx->stats.bake_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return idx;
    }

    // Main thread. Cheap when a bake is already running or the map isn't ready.
    void MaybeRequestBake()
    {
        if (g_bake_in_flight.load(std::memory_order_relaxed)) return;
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) return;
        const MapKey key = LiveKey();
        if (!key.props) return;
        if (g_bake_in_flight.exchange(true)) return;
        const uint32_t gen = g_generation.fetch_add(1) + 1;
        auto snap = std::make_shared<Snapshot>();
        try {
            TakeSnapshot(key, snap.get());
        }
        catch (...) {
            g_bake_in_flight.store(false);
            Log::Log("[propsurface] snapshot threw; prop draping disabled until the next map load");
            return;
        }
        Resources::EnqueueWorkerTask([snap, gen] {
            // The worker pool runs tasks bare inside a jthread: an escaping exception (e.g. bad_alloc
            // on a torn snapshot) would std::terminate the game, so everything is contained here.
            try {
                std::unique_ptr<PropIndex> idx(BuildIndex(*snap));
                if (g_generation.load() == gen) {
                    Log::Log("[propsurface] baked map=%u props=%u (render-only %u) meshes=%u tris=%u grid=%ux%u refs=%u snapshot=%.1fms build=%.1fms",
                             idx->key.map_id, idx->stats.props, idx->stats.render_props, idx->stats.meshes, idx->stats.triangles,
                             idx->stats.cells_x, idx->stats.cells_y, idx->stats.refs, idx->stats.snapshot_ms, idx->stats.bake_ms);
                    RetireIndex(g_index.exchange(idx.release()));
                }
            }
            catch (...) {
                Log::Log("[propsurface] bake threw; keeping the previous index");
            }
            g_bake_in_flight.store(false);
        });
    }

    const PropIndex* Acquire()
    {
        const PropIndex* idx = g_index.load(std::memory_order_acquire);
        if (idx) {
            if (idx == g_validated && g_validated_frame == g_frame) return idx;
            if (idx->key == LiveKey()) {
                g_validated = idx;
                g_validated_frame = g_frame;
                return idx;
            }
        }
        MaybeRequestBake();
        return nullptr;
    }

    // Exact plane z of the triangle at (x,y), clamped to the triangle's z-range like the game's
    // RayTriangleMinZ (FUN_0065d5f0).
    float TriZAt(const Tri& t, const float x, const float y)
    {
        const float zmin = std::min({t.az, t.bz, t.cz});
        const float zmax = std::max({t.az, t.bz, t.cz});
        const float ux = t.bx - t.ax, uy = t.by - t.ay, uz = t.bz - t.az;
        const float vx = t.cx - t.ax, vy = t.cy - t.ay, vz = t.cz - t.az;
        const float nz = ux * vy - uy * vx;
        if (nz == 0.f) return zmin;
        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float z = t.az + (nx * (t.ax - x) + ny * (t.ay - y)) / nz;
        return std::clamp(z, zmin, zmax);
    }

    bool TriContainsXY(const Tri& t, const float x, const float y)
    {
        const float d1 = (t.bx - t.ax) * (y - t.ay) - (t.by - t.ay) * (x - t.ax);
        const float d2 = (t.cx - t.bx) * (y - t.by) - (t.cy - t.by) * (x - t.bx);
        const float d3 = (t.ax - t.cx) * (y - t.cy) - (t.ay - t.cy) * (x - t.cx);
        const bool has_neg = d1 < 0.f || d2 < 0.f || d3 < 0.f;
        const bool has_pos = d1 > 0.f || d2 > 0.f || d3 > 0.f;
        return !(has_neg && has_pos);
    }

    bool CellRange(const PropIndex& idx, const float x, const float y, uint32_t* lo, uint32_t* hi)
    {
        if (idx.nx == 0 || x < idx.min_x || y < idx.min_y) return false;
        const auto cx = static_cast<uint32_t>((x - idx.min_x) / idx.cell);
        const auto cy = static_cast<uint32_t>((y - idx.min_y) / idx.cell);
        if (cx >= idx.nx || cy >= idx.ny) return false;
        const size_t c = static_cast<size_t>(cy) * idx.nx + cx;
        *lo = idx.cell_start[c];
        *hi = idx.cell_start[c + 1];
        return true;
    }
}

bool PropSurface::HighestZAt(const float x, const float y, float* out)
{
    if (!g_enabled.load(std::memory_order_relaxed)) return false; // caller falls back to QueryAltitude
    // An empty index (bake saw no usable prop geometry) keeps callers on the QueryAltitude fallback
    // instead of silently degrading every drape to terrain-only.
    const PropIndex* idx = Acquire();
    if (!idx || idx->tris.empty()) return false;
    float best = kNoData;
    uint32_t lo, hi;
    if (CellRange(*idx, x, y, &lo, &hi)) {
        for (uint32_t i = lo; i < hi; ++i) {
            if (idx->refs_zmin[i] >= best) break; // sorted by zmin: nothing below can be higher
            const Tri& t = idx->tris[idx->refs[i]];
            if (!TriContainsXY(t, x, y)) continue;
            const float z = TriZAt(t, x, y);
            if (z < best) best = z;
        }
    }
    *out = best;
    return true;
}

bool PropSurface::ClosestZAt(const float x, const float y, const float ref, float* out)
{
    if (!g_enabled.load(std::memory_order_relaxed)) return false; // caller falls back to QueryAltitude
    const PropIndex* idx = Acquire();
    if (!idx || idx->tris.empty()) return false;
    float best = kNoData, best_d = FLT_MAX;
    uint32_t lo, hi;
    if (CellRange(*idx, x, y, &lo, &hi)) {
        for (uint32_t i = lo; i < hi; ++i) {
            if (idx->refs_zmin[i] - ref >= best_d) break; // every later candidate z >= zmin, so |z-ref| >= best_d
            const Tri& t = idx->tris[idx->refs[i]];
            if (!TriContainsXY(t, x, y)) continue;
            const float z = TriZAt(t, x, y);
            const float d = std::fabs(z - ref);
            if (d < best_d) {
                best_d = d;
                best = z;
            }
        }
    }
    *out = best;
    return true;
}

void PropSurface::SetEnabled(const bool on)
{
    g_enabled.store(on, std::memory_order_relaxed);
}

bool PropSurface::Enabled()
{
    return g_enabled.load(std::memory_order_relaxed);
}

bool PropSurface::Ready()
{
    const PropIndex* idx = g_index.load(std::memory_order_acquire);
    return idx && idx->key == LiveKey();
}

void PropSurface::BeginFrame()
{
    ++g_frame;
    DrainRetired();
}

void PropSurface::Terminate()
{
    g_generation.fetch_add(1); // orphan any in-flight bake so it never publishes
    g_validated = nullptr;
    delete g_index.exchange(nullptr);
    DrainRetired(); // workers are joined by now (Resources terminated first), nothing can re-add
    g_bake_in_flight.store(false);
}

bool PropSurface::GetStats(Stats* out)
{
    const PropIndex* idx = g_index.load(std::memory_order_acquire);
    if (!idx) return false;
    *out = idx->stats;
    return true;
}

#if defined(_DEBUG) || defined(GWTB_HARNESS)
void PropSurface::DebugDumpProp(const uint32_t prop_index)
{
    const GW::MapContext* mc = GW::GetMapContext();
    if (!mc || !mc->props || prop_index >= mc->props->propArray.size()) {
        Log::Log("[propmesh] prop %u out of range (props=%u)", prop_index, mc && mc->props ? mc->props->propArray.size() : 0);
        return;
    }
    const GW::MapProp* p = mc->props->propArray[prop_index];
    if (!p) {
        Log::Log("[propmesh] prop[%u] = null", prop_index);
        return;
    }
    Log::Log("[propmesh] prop[%u] file=0x%X pos=(%.1f,%.1f,%.1f) rot=%.3f handle=%p", prop_index, p->model_file_id,
             p->position.x, p->position.y, p->position.z, p->rotation_angle, static_cast<const void*>(p->interactive_model));
    const void* h = p->interactive_model;
    if (!h) return;
    Log::Log("[propmesh]   handle key=0x%08X obj=%p", Read<uint32_t>(h, 0x08), Read<const void*>(h, 0x00));
    const void* mdl = ResolveHandle(h, kTagMdl);
    if (!mdl) {
        Log::Log("[propmesh]   handle tag mismatch (want 'mdl ')");
        return;
    }
    const auto flags = Read<uint32_t>(mdl, 0xe4);
    SnapProp sp;
    BuildTransform(mdl, &sp);
    Log::Log("[propmesh]   mdl=%p flags=0x%X rot=%d scale=%.3f t=(%.1f,%.1f,%.1f) r0=(%.3f,%.3f,%.3f) r1=(%.3f,%.3f,%.3f) r2=(%.3f,%.3f,%.3f)",
             mdl, flags, (flags & 0x20000) ? 1 : 0, Read<float>(mdl, 0x34), sp.t[0], sp.t[1], sp.t[2],
             sp.r[0], sp.r[1], sp.r[2], sp.r[3], sp.r[4], sp.r[5], sp.r[6], sp.r[7], sp.r[8]);
    const auto* md = Read<const uint8_t*>(mdl, 0x08);
    if (!md) {
        Log::Log("[propmesh]   ModelData null");
        return;
    }
    const auto dump_mesh = [&sp](const char* kind, const uint32_t s, const MeshView& mv) {
        float zmin = FLT_MAX, zmax = -FLT_MAX, xmin = FLT_MAX, xmax = -FLT_MAX, ymin = FLT_MAX, ymax = -FLT_MAX;
        for (uint32_t v = 0; v < mv.vert_count; ++v) {
            const auto* src = reinterpret_cast<const float*>(mv.verts + static_cast<size_t>(v) * mv.stride);
            const float wx = sp.r[0] * src[0] + sp.r[1] * src[1] + sp.r[2] * src[2] + sp.t[0];
            const float wy = sp.r[3] * src[0] + sp.r[4] * src[1] + sp.r[5] * src[2] + sp.t[1];
            const float wz = sp.r[6] * src[0] + sp.r[7] * src[1] + sp.r[8] * src[2] + sp.t[2];
            xmin = std::min(xmin, wx); xmax = std::max(xmax, wx);
            ymin = std::min(ymin, wy); ymax = std::max(ymax, wy);
            zmin = std::min(zmin, wz); zmax = std::max(zmax, wz);
        }
        Log::Log("[propmesh]   %s[%u] tris=%u verts=%u stride=%u | world x[%.0f..%.0f] y[%.0f..%.0f] z[%.0f..%.0f]",
                 kind, s, mv.index_count / 3, mv.vert_count, mv.stride, xmin, xmax, ymin, ymax, zmin, zmax);
    };
    const uint32_t submodels = Read<uint32_t>(md, 0xac);
    const auto* handles = Read<const void* const*>(md, 0xb0);
    const uint32_t render_n = Read<uint32_t>(md, 0xa4);
    const auto* recs = Read<const uint8_t* const*>(md, 0xa0);
    Log::Log("[propmesh]   ModelData=%p collision=%u (%p) render=%u (%p)", md, submodels, handles, render_n, recs);
    if (render_n > 4096) recs = nullptr; // same plausibility gate as the snapshot walk
    for (uint32_t s = 0; handles && s < std::min(submodels, kMaxSubmodels); ++s) {
        MeshView mv;
        if (!ReadMeshView(handles[s], &mv)) {
            Log::Log("[propmesh]   col[%u] handle=%p key=0x%08X UNREADABLE", s, handles[s],
                     handles[s] ? Read<uint32_t>(handles[s], 0x08) : 0);
            continue;
        }
        dump_mesh("col", s, mv);
    }
    for (uint32_t s = 0; recs && s < std::min(render_n, 4096u); ++s) {
        MeshView mv;
        if (!ReadRenderRecord(recs[s], &mv)) {
            const uint32_t fmt = recs[s] ? Read<uint32_t>(recs[s], 0x14) : 0;
            Log::Log("[propmesh]   rnd[%u] rec=%p i0=%u nv=%u fmt=0x%X stride=%u REJECTED", s, recs[s],
                     recs[s] ? Read<uint32_t>(recs[s], 0x04) : 0, recs[s] ? Read<uint32_t>(recs[s], 0x10) : 0,
                     fmt, fmt ? VertexStride(fmt) : 0);
            continue;
        }
        dump_mesh("rnd", s, mv);
    }
}
#endif
