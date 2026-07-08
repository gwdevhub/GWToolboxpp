#include "stdafx.h"

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/TerrainDrape.h>

float TerrainDrape::QueryAltAt(const float x, const float y, const uint32_t zplane)
{
    GW::GamePos p{x, y, zplane};
    return GW::Map::QueryAltitude(&p);
}

float TerrainDrape::DrapeZ(const float x, const float y, const uint32_t zplane, const uint32_t n_planes, const float ref)
{
    float best = ref, best_d = FLT_MAX;
    for (uint32_t zp = 0; zp < n_planes; ++zp) {
        const float a = QueryAltAt(x, y, zp);
        if (a == 0.f) continue;
        const float d = std::fabs(a - ref);
        if (d < best_d || (d == best_d && zp == zplane)) {
            best_d = d;
            best = a;
        }
    }
    return best;
}

float TerrainDrape::SurfaceZ(const float x, const float y, const uint32_t, const uint32_t n_planes)
{
    float best = 0.f;
    for (uint32_t zp = 0; zp < n_planes; ++zp) {
        const float a = QueryAltAt(x, y, zp);
        if (a != 0.f && (best == 0.f || a < best)) best = a;
    }
    return best;
}

uint32_t TerrainDrape::PathingPlaneCount()
{
    const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
    return pm ? static_cast<uint32_t>(pm->size()) : 0;
}
