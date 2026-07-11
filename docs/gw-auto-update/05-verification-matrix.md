# 05 — Verification matrix: what "all functionality works" means

Two layers. **Layer 1 (survey)** converges the mechanical majority — zero null anchors.
**Layer 2 (functional)** catches the semantic tail the survey can't see (enum/offset
shifts, new hooks). An update is "fixed" only when both pass.

## Layer 1 — scan survey (necessary, not sufficient)

Pass = `scan-survey.ps1` on the post-injection `log.txt` reports **0 broken** and
**0 missing** `[SCAN]` anchors across GWCA + toolbox, and the GWCA delay-load ABI assert
(`GWToolbox.cpp:718`, "Failed to find a valid proc address in GWCA…") did **not** fire.
This is cheap, fully automated, and the primary loop signal.

## Layer 2 — functional matrix (drives the semantic tail)

Each row is a probe the harness can trigger and a signal that is machine-checkable from
`log.txt` / `harness_status.txt`. Rows marked ⊕ need **new harness verbs** (build-out in
[07](07-roadmap-and-8.29-dryrun.md)); the rest exist today.

| # | Subsystem | GWCA area exercised | Probe | Pass signal |
|---|---|---|---|---|
| 1 | Injection / init | GWCA.cpp context anchors, MemoryMgr | `harness.ps1 up` | `harness_initialized`; no DLI assert |
| 2 | Login / pregame | GWCA PreGameContext, UIMgr | auto-login (`login`) | reaches char-select then in-game |
| 3 | Map / world | MapMgr, WorldContext | on map load | correct map id logged ⊕ |
| 4 | Agents | AgentMgr (`AgentArrayPtr`, char array) | `dumpnav` / dump agents ⊕ | non-empty agent list, sane positions |
| 5 | Movement / pathing | AgentMgr `MoveTo`, MapMgr geometry | `setgoal`→`repath` / `goto` | `path_set(...)` not `path_failed` |
| 6 | Party | PartyMgr offsets | dump party ⊕ | self + heroes present, correct ids |
| 7 | Skillbar | SkillbarMgr, Skills.h ids | dump skillbar ⊕ | 8 slots, ids match equipped |
| 8 | Items / inventory | ItemMgr offsets, InventoryManager | dump bags ⊕ | bag/slot counts sane, ids resolve |
| 9 | Chat | ChatMgr (many raw scans) | send + read a chat line ⊕ | message round-trips, colors resolve |
| 10 | Dialog / NPC | DialogModule (`GmNpc.cpp` anchor) | open an NPC dialog ⊕ | dialog buttons enumerated |
| 11 | UI messages | UIMgr, UIMessages.h enum | trigger a known UI message ⊕ | correct handler fires |
| 12 | Camera | CameraMgr (`GmCam.cpp`, cam patch) | toggle cam unlock ⊕ | mode toggles, no crash |
| 13 | Merchant / trade | MerchantMgr, TradeMgr | open merchant ⊕ | item list populates |
| 14 | Quest / markers | QuestMgr | `repath` via quest marker | marker path computes |
| 15 | Render / compositor | RenderMgr end-scene hook | overlay draws | in-world overlay visible, no crash ⊕ |

**Zones to exercise:** at minimum an outpost (UI/merchant/party/skillbar) **and** an
explorable (agents/movement/pathing/render). The harness auto-waypoint gives a
deterministic explorable probe; an outpost probe set is a build-out item.

## Regression gates (must never worsen)

- **No crash** across the probe run: watch `.../M-TH/crashes/*.dmp`. A new dump ⇒ fail;
  symbolize it (repo memory `reference_clang_build_and_logs` documents the exact minidump
  + `llvm-symbolizer` recipe) and feed the faulting module back into re-derivation.
- **No `path_failed`** where the green baseline produced `path_set` for the same goal.
- **GWCA compile clean** — a tripped `static_assert(sizeof/offsetof)` is a *good* early
  signal of an offset shift; resolve it (don't silence it) before proceeding.

## Baseline capture (do this on a known-good pairing first)

Before injecting any artificial breakage (the 8.29 dry-run) or trusting the loop on a
real update, run the whole matrix on the **current** toolbox+GWCA against the **current**
live `Gw.exe` and snapshot the expected-pass results to
`tools/gw-update/baseline.json`. The current `dev`/`master` trees are already updated for
the live client, so this run should be all-green — that green snapshot is the reference
every subsequent fix cycle is measured against. (Task #4.)

## Machine-checkability

Every row must reduce to a string match in `log.txt`/`harness_status.txt` or a file check
(no dump, exit status). Where a subsystem has no current machine signal, the build-out
adds a harness verb that dumps the relevant GWCA state and a one-line `[verify] <row>: OK`
emit, so the orchestrator scores the matrix without a human in the loop.
