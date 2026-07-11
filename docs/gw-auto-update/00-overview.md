# 00 — Overview: autonomous GWToolbox/GWCA repair after a Guild Wars update

## The goal

When ArenaNet ships a new `Gw.exe`, byte-signature scans and hardcoded offsets in
**GWCA** (and, to a lesser extent, **GWToolbox**) break. Today a human re-derives the
addresses in Ghidra and hand-edits the code (see the `"Updated for 38092"` class of
commits). This project builds a system that does that **end to end, autonomously**:

> detect the update → re-derive the broken anchors from the new binary in Ghidra →
> edit GWCA → build + `create_release` → edit the toolbox → build → launch Guild Wars,
> inject, and verify every subsystem works → converge → present a reviewable diff.

The user reviews and commits the final diff. Everything up to that point is automated.

## Why this is feasible (the four enabling facts)

1. **Breakage self-reports.** Every scanned address is logged as
   `[SCAN] <name> = %p` (120 such lines in GWCA). A broken scan logs `= 00000000`.
   Parsing `log.txt` after injection yields the exact per-anchor breakage list — no
   human observation needed. See [02-detection-and-feedback.md](02-detection-and-feedback.md).
2. **Most anchors self-heal.** ~50 GWCA + many toolbox anchors use
   `FindAssertion(file, msg, line)` / `FindUseOfString(str)`, which re-find ArenaNet's
   shipped assert/string literals *by content*. They survive nearly every update. The
   real work is the **~54 raw byte-pattern `Find` sites in GWCA + the raw-`Find`
   minority in the toolbox** (~70–90 total). See [01-failure-surface.md](01-failure-surface.md).
3. **A programmable RE surface exists.** The `ghidra` MCP server
   (`https://ghidra-mcp.gwtoolbox.com/mcp`, documented in
   `.github/claude/ghidra-mcp.md`) gives read-only headless Ghidra with a recent
   `Gw.<build>.exe` loaded — `search_functions`, `search_strings` + `get_xrefs_to`,
   `decompile_function`, call-graph traversal. This is how the automation re-derives
   addresses/signatures. See [03-ghidra-rederivation.md](03-ghidra-rederivation.md).
4. **A closed test loop exists.** The `TestHarness` module + `scripts/harness.ps1`
   already launch GW, auto-login, inject the freshly-built DLL, drive in-game actions
   (waypoint/path/nav dump), self-unload for relink, and mirror the log. This is the
   verification actuator. See [04-build-release-inject.md](04-build-release-inject.md)
   and [05-verification-matrix.md](05-verification-matrix.md).

## The validated pipeline

```
                         ┌─────────────────────────────────────────────┐
   ANet ships Gw.exe ──► │ 1. DETECT                                     │
   (via GWLauncher)      │    build-id via GetFileId port / SHA vs state │
                         └───────────────┬─────────────────────────────┘
                                         │ build changed
                         ┌───────────────▼─────────────────────────────┐
                         │ 2. SURVEY (baseline breakage)                 │
                         │    build assert-free GWCA + Debug toolbox,    │
                         │    inject, parse log.txt → list of null       │
                         │    [SCAN] anchors + owning file:line          │
                         └───────────────┬─────────────────────────────┘
                                         │ N broken anchors
                         ┌───────────────▼─────────────────────────────┐
                         │ 3. RE-DERIVE (Ghidra MCP, per anchor)         │
                         │    map anchor→function; extract fresh unique  │
                         │    signature OR convert raw Find→assertion/   │
                         │    string anchor; re-derive enum/offset shifts │
                         └───────────────┬─────────────────────────────┘
                         ┌───────────────▼─────────────────────────────┐
                         │ 4. PATCH + BUILD GWCA                         │
                         │    edit Source/*.cpp + Include headers;       │
                         │    cmake --build build --target create_release│
                         │    → repopulates GWCA/releases/latest         │
                         │      (the toolbox Dependencies/GWCA symlink)  │
                         └───────────────┬─────────────────────────────┘
                         ┌───────────────▼─────────────────────────────┐
                         │ 5. BUILD TOOLBOX + fix its own raw scans      │
                         │    cmake --build build --config Debug         │
                         │      --target GWToolboxdll                    │
                         └───────────────┬─────────────────────────────┘
                         ┌───────────────▼─────────────────────────────┐
                         │ 6. INJECT + VERIFY (harness.ps1)              │
                         │    launch GW, auto-login, inject; parse       │
                         │    log.txt for residual nulls; run functional │
                         │    probes (path/nav/dialog/inventory/…)       │
                         └───────────────┬─────────────────────────────┘
                                         │ residual breakage?
                              ┌──────────┴───────────┐
                          yes │ loop to step 3        │ no
                              ▼                       ▼
                      (converge, ≤K rounds)   ┌───────────────────────┐
                                              │ 7. PRESENT DIFF        │
                                              │  staged, NOT committed │
                                              │  human reviews/commits │
                                              └───────────────────────┘
```

## Repo & tool map (absolute paths on this machine)

| Thing | Path |
|---|---|
| Toolbox repo (orchestration hub) | `C:\Users\m\source\repos\gwtoolboxpp` |
| GWCA source (rebuildable) | `C:\Users\m\source\repos\GWCA` |
| GWCA release output (symlink target) | `C:\Users\m\source\repos\GWCA\releases\latest` |
| Toolbox → GWCA symlink | `gwtoolboxpp\Dependencies\GWCA` → `GWCA\releases\latest` |
| GWLauncher source (build-id reader) | `C:\Users\m\source\repos\gwlauncher` |
| Ghidra install (headless fallback) | `C:\Users\m\Downloads\ghidra_12.0.3_PUBLIC` |
| Ghidra MCP (primary RE surface) | `https://ghidra-mcp.gwtoolbox.com/mcp` |
| Shared Ghidra repo (RE ground truth) | `ghidra://ghidra.gwtoolbox.com/Guild Wars` (`Gw.388118.exe`, marc/medion1) |
| Live client | `C:\Program Files (x86)\Guild Wars\Gw.exe` |
| Harness driver | `gwtoolboxpp\scripts\harness.ps1` |
| Runtime log (Debug only) | `%OneDrive%\Documents\GWToolboxpp\M-TH\log.txt` |
| New automation lives here | `gwtoolboxpp\tools\gw-update\` + `gwtoolboxpp\scripts\gw-update\` |

## Human-in-the-loop gates (hard rules)

- **Never commit.** The pipeline stops at *staged + validated*; the user reviews and
  writes the commit message. (Repo memory: `feedback_never_commit`.)
- **Never delete `bin/`.** Only `build/` / `build-clang/` are safe to wipe.
- **Approve the final diff.** GWCA/toolbox source edits are presented as a diff for
  human sign-off before any release/tag.
- **RE is read-only.** Ghidra MCP work never writes back to the shared repo; findings
  are reported, a human persists them.
- Launching GW, auto-login, and injecting the dev DLL are pre-authorized (that is what
  the harness is for) and need no per-run approval.

## Convergence model

An update is "fixed" when, against the new client: (a) the scan-survey reports **zero**
null anchors across GWCA + toolbox, **and** (b) the functional verification matrix
([05](05-verification-matrix.md)) passes. The loop (steps 3→6) repeats until both hold
or a bounded round count `K` is hit, after which it reports the residual with full
context for a human. Partial convergence is normal and acceptable — GWCA history shows
updates landed incrementally (`"Updated for 38015, stoc still to do"`).

## Document index

- [01-failure-surface.md](01-failure-surface.md) — what breaks, measured inventory.
- [02-detection-and-feedback.md](02-detection-and-feedback.md) — update detection + the `[SCAN]` survey loop.
- [03-ghidra-rederivation.md](03-ghidra-rederivation.md) — Ghidra MCP + the per-anchor re-derivation playbook.
- [04-build-release-inject.md](04-build-release-inject.md) — exact build/release/inject commands.
- [05-verification-matrix.md](05-verification-matrix.md) — what "all functionality works" means, concretely.
- [06-orchestration.md](06-orchestration.md) — control loop, workflow designs, safety.
- [07-roadmap-and-8.29-dryrun.md](07-roadmap-and-8.29-dryrun.md) — phased build-out + the 8.29 validation experiment.
