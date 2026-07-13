#pragma once

#include <cstdint>

// Prop-surface height oracle (tasks/prop-height-oracle-plan.md): the world-space collision triangles of
// every map prop ('mgrg' meshes reachable from MapProp+0x58) baked once per map into a uniform XY grid.
// The snapshot of the live model data is taken on GW's main thread; the heavy transform/grid build runs
// on a Resources worker; the finished immutable index is published by atomic pointer swap. Queries are
// pure math on that index -- no game call, no allocation -- cheap enough for thousands per frame.
// Until the bake lands (or after a map change invalidates the key) queries return false and callers fall
// back to walkable-plane draping. Unlike the pathing planes, this covers NON-walkable props (railings,
// fences, statues), so overlays can drape on top of them.
namespace PropSurface {
    // "No covering prop surface" sentinel (up is -z; real surfaces are finite, usually negative).
    inline constexpr float kNoData = 3.402823466e+38f;

    // Master switch (default OFF): overlays currently drape on terrain + walkable planes only, as
    // before the prop oracle. While disabled the queries no-op and never kick the per-map bake, so the
    // whole PropSurfaceIndex stays dormant. Flip on to drape onto non-walkable props (railings/fences).
    void SetEnabled(bool on);
    bool Enabled();

    // Highest covering prop surface at (x,y): min z among triangles containing the point, or kNoData.
    // Returns false while no index for the live map is published (caller falls back to QueryAltitude).
    // Main thread only: lazily snapshots the live map context to kick the bake.
    bool HighestZAt(float x, float y, float* out);

    // Covering prop surface nearest to `ref` (continuity draping for paths). Same contract as above.
    bool ClosestZAt(float x, float y, float ref, float* out);

    // True when the published index was baked from the currently-loaded map.
    bool Ready();

    // Once per frame from GWToolbox::Draw: lets the queries re-validate the index's map key only once
    // per frame instead of per query.
    void BeginFrame();

    // Frees the published index. Call only after draw callbacks are gone and workers are joined
    // (GWToolbox::Terminate, next to GameWorldCompositor::Terminate).
    void Terminate();

    struct Stats {
        uint32_t props;          // props that contributed triangles
        uint32_t render_props;   // subset with no collision mesh, served from render records
        uint32_t meshes;         // submeshes walked (collision + render)
        uint32_t triangles;      // world-space triangles kept
        uint32_t skipped_props;  // props with no resolvable geometry at all
        uint32_t cells_x, cells_y;
        uint32_t refs;           // triangle-in-cell registrations
        float bake_ms;           // worker-side build time
        float snapshot_ms;       // main-thread copy time
    };
    bool GetStats(Stats* out); // false while no index is published

#if defined(_DEBUG) || defined(GWTB_HARNESS)
    // Harness diagnostic: log one prop's collision-model chain hop by hop ([propmesh] lines).
    void DebugDumpProp(uint32_t prop_index);
#endif
}
