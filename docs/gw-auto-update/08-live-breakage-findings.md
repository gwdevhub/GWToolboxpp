# 08 — Live breakage findings: 8.29/8.30 vs client build 389033

Investigation of the breakages the user reported (8.29 crash-on-inject; Xunlai-chest
hotkey crash; hero bars not showing) against the **live client build 389033**, using
ReVa (local Ghidra MCP) to diff the RE'd baseline `Gw.388118.exe` against the imported
live `Gw.exe` (389033), plus the `tools/gw-update/` scanners. GWCA in play: 4.7.0.1
(bundled identically in 8.29 and 8.30).

## Method
- Imported the live `Gw.exe` (389033) into Ghidra, analyzed it (local, not checked in).
- 388118 = the reverse-engineered baseline GWCA's structs/scans are derived from.
- Verified specific offsets/IDs by decompiling 389033 directly and by scanning its
  `.text` for `push <imm>` byte signatures with `tools/gw-update/verify_pattern.py`.
- VT correlation was attempted but is impractically slow here (symbol-name correlator
  over 183k symbols; even body-only correlation stalls on the function-reference stage).
  Direct decompilation of 389033 (which stays responsive) was the productive path.

## Confirmed findings

### 1. 8.29 crash-on-injection — RESOLVED (packaging, already fixed)
Not a client-offset break. The Jun-29 GWCA commits **"GWCA static lib" (7cd9fe9)** +
**"Publish static deps" (790c7f4)** shipped in 8.29 packaged GWCA wrong for the toolbox's
delay-loaded `gwca.dll`, crashing on inject. **Reverted Jun 30** (f4e0e11, df34712), so
8.30 is fine. Matches "8.29 crashed on inject, 8.30 doesn't." No action needed.

### 2. The Frame subsystem is UNCHANGED in 389033 (strong evidence)
GWCA's `GW::UI::Frame` layout is **correct** for 389033:
- **`sizeof(Frame) == 0x1c8`** — the game's frame destructor frees with size `0x1c8`
  (`FUN_005ae1ab(frame, 0x1c8)` at 389033 `0x0062f430`). GWCA `static_assert` matches.
- `frame_id @ 0xbc` — 389033 reads `*(frame + 0xbc)` as the id. Matches.
- dirty flag `@ 0xc0 & 0x100` — 389033 redraw-trigger callers test `*(frame+0xc0) & 0x100`.
- redraw list node `@ 0xd0` — game redraw-enqueue (`0x0064ea20`) links `frame+0xd0`; GWCA
  passes `&frame->field44_0xd0`. Matches.
- ⇒ `position @ 0xd8` (contiguous after the node) and `FramePosition` size `0x44` are
  correct; `frame_state @ 0x18c` unchanged (struct total size unchanged).

### 3. The UIMessage enum is NOT shifted
The `0x10000000`-block message IDs appear as raw `push <id>` immediates in the game.
Confirmed present in 389033 (via `.text` scan): `kShowXunlaiChest 0x10000040` (×2),
`kFloatingWindowMoved 0x10000084`, `kShowHeroPanel 0x100001a4` (×3), and GWCA's
`SendUIMessage` anchor `0x100000de` (×2). All GWCA raw scans also resolve (static survey
green). So no message-ID insertion/shift affecting these.

## Conclusion — reorients the two live bugs
Because the **Frame struct and UIMessage IDs are unchanged**, the hero-bars and
Xunlai-hotkey symptoms are **not** client struct/enum breakages introduced by
388118→389033. They are one of:
- **Hero bars:** a **toolbox-side feature bug, deliberately disabled by the devs** —
  commit `8f4ee20e` *"@3vcloud temporarily disable hero panel position frame stuff,
  causes UI draw bugs and presumably crashes."* Module history (`fb4d0b21` add →
  `42a90888` "retry restore in Update for up to 200ms" → `936acc94` "simplified, sidestep
  gw bug" → disabled) shows a long-running fight with UI timing, **not** a 388118→389033
  regression. The `frame->position = saved` write is at the correct offset (`0xd8`), so
  the draw bugs come from the write itself, most likely: **it copies the WHOLE
  `FramePosition` (17 dwords incl. game-derived `content_*`/`screen_*`/`viewport_*`/
  `scale_factor`)**, whereas GWCA's own `GW::UI::SetFramePosition` deliberately writes
  **only** the logical rect (`flags/left/right/bottom/top`) and lets the game recompute
  the derived fields on redraw. Restoring stale derived/screen-space values (e.g. after a
  resolution/UI-scale change, or before the frame is laid out) misrenders; a mistimed
  `TriggerFrameRedraw` on a frame mid-creation is the likely crash.
  **Candidate fix (toolbox-side, needs visual verification — I can't confirm rendering
  headlessly):** replace the full-struct copy with `GW::UI::SetFramePosition(frame,
  saved)` (logical-only) and keep the deferred/retry restore. This is a feature re-enable
  that a human must eye-test; do NOT touch GWCA offsets.
- **Xunlai crash ("sometimes"):** not a Frame/enum shift. Candidates: the `Item`/`Bag`
  or `Player` struct (`CanAccessXunlaiChest` reads `Player+0x34 reforged_or_dhuums_flags`),
  or an intermittent state in the `/chest` → `kShowXunlaiChest` hook path. The recent 8.30
  crash dumps are mostly **NVIDIA D3D driver crashes** (`nvd3dum.dll`) and **C++ stream
  exceptions** (`_Xfiopen`/`num_put`), not this path — so a targeted repro dump is needed.

## Recommended next steps
1. **Runtime repro with the Debug build** (builds clean; `bin/Debug/GWToolboxdll.dll`):
   add a harness verb to (a) dump a live frame's fields by label (AgentCommander0..6,
   InvAccount) to confirm layout at runtime, and (b) trigger `/chest` to capture a Xunlai
   crash dump — then symbolize with the current Debug PDB (clean symbols).
2. **Check other game structs for real client shifts** (Agent/Item/Party/Skill) the same
   way the Frame size was confirmed (sized alloc/free constants + field-access offsets).
3. Treat the hero-panel restore as a toolbox logic bug, not a GWCA offset fix.

## Artifacts
- Live 389033 imported to the Ghidra project as `/Gw.exe` (local only, not checked in).
- `tools/gw-update/verify_pattern.py` used for `push`-immediate presence checks.
- Crash-dump triage: `tools/gw-update/symbolize_dump.py` (8.29 = bad-ptr/driver;
  8.30 Jul-1 = C++ exceptions in stream/file code; 8.30 Jul-2 = nvd3dum driver crash).
