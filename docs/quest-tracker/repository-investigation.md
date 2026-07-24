# Repository investigation — quest tracker

Investigation of whether a safe, observation-only in-game quest tracker can be built in this GWToolboxpp fork. **No production code was modified for this document.**

## Verdict (repository view)

Existing Toolbox surfaces already observe the live quest log, active quest, objective strings, mission objectives, and mission/bonus bitsets. There is **no** durable quest-log history store and **no** game API for permanent turn-in history after a quest leaves the log. A tracker is feasible by adding isolated observation + persistence modules; see [data-availability.md](data-availability.md) and [proposed-architecture.md](proposed-architecture.md).

## Registration and lifecycle

### Base lifecycle

[`GWToolboxdll/ToolboxModule.h`](../../GWToolboxdll/ToolboxModule.h) — `ToolboxModule` exposes `Initialize`, `SignalTerminate`, `CanTerminate`, `Terminate`, `Update`, `Draw`, `LoadSettings`, `SaveSettings`, `DrawSettingsInternal`.

Optional enable path:

1. [`ToolboxSettings.cpp`](../../GWToolboxdll/Modules/ToolboxSettings.cpp) — `optional_modules` (~162–263) lists toggles including `QuestModule::Instance()`, `ActiveQuestWidget::Instance()`, `CompletionWindow::Instance()`.
2. `ToolboxSettings::LoadModules` → `GWToolbox::ToggleModule` (~293–295).
3. `GWToolbox::ToggleTBModule` — enable runs `Initialize` then `LoadSettings`; disable runs `SaveSettings` then `SignalTerminate`.

**Later production touch (not done here):** registering a new tracker window/module requires an entry in `optional_modules`. That is the expected minimal upstream edit.

### Build / source inclusion

[`GWToolboxdll/CMakeLists.txt`](../../GWToolboxdll/CMakeLists.txt) uses `file(GLOB … CONFIGURE_DEPENDS)` for `Modules/*`, `Widgets/*`, `Windows/*`. New Module/Window/Widget sources under those trees are picked up without editing the CMake source list. CMake changes would only be needed later for a **separate test target**, not for MVP module files.

Documented build ([`README.md`](../../README.md)):

```text
cmake --preset=vcpkg
cmake --build build --config RelWithDebInfo
```

CI ([`.github/workflows/cmake.yml`](../../.github/workflows/cmake.yml)) builds; there is no CTest/`add_test` step.

### Testing reality

- No general unit-test / CTest framework in this repo.
- [`TestHarness`](../../GWToolboxdll/Modules/TestHarness.h) (`_DEBUG` / `GWTB_HARNESS`) is an **in-game development harness** (file channel commands), not a unit-test runner.
- Pure reducer logic should be designed unit-testable; concrete integration is deferred (see MVP plan Phase 2).

## Existing quest-related surfaces

### `QuestModule`

| | |
|--|--|
| Paths | [`QuestModule.h`](../../GWToolboxdll/Modules/QuestModule.h), [`QuestModule.cpp`](../../GWToolboxdll/Modules/QuestModule.cpp) |
| Type | `ToolboxModule` singleton |
| Purpose | QoL for quest log pathing, custom world marker, optional keep-active-quest |

**Lifecycle (cpp):** `Initialize` registers UI hooks and quest-log row hook; `Update` handles map-load / missing quest info / path refresh; `SignalTerminate` removes hooks and clears paths; `CanTerminate` waits for custom marker clear; `Terminate` clears init flag; `LoadSettings` / `SaveSettings` for path/marker settings.

**Important APIs used:**

- `GW::QuestMgr::{GetQuestLog,GetQuest,GetActiveQuestId,SetActiveQuestId,RequestQuestInfo}`
- UI: `kQuestAdded`, `kQuestDetailsChanged`, `kClient/ServerActiveQuestChanged`, `kSendSetActiveQuest`, `kSendAbandonQuest`, `kMapLoaded`, `kStartMapLoad`
- `ParseQuestObjectives(QuestID)` — splits `Quest::objectives` on `0x2`; completed marker leading wchar `0x2af5`

**Synthetic quest:** anonymous `custom_quest_id = (QuestID)0x0000fdd` injected into the log for the custom marker. Any tracker **must filter** this id and ignore synthetic Add/Remove from marker set/clear.

**Not a progress tracker:** no quest history, no completion archive.

Note: GWCA also exports `GW::Module QuestModule` in [`QuestMgr.h`](../../Dependencies/GWCA/include/GWCA/Managers/QuestMgr.h) — unrelated to Toolbox’s class.

### `ActiveQuestWidget`

| | |
|--|--|
| Paths | [`ActiveQuestWidget.h`](../../GWToolboxdll/Widgets/ActiveQuestWidget.h), [`.cpp`](../../GWToolboxdll/Widgets/ActiveQuestWidget.cpp) |
| Type | `ToolboxWidget` — `"Active Quest Info"` |

**Initialize:** registers `kQuestDetailsChanged`, `kQuestAdded`, `kClientActiveQuestChanged`, `kObjectiveComplete`, `kObjectiveAdd`, `kObjectiveUpdated` → force refresh.

**Update display split:**

1. Normal quest — `GetQuest(qid)` + `QuestModule::ParseQuestObjectives`
2. Mission mode — `(int32_t)qid == -1` reads `WorldContext::mission_objectives` with `OBJECTIVE_FLAG_BULLET` / `OBJECTIVE_FLAG_COMPLETED`

Demonstrates that **quest-log objectives** and **mission objectives** are different channels (see data-availability).

### `CompletionWindow` (reference only)

| | |
|--|--|
| Paths | [`CompletionWindow.h`](../../GWToolboxdll/Windows/CompletionWindow.h), [`.cpp`](../../GWToolboxdll/Windows/CompletionWindow.cpp), [`CompletionWindow_Constants.h`](../../GWToolboxdll/Windows/CompletionWindow_Constants.h) |

Persists per-character mission/vanquish/skill/map/HoM-style progress in `character_completion.json` via `Resources::GetPath` (legacy `.ini` migration). Live refresh via `ParseCompletionBuffer` from `WorldContext` bit arrays (`missions_completed`, `missions_bonus`, HM variants, etc.) and UI messages such as `kMissionComplete`.

**For the quest tracker:** treat as a **reference implementation** of bitset parsing and JSON persistence patterns. Do **not** take a runtime dependency. Mission/bonus for the tracker must be read from GWCA bitsets (or an owned adapter).

Keyed today largely by character **name** plus an account string field — the tracker plan prefers `account_uuid + character_uuid` instead (see architecture).

### Related surfaces

| Symbol | Path | Relevance |
|--------|------|-----------|
| `DialogModule::QuestDialogType` | [`DialogModule.h`](../../GWToolboxdll/Modules/DialogModule.h) | `TAKE`, `REWARD`, `ENQUIRE_REWARD`, etc. for correlated turn-in/abandon evidence |
| `DailyQuests` | [`DailyQuestsWindow.*`](../../GWToolboxdll/Windows/DailyQuestsWindow.cpp) | Rotation calendars; incidental quest-log name checks |
| `InfoWindow` quest section | [`InfoWindow.cpp`](../../GWToolboxdll/Windows/InfoWindow.cpp) | Debug: active quest / missing info / `RequestQuestInfo` |
| World map / minimap | `WorldMapWidget`, `SymbolsRenderer` | Markers, set-active, colors via `QuestMgr` / `QuestModule` |
| Contract | [`docs/contracts/quest_progress_contract_v1.md`](../contracts/quest_progress_contract_v1.md) | Export schema, sources, confidence |

## GWCA surfaces (summary)

Full reliability analysis: [data-availability.md](data-availability.md).

| Area | Primary symbols / files |
|------|-------------------------|
| Quest snapshot | `GW::QuestMgr`, `GW::Quest`, [`Quest.h`](../../Dependencies/GWCA/include/GWCA/GameEntities/Quest.h), [`QuestMgr.h`](../../Dependencies/GWCA/include/GWCA/Managers/QuestMgr.h) |
| World context | `quest_log`, `active_quest_id`, `mission_objectives`, `missions_*` in [`WorldContext.h`](../../Dependencies/GWCA/include/GWCA/Context/WorldContext.h) |
| UI messages | [`UIMessages.h`](../../Dependencies/GWCA/include/GWCA/Constants/UIMessages.h) — `kQuest*`, `kObjective*`, `kMissionComplete`, `kSendAbandonQuest` |
| StoC | `GAME_SMSG_QUEST_ADD` / typed `QuestAdd`; `GAME_SMSG_QUEST_REMOVE` opcode without typed struct in-tree |
| Identity | `CharContext::player_uuid[4]`, `player_name` ([`CharContext.h`](../../Dependencies/GWCA/include/GWCA/Context/CharContext.h)); `GW::AccountMgr::GetAccountUuid` / `GetCurrentPlayerName` ([`ToolboxUtils.h`](../../GWToolboxdll/Utils/ToolboxUtils.h)) |

## Persistence patterns to reuse (patterns only)

| Pattern | Where | Tracker use |
|---------|-------|-------------|
| Module settings JSON | `SettingsDoc` / `modules/` under computer config folder | Window/module toggles and UI prefs |
| Dedicated glaze JSON at `Resources::GetPath` | e.g. `character_completion.json` | Owned `quest_progress.json` (or similar), **not** shared with Completion |
| Account UUID files | `GuildWarsSettingsModule`, `AccountInventoryWindow` | Prefer UUID-based keys over name |

## Gaps relative to a full tracker

| Need | Present today? |
|------|----------------|
| Live quest list + objectives | Yes (QuestModule / ActiveQuestWidget / InfoWindow) |
| In-log `IsCompleted()` observation | Available via GWCA; not persisted as quest history |
| Permanent turn-in history | No game API; no Toolbox quest history store |
| Confidence / disappearance semantics | Not modeled in production UI |
| Character UUID–keyed quest progress | Not for quests (Completion uses name-centric map) |
| Codex export | Contract exists; no producer yet |

## Upstream-safety notes

- Prefer **new** Module / Window / helper files under existing globs.
- Justify any edit to shared files; expected minimum later is `ToolboxSettings.cpp` registration only.
- Do not update dependencies without approval.
- Do not commit generated build output.

## Related docs

- [data-availability.md](data-availability.md)
- [proposed-architecture.md](proposed-architecture.md)
- [plans/quest-tracker-mvp.md](plans/quest-tracker-mvp.md)
