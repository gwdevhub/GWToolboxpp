# 06 — Orchestration: control loop, workflows, safety

## The control loop (one entrypoint)

`scripts/gw-update/run.ps1` is the top-level driver. It is a state machine over the seven
stages in [00](00-overview.md), persisting progress to `tools/gw-update/state.json` so it
is resumable and idempotent:

```
STATE: IDLE ─(detect: build changed)─► SURVEY ─► REDERIVE ─► PATCH_BUILD_GWCA
   ─► BUILD_TOOLBOX ─► INJECT_VERIFY ─(residual)─► REDERIVE (loop, ≤K)
                                      └(clean+matrix pass)─► PRESENT_DIFF ─► IDLE
```

Each stage is a small, independently runnable step (so a human can drive one stage by
hand, and the autonomous loop and the manual harness share the same primitives):

| Stage | Command | Reads | Writes |
|---|---|---|---|
| detect | `detect-build.ps1` | live `Gw.exe`, `state.json` | `state.json` |
| survey | build (assert-free GWCA + Debug tb) → `harness.ps1 up` → `scan-survey.ps1` | `log.txt`, `anchor-index.json` | `survey.json` |
| rederive | Ghidra MCP fan-out (workflow) | `survey.json`, `anchor-index.json` | `rederivation-log.json`, edits |
| patch+build GWCA | `cmake --build build --target create_release` | edits | `releases/latest` |
| build toolbox | `cmake --build build --config Debug --target GWToolboxdll` | edits, new GWCA | `bin\Debug\...` |
| inject+verify | `harness.ps1 reload` → `scan-survey.ps1` + matrix | `log.txt`, `baseline.json` | `verify.json` |
| present | git diff (staged) | edits | review summary |

## Where workflows fit (the user asked for workflow-driven advancement)

The loop is deterministic control flow (a script); the **model-heavy, fan-out stages are
workflows**. Three workflow shapes, run as the loop reaches each stage:

### W1 — Toolkit bootstrap (no game, no Ghidra)
Build + adversarially verify the deterministic toolkit in parallel: `detect-build`,
`scan-survey`, `anchor-index`, `verify-pattern` (masked on-disk re-scan matching GWCA's
matcher). Each component: implement → verify against the live `Gw.exe` / a captured
`log.txt` sample. This is the first thing to run (Task #3) because everything else
consumes its outputs.

### W2 — Re-derivation fan-out (Ghidra MCP)
The core parallel win. Pipeline over `survey.json`'s broken anchors:
```
pipeline(brokenAnchors,
  a => agent(rederivePrompt(a), {phase:'Rederive', schema:REDERIVE, /* MCP: ghidra */}),
  proposal => agent(verifyPrompt(proposal), {phase:'Verify', schema:VERDICT})   // adversarial
)
```
- **Stage 1 (rederive):** one agent per broken anchor, each using the Ghidra MCP + the
  decision ladder in [03](03-ghidra-rederivation.md) §C, returning `{anchor, new_anchor,
  method, evidence, uniqueness_verified}`.
- **Stage 2 (verify):** an independent agent re-checks each proposal — re-runs the masked
  scan for uniqueness on the on-disk binary, confirms the xref/decompile evidence, and
  votes real/refuted. Only verified proposals become edits.
- Enum/offset re-derivation is a **separate, smaller, higher-effort lane** (cross-cutting,
  needs whole-header reasoning) rather than the per-anchor fan-out.
Edits are applied to an **isolated worktree** (`isolation:'worktree'`) so parallel edits
across GWCA files don't collide, then merged.

### W3 — Verification scoring
After each rebuild+inject, score the matrix ([05](05-verification-matrix.md)) — cheap
parallel string-checks over `log.txt`/`status`, plus the dump-diff probes. Feeds
`residual` back to the loop.

> Only run workflows when the work is genuinely fan-out (many independent anchors) or
> needs adversarial verification. A 2-anchor break is faster handled inline. The loop
> chooses inline vs workflow by `survey.json` size.

## Convergence & termination

- Loop stages 3→6 until `survey.json.broken == 0` **and** matrix passes, or `K` rounds
  (default 4) elapse.
- Each round must **strictly reduce** the broken+failing count; if a round makes no
  progress twice consecutively, stop and escalate (the residual anchors are the hard,
  likely-semantic ones — enum/offset/new-hook — that need human RE judgment).
- Cascades: an early-failing foundational anchor (MemoryMgr, GameThread, StoC handler
  table) can null many downstream `[SCAN]` lines. Fix foundational anchors first
  (order by the ranked list in [01](01-failure-surface.md) §5), re-survey, then the
  count often collapses.

## Safety gates (enforced by the driver)

- **Never commit / never tag.** `PRESENT_DIFF` runs `git add -A` at most and prints the
  diff + `rederivation-log.json`; a human reviews and commits. (Memory: `feedback_never_commit`.)
- **Never delete `bin/`.** Clean rebuilds wipe only `build/` / `build-clang/`. (Memory:
  `feedback_never_delete_bin`.)
- **Isolated experimentation.** The 8.29 dry-run and any risky revert run in a **git
  worktree**, never the user's live working tree.
- **RE read-only.** No writes to the shared Ghidra Server; `rederivation-log.json` is the
  durable artifact for a human to persist.
- **Crash containment.** A new `crashes/*.dmp` halts the loop, symbolizes, and routes the
  faulting module back to re-derivation rather than blindly re-injecting.
- **Bounded game automation.** Launch/login/inject are pre-authorized; the driver still
  guards against runaway relaunch loops (max injects/hour).

## Fallbacks

- **Ghidra MCP down** → local `analyzeHeadless.bat` against
  `ghidra://ghidra.gwtoolbox.com/Guild Wars` (program `Gw.388118.exe`), `-connect marc`
  with the password piped on stdin, running a `-postScript` that exports
  candidate signatures. Mind the local Ghidra 12.0.3 vs server-version compatibility.
- **New client build not yet in Ghidra** → `load_program`/`-import` the on-disk `Gw.exe`.
- **Survey build strips `GWCA_INFO`** → build a dedicated RelWithDebInfo "diagnostic"
  GWCA with always-on scan reporting (see [02](02-detection-and-feedback.md) §B.2).
