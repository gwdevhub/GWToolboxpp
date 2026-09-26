# State 0 — GWToolboxpp fork

**Status:** provisional inventory, updated 2026-09-26. This is planning context, not an approved TaskSpec or release verdict. Verify the relevant rows against the current branch and live build before implementation. [AI_OPERATING_CARD.md](AI_OPERATING_CARD.md) routes to the controlling governance.

## 1. Product one-pager

- **What it is:** a GWToolboxpp fork that includes an observation-only Quest Tracker and Contract v1 export for Tyrian Wayfarer, alongside upstream Toolbox functionality.
- **Who it serves:** Guild Wars players who want local, character-bound progress observations and a manual transfer into Wayfarer.
- **What it does not claim:** automatic permanent completion from a quest disappearing, progress synchronization, or a verified public beta based on tests alone.
- **Fork boundary:** [PR #10](https://github.com/vinogitz/GWToolboxpp/pull/10) merged the Quest Tracker onto the fork's `master` based on upstream `8.34_Beta2`; it did not target upstream `gwdevhub/GWToolboxpp`.

## 2. Evidence-based inventory

Here “reported” means the cited PR/document states a result; it is not a fresh run or in-game verification by this document.

| Area | State and evidence | Remaining check |
| --- | --- | --- |
| C++/CMake fork build | PR #10 reports a RelWithDebInfo DLL build on `8.34_Beta2` | Recheck on the chosen release commit and local target environment |
| Quest Tracker producer | PR #10 includes observation, store 1.2, journey baseline ingest and Contract v1 export | In-game observation and export smoke |
| Automated tests | PR #10 reports `QuestProgressTests` 1198 passed, 0 failed | Run the relevant suite on the target commit |
| Toolbox → Wayfarer bridge | [Public beta gate](../quest-tracker/verification/public-beta-gate.md) documents the export/import path | Manual Pre-Searing export → import → Life journey remains unchecked there |
| Fork updater behavior | PR #10 says the official DLL must not silently replace this fork build | Manual updater check remains unchecked in PR #10 |
| CI | Repository has workflows; this inventory has no current workflow-run verdict | Inspect checks for the chosen commit |
| Governance | [AI factory](AI_SOFTWARE_FACTORY.md) merged to `master` | Use the approved TaskSpec and review process for the next task |

The beta checklist is dated 2026-09-09 and mentions an older build label. Reconcile its release version and evidence with PR #10 before treating it as a current release gate.

## 3. Architecture and ownership map

| Concern | Controlling area |
| --- | --- |
| Game observations and lifecycle | `GWToolboxdll/Modules/QuestObservationService.*`, `QuestProgressLive.*`, `QuestTrackerWindow.*` |
| Per-character state, transitions and persistence | `GWToolboxdll/Modules/QuestProgressService.*`, `QuestJourneyBaselineTransition.*`, `QuestProgressStore.*`, `QuestProgressJsonCodec.*` |
| Contract v1 producer | `QuestProgressContractExporter.*` and `docs/contracts/quest_progress_contract_v1.md` |
| Consumer behavior | Tyrian Wayfarer repository; changes to the shared contract require coordination there |
| Tests | `tests/QuestProgressTests/` |

These paths are a navigation map, not a substitute for inspecting current code. Existing `AGENTS.md`, scoped Cursor rules, contracts and issue constraints take precedence. Never turn absence from the quest log into confirmed completion.

## 4. Candidate next steps

1. **Read-only release evidence audit:** choose an exact `master` SHA; check build, tests, CI and whether the old beta checklist still describes that build.
2. **Manual in-game bridge smoke:** on the chosen fork build, verify Pre-Searing export → Wayfarer import → Life journey and the updater behavior. Record versions, paths and observed results.
3. **Select one bounded change if a real gap remains:** the Architect writes a Small/Medium/Large TaskSpec with tests and stop conditions. Do not infer a new feature requirement from this candidate list.

Human product priority, PR readiness and release decisions remain human. See [REVIEW_GOVERNANCE.md](REVIEW_GOVERNANCE.md) for implementation handoff and independent review.
