# Fix patterns used in this repo

Concrete shapes taken from merged perf fixes. Prefer reusing these over inventing a new
mechanism; they are already reviewed and tested against the game.

## 1. Explicit D3D state save instead of a full state block

`723b9560 fix performance on idle objectivetimer continuously saving full d3d block`

Before - ~45us per use per frame, and the block is invalidated by a GW device Reset:

```cpp
struct D3DStateGuard {
    IDirect3DStateBlock9* block = nullptr;
    explicit D3DStateGuard(IDirect3DDevice9* dev) { dev->CreateStateBlock(D3DSBT_ALL, &block); }
    ~D3DStateGuard() { if (block) { block->Apply(); block->Release(); } }
};
```

After - `GWToolboxdll/D3DContainers.cpp` saves only the union of states toolbox actually writes
(`GUARDED_RENDER_STATES`, `GUARDED_TEXTURE_STAGE_STATES`, `GUARDED_SAMPLER_STATES`, transforms,
shaders, streams, viewport, scissor), ~0.5us. When a renderer starts setting a new state, extend
those lists - otherwise GW's rendering gets corrupted.

Second half of that fix: renderers took the guard (and set up the pipeline) even when they had
nothing to draw. Early-out before constructing the guard.

## 2. Cache draped geometry, invalidate on anchor movement

`8b0b7304 perf(skill-range-rings): cache draped geometry, re-drape only when anchor moves`

```cpp
if (anchor_pos != cached_anchor || map_id != cached_map_id) {
    RebuildDrapedGeometry();
    cached_anchor = anchor_pos;
    cached_map_id = map_id;
}
DrawCached();
```

For an input that changes every frame (player position), add a threshold and/or a `TIMER_DIFF`
throttle so a walking player does not rebuild continuously.

## 3. Index instead of linear scan

`f918923f perf(in-game rendering): index navmesh point-location, throttle path re-anchor`

`NavMesh::DrapeHeightAt` scanned every ground trapezoid (up to 0x8000) per vertex per frame.
Fix: bucket trapezoids into Y rows (CSR) at build time; a query scans one row plus a small
overflow list for trapezoids too tall to bucket. Validated against the exhaustive scan over 80k
queries before landing - do the same when replacing an exact algorithm with an indexed one.

## 4. Cull off-screen rows in unbounded lists

`61893e1e objectivetimerwindow giga slow drawing`

Split the filter out of `Draw` so it is cheap to ask, then coalesce contiguous off-screen
collapsed rows into a single `Dummy` (keeps the scroll extent correct):

```cpp
const float row_height = ImGui::GetFrameHeight();
const float spacing = ImGui::GetStyle().ItemSpacing.y;
float skipped_height = 0.f;
const auto flush_skipped = [&skipped_height] {
    if (skipped_height > 0.f) {
        ImGui::Dummy(ImVec2(1.f, skipped_height));
        skipped_height = 0.f;
    }
};
for (auto& row : rows) {
    if (row.IsFilteredOut()) continue;
    if (row.IsCollapsedRow()) {
        const float y = ImGui::GetCursorScreenPos().y + (skipped_height > 0.f ? skipped_height + spacing : 0.f);
        if (!ImGui::IsRectVisible({0.f, y}, {1.f, y + row_height})) {
            skipped_height = skipped_height > 0.f ? skipped_height + spacing + row_height : row_height;
            continue;
        }
    }
    flush_skipped();
    row.Draw();
}
flush_skipped();
```

For uniform-height tables, `ImGuiListClipper` is simpler - use it when rows do not vary.

## 5. Cache formatted strings

`ObjectiveTimerWindow::ObjectiveSet::GetDurationStr` keeps a `cached_time` buffer and only
re-formats when the underlying value changes. Same idea for any per-row label, timer or
percentage that is `std::format`-ed inside `Draw`.

## 6. Move the work off the frame thread, or make it lazy

- `Resources::EnqueueWorkerTask([data = std::move(data)] { WriteToDisk(data); })` - the pattern
  the Performance window itself uses to stream CSV without touching disk in the draw loop.
- `91994a5e perf(account-inventory): fetch item icons lazily during draw` - only request the
  resource for rows actually being drawn.

## 7. Cache a whole-map computation, keyed on its real invalidation state

`3d23ad84 perf fix` (Pathing)

`FindReachableTrapezoids` visits every adjacency on the map and tests each against every travel
portal - far too expensive per query. The accessor caches it and rebuilds only when the state
that can change the answer changes:

```cpp
const std::unordered_set<const GW::PathingTrapezoid*>& CachedReachableTrapezoids()
{
    const auto map_id = GW::Map::GetMapID();
    const auto instance_type = GW::Map::GetInstanceType();
    std::vector<uint32_t> blocked;
    CopyBlockedPlanes(blocked);
    if (map_id != reachable_cache_map || instance_type != reachable_cache_instance || blocked != reachable_cache_blocked_planes) {
        auto found = FindReachableTrapezoids();
        // Empty means it could not find the player to start from, which callers read as
        // "assume everything is reachable". Keeping that would pin the assumption for the
        // life of the map; the walk bails immediately in that state, so retrying is free.
        if (found.empty()) return reachable_cache = {};
        reachable_cache = std::move(found);
        reachable_cache_blocked_planes = std::move(blocked);
        reachable_cache_map = map_id;
        reachable_cache_instance = instance_type;
    }
    return reachable_cache;
}
```

Three things to copy: the key is map + instance + gate state (nothing else moves regions in and
out of reach), the accessor returns `const&`, and a **degenerate result is never stored**.

Related failure in the same commit: `EnsureNavCells` rebuilt when `CopyBlockedPlanes` returned
nothing (no pathing context), so the key never matched and a per-map build became a per-frame
full-map sweep. Bail instead of building against missing input.

`e4d3805c` adds the lifetime half: keep the expensive per-map result in a session cache
(`std::map<MapID, MapProbe>`, keyed additionally on the character), and let `ResetState()` clear
only transient overlay state - not the sweep.

## 8. Hoist inner-loop invariants into the per-map cache

`3d23ad84 perf fix` (Pathing) - `CrossesTravelPortal` is called for every adjacency in the
reachability walk, hundreds of thousands of times, and each call recomputed the portal's gate
line from its facing:

```cpp
const float cos_f = cosf(portal.facing_radians);   // per call, per portal
const float sin_f = sinf(portal.facing_radians);
const GW::Vec2f left{...}, right{...};
```

Fix: resolve each portal to a `PortalDoorway { left, right, pos, radius_sq, has_facing }` once
when the per-map portal cache is built, and let the hot loop read the precomputed line.

## 9. Dense grid instead of a map keyed by cell

`3d23ad84 perf fix` (CartographerWidget) - `std::map<std::pair<int,int>, GW::GamePos>` plus
`std::set<std::pair<int,int>>` became a flat grid, because the build touches every trapezoid on
the map and "a tree node plus a log-n descent per touch is the difference between a hitch and
no hitch":

```cpp
struct NavCells {
    int x0 = 0, y0 = 0, width = 0, height = 0;
    std::vector<uint8_t> ground, standable;
    std::vector<GW::GamePos> stand;
    std::vector<float> line_x, line_y;   // grid lines in game coords, one conversion per line
    bool InGrid(int cx, int cy) const;
    size_t Index(int cx, int cy) const { return static_cast<size_t>(cy - y0) * width + (cx - x0); }
};
```

The `line_x`/`line_y` arrays are the same idea one level down: one coordinate conversion per
grid line instead of two per trapezoid touched, and being the same call on the same input they
cannot drift from the anchor.

## 10. Hook the client's own change message instead of polling

`75fbfeee Cartographer: recompute on the client's own carto-updated message`

Traced in Ghidra: the StoC reveal handler writes the tile block and broadcasts UI message
`0x10000090` with no payload. The module hooks that instead of recomputing every second. Because
the message only says "something changed", it diffs a snapshot of the client's bitmap to find
what actually flipped:

```cpp
for (uint32_t i = 0; i < grid.dword_count; i++) {
    const uint32_t changed = carto_snapshot[i] ^ grid.bits[i];
    if (!changed) continue;
    carto_snapshot[i] = grid.bits[i];
    ...  // emit only the tiles whose bit flipped
}
```

If the snapshot size does not match, fall back to a wholesale rebuild - there is no basis for a
diff.

## 11. Refresh the one entity, not the whole instance

`905b79a3 Refresh only the agent whose allegiance changed` - the handler toggled the global name
tag filter off and on, which rebuilds every tag in the instance, for a packet that carried the
agent id. GWCA gained `RefreshAgentNameTag`; the global-toggle signature and its two globals
were deleted. Look for the same shape wherever a single-entity event triggers a global reset.

Related: `13969de7 Game Settings: cache nametag player colors per map instance` - resolving a
name colour walked the friend list and guild roster *on every hover*; now cached in an
`unordered_map` keyed by player name and cleared on map load and party join/leave.

And `26ffb484` - prefer the client's own forced-refresh helper over faking a refresh by
toggling state.

## 12. Move the computation offline

`9bb0a10b Cartographer: drop the runtime bake` - the in-game bake (a per-tick `StepBake` driver,
a per-map trapezoid walk, a largest-component search and a `.bin` writer) was deleted once
`tools/bake_cartography/` produced the same table offline from the same sources into
`CartographyData.h`. If a computation depends only on `Gw.dat`, `AreaInfo` or constants, it does
not need a client and does not belong at runtime.

## 13. Consolidate duplicated helpers, then optimise once

`57c399e3 refactor(in-game rendering): share one pruned multi-plane altitude query` -
`GameWorldRenderer` and `RiverModule` each carried their own `HighestSurfaceZ`/`ClosestSurfaceZ`:
four copies of the same all-planes `QueryAltitude` loop, three different "no data" sentinels, and
none of them pruning planes with no trapezoid near the point (which `TerrainDrape` had been doing
for a while). Collapsed into `TerrainDrape::HighestZ`/`HighestZOnPlanes`/`ClosestZ` over one
`PrunedPlaneZ`. When you find the same hot helper copy-pasted, fix the copies into one first.

## 14. Compute natively instead of querying the client per element

`a96dff52 perf(terrain): compute draped altitude natively instead of per-frame QueryAltitude` -
when a GW API call is the inner loop, see whether the data can be read once and evaluated in
toolbox instead. Ghidra is available for finding the underlying structures.

## Measuring a fix

1. Windows -> Performance, enable `stream_to_csv`.
2. Reproduce the scenario for at least ~30s, note the module's avg/max us.
3. Apply the fix, repeat into a second CSV, diff in the **Compare** tab.
4. CSVs are per-compiler (`performance_log_msvc-*.csv` / `performance_log_clang-*.csv`) in the
   settings folder - compare like with like.
