# GWToolboxpp fork TaskSpec template

Use this as the Architect → Cursor contract. Choose the smallest form under `docs/ai/AI_SOFTWARE_FACTORY.md`. Identify the issue, target branch/base SHA or reviewed HEAD, TaskSpec reference/version, and approved slice when relevant.

## Small

```md
# TaskSpec — <title>
## Goal
## In scope
## Constraints
## Required validation
## Stop conditions
```

## Medium

```md
# TaskSpec — <title>
## Goal and business outcome
## In scope / Out of scope
## Current / Desired behavior
## Architecture and contract impact
## Required tests and validation
## Acceptance criteria
## Stop conditions
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
## 13. Acceptance criteria
## 14. Risks / failure modes
## 15. Stop conditions
```

Write observable acceptance criteria and name actual repository commands where known. Tests must prove behavior or contract, not restate private implementation. If a section does not apply, say why. In this repository account for observation versus inferred completion; character identity; state transitions; persistence/restart; producer capacity and validity; consumer contract drift; build compatibility when relevant. Validation may include the existing focused Quest Tracker/contract tests and applicable C++ build or CI checks named in the task; report when Windows/in-game validation is unavailable; choose exact checks based on the changed surface, and report unavailable required gates.

Cursor reads `AGENTS.md`, this TaskSpec, the relevant controlling sources and `docs/ai/REVIEW_GOVERNANCE.md`; verifies folder/branch/HEAD/clean state; works only on the approved slice; reports exact checks and HEAD; stops on a stop condition. Do not automatically start the next task.
