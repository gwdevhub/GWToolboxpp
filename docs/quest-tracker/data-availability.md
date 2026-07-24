# Data availability — quest tracker

What GWCA and existing Toolbox code can observe, at what confidence, and what absence means. Aligns with [quest_progress_contract_v1.md](../contracts/quest_progress_contract_v1.md) and `.cursor/rules/game-state-confidence.mdc`.

**Rule:** never treat quest disappearance alone as completion.

## Scope model

| Scope | What | Lifetime |
|-------|------|----------|
| Snapshot | Quest log, active quest, `log_state`, encoded strings, mission objectives, mission/bonus bitsets | Current character / valid world context; invalid during load / char select |
| Event | UI messages / StoC packets | Transient; only while Toolbox is hooked |
| Persistent (game) | Mission/bonus/vanquish-style bit arrays in `WorldContext` | Character progress held by the game; visible while online |
| Persistent (Toolbox) | Local JSON/history the tracker may write | Not a game API |

There is **no** GWCA API for “all quests ever turned in.” Offline reconstruction of remotions is impossible from the client alone.

## Completion kinds (must not collapse)

1. **In-log completion (observed while still present)**  
   - `GW::Quest::log_state & 0x2` / `Quest::IsCompleted()` on an entry still in `GetQuestLog()`.  
   - Source: `game_snapshot`. Confidence: **confirmed** for that observation.  
   - Means objectives done / ready-for-reward **while the quest remains in the log** — not permanent historical turn-in.

2. **Correlated reward / turn-in**  
   - Evidence such as `DialogModule::QuestDialogType::REWARD` (or `ENQUIRE_REWARD`) paired with a subsequent `kQuestRemoved` / absence from a fresh snapshot.  
   - Confidence: **probable** pending runtime validation. Do **not** promote to fully confirmed completion unless the contract explicitly permits it **and** runtime traces validate the pairing.

3. **Unknown historical / offline completion**  
   - Quest absent relative to a prior local store with no live evidence (Toolbox was offline, or remove without correlating dialog/abandon).  
   - Outcome: **unknown** / `uncertain`. Never invent `completed_*`.

Correlated **abandon** (`kSendAbandonQuest` + removal) is likewise **probable**, not unconditional proof.

## Reliability matrix

| Concern | Reliability | Observation method | Notes |
|---------|-------------|--------------------|-------|
| Account identity | confirmed (session) | `GW::AccountMgr::GetAccountUuid()` ([`ToolboxUtils.h`](../../GWToolboxdll/Utils/ToolboxUtils.h)) | Prefer portal UUID |
| Character identity | confirmed (session) | `CharContext::player_uuid[4]` ([`CharContext.h`](../../Dependencies/GWCA/include/GWCA/Context/CharContext.h)) | Preferred persistence key with account UUID |
| Character name | display / fallback | `player_name` / `GetCurrentPlayerName()` | Metadata only; **do not** auto-merge records by name alone |
| Quest log | confirmed (snapshot) | Fresh `GW::QuestMgr::GetQuestLog()` after map load or quest UI events | Filter synthetic `0xfdd` custom marker |
| Selected / active quest | confirmed | `GetActiveQuestId()` / `GetActiveQuest()`; `kClientActiveQuestChanged`, `kServerActiveQuestChanged` | Game may auto-select newly added quests |
| In-log completion flag | confirmed while present | `Quest::IsCompleted()` / `log_state` | See completion kind (1) |
| Normal quest objectives | confirmed when strings present | After `kQuestDetailsChanged` / `kQuestAdded` / `kQuestRemoved`, snapshot log and parse `Quest::objectives` (`0x2` split, `0x2af5` complete) via `QuestModule::ParseQuestObjectives` | Null `objectives` ⇒ not yet fetched (`RequestQuestInfoId`), not “no objectives” |
| Mission objectives | confirmed for instance UI | After `kObjectiveAdd` / `kObjectiveComplete` / `kObjectiveUpdated`, snapshot `WorldContext::mission_objectives` | Separate channel; typically clears on map leave |
| Quest added | confirmed (presence) | `kQuestAdded` and/or StoC `QuestAdd`, then snapshot | |
| Quest removed (presence loss) | confirmed loss; **outcome unknown** | `kQuestRemoved` then fresh `GetQuestLog()` | Never treat as completion by itself |
| Correlated abandon | probable | `kSendAbandonQuest` paired with removal | Not unconditional proof |
| Correlated reward/turn-in | probable | REWARD / ENQUIRE_REWARD dialog paired with removal | Pending runtime validation before any stronger claim |
| Permanent / offline quest completion | unknown from game | Local history, manual input, import only | Cannot reconstruct remotions while offline |
| Mission / bonus maps | confirmed via bitsets | Owned read of `missions_completed`, `missions_bonus`, `missions_completed_hm`, `missions_bonus_hm`; refresh on `kMissionComplete` / related | **Not** via `CompletionWindow` runtime; mission maps ≠ quest log |
| Character switch | confirmed on UUID change | Diff `player_uuid` (+ account) on `kMapLoaded` / identity change | Switch store; do not carry prior character’s log as confirmed |
| Offline reconstruction | remotions unknown | Compare later snapshot to last store | Diffs → unknown/uncertain only; never auto-complete |

## Quest objectives vs mission objectives

These must stay separate in observation and UI.

### Normal quest log

**Preferred update path:**

1. Receive `kQuestDetailsChanged`, `kQuestAdded`, and/or `kQuestRemoved` ([`UIMessages.h`](../../Dependencies/GWCA/include/GWCA/Constants/UIMessages.h)).
2. Take a fresh `GetQuestLog()` / `GetQuest` snapshot.
3. Copy fields into owned structs (do not retain GWCA pointers).
4. Parse objective encoded strings when present; otherwise schedule throttled `RequestQuestInfoId`.

Events are **triggers**, not complete authoritative state by themselves.

### Mission instance objectives

**Preferred update path:**

1. Receive `kObjectiveAdd`, `kObjectiveComplete`, `kObjectiveUpdated`.
2. Snapshot `WorldContext::mission_objectives` (`MissionObjective::{objective_id,enc_str,type}`).
3. ActiveQuestWidget treats `(int32_t)GetActiveQuestId() == -1` as mission mode — instance UI, not a normal quest id.

Do not feed `kObjective*` into quest-log objective reducers.

## Snapshot APIs (concrete)

[`QuestMgr.h`](../../Dependencies/GWCA/include/GWCA/Managers/QuestMgr.h):

- `GetQuestLog()`, `GetQuest(QuestID)`, `GetActiveQuestId()`, `GetActiveQuest()`
- `RequestQuestInfo` / `RequestQuestInfoId` — populate missing description/objectives/markers
- `AbandonQuest` / `SetActiveQuestId` — **actions**; observation may watch outbound UI (`kSendAbandonQuest`, `kSendSetActiveQuest`) but must not automate them for the tracker

[`Quest.h`](../../Dependencies/GWCA/include/GWCA/GameEntities/Quest.h):

- Fields: `quest_id`, `log_state`, `location`, `name`, `npc`, `map_from`/`map_to`, `marker`, `description`, `objectives`
- Helpers: `IsCompleted()`, `IsCurrentMissionQuest()`, `IsPrimary()`, `IsAreaPrimary()`

[`WorldContext.h`](../../Dependencies/GWCA/include/GWCA/Context/WorldContext.h) (~227–240):

- `active_quest_id`, `quest_log`, `mission_objectives`
- `missions_completed`, `missions_bonus`, `missions_completed_hm`, `missions_bonus_hm`

## StoC notes

| Opcode | ID | Typed struct | Tracker use |
|--------|-----|--------------|-------------|
| `GAME_SMSG_QUEST_ADD` | `0x0049` | `StoC::QuestAdd` | Optional add signal; still refresh snapshot |
| `GAME_SMSG_QUEST_REMOVE` | `0x0052` | **None in-tree** | Prefer UI `kQuestRemoved` + snapshot |
| Description / marker / name updates | `0x004C`–`0x0054` | Mostly untyped | Prefer `kQuestDetailsChanged` + snapshot |

## Absence semantics

1. Not in quest log ⇒ not currently held — **not** “never taken” and **not** “completed.”
2. `kQuestRemoved` alone ⇒ presence loss confirmed; reason unknown.
3. `IsCompleted() == true` while still in log ⇒ in-log completion / ready-for-reward for that snapshot only.
4. Null `objectives` / `description` ⇒ fetch pending (`RequestQuestInfoId`), not empty quest.
5. During loading / no world context ⇒ snapshots invalid; wait for `kMapLoaded` and a controlled character.
6. Toolbox offline ⇒ no events; only a later snapshot. Remotions that happened offline cannot be proven.
7. Mission objectives clearing on map leave is expected and unrelated to quest-log completion.
8. Synthetic custom marker quest `0xfdd` is not real progress.

## Character / account identity

Preferred persistence identity:

```text
account_uuid  = GW::AccountMgr::GetAccountUuid()
character_uuid = CharContext::player_uuid[4]  // copy into owned GUID/bytes
```

Character name (`player_name` / `GetCurrentPlayerName()`) is **display metadata** and a **temporary fallback** only when UUID is unavailable. Do **not** automatically merge stores based only on matching names.

## Mission / bonus completion data

- Read the same GWCA bitsets `CompletionWindow::ParseCompletionBuffer` uses, via an **owned** adapter.
- Use `kMissionComplete`, `kDungeonComplete`, `kVanquishComplete` (and map-load) as **refresh triggers**, then re-read bitsets.
- Source tag for export: `mission_completion_data`.
- These bits describe **mission map / bonus** progress, not general quest-log turn-ins.

`CompletionWindow` remains documentation/reference only — no runtime coupling.

## Offline / character-switch reconstruction

| Situation | Allowed inference |
|-----------|-------------------|
| New quest appears vs last store | First observed active (confirmed presence) |
| Quest still present with same/changed flags | Update from snapshot |
| Quest missing vs last store, no abandon/reward evidence | Unknown outcome (uncertain); **not** completed |
| Character UUID changed | New store context; do not merge by name |
| Mission bit newly set | Confirmed mission/bonus for that map bit |

## Related docs

- [repository-investigation.md](repository-investigation.md)
- [proposed-architecture.md](proposed-architecture.md)
- [plans/quest-tracker-mvp.md](plans/quest-tracker-mvp.md)
