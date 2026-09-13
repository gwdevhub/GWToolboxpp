# AI review governance

This repository uses a bounded Cursor → ChatGPT senior-review loop. It is designed for reliable C++ and Quest Tracker changes without repeatedly paying for a whole-repository audit.

## Authority

| Role | May do | Must not do without explicit human instruction |
| --- | --- | --- |
| Human | approve scope and slices; decide priority, PR readiness, merge, release | — |
| Cursor | implement one approved slice; run relevant checks; prepare factual handoff; apply one accepted repair batch | widen scope; alter CI, dependencies, generated assets, PR state, merge, rebase, force-push, release, or deploy |
| ChatGPT reviewer | independent read-only review; return one bounded repair batch; issue a gate verdict | edit, commit, push, alter CI/policy, change PR lifecycle, merge, release, or silently advance a gate |

Existing issue/PR constraints remain stricter than this document. In particular, Quest Tracker evidence rules, observation semantics, persistence boundaries, and inter-repo Contract v1 constraints remain binding.

## Usage-efficient review rhythm

1. Cursor runs the narrowest relevant checks and produces a compact handoff.
2. The first substantial slice gets a full review.
3. Later repair rounds get a **delta review**: compare `previous-reviewed-sha..HEAD`, prove old findings are closed, and inspect direct call sites, state transitions, persistence effects, and tests only.
4. Cursor groups accepted repairs into one coherent batch and reruns affected checks.
5. One final full acceptance review occurs per PR immediately before the human PR decision.

Do not repeat a full audit after a small repair commit. Escalate only if a public contract, persisted format, identity binding, state transition, test harness/CI, dependency, or declared slice boundary changes.

## Required handoff

```md
# Review handoff: <issue / slice>
- Review type: <delta | final acceptance>
- Goal / in scope:
- Non-goals:
- Branch:
- Review base SHA:
- Previous reviewed SHA: <none | SHA>
- Current HEAD SHA:
- Changed paths:
- Behavior, transition, or persistence change:
- Checks actually run: <command → pass/fail/skipped>
- Known limitations / deferred work:
- Requested focus:
```

No raw logs unless a failure needs evidence.

## Reviewer prompt

```text
Read-only <delta|final acceptance> review for <issue/slice>.
Read AGENTS.md and docs/ai/REVIEW_GOVERNANCE.md first.
Base: <SHA>; previous reviewed: <SHA or none>; head: <SHA>.
In scope: <...>. Non-goals: <...>.
Do not edit, commit, push, alter CI/policy, change PR state, or merge.
For delta review, inspect only the stated delta, earlier findings, and direct consequences.
Report reproducible P0–P2 findings only: severity, path/symbol, trigger, consequence, evidence, smallest safe repair.
End with exactly one: PASS, REPAIR REQUIRED, or HUMAN DECISION REQUIRED.
```

P0 means security/data-loss/outage risk. P1 means likely normal-use incorrectness or broken contract/persistence behavior. P2 is a real edge defect worth fixing before a dependent slice. P3 is optional and omitted unless requested.
