---
description: Establish repository state and the approved task before implementation.
---

Read `AGENTS.md`, `docs/ai/AI_SOFTWARE_FACTORY.md`, `docs/ai/WORKSPACE_WORKFLOW.md`, and `docs/ai/HUMAN_LANGUAGE_POLICY.md`.

For `$ARGUMENTS`, begin with read-only probes and the workspace status table. Identify whether this is read-only planning, approved implementation, repair, or review. Read the relevant TaskSpec and scoped rules; report target repo, verified base/HEAD, scope, peer impact, checks and next action in Hungarian.

Do not create or switch a branch for a read-only task. For approved implementation, announce the exact branch/base before creating it; use the same branch for repairs. Preserve unexpected local work and report material blockers. Follow existing user authorization; this command itself grants no implementation, push, PR or release permission.
