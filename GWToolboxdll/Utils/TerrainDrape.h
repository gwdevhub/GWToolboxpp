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

    // Surface altitude at (x,y), choosing the plane closest to `ref`.
    float DrapeZ(float x, float y, uint32_t zplane, uint32_t n_planes, float ref);

    // Highest static surface at (x,y) across all planes. Returns 0.f when nothing has data.
    float SurfaceZ(float x, float y, uint32_t zplane, uint32_t n_planes);

    // Plane count of the current pathing map; 0 while the map is still loading.
    uint32_t PathingPlaneCount();
}
