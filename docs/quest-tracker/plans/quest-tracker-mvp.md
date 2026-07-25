# Plan: Quest Tracker MVP

## Goal

Ship a safe, observation-only quest tracker in this GWToolboxpp fork: live quest list and objectives, then per-character history, then Codex-compatible export/manual corrections — without automating gameplay or treating disappearance as completion.

## Non-goals

- Automate quest take, abandon, reward, travel, combat, or dialogue.
- Fabricate permanent completion from removal or offline gaps.
- Runtime dependency on `CompletionWindow`.
- Mass formatting, unrelated renames, or dependency updates.
- Implementing a new CTest/test executable as part of this investigation (decision deferred to Phase 2).

## Current implementation (baseline)

| Piece | Role |
|-------|------|
| `QuestModule` | Pathing, custom marker `0xfdd`, `ParseQuestObjectives` |
| `ActiveQuestWidget` | Active quest + mission-objective display |
| `CompletionWindow` | Mission/bonus persistence reference only |
| `DialogModule::QuestDialogType` | Correlated REWARD / TAKE dialog encoding |
| Contract v1 | [`docs/contracts/quest_progress_contract_v1.md`](../../contracts/quest_progress_contract_v1.md) (mirrored from GuildWarsCodex; Toolbox = producer, Codex = consumer) |
| Capability notes | [`docs/contracts/quest_progress_contract_v1_toolbox_capability_notes.md`](../../contracts/quest_progress_contract_v1_toolbox_capability_notes.md) (non-normative) |

See [repository-investigation.md](../repository-investigation.md).

## Verified vs unknown GWCA data

**Verified (live):** quest log snapshot, active quest, in-log `log_state` / `IsCompleted()`, objective strings when fetched, quest add/remove UI events, mission objectives array, mission/bonus bitsets, account UUID, character `player_uuid`.

**Confidence-aware / probable only:** abandon via `kSendAbandonQuest` + remove; reward via REWARD dialog + remove.

**Unknown from game:** permanent historical turn-in after removal; remotions while Toolbox offline.

Full matrix: [data-availability.md](../data-availability.md).

## Target classes (future)

Per [proposed-architecture.md](../proposed-architecture.md):

- `QuestObservationService` — owns HookEntries; copy-out observations; throttled `RequestQuestInfoId`
- `QuestProgressReducer` — pure, unit-testable
- `QuestProgressStore` / `QuestHistoryStore` — `account_uuid + character_uuid`
- Mission/bonus bitset adapter (owned)
- `QuestTrackerWindow` — UI

## Lifecycle / callbacks / teardown

Document and implement (when coding begins):

| Hook | Behavior |
|------|----------|
| `Initialize` | Register callbacks; ObservationService owns HookEntries |
| `Update` | Drain queue; reduce; throttle requests; flush persistence off callback path |
| `SignalTerminate` | Unregister callbacks; stop requests; safe flush |
| `Terminate` | Clear HookEntries and owned caches; no dangling GWCA pointers |

Rules: no GWCA pointer retention past callback/frame; no disk I/O in callbacks; lightweight callbacks; no throw across game boundaries.

## Persistence

- Module settings: `SettingsDoc`
- Progress/history: owned JSON under `Resources::GetPath`
- Key: `account_uuid + character_uuid`; name = metadata / UUID-unavailable fallback only; **no name-only auto-merge**

## UI

- Phase 1: live list + objectives (quest vs mission channels separated); clickable rows set the game's active quest (narrow user-initiated mutation; not progress persistence)
- Phase 2: history visibility / character context
- Phase 3: manual corrections + export/import controls

## Performance

- Event → snapshot, not event-as-authority
- Throttle/dedupe `RequestQuestInfoId`
- No network / blocking I/O in `Draw`/`Update` callbacks

## Failure handling

- Loading / null context: skip
- Missing objectives: pending + throttled fetch
- Bare remove: unknown
- Malformed JSON: tested parser path; no silent data loss

## Exact files (future implementation)

**New (preferred):**

- Observation / reducer / store helpers under `GWToolboxdll/Modules/` or a small dedicated folder already covered by GLOB (or `Windows/` for the window)
- `QuestTrackerWindow.h` / `.cpp`

**Upstream touch:**

- [`GWToolboxdll/Modules/ToolboxSettings.cpp`](../../../GWToolboxdll/Modules/ToolboxSettings.cpp) — add to `optional_modules` only

**Not required for MVP sources:**

- [`GWToolboxdll/CMakeLists.txt`](../../../GWToolboxdll/CMakeLists.txt) — Module/Window GLOB already includes new files

**CMake later only if:** an approved separate test target is added.

**Do not modify for coupling:** `CompletionWindow.*` (reference only). Prefer call-level reuse of `QuestModule::ParseQuestObjectives` without changing pathing behavior.

## Build / tests

- Build: `cmake --preset=vcpkg` then `cmake --build build --config RelWithDebInfo`
- Compilation does not prove game-state behavior — in-game verification required each phase
- `TestHarness` = in-game development harness, **not** a CTest unit framework
- Reducer must be written pure/unit-testable; **Phase 2 decides** concrete test integration (no commitment to a new test executable in this plan)

## Manual in-game verification (per phase)

### Phase 1

- [ ] Map load shows current quest log (excluding `0xfdd`)
- [ ] Clicking a quest row sets the game's active quest (highlight + details; native quest log/marker)
- [ ] Clicking already-active / mission_mode / loading does not mutate selection
- [ ] Active quest selection updates list/highlight
- [ ] Objective bullets update after `kQuestDetailsChanged` path (complete vs incomplete)
- [ ] In-log `IsCompleted()` reflected without claiming permanent history
- [ ] Mission objectives path (`qid == -1` / `kObjective*`) does not corrupt quest-log rows
- [ ] Custom marker set/clear does not appear as real quest progress
- [ ] No requests every frame when objectives missing (throttle observable)
- [ ] Map transition follows game active quest (no forced reselection)

### Phase 2

- [ ] Progress keyed by account + character UUID across relog
- [ ] Character switch loads the other UUID’s store (no name-only merge)
- [ ] Remove without dialog → unknown/uncertain history, not completed
- [ ] Abandon pairing → probable abandoned
- [ ] Reward dialog pairing → probable turn-in (not auto confirmed unless validated)
- [ ] Offline gap (edit store or play without Toolbox) → no fabricated completions
- [ ] Mission/bonus bit refresh via owned adapter, independent of CompletionWindow

### Phase 3

- [ ] Export matches contract v1 envelope; consumers can reject bad major version
- [ ] Import idempotent; manual marks use `confidence=manual`
- [ ] Codex round-trip of sample progress

---

## Phases

### Phase B3 — Contract v1 documentation synchronization (complete for docs)

**Done in this documentation phase**

- Mirrored canonical Contract files from GuildWarsCodex (`quest_progress_contract_v1.md` + example JSON)
- Toolbox-local capability notes (non-normative)
- Cursor rules / doc references for producer/consumer roles and honesty invariants

**Explicitly not done by Phase B3**

- No Contract exporter / parser implementation
- No history persistence / `QuestProgressStore` / `QuestHistoryStore`
- No new quest observation production work started by this task
- Observation capabilities are **not** “complete” merely because Contract docs exist

**Next:** any further Toolbox implementation phase (observation hardening, persistence, or export) must be **separately approved**.

### Phase 1 — Live list / objectives / active-quest click

**Scope**

- ObservationService + window showing current quests and objectives
- Quest UI events → fresh `GetQuestLog()` snapshot
- Mission objectives on a separate snapshot path
- Throttled `RequestQuestInfoId`
- HookEntry ownership and copy-out rules
- Settings only (no progress history file yet)
- Clickable normal quest rows enqueue `SetActiveQuestId` on the game thread; refresh via existing active-quest callbacks; not stored in contract history

**Out of scope:** persistence of history, export, manual completion, CompletionWindow calls, accept/abandon/reward, travel-on-click, map-load forced reselection, prerequisite inference

**Exit criteria:** builds; Phase 1 in-game checklist passes

### Phase 2 — Per-character persistence / history

**Scope**

- `account_uuid + character_uuid` stores
- Append-only history for add / objective change / in-log ready_for_reward / remove-unknown / abandon-probable / reward-probable
- Offline diff policy: unknown/uncertain only
- Owned mission/bonus bitset observation
- Malformed/old-version JSON parsing designed for tests
- **Decision checkpoint:** how to run pure reducer unit tests (existing process, ad-hoc harness, or future target) — document choice before expanding test surface

**Exit criteria:** persistence survives relog and character switch; disappearance never becomes completed

### Phase 3 — Export / manual corrections / Codex compatibility

**Scope**

- Export/import per mirrored contract v1 (byte-identical `docs/contracts/`)
- Manual complete/abandon with `manual` confidence
- `mission_completion_data` from owned bitset reads (separate from quest-log states)
- Idempotent import; reject unsupported major versions
- Stable structured identity + SHA-256 fingerprints; no runtime `std::hash`

**Exit criteria:** sample Codex-compatible file validated; manual corrections round-trip

---

## Deferred work

- Promoting probable reward/abandon to confirmed (needs contract language + runtime traces)
- StoC typed `QUEST_REMOVE` if GWCA gains structs (optional; UI path sufficient)
- Shared library of quest canon metadata (names, campaigns) — out of band from progress
- Dedicated CTest executable (only if Phase 2 decision requires it)
- UI polish, filters, search, multi-account browser
- **Prerequisite-based historical completion inference:** only strict mandatory completion prerequisites; inferred completion distinguishable from observed/manual; not Phase 1

## Upstream-safety checklist (when implementing)

- [ ] New files preferred; `ToolboxSettings.cpp` justified
- [ ] No CompletionWindow dependency
- [ ] No dependency version bumps
- [ ] No generated build artifacts committed
- [ ] Callbacks unregistered on terminate

---

## STOP

Documentation for this investigation is complete.

**Do not implement Phase 1 or any production code as part of this investigation deliverable.**

Further work requires an explicit implementation approval.

STOP
