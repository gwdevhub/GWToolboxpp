#pragma once

#include <cstdint>

// Surface-draping helpers shared by the in-world overlay modules (danger rings, loot beacons,
// skill range rings). GW's "no altitude data" sentinel is 0.f, and up is -z (highest surface = min z).
//
// Plane 0 (the terrain heightfield) is read directly from the mapped terrain object -- no game call,
// no map-context swap -- so it is safe from any thread and cheap enough to sample per vertex per frame.
// Non-zero planes (bridges/prop surfaces) still route through GW::Map::QueryAltitude, which swaps the
// global map context, so the DrapeZ/SurfaceZ helpers must run ONLY on the render thread. They prune the
// old all-planes loop to just the planes whose pathing trapezoid actually contains (x,y).
namespace TerrainDrape {
    // Exact terrain surface z at (x,y) on plane 0, interpolated from the heightfield. Returns 0.f when
    // (x,y) is outside the terrain or the map is not loaded (matching GW's OOB sentinel). Native read.
    float NativeTerrainZ(float x, float y);

    float QueryAltAt(float x, float y, uint32_t zplane);

    // Surface altitude at (x,y), choosing the surface closest to `ref`. Once the PropSurface index is
    // baked, candidates are terrain + every prop collision surface; until then, terrain + walkable planes.
    float DrapeZ(float x, float y, uint32_t zplane, uint32_t n_planes, float ref);

    // Highest static surface at (x,y). Returns 0.f when nothing has data. Once the PropSurface index is
    // baked this includes NON-walkable props (railings, fences), so overlays drape on top of them.
    float SurfaceZ(float x, float y, uint32_t zplane, uint32_t n_planes);

    // Plane count of the current pathing map; 0 while the map is still loading.
    uint32_t PathingPlaneCount();

    // Verification helper (harness `drapeverify`). Fills three altitudes at (x,y):
    //   old_all   -- the previous behavior: GW::Map::QueryAltitude on EVERY plane, highest surface (min z)
    //   new_z     -- the current SurfaceZ (native plane-0 read + trapezoid-pruned non-zero planes)
    //   prune_gp0 -- the pruned non-zero planes combined with the GAME's plane-0 value, so the trapezoid
    //                pruning can be checked in isolation from the native-vs-game plane-0 sampling delta.
    // Any nullptr out-pointer is skipped. Render thread only (uses QueryAltitude).
    void DrapeCompare(float x, float y, uint32_t n_planes, float* old_all, float* new_z, float* prune_gp0);
}
