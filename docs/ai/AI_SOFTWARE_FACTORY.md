# GWToolboxpp fork AI Software Factory

## Purpose and authority

This contract defines the Human → Architect → Cursor → deterministic gates → independent Reviewer → Human workflow. It adds process to existing project rules; it does not override issue/PR constraints, accepted architecture, or repository-specific Cursor rules.

| Actor | Responsibility |
| --- | --- |
| Human | Product goal, priority, scope approval, residual risk, PR readiness, merge, release and deployment decisions |
| GPT Architect | Interpret the approved goal, inspect controlling sources, choose task size, plan boundaries, tests and stop conditions, write the TaskSpec |
| Cursor | Implement only the approved TaskSpec/slice, run required checks, record exact evidence and hand off |
| Deterministic gates | Report machine-verifiable PASS/FAIL evidence; absent or skipped required checks are not passes |
| GPT Reviewer | Independently compare original requirement, TaskSpec, diff and evidence; return PASS, REPAIR REQUIRED or HUMAN DECISION REQUIRED |
| GitHub/CI | Execute configured checks and PR lifecycle; it does not grant product authority |

An agent cannot approve its own implementation as independent review. No agent silently advances a human authority gate. Cursor does not merge, release or deploy without explicit human direction.

## Task sizing

- **Small:** isolated wording, UI label or narrow low-risk correction without a contract change. TaskSpec: goal, in scope, constraints, validation, stop conditions.
- **Medium:** bounded behavior within an existing ownership boundary. Include current/desired behavior, out of scope, architecture impact, tests, acceptance criteria and stop conditions.
- **Large / architecture-sensitive:** persistence, migration, identity, canon/provenance, public or cross-repository contract, dependency, CI/build, architecture boundary or deployment. Use the full TaskSpec.

Choose the smallest shape that resolves material ambiguity. Size never waives mandatory repository gates.

## Handoffs

1. Human states what should change and why, or points to an approved issue.
2. Architect reads `AGENTS.md`, `docs/ai/ARCHITECT_GOVERNANCE.md`, `docs/ai/TASK_SPEC_TEMPLATE.md` and controlling sources. It produces a bounded TaskSpec for human scope approval.
3. Cursor checks folder, branch, HEAD and working-tree state; implements only the approved slice; runs exact applicable checks. An unexpected dirty worktree, wrong branch or base is reported before implementation.
4. Cursor supplies the factual handoff in `docs/ai/REVIEW_GOVERNANCE.md`.
5. Reviewer performs independent scoped review; the human decides the PR lifecycle.

A TaskSpec may live in a versioned `docs/tasks/` file, an issue, or the implementation request. Give both Cursor and Reviewer the same approved contract and identify its version/reference. Do not require a permanent file for every small task.

## Project boundaries

Existing C++/Windows and Quest Tracker rules remain binding: Contract v1 producer observation only, no inferred completion from quest disappearance, identity binding, lifecycle and persistence invariants, and byte/semantic compatibility with Tyrian Wayfarer. Upstream repository instructions in AGENTS.md still apply.

## Stop and escalation

Stop and state the blocking fact, affected decision, and smallest decision needed when implementation would require unapproved scope, architecture, dependency/toolchain, persistence/schema, identity, security, CI/deployment or public/cross-repository contract changes; an accepted rule conflicts; required validation cannot run; or the work expands beyond the approved slice. Report unavailable evidence honestly. Do not treat missing tests or a green unrelated check as a pass.

Read only the controlling task, nearest code/tests and relevant rules/contracts initially. The repository and accepted documents outrank chat assumptions.
