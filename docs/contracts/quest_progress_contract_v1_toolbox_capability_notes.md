# Quest Progress Contract v1 — Toolbox capability notes

**Status:** Non-normative (GWToolboxpp Phase B3 documentation only)  
**Does not alter:** [`quest_progress_contract_v1.md`](quest_progress_contract_v1.md) or the example JSON.

GuildWarsCodex is the Contract **consumer**. GWToolboxpp is the intended **producer**.
This note records what this repository can currently observe versus what Contract v1
can represent. Contract support alone does **not** make a capability confirmed.

**Phase B3 scope:** documentation synchronization only. No Contract exporter, history
store, or new production observation work was started by this phase.

Labels:

| Label | Meaning |
|-------|---------|
| confirmed observable | Concrete symbols/files already present; live evidence available when Toolbox is running |
| probably observable | Evidence path exists but needs runtime correlation / validation |
| uncertain | Partial signals; outcome cannot be asserted honestly |
| not currently observable | No game or Toolbox API currently yields this |
| manual-only | Requires explicit user input |
| not yet investigated | Not audited in sufficient depth for a stronger claim |

---

## Capability matrix

| Contract concept | Status | Evidence / notes |
|------------------|--------|------------------|
| Character display name | confirmed observable | `GW::AccountMgr::GetCurrentPlayerName()` in [`ToolboxUtils.h`](../../GWToolboxdll/Utils/ToolboxUtils.h) / [`ToolboxUtils.cpp`](../../GWToolboxdll/Utils/ToolboxUtils.cpp); `CharContext::player_name` in [`CharContext.h`](../../Dependencies/GWCA/include/GWCA/Context/CharContext.h). Display metadata only. |
| Stable external character key | confirmed observable | Raw session identity: `CharContext::player_uuid[4]` ([`CharContext.h`](../../Dependencies/GWCA/include/GWCA/Context/CharContext.h)); account side via `GW::AccountMgr::GetAccountUuid()` ([`ToolboxUtils.h`](../../GWToolboxdll/Utils/ToolboxUtils.h)). Contract `characterKey` serialization/export is **not implemented**. |
| Pre-Searing status | confirmed observable | `GW::Map::IsPreSearing` / map-id overload in [`ToolboxUtils.cpp`](../../GWToolboxdll/Utils/ToolboxUtils.cpp) (`Map` helpers); used by [`CompletionWindow.cpp`](../../GWToolboxdll/Windows/CompletionWindow.cpp) (`is_pre_searing`). Current-map / character-map based, not a separate quest API. |
| Quest log entries | confirmed observable | `GW::QuestMgr::GetQuestLog()` ([`QuestMgr.h`](../../Dependencies/GWCA/include/GWCA/Managers/QuestMgr.h)); live copy-out in [`QuestObservationService`](../../GWToolboxdll/Modules/QuestObservationService.h). Filter synthetic custom marker `0xfdd` ([`QuestModule.cpp`](../../GWToolboxdll/Modules/QuestModule.cpp)). |
| Numeric game quest ID | confirmed observable | `GW::Quest::quest_id` ([`Quest.h`](../../Dependencies/GWCA/include/GWCA/GameEntities/Quest.h)); mirrored as `OwnedQuestEntry::quest_id`. |
| Quest display name | confirmed observable | Encoded `GW::Quest::name`; decoded via `GuiUtils::EncString` patterns ([`EncString.h`](../../GWToolboxdll/Utils/EncString.h)); Phase 1 holds `name_encoded` in `OwnedQuestEntry`. Absent/null name ⇒ fetch pending, not empty title. |
| Current active quest | confirmed observable | `GW::QuestMgr::GetActiveQuestId()`; UI `kClientActiveQuestChanged` / `kServerActiveQuestChanged` ([`UIMessages.h`](../../Dependencies/GWCA/include/GWCA/Constants/UIMessages.h)); used by [`ActiveQuestWidget.cpp`](../../GWToolboxdll/Widgets/ActiveQuestWidget.cpp) and `QuestObservationService`. Mission mode when `(int32_t)id == -1`. |
| Objective text | confirmed observable | Quest objectives from `GW::Quest::objectives` after parse (`0x2` split) in [`QuestModule::ParseQuestObjectives`](../../GWToolboxdll/Modules/QuestModule.cpp) / owned Phase 1 parse; mission text from `WorldContext::mission_objectives` / `MissionObjective::enc_str`. Null objectives ⇒ `RequestQuestInfoId` pending, not “no objectives.” |
| Objective completion flag | confirmed observable | Quest: leading `0x2af5` marker in objective segments ([`QuestModule.cpp`](../../GWToolboxdll/Modules/QuestModule.cpp)); mission: `MissionObjective::type` completed bit (see [`ActiveQuestWidget.cpp`](../../GWToolboxdll/Widgets/ActiveQuestWidget.cpp)). Confirmed only for the current snapshot. |
| Quest-added observation | confirmed observable | UI `kQuestAdded` then fresh `GetQuestLog()` snapshot ([`UIMessages.h`](../../Dependencies/GWCA/include/GWCA/Constants/UIMessages.h); [`QuestModule.cpp`](../../GWToolboxdll/Modules/QuestModule.cpp); `QuestObservationService`). Confirms presence, not later completion. |
| Quest-removed observation | confirmed observable | UI `kQuestRemoved` then fresh log snapshot. Confirms **presence loss only**; outcome remains unknown unless other evidence exists. |
| Abandon observation | probably observable | Outbound UI `kSendAbandonQuest` ([`UIMessages.h`](../../Dependencies/GWCA/include/GWCA/Constants/UIMessages.h)) paired with subsequent removal ([`QuestModule.cpp`](../../GWToolboxdll/Modules/QuestModule.cpp) listens). Correlation not promoted to confirmed completion/abandon in Contract export yet (no exporter). |
| Ready-for-reward observation | confirmed observable | In-log `GW::Quest::IsCompleted()` / `log_state & 0x2` while entry remains in log ([`Quest.h`](../../Dependencies/GWCA/include/GWCA/GameEntities/Quest.h); `OwnedQuestEntry::in_log_completed`). Means ready-while-present — **not** permanent turn-in history. |
| Confirmed reward / completion observation | uncertain | Dialog types `QuestDialogType::REWARD` / `ENQUIRE_REWARD` in [`DialogModule.h`](../../GWToolboxdll/Modules/DialogModule.h) may correlate with removal. Treat as **probable** pending runtime validation. Disappearance alone MUST NOT become `completed_observed` + `confirmed`. No durable completion API after the quest leaves the log. |
| Mission completion data | confirmed observable | `WorldContext::missions_completed`, `missions_bonus`, HM variants ([`WorldContext.h`](../../Dependencies/GWCA/include/GWCA/Context/WorldContext.h)); parsed by [`CompletionWindow::ParseCompletionBuffer`](../../GWToolboxdll/Windows/CompletionWindow.cpp) (reference pattern only). Maps to Contract source `mission_completion_data` for **mission/bonus map bits**, not general quest-log turn-ins. Tracker must use an owned read, not CompletionWindow runtime coupling. |
| Previous Toolbox-local progress | not currently observable | No quest-progress history JSON / `QuestProgressStore` yet. Phase 1 is live snapshot only. `CompletionWindow` stores mission/vanquish-style data (`character_completion.json`), not Contract quest history. |
| Historical reconstruction while Toolbox was not running | not currently observable | No client API for remotions offline. Later snapshot diffs vs a future local store remain **uncertain** / unknown outcome — never auto-complete. |

---

## Cross-cutting honesty rules (producer)

These mirror Contract + Cursor rules; they do not extend the shared schema:

- Observational only — Contract export must not authorize gameplay automation.
- Quest disappearance is not completion.
- Emit only sources/confidence justified by evidence (`game_snapshot`, `game_event`, `mission_completion_data`, `toolbox_existing_data`, `manual_user_input`, `imported_history`).
- `producer.version` is metadata and MUST NOT participate in event identity / fingerprints.
- Exporter (when approved later) MUST follow mirrored Contract identity + SHA-256 canonical fingerprint rules; MUST NOT use runtime `std::hash` for portable fingerprints.
- History is append-only once persistence exists.

---

## Related docs

- Normative Contract: [`quest_progress_contract_v1.md`](quest_progress_contract_v1.md)
- Example: [`examples/quest_progress_contract_v1.example.json`](examples/quest_progress_contract_v1.example.json)
- Observation matrix: [`../quest-tracker/data-availability.md`](../quest-tracker/data-availability.md)
- Investigation: [`../quest-tracker/repository-investigation.md`](../quest-tracker/repository-investigation.md)
