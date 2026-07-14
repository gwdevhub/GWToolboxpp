#pragma once

#include <cstdint>

// Prop-surface height oracle (tasks/prop-height-oracle-plan.md): every map prop's world-space collision triangles ('mgrg' meshes from MapProp+0x58) baked once per map into a uniform XY grid.
// Main-thread snapshot -> Resources-worker build -> immutable index published by atomic pointer swap; queries are pure math (no game call, no allocation, thousands/frame).
// Queries return false until the bake lands / after a map change (caller falls back to walkable-plane draping); unlike the pathing planes this covers NON-walkable props (railings, fences, statues).
namespace PropSurface {
    // "No covering prop surface" sentinel (up is -z; real surfaces are finite, usually negative).
    inline constexpr float kNoData = 3.402823466e+38f;

    // Master switch (default OFF): while disabled queries no-op and never kick the per-map bake, so overlays drape on terrain + walkable planes only. On = drape onto non-walkable props (railings/fences).
    void SetEnabled(bool on);
    bool Enabled();

    // Highest covering prop surface at (x,y): min z among triangles containing the point, or kNoData; false while no index for the live map is published (caller falls back to QueryAltitude). Main thread only: lazily kicks the bake.
    bool HighestZAt(float x, float y, float* out);

    // Covering prop surface nearest to `ref` (continuity draping for paths). Same contract as above.
    bool ClosestZAt(float x, float y, float ref, float* out);

    // True when the published index was baked from the currently-loaded map.
    bool Ready();

    // Once per frame from GWToolbox::Draw: re-validate the index's map key once per frame instead of per query.
    void BeginFrame();

    // Frees the published index; call only after draw callbacks are gone and workers are joined (GWToolbox::Terminate, next to GameWorldCompositor::Terminate).
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
