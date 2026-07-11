# 02 — Detection & the feedback loop

Two distinct signals: **(A)** "did the client update?" (the trigger) and **(B)** "which
anchors are broken right now?" (the per-round survey that drives re-derivation). Both are
mechanical and need no human observation.

## A. Update detection (the trigger)

The PE version resource of `Gw.exe` is static (`1,0,0,1` — ArenaNet never bumps it), so
we detect updates by **build id** and/or **content hash**.

### A.1 Build id — port of GWLauncher's `GetFileId()`

`gwlauncher\GW Launcher\Guildwars\GuildWarsExecutableParser.cs` already extracts the
client build number without running the game:

1. PE-parse `Gw.exe`, locate the `.rdata` section.
2. Pattern-scan `.rdata` for the assertion string **`index < arrsize(s_eula)`**.
3. Find the `MOV ecx, <addr>; CALL` that references that string.
4. Read the adjacent `MOV eax, imm32; RET` — `imm32` is the build/file id
   (e.g. `388118`, matching the shared Ghidra repo program `Gw.388118.exe`).

Reimplement this as `tools/gw-update/detect-build.ps1` (or `.py`) so the pipeline can
read the build id of any `Gw.exe` on disk. This is GWCA-independent (works even when
every GWCA scan is broken), which is exactly what we need at the start of a fix cycle.

### A.2 Content hash + state file

Also record `SHA-256(Gw.exe)`. The live baseline captured during recon:

```
Gw.exe  C:\Program Files (x86)\Guild Wars\Gw.exe
        SHA-256 8B52264F3A848BCB8BC0F38BE35C3663EA377059B9BB31FA6EFD8170C3B1B203
        size 10,479,296 bytes   mtime 2026-07-01 10:59:26
```

State file `tools/gw-update/state.json` holds `{ last_build, last_sha, last_fixed_commit }`.
An update = live build id ≠ `last_build` (or sha mismatch). GWLauncher's
`AutoUpdate`/`CheckForUpdates` (in `Settings.json`) is what pulls the new exe; our
detector polls the on-disk `Gw.exe` and does not itself talk to ArenaNet.

### A.3 (Secondary) runtime accessor

Once a working GWCA is loaded, `GW::MemoryMgr::GetGWVersion()`
(`GWCA/Source/MemoryMgr.cpp:205`) returns the live build. Useful for a sanity cross-check
in-game, but it depends on its own scan surviving, so it is **not** the primary trigger.

## B. Breakage survey (the per-round feedback)

### B.1 The `[SCAN] name = %p` channel

Each GWCA/toolbox module `Init()` resolves its addresses then logs every one:

```cpp
GWCA_INFO("[SCAN] AgentArrayPtr = %p", AgentArrayPtr);   // 120 such lines in GWCA
```

A broken scan leaves the variable null → the log line reads `= 00000000`. So the survey
is: **grep `log.txt` for `[SCAN] ... = 00000000`** → the set of broken anchors, each with
a name that maps straight back to its owning file (via the anchor index, below).

`tools/gw-update/scan-survey.ps1`:
- Input: `log.txt` (post-injection).
- Output: `survey.json` = `[{ anchor, value, file, line, kind, target_hint }]` for every
  `[SCAN]` line, flagged `broken` when the value is 0 / non-canonical.
- Also flags **missing** lines: an anchor present in the index but with no `[SCAN]`
  line at all (module `Init()` bailed early — often a cascade from an earlier failure).

### B.2 Build the survey against an **assert-free** GWCA

Critical design point. GWCA guards each scan with `#ifdef _DEBUG GWCA_ASSERT(x)`
(188 asserts). In a **Debug** GWCA the *first* broken scan aborts `GW::Initialize()`, so
you only ever see one failure per run — useless for a survey.

Therefore the survey uses a **non-`_DEBUG` GWCA** (RelWithDebInfo or MinSizeRel — the
`create_release`/`releases/latest` DLL is MinSizeRel, already assert-free). It logs all
`[SCAN]` lines and leaves nulls in place instead of aborting, so **one injection surveys
every broken anchor at once.** Confirm `GWCA_INFO` is not compiled out of that config
(register a GWCA `LogHandler`, `GWCA/Include/GWCA/Utilities/Debug.h:40`, from the toolbox
so the lines reach `log.txt`); if release strips `GWCA_INFO`, add a dedicated
`GWCA_SCAN_REPORT`-style always-on emit or build a `RelWithDebInfo` diagnostic GWCA for
survey rounds. (Action item, verify during toolkit bootstrap.)

### B.3 The toolbox side must log too

`log.txt` mirroring is gated `#ifdef _DEBUG` (`GWToolboxdll/Logger.cpp:158`). So the
**toolbox** half of the loop must be a **Debug** build (harness + log mirror both on), or
the mirror must be extended to the `GWTB_HARNESS` path. Recommended: toolbox = Debug,
GWCA = MinSizeRel/RelWithDebInfo (assert-free). The two configs are independent (GWCA is
a delay-loaded DLL), so this pairing is valid.

### B.4 The anchor index (the map from name → source)

`tools/gw-update/anchor-index.(ps1|py)` scans the GWCA + toolbox source once and emits
`anchor-index.json`: for every scan site — `{ repo, file, line, anchor_var, kind
(raw|string|assertion), pattern_or_string, target_symbol_hint }`. Built by matching:

- `GWCA_INFO("[SCAN] <name> = %p", <var>)` → the `<name>`↔`<var>` mapping.
- the nearest preceding `Scanner::Find(...)` / `FindAssertion(...)` / `FindUseOfString(...)`
  → the anchor kind + the literal pattern/string.

This index is what joins a `broken` survey entry to the exact line to edit and tells the
re-derivation step whether it can convert a raw pattern to a resilient anchor.

## C. Failure taxonomy the survey cannot see directly

Some breakage does **not** null a `[SCAN]` line and needs extra probes:

- **Struct offset shift** — the scan resolves fine, but a field read is wrong. Silent
  unless a `static_assert(sizeof/offsetof)` in `Include/GWCA/**/*.h` trips at **compile
  time**. So: (a) a clean GWCA compile catches guarded offset changes; (b) unguarded ones
  surface only as wrong in-game behavior → caught by the functional matrix ([05]).
- **Enum value shift** (UI message ids, preference indices, agent/skill ids) — no null
  scan; manifests as wrong UI messages / wrong ids. Caught by targeted functional probes
  (e.g. dialog, salvage, skill tooltip) and by decompiling the relevant tables in Ghidra.
- **New anchor needed** — ArenaNet adds a function GWCA must now hook (rare; e.g. the
  `patch_cam_update` added in `"Updated for 38092"`). Not a survey hit; found by feature
  regression + RE.

These are why verification is two-layered: **survey (null scans) + functional matrix**.
The survey converges the mechanical majority; the matrix catches the semantic tail.
