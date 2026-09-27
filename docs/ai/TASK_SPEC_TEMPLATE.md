# GWToolboxpp fork TaskSpec template

Use this as the Architect → Cursor contract. Choose the smallest form under `docs/ai/AI_SOFTWARE_FACTORY.md`. Identify the issue, target branch/base SHA or reviewed HEAD, TaskSpec reference/version, and approved slice when relevant.

## Small

```md
# TaskSpec — <title>
## Goal
## In scope
## Constraints
## Required validation
Prefer cheap falsification. Name preflight before any heavyweight command.
State whether expensive validation is mandatory for acceptance.
## Stop conditions
Include: do not run heavyweight validation if preflight proves it cannot succeed.
```

## Medium

```md
# TaskSpec — <title>
## Goal and business outcome
## In scope / Out of scope
## Current / Desired behavior
## Architecture and contract impact
## Required tests and validation
- Preflight:
- Narrow validation:
- Heavyweight validation:
- Known environment blockers:
- Expensive validation mandatory for acceptance: <yes | no>
## Acceptance criteria
## Stop conditions
Do not run heavyweight validation if preflight proves it cannot succeed.
```

## Large / architecture-sensitive

```md
# TaskSpec — <title>
## 1. Goal
## 2. Business outcome
## 3. In scope
## 4. Out of scope
## 5. Current behavior
## 6. Desired behavior
## 7. Architecture impact
## 8. Source of truth / ownership
## 9. Implementation constraints
## 10. Implementation slices
## 11. Required tests
## 12. Required validation
Follow Heavy Validation Economy: cheap falsification before heavyweight validation.
### 12a. Preflight (when expensive validation exists)
### 12b. Heavyweight validation
### 12c. Known environment blockers
### 12d. Expensive validation mandatory?
## 13. Acceptance criteria
## 14. Risks / failure modes
## 15. Stop conditions
Include: do not run heavyweight validation if preflight proves it cannot succeed.
```

Write observable acceptance criteria and name actual repository commands where known. Tests must prove behavior or contract, not restate private implementation. If a section does not apply, say why. In this repository account for observation versus inferred completion; character identity; state transitions; persistence/restart; producer capacity and validity; consumer contract drift; build compatibility when relevant. Validation may include the existing focused Quest Tracker/contract tests and applicable C++ build or CI checks named in the task; report when Windows/in-game validation is unavailable; choose exact checks based on the changed surface, and report unavailable required gates.

Prefer **cheap falsification** and **preflight** before **heavyweight validation**; see Heavy Validation Economy in `docs/ai/AI_SOFTWARE_FACTORY.md`. When expensive validation exists, specify preflight, known blockers, STOP wording, and whether it is mandatory for acceptance. Recommended STOP wording: `Do not run heavyweight validation if preflight proves it cannot succeed.`

Cursor reads `AGENTS.md`, this TaskSpec, the relevant controlling sources and `docs/ai/REVIEW_GOVERNANCE.md`; verifies folder/branch/HEAD/clean state; works only on the approved slice; runs required validation under Heavy Validation Economy; reports exact checks and HEAD; stops on a stop condition, including when preflight or a known deterministic blocker proves mandatory heavyweight validation cannot succeed (**acceptance BLOCKED**; do not claim PASS); includes heavy-validation skip fields in the handoff when applicable. Do not automatically start the next task.
