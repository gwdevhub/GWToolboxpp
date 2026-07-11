# 07 — Roadmap & the 8.29 dry-run validation

## Phased build-out

Each phase is independently useful and leaves the tree shippable. Phases 1–3 need no game
running; the risk ramps up gradually.

### Phase 1 — Deterministic toolkit (Task #3, Workflow W1)
Ship `tools/gw-update/`:
- `detect-build.ps1` — build-id via the `index < arrsize(s_eula)` port + SHA + `state.json`.
- `anchor-index.(ps1|py)` — the master index of all ~219 scan sites (repo/file/line/var/
  kind/pattern/target-hint).
- `scan-survey.ps1` — `log.txt` → `survey.json` (broken/missing anchors joined to source).
- `verify-pattern.(ps1|py)` — masked on-disk re-scan matching GWCA's matcher, for
  uniqueness checks.
**Exit:** all four run against the live `Gw.exe` and a captured `log.txt`; adversarially
verified. No game needed.

### Phase 2 — Ghidra MCP online (Task #2)
Register the `ghidra` MCP in `.mcp.json`; reload; smoke-test `connect_instance` +
`list_open_programs` + `search_strings("index < arrsize(s_eula)")` → `get_xrefs_to`.
Confirm the loaded build id. Document the `analyzeHeadless.bat` fallback and test it once.
**Exit:** can decompile a named function and resolve a string xref from the automation.

### Phase 3 — Green baseline (Task #4)
Build current GWCA (MinSizeRel, assert-free) + toolbox (Debug), inject into the live
client, run the full survey + functional matrix, snapshot `baseline.json` (all-green).
**Exit:** survey reports 0 broken; matrix passes; reference captured.

### Phase 4 — Re-derivation loop on real breakage (Task #5 = the 8.29 dry-run)
Wire W2 (re-derive fan-out) + W3 (verify) into `run.ps1`; prove it on the 8.29 experiment
below. **Exit:** the loop autonomously drives a broken pairing back to green and its fixes
match the answer key (see below).

### Phase 5 — Functional-matrix harness verbs
Add the ⊕ harness verbs from [05](05-verification-matrix.md) (dump agents/party/skillbar/
inventory, open dialog/merchant, trigger a UI message, outpost probe set), each emitting a
machine-checkable `[verify] <row>: OK`. **Exit:** matrix scored end-to-end without a human.

### Phase 6 — Full autonomy + trigger (Task #6)
`run.ps1` as the resumable state machine; a scheduled/`loop` trigger that polls
`detect-build.ps1` and, on a real update, runs the whole pipeline and presents a staged
diff. **Exit:** a real ArenaNet update is fixed to a reviewable diff with no human until
the commit gate.

## The 8.29 dry-run (the headline validation)

**Idea.** Tag `8.29_Release` was correct for an *older* client. The **current live
`Gw.exe`** is newer. So building the 8.29-era toolbox+GWCA and running it against today's
client reproduces a real "the game updated under us" break — and, crucially, the commits
that landed *after* 8.29 are the **answer key**: we can score the autonomous fixes against
what humans actually changed.

**Why it's a fair test.** The break is real (older signatures vs newer binary), the input
is exactly what the pipeline consumes (a broken pairing + a new `Gw.exe`), and there's an
objective grade (later commits). It exercises every stage except live update-detection.

**Procedure (all in an isolated worktree; never the live tree; never commit):**
1. **Pin the pairing.** `git -C gwtoolboxpp worktree add ../gwtb-829 8.29_Release`.
   Identify the GWCA revision 8.29 shipped against (the `Dependencies/GWCA` content at
   that tag, or the GWCA commit dated to the 8.29 release) and check GWCA out to it in its
   own worktree. Record both.
2. **Establish the answer key.** Enumerate GWCA + toolbox commits *after* that pairing up
   to today that are update fixes (`"Updated for NNNNN"`, agent/skill-id, offset, scan
   changes). This is the graded target set — store `answer-key.json` (file:line:anchor).
3. **Reproduce the break.** Build 8.29 GWCA (MinSizeRel, assert-free) + toolbox (Debug),
   inject into the live client, run `scan-survey.ps1`. Expect a non-trivial broken set.
4. **Run the loop blind.** Drive W2/W3 re-derivation + rebuild + re-inject to convergence,
   *without* consulting the answer key.
5. **Grade.** Compare the pipeline's `rederivation-log.json` + resulting diff against
   `answer-key.json`:
   - anchors fixed that the humans also fixed (true positives),
   - anchors the humans fixed that we missed (false negatives → hardest cases, likely
     semantic enum/offset),
   - anchors we changed that humans didn't (investigate — either a valid alternative
     anchor or an over-edit).
   Functionally, the matrix passing on the live client is the ultimate grade, independent
   of textual match (a re-anchored `FindAssertion` that differs from the human's raw
   `Find` but resolves correctly is a *win*, not a miss).

**Caveats to expect.** Building 8.29-era code with the current toolchain may need minor
build fixups (record them separately from client-fix edits so grading isn't polluted).
The 8.29→now gap may span *several* client builds at once, so the break is larger than a
single update — a stress test, not the minimal case. If it's too large to converge, also
run a **single-step** variant: the most recent `"Updated for NNNNN"` commit's parent vs
that commit, which isolates exactly one update's worth of breakage.

## Immediate next actions (this session)

1. **Task #3 / W1** — launch the toolkit-bootstrap workflow now (deterministic, no game).
2. **Task #2** — register the Ghidra MCP and smoke-test (needs a session reload).
3. **Task #4** — green baseline once the toolkit exists.
4. Then **Task #5** — the 8.29 dry-run, in a worktree.
