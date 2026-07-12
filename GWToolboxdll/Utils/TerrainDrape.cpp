#include "stdafx.h"

#include <cfloat>
#include <cmath>
#include <cstddef>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/TerrainDrape.h>

namespace {
    // GW's terrain heightfield object (MapContext+0x84). Only the fields the altitude sampler touches
    // are named; layout reverse-engineered from Terrain_QueryAltitude (Gw.exe FUN_0074eb10). See
    // memory reference-gw-terrain-altitude-internals for the full derivation.
    struct GwTerrain {
        uint32_t type;           // +0x00
        uint32_t dim_x;          // +0x04 tiles in x (multiple of 32)
        uint32_t dim_y;          // +0x08 tiles in y
        uint32_t chunks_x;       // +0x0C dim_x >> 5
        uint32_t chunks_y;       // +0x10 dim_y >> 5
        uint32_t flags;          // +0x14 bit0: clamp z>0 result to 0
        float min_x;             // +0x18
        float min_y;             // +0x1C
        float max_x;             // +0x20
        float max_y;             // +0x24 grid anchored at (min_x, max_y)
        uint32_t pad0[2];        // +0x28
        uint32_t grid_dim_x;     // +0x30
        uint32_t grid_dim_y;     // +0x34
        uint32_t chunks_per_row; // +0x38 (dim_x >> 5) + 1  (the +1 is a border chunk column)
        uint32_t chunk_rows;     // +0x3C (dim_y >> 5) + 1
        float** chunk_ptrs;      // +0x40 chunks_per_row * chunk_rows entries -> 32x32 float32
        uint32_t chunk_count;    // +0x44
        uint32_t chunk_capacity; // +0x48
    };
    static_assert(offsetof(GwTerrain, dim_x) == 0x04);
    static_assert(offsetof(GwTerrain, flags) == 0x14);
    static_assert(offsetof(GwTerrain, min_x) == 0x18);
    static_assert(offsetof(GwTerrain, max_y) == 0x24);
    static_assert(offsetof(GwTerrain, chunks_per_row) == 0x38);
    static_assert(offsetof(GwTerrain, chunk_ptrs) == 0x40);
    static_assert(offsetof(GwTerrain, chunk_capacity) == 0x48);

    constexpr float kTile = 96.0f;
    constexpr float kNoData = FLT_MAX; // internal "no surface" sentinel; mapped to GW's 0.f at the API edge

    const GwTerrain* GetTerrain()
    {
        const GW::MapContext* mc = GW::GetMapContext();
        if (!mc || !mc->terrain) return nullptr;
        const auto* t = static_cast<const GwTerrain*>(mc->terrain);
        // Cheap sanity gate against a torn-down / mid-load terrain object.
        if (t->dim_x == 0 || t->dim_x > 0x8000 || t->dim_y == 0 || t->dim_y > 0x8000) return nullptr;
        if (t->chunks_per_row == 0 || !t->chunk_ptrs) return nullptr;
        return t;
    }

    float SampleGrid(const GwTerrain* t, const uint32_t gx, const uint32_t gy)
    {
        const uint32_t ci = (gy >> 5) * t->chunks_per_row + (gx >> 5);
        if (ci >= t->chunk_capacity) return kNoData;
        const float* chunk = t->chunk_ptrs[ci];
        if (!chunk) return kNoData;
        return chunk[(gy & 31) * 32 + (gx & 31)];
    }

    // cross2d(P,A,B) = (P.y-A.y)(B.x-A.x) - (P.x-A.x)(B.y-A.y); the exact side test GW uses (FUN_007294e0).
    float Cross2D(const float px, const float py, const float ax, const float ay, const float bx, const float by)
    {
        return (py - ay) * (bx - ax) - (px - ax) * (by - ay);
    }

    // GW's strict point-in-trapezoid test (FUN_0072b680): y-band open, left edge >= 0, right edge <= 0.
    bool TrapezoidContains(const GW::PathingTrapezoid* pt, const float x, const float y)
    {
        if (!(pt->YB < y && y < pt->YT)) return false;
        if (Cross2D(x, y, pt->XTL, pt->YT, pt->XBL, pt->YB) < 0.f) return false;
        if (Cross2D(x, y, pt->XTR, pt->YT, pt->XBR, pt->YB) > 0.f) return false;
        return true;
    }

    // Walk one plane's point-location DAG (root at PathingMap::root_node) to the containing trapezoid.
    // Returns true iff a real trapezoid of this plane covers (x,y). Mirrors GW's exact locator
    // (FUN_0072aa80); the toolbox's older Pathing::FindTrapezoid has an inverted entry guard, so this is
    // implemented fresh rather than reused.
    bool PlaneContains(const GW::PathingMap& pm, const float x, const float y)
    {
        const GW::Node* n = pm.root_node;
        int guard = 100000;
        while (n && guard-- > 0) {
            switch (n->type) {
                case 0: { // XNode
                    const auto* xn = static_cast<const GW::XNode*>(n);
                    const float d = (y - xn->pos.y) * xn->dir.x - (x - xn->pos.x) * xn->dir.y;
                    n = d >= 0.f ? xn->right : xn->left;
                    break;
                }
                case 1: { // YNode
                    const auto* yn = static_cast<const GW::YNode*>(n);
                    n = (y > yn->pos.y) ? yn->above
                        : (y < yn->pos.y) ? yn->below
                        : (x >= yn->pos.x) ? yn->above : yn->below;
                    break;
                }
                case 2: { // SinkNode -- the leaf's trapezoid contains (x,y) only if the point is really on it
                    const auto* sn = static_cast<const GW::SinkNode*>(n);
                    return sn->trapezoid && TrapezoidContains(sn->trapezoid, x, y);
                }
                default:
                    return false;
            }
        }
        return false;
    }
}

float TerrainDrape::NativeTerrainZ(const float x, const float y)
{
    const GwTerrain* t = GetTerrain();
    if (!t) return 0.f;

    const float gxf = (x - t->min_x) / kTile;
    const float gyf = (t->max_y - y) / kTile; // grid rows grow toward -y
    if (gxf < 0.f || gyf < 0.f) return 0.f;

    const auto gxi = static_cast<uint32_t>(gxf);
    const auto gyi = static_cast<uint32_t>(gyf);
    if (gxi >= t->dim_x || gyi >= t->dim_y) return 0.f; // gxi+1 <= dim_x reads the border chunk column

    const float fx = gxf - static_cast<float>(gxi);
    const float fy = gyf - static_cast<float>(gyi);

    const float zA = SampleGrid(t, gxi, gyi);
    const float zB = SampleGrid(t, gxi + 1, gyi);
    const float zC = SampleGrid(t, gxi, gyi + 1);
    const float zD = SampleGrid(t, gxi + 1, gyi + 1);
    if (zA == kNoData || zB == kNoData || zC == kNoData || zD == kNoData) return 0.f;

    // Cell is split on the B-C diagonal (fx+fy==1), matching the game's triangulation.
    float z;
    if (fx + fy <= 1.f)
        z = zA + fx * (zB - zA) + fy * (zC - zA);
    else
        z = zB + (fx + fy - 1.f) * (zD - zB) + (1.f - fx) * (zC - zB);

    if ((t->flags & 1u) && z > 0.f) z = 0.f; // GW clamps below-zero-plane surfaces
    return z;
}

float TerrainDrape::QueryAltAt(const float x, const float y, const uint32_t zplane)
{
    if (zplane == 0)
        return NativeTerrainZ(x, y);
    GW::GamePos p{x, y, zplane};
    return GW::Map::QueryAltitude(&p);
}

float TerrainDrape::DrapeZ(const float x, const float y, const uint32_t zplane, const uint32_t n_planes, const float ref)
{
    float best = ref, best_d = FLT_MAX;

    const float ground = NativeTerrainZ(x, y);
    if (ground != 0.f) {
        best_d = std::fabs(ground - ref);
        best = ground;
    }

    const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
    if (pm) {
        const auto planes = std::min<uint32_t>(n_planes, static_cast<uint32_t>(pm->size()));
        for (uint32_t zp = 1; zp < planes; ++zp) {
            if (!PlaneContains((*pm)[zp], x, y)) continue;
            const float a = QueryAltAt(x, y, zp);
            if (a == 0.f) continue;
            const float d = std::fabs(a - ref);
            if (d < best_d || (d == best_d && zp == zplane)) {
                best_d = d;
                best = a;
            }
        }
    }
    return best;
}

float TerrainDrape::SurfaceZ(const float x, const float y, const uint32_t, const uint32_t n_planes)
{
    // Highest surface = min z. Plane-0 ground is a native read; only planes whose trapezoid actually
    // covers (x,y) can add a higher (bridge/prop) surface, so the old blind all-planes loop is pruned
    // to those candidates.
    float best = NativeTerrainZ(x, y); // 0.f when off-terrain

    const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
    if (pm) {
        const auto planes = std::min<uint32_t>(n_planes, static_cast<uint32_t>(pm->size()));
        for (uint32_t zp = 1; zp < planes; ++zp) {
            if (!PlaneContains((*pm)[zp], x, y)) continue;
            const float a = QueryAltAt(x, y, zp);
            if (a != 0.f && (best == 0.f || a < best)) best = a;
        }
    }
    return best;
}

uint32_t TerrainDrape::PathingPlaneCount()
{
    const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
    return pm ? static_cast<uint32_t>(pm->size()) : 0;
}
