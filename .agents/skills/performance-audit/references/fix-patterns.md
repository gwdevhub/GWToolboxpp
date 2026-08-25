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

## 7. Compute natively instead of querying the client per element

`a96dff52 perf(terrain): compute draped altitude natively instead of per-frame QueryAltitude` -
when a GW API call is the inner loop, see whether the data can be read once and evaluated in
toolbox instead. Ghidra is available for finding the underlying structures.

## Measuring a fix

1. Windows -> Performance, enable `stream_to_csv`.
2. Reproduce the scenario for at least ~30s, note the module's avg/max us.
3. Apply the fix, repeat into a second CSV, diff in the **Compare** tab.
4. CSVs are per-compiler (`performance_log_msvc-*.csv` / `performance_log_clang-*.csv`) in the
   settings folder - compare like with like.
