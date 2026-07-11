# 09 — Keeping the Ghidra project updated to the latest GW client

Goal: on every Guild Wars client update, add the new client to the shared Ghidra project
as a build-numbered program `Gw.<build>.exe`, analyze it, and **keep all prior builds
intact** (never delete `Gw.388118.exe`, etc.). This gives a version history for RE and
lets each new build inherit the analyst's names/types from the previous one.

Shared repo: `ghidra://ghidra.gwtoolbox.com/Guild Wars` (login `marc`). Driven via the
local **ReVa** MCP against the running Ghidra.

## Procedure (agent-driven, idempotent)

1. **Detect build** — `python tools/gw-update/detect_build.py` on the live client →
   build id `N` (e.g. 389033). GWCA-independent (works even when scans are broken).
2. **Skip if present** — `list-project-files "/"`; if `Gw.N.exe` already exists, done.
3. **Build-named copy** — copy `C:\Program Files (x86)\Guild Wars\Gw.exe` →
   a temp `Gw.N.exe` (the program name derives from the filename, matching the
   `Gw.388118.exe` convention).
4. **Import + version** — ReVa `import-file { path: <temp Gw.N.exe>,
   enableVersionControl: true, recursive: false }`.
5. **Analyze + check in** — ReVa `analyze-program { programPath: "/Gw.N.exe",
   persist: "auto" }`. `persist:auto` checks the analyzed program into the shared repo
   because it is under version control. (~2.5–3 min for GW.)
6. **(Enhancement) inherit markup** — transfer the analyst's names/types from the prior
   build so the new one is immediately useful: ReVa `diff-create-session {source:
   /Gw.<prev>.exe, dest: /Gw.N.exe, correlators:[exact-instructions, exact-mnemonics,
   function-reference]}` then `diff-transfer-markup`. (VT correlation on GW is slow here —
   run it as a background job; not required for the base import.)
7. **Never delete prior builds.** Only ever ADD `Gw.<newer>.exe`.

`tools/gw-update/prep-ghidra-import.ps1` performs the deterministic steps 1–3 (detect
build, dupe-check hint, copy to a build-named file) and prints the ReVa calls to run.

## Known quirks / caveats (observed 2026-07-03)

- **`.0` duplicate on import.** Ghidra's batch importer produces a second program
  `Gw.N.exe.0` (same MD5) alongside `Gw.N.exe`, even with `recursive:false`. It is
  harmless duplicate noise but gets versioned too. **ReVa has no delete-program tool and
  the user's Ghidra isn't PyGhidra (so `run-script` is unavailable)** — so the `.0` (and
  any scratch programs) must be deleted in the **Ghidra GUI** (right-click → Delete), or
  by importing via the standalone `analyzeHeadless.bat -import` (single-program, no `.0`)
  when the GUI project isn't holding a lock.
- **Checkin is outward-facing.** Steps 4–5 push to the shared team server. This matches
  how `Gw.388118.exe` exists there and is additive + reversible (a program version can be
  deleted), but be aware other users will pull it.
- **Address ambiguity in diffs.** Both `Gw.<a>.exe` and `Gw.<b>.exe` load at ImageBase
  `0x400000`, so pass function **names** or explicit source/dest scope to `diff-function`
  (a bare address resolves on the source side).
- **Alternative headless path** (no running GUI needed): local Ghidra 12.0.3
  `support\analyzeHeadless.bat ghidra://ghidra.gwtoolbox.com/"Guild Wars" -connect marc
  -p -import <Gw.N.exe> -commit "..."` — mind the interactive password prompt (pipe on
  stdin) and the client-vs-server Ghidra version compatibility.

## Status (2026-07-03, session 3 — full migration to the current client)

The client self-patched again this morning (mtime 08:54): the on-disk `Gw.exe` is now
MD5 `a6e76703795ef2904e990affed2eefa1`, **newer than 389033** (which was MD5 `7d6c81af…`).
The running client at the 10:31 crash was this a6e767 build, so it is the one that must be
symbolized against.

Done this session:
- Imported the current on-disk `Gw.exe` as **`/Gw_current_a6e767.exe`** (main image;
  the batch importer also made the usual `.0` duplicate) and ran full analysis
  (19,020 functions, ~2.6 min, saved locally — `enableVersionControl:false`).
- Ran Version-Tracking correlation `/Gw.388118.exe` (source, fully symbolized) →
  `/Gw_current_a6e767.exe` (dest). Default correlators finished in ~178 s and matched
  **17,925 / 18,763** source functions, **all at similarity 1.0** (the two builds are
  structurally near-identical — a minor version bump, same compiler). A follow-up
  `combined-reference` pass over the residual matched 355 more (residual 838 → 483).
- Transferred markup (`diff-transfer-markup`, confidence 0.9999 then 0.5 for the residual):
  names, prototypes, calling conventions, locals, comments, and datatypes.
  **71,853 named symbols** now on the current build. Saved to disk (`analyze-program
  persist:save`).
- **Validated end-to-end:** crash EIP RVA `0x87bbb` → Ghidra `0x487bbb`, which was
  mid-instruction garbage against 388118, now resolves cleanly inside **`AssertionFailed`**
  (`0x487ba0`). So exception `0x80000003` on the GToB load was a **game-side assertion**,
  not a toolbox memory bug.

**Use `/Gw_current_a6e767.exe` for all current-client symbolization, not `/Gw.388118.exe`.**

### New caveats found this session
- **Build number is now runtime-only.** `GetFileId` no longer returns the build as an
  inline `MOV eax,imm32` (388118: `0x5ec16`); this build returns `&DAT_01087700`, a global
  that reads 0 both statically and in the live (crashed) process. So `detect_build.py`'s
  ".rdata assertion → MOV eax,imm32" scan likely no longer finds the build id on this
  build — revisit detection before relying on a build-numbered filename. The program is
  named by MD5 here because the integer build was not recoverable.
- **`diff-status` / async job polling is unreliable in this ReVa instance** (returns
  "session expired" whenever a VT session is touched, independent of job state). The
  markup transfer still runs and commits — monitor it via **JVM CPU sampling** of the
  Ghidra `javaw` PID and **spot-check applied names** (`get-decompilation`), not
  `diff-status`. `get-function-count` is also cached/stale after writes — use
  `get-symbols-count` instead.
- **PyGhidra is off** (no `run-script`), and the project is **local**
  (`C:\Users\m\Ghidra\GW.gpr`, no Ghidra Server process) and locked by the running GUI,
  so headless is blocked while the GUI is open. The per-match `diff-apply-match` write
  path works; the bulk `diff-transfer-markup` async job works but can't be polled.
- Global **data labels did not transfer** (only function correlators were run), so
  referenced globals inside functions still show as `DAT_…`. Fine for crash
  symbolization; a data-correlator pass would be needed for full global RE.

- Cleanup still pending (GUI, no delete tool via ReVa): `Gw.exe`, `Gw.exe.0`,
  `Gw.389033.exe.0`, `Gw.388118.exe.1`, and `Gw_current_a6e767.exe.0`. `Gw.388118.exe`
  and `Gw.389033.exe` are kept intact by design.
