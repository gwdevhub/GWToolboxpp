---
name: performance-audit
description: Audit GWToolbox++ for in-game performance problems - FPS drops, stutter, slow modules/widgets, hot per-frame code, and Debug-build container overhead (we play in Debug, so iterator debugging cost counts). Use when asked to "audit performance", "find what's tanking FPS", "why is toolbox slow in debug", "profile a module", or before a release to sweep for regressions. Covers the frame cost model, the built-in Performance window, a per-class checklist with grep recipes, and the fix patterns already used in this repo.
---

# GWToolbox++ performance audit

Toolbox runs inside the Guild Wars client's frame. Every microsecond a module spends is a
microsecond the game does not get, and the costs are additive across ~80 modules, so
"only 200us" in five widgets is a visible framerate loss. This skill finds that work.

## 1. Know the cost model before reading any code

Two hot entry points per module (`GWToolboxdll/ToolboxModule.h`):

- `Update(float delta)` - **game thread**, once per frame, for *every loaded module* whether or
  not its window is visible. Cheapest place to be wrong and the most commonly abused.
- `Draw(IDirect3DDevice9*)` - **render thread**, inside the ImGui frame, for every *enabled*
  module. UI elements draw their window here; plain modules paint overlays here.

Plus: GWCA hooks/callbacks (`UIMgr` message callbacks, packet callbacks, `RenderHook`),
and direct D3D work in the world renderers (`Widgets/Minimap/*`, `Modules/*Module.cpp` overlays).

Three multipliers to keep in mind:

- **Per frame x per module x per element.** A loop over agents/items/rows inside `Draw` is
  O(frame x N). ImGui submits real layout work per item even for off-screen rows.
- **Debug builds are a first-class target, not an afterthought.** The team *plays* in Debug so
  crashes are caught at runtime, so Debug framerate matters as much as Release. MSVC iterator
  debugging and non-inlined `std::` wrappers make container access 10-50x more expensive there
  - see class F, which is the one class where a Debug-only measurement is enough to justify a
  fix.
- **D3D state.** Anything touching the device between GW's own draws must save/restore state.
  Doing that the naive way (`CreateStateBlock(D3DSBT_ALL)`) costs ~45us *per use per frame*;
  the explicit `D3DStateGuard` in `GWToolboxdll/D3DContainers.cpp` costs ~0.5us.

## 2. Measure first

Toolbox has a built-in profiler: **Windows -> Performance** (`Windows/PerformanceWindow.cpp`).
It reports min/avg/max microseconds over a rolling 5s window for:

- frame period, total toolbox `Update`, total toolbox `Draw`, D3D `Present`
- per-module `update` and `draw`
- per-module UI message callback cost, keyed by message id

Settings: `slow_threshold_us` (highlight threshold) and `stream_to_csv`, which appends 1s
snapshots to `performance_log_<compiler>.csv` in the settings folder. The window's **Compare**
tab loads two CSVs and diffs them - that is the before/after harness for any fix.

Ask for (or capture) a CSV from the scenario that hurts: idle in an outpost, busy explorable
with a full party, and the specific window/widget open. Anything over ~100us avg is worth a
look; over ~1000us is a bug.

If no numbers are available, say so and treat the static findings below as *candidates*, not
confirmed regressions.

## 3. Static audit checklist

Work through these classes. For each hit, judge it against: *does this run every frame? does
it scale with agents/items/rows? does it allocate? does it touch the device, disk or network?*

### A. D3D state and device churn
- `CreateStateBlock`, `GetRenderState`/`SetRenderState` loops, `GetTransform`, `GetTexture`
  called outside an actual draw.
- Renderers that construct a `D3DStateGuard` (or set up the pipeline) even when they end up
  drawing nothing - guard the early-out *first*, then take the state.
- Vertex buffer re-creation per frame instead of a persistent buffer + `Lock(D3DLOCK_DISCARD)`.
- New states written by a renderer but missing from the guarded lists in `D3DContainers.cpp`
  (a correctness bug, not perf - flag it).

```
grep -rn "CreateStateBlock\|SetRenderState\|CreateVertexBuffer" GWToolboxdll
grep -rn "D3DStateGuard" GWToolboxdll
```

### B. Unbudgeted per-frame recomputation of geometry / pathing
The repeat offender in this codebase. Terrain draping, navmesh queries and path re-anchoring
are O(map) and were each an FPS collapse until cached:

- `perf(skill-range-rings)`: re-draped ~1000 vertices x n planes every frame -> cache geometry,
  re-drape only when the anchor moves.
- `perf(in-game rendering)`: `NavMesh::DrapeHeightAt` linear-scanned up to 0x8000 trapezoids per
  vertex per frame -> CSR row buckets + throttled re-anchor.

Look for `QueryAltitude`, `DrapeHeightAt`, `SurfaceZ`, pathfinding calls, and any geometry
rebuild inside `Draw`. The fix pattern is always: cache the result, key it on what actually
changed (anchor position, map id, party, skill), and throttle with `TIMER_DIFF` when the input
changes continuously.

Also check the *inside* of these loops for invariants that could be hoisted. The reachability
walk tested every adjacency against every travel portal and recomputed `sinf`/`cosf` from the
portal's facing on each one; precomputing the gate line once per map (`3d23ad84`) removed
hundreds of thousands of trig calls. Anything derived only from map-static data belongs in the
per-map cache, not the inner loop.

```
grep -rn "QueryAltitude\|DrapeHeightAt\|SurfaceZ" GWToolboxdll
grep -rn "Pathing\|BuildPath\|GeneratePath" GWToolboxdll/Windows/Pathfinding GWToolboxdll/Widgets
```

### C. Per-frame scans and unbounded ImGui lists
- `GW::Agents::GetAgentArray()` / item array / party array walked inside `Draw` or `Update`
  when a cached, event-invalidated view would do.
- Nested loops over agents x effects, agents x skills, items x filters.
- Lists that grow without bound (past runs, chat log, drop history, completion tables) drawn
  row-per-item. Even a collapsed `CollapsingHeader` costs a label measure plus item layout.
  The established fix (`ObjectiveTimerWindow`) is: filter first, then for off-screen rows
  coalesce contiguous runs into a single `ImGui::Dummy` so the scroll extent stays correct.
- `ImGui::BeginTable` over thousands of rows without `ImGuiListClipper`.

```
grep -rn "GetAgentArray()\|GetPartyInfo()\|GetItemArray" GWToolboxdll
grep -rn "IsRectVisible\|ImGuiListClipper" GWToolboxdll   # who already culls - and who doesn't
```

### D. Allocation and string churn per frame
- `std::format` / `std::string` concatenation / `snprintf` into a fresh string, per row, per
  frame. Cache the formatted text and only rebuild when the value changes (see
  `ObjectiveSet::GetDurationStr`'s `cached_time`).
- `std::vector` built and destroyed each frame - hoist to a member and `clear()` + `reserve()`,
  or make it `static` at namespace scope in the module's anonymous namespace.
- Returning containers by value from helpers called in a loop.
- `std::to_string`/`std::stringstream` anywhere in a draw path.

```
grep -rn "std::format\|std::to_string\|stringstream" GWToolboxdll/Widgets GWToolboxdll/Windows
grep -rnE "std::vector<[^>]+> [a-z_]+;" GWToolboxdll --include=*.cpp   # locals in hot functions
```

### E. Containers and lookups
- `std::map` / `std::unordered_map` keyed by `std::string` looked up every frame - each lookup
  hashes or compares a string, and often *constructs* one from a `const char*`. Prefer an id,
  an enum, a `string_view` key, or resolving the iterator once and holding it.
- `std::map` where a flat `std::vector` + index would be both smaller and cache-friendly. The
  codebase has ~150 `std::map` declarations; the ones that matter are those touched per frame.
- Repeated `.find()` on the same key in one function - do it once.
- `operator[]` on a map in a read path (it inserts, and grows the map forever).
- **Spatial data in a `std::map<std::pair<int,int>,...>` / `std::set<std::pair<int,int>>`.** One
  node allocation plus a log-n descent per touch, over a build that touches every trapezoid on
  the map, is the difference between a hitch and no hitch. The Cartographer's `NavCells`
  (`3d23ad84`) moved to a dense `std::vector<uint8_t>` grid with an `x0/y0/width/height` origin
  and an `Index(cx, cy)` helper. Use that shape whenever the key space is a bounded grid.
- Returning a container by value from a cache accessor - hand back a `const&`.

```
grep -rn "std::map<std::string\|unordered_map<std::string" GWToolboxdll
grep -rn "std::map<std::pair\|std::set<std::pair" GWToolboxdll
```

### F. Debug-build container overhead

**We play in Debug.** Running the Debug build in-game is how crashes get caught, so a module
that is fine in Release and unusable in Debug is a real bug to this project, not a build
artifact. Treat a Debug-only regression as actionable on its own.

`CMAKE_MSVC_RUNTIME_LIBRARY` is `MultiThreadedDebug` for the Debug config, which means MSVC's
default `_ITERATOR_DEBUG_LEVEL=2`: every iterator carries a back-pointer to its container and is
registered in a linked list, every `++`/`*`/comparison validates against the owner, every
container mutation walks and invalidates that list, and none of the `std::` wrappers inline. A
`std::map` lookup that is a handful of compares in Release becomes a checked tree walk with
debug-heap allocation on every node insert.

Do **not** try to fix this by flipping `_ITERATOR_DEBUG_LEVEL` project-wide: the level is part
of the ABI, `Dependencies/GWCA/lib/gwca.lib` ships as one prebuilt binary used by both configs,
and the vcpkg deps are built to their triplet's default. MSVC's `detect_mismatch` linker check
exists precisely because mixing levels is an ODR violation. Fix the hot code instead.

**`std::vector` - take the buffer once.** In a hot loop, hoist `.data()` and `.size()` and index
the raw pointer (or wrap it in a `std::span`), so the loop pays no per-element owner check:

```cpp
const auto* items = v.data();
const size_t count = v.size();
for (size_t i = 0; i < count; i++) { ... items[i] ... }
```

Only "when safe to do so": the container must not be resized or reallocated inside the loop
(that dangles the pointer - which is exactly what iterator debugging would have caught for you),
indices must come from `size()`, and `data()` may be null when empty, so early-out on `empty()`.
Use `.data()`, never `&v[0]`, which asserts on an empty vector. For anything not measurably hot,
leave the range-for - the checks are worth having.

**`std::map` / `std::unordered_map` - look up once, hold the result.** Every lookup in Debug is
a checked walk, and the usual sin is doing several against the same key:

```cpp
const auto it = m.find(key);        // once
if (it == m.end()) return;
auto& entry = it->second;           // then work through the reference
```

- Never `m[key]` in a read path - it default-constructs and inserts, growing the map forever and
  allocating a node through the debug heap.
- Iterate once with `for (auto& [k, v] : m)` rather than looping keys and looking each one up.
- Better still, get out of the node-based container: for a bounded integer/enum key use a flat
  `std::vector`/`std::array` indexed directly, which is both faster in Release and free of the
  per-node debug bookkeeping (see the `NavCells` grid in class E). For a small set, a sorted
  `std::vector` plus `std::ranges::lower_bound`, or even a linear scan, beats a `std::map`.
- Cache the resolved pointer/reference across frames when the key is stable (module state, the
  player's agent, the current map's entry) instead of re-looking it up every frame.

**Everything else in a hot loop:**
- `at()` anywhere in a loop - bounds-checked in both configs, and non-inlined in Debug.
- Passing containers by value, range-for over a temporary, returning containers by value from a
  helper called per element. Pass `const&` or `std::span`.
- `push_back` in a loop without `reserve()` - each reallocation is a debug-heap alloc plus a
  full iterator-invalidation sweep.
- Deep helper chains that only inline in Release (tiny getters inside per-vertex loops).
- `std::function` called per element instead of a template parameter or a plain lambda.
- `std::string` temporaries (`substr`, concatenation, `c_str()` round-trips) - prefer
  `std::string_view` over the existing buffer.

Sanity check the direction of a "fix": if raw-pointer access makes a loop dramatically faster in
Debug but changes nothing in Release, the win is real but it is *also* telling you the loop runs
far more often than it should - check class B/C/J before settling for the micro-optimisation.

```
grep -rn "\.at(" GWToolboxdll
grep -rn "\[[a-z_]*\] =\|\[key\]\|\[name\]" GWToolboxdll --include=*.cpp   # map operator[] in read paths
grep -rn "push_back" GWToolboxdll | grep -v reserve   # then check the enclosing loop
grep -rn "std::function<" GWToolboxdll
```

### G. Blocking work on the frame threads
- File IO (`std::ifstream`/`std::ofstream`/`CreateFile`), `Resources::` loads, texture decode,
  network calls, `std::mutex` contention, JSON parse - none of these belong in `Update`/`Draw`.
  Push them through `Resources::EnqueueWorkerTask` (47 call sites already do) or make them lazy
  (`perf(account-inventory): fetch item icons lazily during draw`).
- Settings saved on change per frame instead of debounced.

```
grep -rn "ifstream\|ofstream\|CreateFileW\|curl_easy_perform" GWToolboxdll/Widgets GWToolboxdll/Windows
```

### H. Polling where an event exists, and recomputing where a diff would do
- `Update()` that re-reads game state every frame (or on a 1s timer) to detect a change usually
  has a GWCA callback available (`UIMgr` messages, `StoC` packet callbacks, `Map` state). Polling
  costs a frame's work 80 times over; a callback costs nothing while idle. If no callback looks
  right, the client almost certainly broadcasts one - use Ghidra to find it, as `75fbfeee` did
  (traced the reveal packet handler to UI message `0x10000090` and replaced a per-second
  recompute with a hook on it).
- When the event says only "something changed", **diff instead of rebuilding**: keep a snapshot,
  XOR against the live data, and recompute only the entries that flipped.
- **Refresh the one thing, not everything.** `905b79a3`: the allegiance handler flashed the
  global name-tag filter off and on, rebuilding every tag in the instance, when the packet
  carried the agent id and a per-agent refresh existed. Look for global invalidations
  (`clear()`, "reset all", visibility toggles) triggered by a single-entity event.
- Conversely, check that registered UI message callbacks are cheap - the Performance window
  breaks these out per message id for a reason.

### I. Work done while idle, hidden or irrelevant
- `Update()` doing full work when the module's window is not `visible`, the feature is disabled
  in settings, or the player is in an outpost / loading screen / not in a relevant map.
- Overlay renderers running with zero elements to draw.
- Timers ticking and strings rebuilding for a hidden widget.
  Early-out at the top: `if (!visible) return;`, `if (!enabled) return;`,
  `if (!GW::Map::GetIsMapLoaded()) return;`.
- **Gate every entry point, not just `Update`.** `e4d3805c` had the world-map relevance check on
  `Update` but not on the overlay draw or the context menus, so those kept working (and drawing
  stale state) on maps the check was meant to exclude.

### J. Cache design and invalidation
Most fixes above are caches, and most cache bugs here are *lifetime* bugs, not miss-rate bugs:

- **Keyed on the wrong thing.** The key must be exactly the state that changes the answer -
  `map_id`, `InstanceType`, blocked planes, character name (`e4d3805c` keys the Cartographer
  sweep on the character, because which tiles matter depends on that character's fog). Too
  narrow and it serves stale data; too broad and it never hits.
- **Thrown away by a transient reset.** `ResetState()` clearing a session-valid per-map sweep on
  every map change and every enable/disable toggle meant returning to a map paid the full cost
  again. Split "transient overlay state" from "expensive, still-valid computation".
- **Caching a degenerate result.** If the computation can return "I could not tell" (no player
  yet, no pathing context), do *not* store it - that pins the wrong assumption for the life of
  the map. Both `3d23ad84` and `e4d3805c` carry explicit comments about exactly this trap.
- **Rebuilt against an empty input.** `CopyBlockedPlanes` returning nothing when there is no
  pathing context made the key compare unequal every call, turning a per-map build into a
  per-frame full-map sweep. Bail out rather than building against missing data.
- A cache accessor should return `const&`, and callers should not copy it.

### K. Duplicated and redundant work
- **The same expensive helper reimplemented in several modules.** `57c399e3` found four copies
  of an all-planes `QueryAltitude` loop across `GameWorldRenderer` and `RiverModule`, none of
  them pruning planes that had no geometry near the point, with three different "no data"
  sentinels. Consolidating into `TerrainDrape` fixed the cost in all four at once.
- **The same game function hooked twice.** `e07d1583`: `DialogModule` and `TextToSpeechModule`
  both hooked the NPC dialog frame callback; GWCA resolves through the JMP at the entry, so the
  second hook chained onto the first (and crashed the client). Two detours is also two costs -
  `grep -rn "CreateHook\|RenderHook" GWToolboxdll` and look for the same target twice.
- **Runtime computation of data that could ship baked.** `9bb0a10b` deleted the Cartographer's
  in-game bake - a per-tick `StepBake` driver doing a per-map trapezoid walk - because
  `tools/bake_cartography` produces the same table offline into `CartographyData.h`. If a
  computation only depends on `Gw.dat` / `AreaInfo` / constants, it does not belong at runtime.

### L. Leaks, unbounded growth and stuck state
Not per-frame cost, but they show up as "toolbox gets slower the longer I play":

- Resource leaks per event - `047cde98` leaked a GDI bitmap on *every cursor change*. Check
  `CreateBitmap`/`CreateDIBSection`/`LoadImage`/`CreateTexture` for a matching delete on every
  path, including early returns.
- Containers that only ever grow: histories, per-agent maps never cleared on map change, caches
  with no eviction and no key on instance.
- State machines that leave a "busy" flag set on a failure path. `fde6b8cf`: `RecalculateMap`
  bailed after `Recalculate` had already set `calculating`, so `IsCalculating()` blocked every
  `Update` for 5 seconds. A stuck flag can mask *or* cause a perf complaint.
- The inverse, worth checking when a perf symptom makes no sense: a fast path that is never
  taken. `1e4350da` was a one-character fix - `hook_attempted` initialised to `true`, so
  `EnsureHook()` always early-returned and the compositor hook was never installed. Confirm the
  code you are about to optimise actually runs.

## 4. Verify before claiming a fix

1. Re-run the same scenario with `stream_to_csv` on, before and after, and diff in the Compare
   tab. Quote the module's avg/max us both ways.
2. Confirm behaviour is unchanged - especially for D3D changes (state must be fully restored;
   GW's own rendering corrupting is worse than the frame cost) and for list culling (scroll
   extent, filtering, click targets).
3. **Measure Debug and Release separately, and report both.** The CSVs are already written per
   compiler (`performance_log_msvc-*.csv` / `performance_log_clang-*.csv`); keep the configs
   apart too. A fix that only helps Debug still counts - we play in Debug - but say so rather
   than implying a Release win that is not there.
4. For a `.data()`/raw-pointer change, re-check the safety conditions by hand: nothing resizes
   the container inside the loop, indices are bounded by `size()`, and the empty case is handled.
   You have removed the checking that would have caught a mistake there.

## 5. Report format

Produce a ranked table, worst first:

| # | Location (`file:line`) | Class (A-L) | What runs per frame | Est. cost / evidence | Fix |
|---|---|---|---|---|---|

Then, for the top items, a short paragraph each: why it is hot, the cheapest correct fix, and
whether it needs a measurement to confirm. Separate **confirmed** (backed by a Performance
window number) from **suspected** (static reading only). Do not pad the list with micro-opts
that no measurement supports - say plainly when a candidate is probably harmless.

## 6. High-risk areas in this codebase

Start here when the audit is unscoped:

- `Widgets/Minimap/*` - `AgentRenderer`, `GameWorldRenderer`, `PingsLinesRenderer`, `PmapRenderer`,
  `CustomRenderer`: per-agent, per-vertex, per-frame D3D work.
- `Modules/*Module.cpp` world overlays - `DangerRings`, `SkillRangeRings`, `LootBeacons`,
  `River`, `Weather`: all take the device and drape geometry.
- `Utils/GameWorldCompositor.cpp` and `Utils/TerrainDrape.*` - shared by every world overlay, so
  a cost here is paid several times over.
- `Windows/Pathfinding/*`, `Modules/CartographerModule.cpp`, `Widgets/CartographerWidget.cpp` -
  map-sized data structures, whole-map graph walks, session-lifetime caches.
- `Modules/GameSettings.cpp` - hooks a lot of client UI and runs on hover/name-tag paths.
- Unbounded-history windows - `ObjectiveTimerWindow`, `DropTrackerWindow`, `CompletionWindow`,
  `AccountInventoryWindow`, `FriendListWindow`.
- Per-agent widgets - `EnemyWindow`, `InfoWindow`, `PartyWindowModule`, `BondsWidget`,
  `EffectsMonitorWidget`, `SkillMonitorWidget`.
- Anything added recently: `git log --oneline -30 origin/dev` and review new `Draw`/`Update`
  bodies, which is where regressions actually enter.

See `references/fix-patterns.md` for the concrete before/after shapes used in past fixes.

## Conventions

Follow `AGENTS.md`: no comments that restate the code. A perf fix that is non-obvious (why a
cache is keyed the way it is, why the explicit state list exists) gets 1-2 lines explaining
*why*, like the header comment on `D3DStateGuard`.
