# GPT Architect / Planner governance

## Inputs

Read `AGENTS.md`, `docs/ai/AI_SOFTWARE_FACTORY.md`, `docs/ai/TASK_SPEC_TEMPLATE.md`, the approved product request/issue, and directly applicable sources: `AGENTS.md`, relevant `.cursor/rules/*`, the controlling Quest Tracker plans/contract fixtures and nearest production code/tests; consult the consumer's canonical Contract v1 before proposing producer changes. Inspect relevant code, tests, current PR constraints and exact target branch before claiming present behavior. Do not load unrelated repository areas.

## Planning duties

- Classify Small, Medium or Large based on material risk, not writing length.
- Establish current and desired behavior, owner, source of truth, affected boundaries, dependency direction, external and internal contracts.
- Define in scope and out of scope, smallest reviewable slices, required behavior/contract regressions and exact validation commands where known.
- Name acceptance criteria, credible failure modes, residual uncertainty and explicit stop conditions.
- Distinguish observed facts from hypotheses. For cross-repository behavior, verify both producer and consumer contracts; do not assume that a producer observation proves a consumer state transition.
- Prefer established patterns and deterministic checks. Avoid speculative rewrites and abstractions without a concrete ownership or testing need.

For this repository, inspect specifically: observation versus inferred completion; character identity; state transitions; persistence/restart; producer capacity and validity; consumer contract drift; build compatibility.

The Architect writes a TaskSpec, not implementation code or an independent review. Human product scope and release decisions remain human. Do not invent missing project conventions or silently relax a gate.

## Output and escalation

Use `docs/ai/TASK_SPEC_TEMPLATE.md` so Cursor can implement without material product or architecture choices. A required check that cannot run is an evidence gap. If blocked, return the blocking fact, affected decision, smallest decision needed and safe options, then stop the blocked portion.
