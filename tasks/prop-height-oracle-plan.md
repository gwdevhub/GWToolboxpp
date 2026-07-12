# Prop Height Oracle — draping overlays on non-walkable props at 360 fps

## Problem

In-world overlays (skill range rings, danger rings, loot beacons, quest path) currently drape only
on the terrain heightfield and on *walkable* pathing planes (bridges/stairs reachable via
`GW::Map::QueryAltitude`). They cannot get the height of **non-walkable props** — e.g. the railings
between the three stacked stair planes in the Great Temple of Balthasar — so overlays draw *under*
those structures instead of *on* them.

Goal: a height oracle that returns the surface Z for **any** `(x,y)` in the map — terrain, walkable
prop surfaces, and non-walkable props alike — cheaply enough to sample hundreds–thousands of points
per frame while holding **360 fps** (2.78 ms/frame total budget).

## Why the obvious approach is too slow

`GW::Map::QueryAltitude` costs ~21 µs/call: it swaps the global map context, derefs handles, and
raycasts a prop collision mesh every call. 1,000 of those is ~21 ms — alone that caps the game at
~45 fps. It also only works for **pathing planes**, so it can't reach non-walkable props at all.

The fix is to move all mesh/transform/handle work to a **one-time precompute at map load** (off the
render thread) and leave the per-query path doing an O(1) lookup with no game calls and no
allocation.

## Architecture: a two-layer height oracle

**Layer 1 — terrain (already implemented).** `TerrainDrape::NativeTerrainZ(x,y)` is a pure heightfield
read (~50–100 ns). Base surface everywhere.

**Layer 2 — precomputed prop-surface index (new).** Everything above terrain (stairs, platforms,
railings) is props. Bake their surfaces into a spatial index once, then look them up in O(1).

Per-query, combine the two:

```cpp
float HeightAt(float x, float y, float ref_z) {
    float z = TerrainDrape::NativeTerrainZ(x, y);      // base layer, native
    const PropIndex* idx = g_prop_index.load(acquire); // null until bake completes
    if (idx) {
        for (const Tri& t : idx->triangles_in_cell(x, y)) {   // typically 0–4 tris
            if (point_in_tri_xy(x, y, t)) {
                const float cz = plane_z(t, x, y);
                z = pick(z, cz, ref_z);                        // policy: see below
            }
        }
    }
    return z;
}
```

No game call, no context swap, no mesh walk, no allocation.

## Phase A — precompute (worker thread, map load, never blocks main)

Follows the resident-cache / `GetResidentMilepathOrPrewarm` pattern: build off-thread, publish by
atomic pointer swap; degrade to terrain-only until ready.

1. **Enumerate `GetMapContext()->props->propArray`** (~644 props in the DoA outpost; Temple is
   similar). For each prop:
   - Read its collision mesh via `MapProp+0x58` model handle → `ModelData+0x08` → submesh table
     `+0xac`/`+0xb0` → `'mgrg'` mesh (verts `+0x1c`, stride `+0x54`, indices `+0x08`, index count
     `+0x10`). Layout is already reverse-engineered (see
     `reference-gw-terrain-altitude-internals` / the prop-planes RE report).
   - Build the model→world transform from `MapProp.position` (`+0x20`) + rotation (`+0x38`
     angle/cos/sin).
   - Transform each triangle to **world space once**; store 3 world verts (or plane eq + XY-AABB).
   - Optionally drop triangles that can never be a top surface (steep downward normals) to shrink the
     set. Keep near-vertical rail faces — the *top* of the railing is what we drape onto.
2. **Insert into a uniform XY grid** (cell ≈ 48–96 gwinches, same order as the terrain tile). Each
   triangle registers in every cell its XY-AABB overlaps. Cells with no props stay empty → those
   queries fall straight through to terrain.
3. **Publish**: `g_prop_index.store(new, release)`. Rebuild per map load; free the old index after a
   frame boundary.

Cost is paid once, off the render thread — zero per-frame impact.

## Phase B — query policy

Two consumers, two rules, both served by how the query picks among candidate surfaces:

- **"Draw on top" (rings, danger zones):** take the **highest** surface (min z, since up = −z) among
  terrain + covering prop triangles. A ring crossing a railing footprint rides onto the rail top.
- **"Follow the path up the stairs" (quest path):** take the surface **nearest the running reference
  z** (`ref_z` = previous dense sample) — the existing `ClosestSurfaceZ` continuity idea, now fed by
  *all* surfaces. A line climbing the three stacked planes tracks each stair ramp without jumping to
  the plane above or sinking to the ground below.

The stairs are likely walkable pathing planes already, but folding them into the same triangle oracle
gives one uniform query for stairs + railings and removes the `QueryAltitude` dependency entirely.

## Performance budget (360 fps = 2.78 ms/frame)

Per query: 1 native terrain read + 1 grid lookup + ≤~4 point-in-triangle tests ≈ **150–300 ns**.

| Workload                              | queries/frame | cost        |
|---------------------------------------|--------------:|-------------|
| Large skill ring (rebuilt each frame) |         ~1,000 | ~0.2 ms     |
| Several overlays at once              |         ~3,000 | ~0.6–0.9 ms |
| Stress                                |         ~5,000 | ~1.0–1.5 ms |

All inside 2.78 ms with headroom for actual rendering. (Old `QueryAltitude` path: ~21 µs/query →
1,000 = ~21 ms; ~100× worse and can't reach non-walkable props.)

**Memory:** only prop triangles. ~10k–100k tris × ~40 bytes ≈ 0.5–4 MB plus the grid — negligible.

## Mesh source — recommendation and fallback

- **Collision mesh (`'mgrg'`) first.** Low-poly, already decoded, and is the surface the game itself
  treats as "height." Down-raycast → first hit = top of the railing's blocker volume = "on the
  railing, not under it."
- **Fallbacks** if collision proves inadequate for a specific prop (e.g. a railing whose collision is
  a thin vertical plane with no usable top): the render mesh (via FrCache, already touched by the
  compositor) or the DAT model via `model_file_id` (toolbox already parses FFNA models). Only reach
  for these if verification on the Temple railings shows gaps.

## Verification plan

1. **Reachability (P0, ~10 min):** guarded probe that walks one elevated non-walkable Temple prop's
   `+0x58` → mesh and dumps world-space triangle count + z-range. This is the one hard technical
   unknown; everything else is mechanical.
2. **Correctness:** on walkable planes, compare `HeightAt` vs `QueryAltitude` over N random points
   (reuse the `drapeverify` harness) — should match within interpolation noise. On railings, compare
   against the prop's known top.
3. **Visual:** stand on the Temple stairs, enable a draped overlay, confirm the line rides all three
   planes and sits *on* the railings. (Needs GW focused for a real fps read.)
4. **Perf:** `drapebench`-style A/B and a foreground fps read with a large ring.

## Risks / edge cases

- **Railing collision may be a vertical blocker without a clean top** → raycast-down still hits its
  top edge, but verify; render/DAT mesh is the fallback.
- **Moving props** (doors, lifts): mostly static; for movers, re-transform that prop's cached *local*
  triangles per frame (cheap, one prop) or exclude them.
- **Map teardown / reload:** oracle is per-map; atomic swap + deferred free avoids torn reads; null
  index → terrain-only fallback, never a crash.
- **Selection policy:** `propArray` is *all* geometry (walls, statues). "Highest surface" alone would
  let a ring climb a wall. Mitigate via the continuity rule (rejects far-off surfaces) and/or tag only
  near-horizontal top triangles as drape-eligible. Decide explicitly per overlay.

## Phasing / milestones

1. **P0** — prove collision-mesh reachability on a Temple railing prop (guarded probe).
2. **P1** — worker-thread bake of the world-space triangle grid for the current map; harness dump of
   its size/extent.
3. **P2** — `HeightAt` query + wire it behind `SurfaceZ`/`DrapeZ` as the prop layer; validate with
   `drapeverify` on walkable planes.
4. **P3** — continuity policy for the 3-plane stairs; visual check on the Temple.
5. **P4** — perf tune (cell size, triangle culling) + foreground fps confirmation.

**Gating unknown:** P0 — whether a non-walkable prop's collision mesh is readable from live memory.
The layout is decoded; validating it is the first concrete step.

## Related

- `reference-gw-terrain-altitude-internals` (memory) — decoded terrain heightfield + prop raycast +
  point-location.
- `project-native-terrain-altitude` (memory) — Layer 1 (native terrain) + trapezoid-pruned
  `SurfaceZ`, PR #2414.
- Harness verbs already built for this line of work: `nativez`, `drapebench`, `drapeverify`,
  `fpsprobe`, `propprobe`.
