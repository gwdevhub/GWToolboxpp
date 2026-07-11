# Autonomous GW-update repair for GWToolbox / GWCA

Plan + build-out for a system that, when ArenaNet updates the Guild Wars client
(`Gw.exe`), **autonomously** re-derives the broken memory anchors, edits and rebuilds
GWCA and GWToolbox, launches and injects into the game, verifies every subsystem, and
presents a reviewable diff — stopping short of committing (a human does that).

## TL;DR — the pipeline

```
detect update (build-id) → survey breakage ([SCAN]=0 lines in log.txt)
→ re-derive broken anchors in Ghidra (MCP) → edit + build GWCA (create_release)
→ build toolbox (Debug) → inject via harness → verify (survey + functional matrix)
→ loop until green → present staged diff  (never auto-commit)
```

## Why it can work

- Breakage **self-reports**: 120 `[SCAN] <name> = %p` log lines → nulls list the exact
  broken anchors.
- Most anchors **self-heal**: ~100 assertion/string anchors re-find ArenaNet's shipped
  literals; only ~54 raw byte-patterns in GWCA (+ a few in the toolbox) need RE.
- A **programmable RE surface** exists: the `ghidra` MCP server (read-only headless
  Ghidra on a recent `Gw.exe`).
- A **closed test loop** exists: `TestHarness` + `scripts/harness.ps1` (launch, login,
  inject, drive, self-unload, log).

## Read in order

| Doc | What |
|---|---|
| [00-overview.md](00-overview.md) | Goal, validated pipeline, repo/tool map, human gates, convergence. |
| [01-failure-surface.md](01-failure-surface.md) | What breaks; measured inventory (~219 scan sites) + history. |
| [02-detection-and-feedback.md](02-detection-and-feedback.md) | Update detection + the `[SCAN]` survey loop; assert-free build. |
| [03-ghidra-rederivation.md](03-ghidra-rederivation.md) | Ghidra MCP + the per-anchor re-derivation decision ladder. |
| [04-build-release-inject.md](04-build-release-inject.md) | Exact GWCA/toolbox build, `create_release`, harness inject. |
| [05-verification-matrix.md](05-verification-matrix.md) | What "all functionality works" means, machine-checkably. |
| [06-orchestration.md](06-orchestration.md) | Control loop, the three workflows, safety, fallbacks. |
| [07-roadmap-and-8.29-dryrun.md](07-roadmap-and-8.29-dryrun.md) | Phased build-out + the 8.29 answer-key validation. |
| [08-live-breakage-findings.md](08-live-breakage-findings.md) | Session findings: live-client breakage hunt vs 389033. |
| [09-ghidra-project-updates.md](09-ghidra-project-updates.md) | Migrating the Ghidra markup to each new client build. |
| [10-crash-triage.md](10-crash-triage.md) | `triage_dumps.py`: batch-symbolize + cluster wild crash dumps. |

## Code that will be built (not yet present)

- `tools/gw-update/` — `detect-build`, `anchor-index`, `scan-survey`, `verify-pattern`,
  and the JSON state/artifacts (`state.json`, `anchor-index.json`, `survey.json`,
  `rederivation-log.json`, `baseline.json`, `verify.json`).
- `scripts/gw-update/run.ps1` — the resumable state-machine driver.
- New `TestHarness` verbs for the functional matrix.
- `.mcp.json` entry for the `ghidra` server.

## Ground rules (from repo memory)

Never commit (stop at staged); never delete `bin/`; keep `#ifdef _DEBUG` gates; the main
game thread never blocks; RE is read-only against the shared Ghidra project.
