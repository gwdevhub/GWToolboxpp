#pragma once

#include <cstdint>

// Surface-draping helpers for the in-world overlay modules (danger rings, loot beacons, skill range rings).
// GW convention: "no altitude data" = 0.f, up is -z (highest surface = min z).
// Plane 0 is a native heightfield read (any thread); non-zero planes use QueryAltitude, so DrapeZ/SurfaceZ are render-thread only.
namespace TerrainDrape {
    // Terrain surface z at (x,y) on plane 0 from the heightfield; 0.f when off-terrain or unloaded (native read, any thread).
    float NativeTerrainZ(float x, float y);

    float QueryAltAt(float x, float y, uint32_t zplane);

    // Surface altitude at (x,y) closest to `ref`: terrain + prop surfaces once the PropSurface index is baked, else terrain + walkable planes.
    float DrapeZ(float x, float y, uint32_t zplane, uint32_t n_planes, float ref);

    // Highest static surface at (x,y); 0.f when no data. Includes non-walkable props (railings, fences) once the PropSurface index is baked.
    float SurfaceZ(float x, float y, uint32_t zplane, uint32_t n_planes);

    // Plane count of the current pathing map; 0 while the map is still loading.
    uint32_t PathingPlaneCount();

    // Verification helper (harness `drapeverify`): fills old_all (QueryAltitude on every plane, min z), new_z (current SurfaceZ), prune_gp0 (game plane-0 + trapezoid-pruned non-zero planes). nullptr skipped. Render thread only.
    void DrapeCompare(float x, float y, uint32_t n_planes, float* old_all, float* new_z, float* prune_gp0);
}
