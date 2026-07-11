#pragma once

#include <cstdint>

// Surface-draping helpers shared by the in-world overlay modules (danger rings, loot beacons,
// skill range rings). QueryAltitude swaps the global map context, so call these ONLY on the
// render thread. GW's "no altitude data" sentinel is 0.f, and up is -z (highest surface = min z).
namespace TerrainDrape {
    float QueryAltAt(float x, float y, uint32_t zplane);

    // Surface altitude at (x,y), choosing the plane closest to `ref`.
    float DrapeZ(float x, float y, uint32_t zplane, uint32_t n_planes, float ref);

    // Highest static surface at (x,y) across all planes. Returns 0.f when nothing has data.
    float SurfaceZ(float x, float y, uint32_t zplane, uint32_t n_planes);

    // Plane count of the current pathing map; 0 while the map is still loading.
    uint32_t PathingPlaneCount();
}
