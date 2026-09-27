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

Follow **Heavy Validation Economy**: cheap falsification and preflight before heavyweight validation; never treat **NOT RUN due to known blocker** as PASS.

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

## Heavy Validation Economy

This section is the normative source for validation cost control. Architect planning, TaskSpecs, Cursor implementation, and review handoffs must follow it. Short references elsewhere must not redefine or weaken these rules.

### Core principle

Before starting an expensive validation command, first prove that the known prerequisites for that command are available.

If a **known deterministic blocker** is already known, do not execute the expensive command merely to reproduce an expected failure.

Prefer **cheap falsification** first:

```text
cheap falsification
  → preflight
  → narrow validation
  → heavyweight validation only when justified
```

Goal: avoid wasted execution time, not lower the gate.

### Expensive / heavyweight validation

Treat a command as expensive (**heavyweight validation**) when one or more apply:

- expected runtime is greater than roughly 5 minutes;
- full MSVC/CMake solution or RelWithDebInfo Toolbox DLL rebuild;
- Linux clang/xwin cross build (`scripts/build-xwin.sh`) when not the cheapest falsifier;
- full test suite when a targeted Quest Tracker / contract / pure-logic subset can falsify the change;
- in-game verification sessions that require a live Guild Wars client;
- large full-repository quality / release gate;
- network-dependent packaging or upload verification;
- any command already observed to regularly take a long time in this repo.

The 5-minute threshold is guidance, not a loophole: a known shorter deterministic failure should still be skipped.

### Preflight

Before **heavyweight validation**, check relevant cheap prerequisites first (**preflight**). Examples:

- required executable / toolchain exists (MSVC, CMake, clang/xwin toolchain when that path is required);
- required tool version / SDK is available;
- Guild Wars client / in-game session is available when the TaskSpec requires live verification;
- expected config/file exists;
- repository/worktree state is compatible with the TaskSpec;
- known environment limitation is not active;
- earlier required narrow gate is green (targeted pure-logic / contract tests as applicable);
- required generated or vendored inputs exist when the change depends on them;
- credentials/config are present when the task explicitly requires them.

Preflight itself must be cheap and deterministic where possible.

### Narrow-first validation order

Preferred order:

1. syntax / static checks (compile of the touched translation unit / narrow target when practical)
2. targeted unit / pure-logic tests
3. targeted parser / domain / Contract v1 tests
4. prerequisite / tool / environment preflight
5. narrow integration validation (persistence parse/load of fixture history, focused module link)
6. heavyweight MSVC/xwin build / in-game verification
7. full quality / release gate

Do not automatically execute every layer after every edit.

Move to the next layer only when:

- the previous cheaper layer passed;
- the next layer can add meaningful evidence;
- its prerequisites are satisfied.

### Failure-fast

If an inexpensive check already proves the current implementation cannot pass acceptance, stop escalating to more expensive checks.

Examples:

- syntax / compile failure of the changed surface → do not start a full RelWithDebInfo solution rebuild or in-game session;
- missing MSVC/CMake or xwin toolchain → do not launch the corresponding full build;
- known dirty-worktree blocker when the TaskSpec requires a clean tree → do not wait for a full gate expected to fail there;
- known storage-quota or upload blocker → do not repeatedly rerun packaging/upload verification unless storage state changed;
- no live Guild Wars client when the TaskSpec requires in-game evidence → mark in-game validation **NOT RUN due to known blocker** rather than spinning on a doomed session setup.

### Known deterministic blocker

If the same environment-dependent failure has already been characterized in the current task/session, do not reproduce it again unless one of these is true:

1. the TaskSpec explicitly requires regression evidence for that exact failure;
2. the environment has materially changed;
3. the previous characterization is no longer sufficient to determine whether the command can succeed.

Otherwise:

- record the **known deterministic blocker**;
- mark the expensive validation as **NOT RUN due to known blocker**;
- continue with the strongest cheaper validation that is still meaningful;
- STOP if the blocked validation is mandatory for acceptance.

Do not convert **NOT RUN due to known blocker** into PASS.

### Mandatory validation protection

Heavy Validation Economy must not weaken quality requirements.

If a mandatory acceptance check cannot run because of environment limitations:

- report it explicitly;
- state that acceptance remains **acceptance BLOCKED**;
- do not claim PASS;
- do not replace it with a weaker test and call it equivalent.

Compilation alone still does not prove game-state behavior; absence of required in-game evidence remains an evidence gap when the TaskSpec makes that evidence mandatory.

### Reproduction budget

For a previously characterized environment failure:

- default maximum reproduction: one successful characterization per unchanged environment state;
- further identical reruns require a reason;
- record the prior evidence reference in the handoff when practical.

### Retry policy

No automatic repeated retries for deterministic failures.

Retry is appropriate only when failure is plausibly transient, for example:

- network timeout;
- temporary service unavailability;
- process startup race explicitly known to be transient.

Do not retry:

- missing executable;
- wrong pinned version;
- invalid config;
- schema failure;
- syntax / compile failure;
- deterministic parser failure;
- known storage quota exhaustion;
- known unsupported environment;
- missing live game client when in-game validation is required.

### Cursor / implementer long-running commands

Never start or keep waiting on a long-running command when the task already contains sufficient deterministic evidence that the command cannot succeed in the current environment.

If a long-running process is already active and a newly observed condition makes success impossible:

- terminate it when safely possible;
- record the reason;
- continue with cheaper useful checks or STOP.

Do not wait for timeout just to obtain an expected failure message.

### Handoff fields when heavyweight validation is skipped

When heavyweight validation is skipped or blocked, the implementation handoff must include:

```md
- Heavy validation attempted: <yes | no>
- Heavy validation status: <ran | NOT RUN due to known blocker | failed | …>
- Preflight result:
- Known blocker:
- Previous evidence reference:
- Acceptance impact: <none | acceptance BLOCKED | …>
- Rerun condition:
```

Example:

```md
- Heavy validation attempted: no
- Heavy validation status: NOT RUN due to known blocker
- Preflight result: no live Guild Wars client
- Known blocker: TaskSpec requires in-game observation evidence
- Previous evidence reference: <session note / prior log path>
- Acceptance impact: acceptance BLOCKED
- Rerun condition: Guild Wars client running with Toolbox injected and Quest Tracker window available
```

Read only the controlling task, nearest code/tests and relevant rules/contracts initially. The repository and accepted documents outrank chat assumptions.
