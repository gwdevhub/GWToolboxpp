# Workspace and branch workflow

This is the shared operational workflow for the two repositories. Roles and authority remain in [AI_SOFTWARE_FACTORY.md](AI_SOFTWARE_FACTORY.md); language is controlled by [HUMAN_LANGUAGE_POLICY.md](HUMAN_LANGUAGE_POLICY.md).

## Establish context before changing anything

A workspace can contain both repositories. Each repository has its own Git state, rules, approved task and review evidence. Never infer a repository or active branch from the workspace title or the previous chat turn.

Report this compact table in Hungarian at task start, before any branch switch/creation, and at handoff:

| Repo / absolute folder | Branch / HEAD | Upstream | Ahead / behind | Working tree | Task / stage |
| --- | --- | --- | --- | --- | --- |

Read only the relevant repository's AGENTS.md, governance and scoped rules. For shared Contract work, also read the peer contract and report both pinned SHAs. Do not load both complete codebases for an unrelated single-repository task.

Use read-only Git probes first: git status --short --branch, git rev-parse HEAD, git branch -vv, git remote -v, git worktree list. An authorized fetch refreshes remote evidence; record whether ahead/behind is fresh or based on cached refs. Do not expose credentials embedded in remote URLs.

## Branch decisions

- Read-only inventory or planning uses the existing branch and creates no branch.
- Before implementation, name the target repository, base ref/SHA, work branch, scope and intended PR base. State why a new branch is needed.
- One approved slice uses one work branch and one PR per affected repository. Repairs continue on the same branch. Use the existing branch when it already contains the approved task; do not create a branch for each review round.
- A dual-repository task may use the same branch name in both repositories, but its commits and PRs remain separate. Report dependency and merge order when relevant.
- GWToolboxpp fork work targets vinogitz/GWToolboxpp master unless the task explicitly says otherwise. Never retarget it to gwdevhub/GWToolboxpp.
- Wayfarer work targets vinogitz/guildwarscodex main unless the task explicitly says otherwise.
- Do not silently switch branches, stash, reset, clean files, delete branches or worktrees, rebase, or force-push to repair unexpected state. Describe the observed state and preserve user work. Resolve routine safe choices within existing authorization; ask only for a material missing decision.
- When a clean task branch is authorized, creating it from the verified base is routine. When isolating work, prefer a named worktree over moving unrelated changes between branches, if the local repository supports it.
- After a merge, report the merge SHA and local/remote status. Advance the local base using --ff-only only when authorized and clean. Inventory obsolete branches; deletion requires explicit scope and proof that they contain no unique work.

## Task stages and evidence

Use plain stages: planning → implementing → awaiting review → repairing → awaiting human merge → merged. Release readiness is a separate decision.

At handoff identify the approved TaskSpec, exact base and HEAD, changed paths, checks actually run, unresolved findings, next owner and next action. State whether another repository changed. A green test result is tied to its exact commit and environment, not carried forward as a fresh run.

An implementer may self-check, but cannot issue its own independent acceptance review. If the same assistant implements the requested change, hand it to an independent reviewer; do not label self-checks an independent PASS.

## Repository cleanup

Cleanup starts with inventory, not deletion. Classify each candidate as active work, merged history, duplicate routing, stale factual claim, generated output, or unclassified. Link retained historical plans from current entry points rather than silently treating them as current instructions. Preserve source/contract/history and user data.

Make documentation/routing consolidation a separate slice from runtime changes. A stale status claim is corrected only with current evidence; otherwise mark it verification pending. Never use a tidy docs tree as proof of build, gameplay, import, backup, or release correctness.

## Returning to beta work

Use [GOVERNANCE_READINESS.md](GOVERNANCE_READINESS.md) to record the operational checkpoint and outstanding local cleanup. Then reconcile existing beta evidence against exact producer and consumer commits. Keep code acceptance, live observation verification, and release approval distinct. Choose one approved beta gap; do not start an account-skill or other feature automatically.
