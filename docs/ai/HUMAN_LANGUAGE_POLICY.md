# Human-facing language policy

This file is the single source for output language. Agent and architecture documentation remains English; do not create full parallel Hungarian copies of technical documentation.

- User-facing handoffs, review summaries, release summaries, implementation status reports, and other Product Owner-facing outputs must be written in Hungarian by default.
- Pull request bodies must contain a full Hungarian version first and a full English version after it, covering the same scope, validation, risks, limitations, and requested human decision.
- Pull request titles should be Hungarian by default unless an existing repository convention explicitly requires English.
- TaskSpecs, architecture contracts, ADRs, source code, code comments, commit messages, machine-readable evidence, error codes, gate IDs, schema field names, API identifiers, and CI output may remain English.
- Preserve exact technical identifiers, commands, SHA values, paths, issue/PR numbers, schema names, field names, gate IDs, error codes, and protocol terms in their original form.
- When preparing a handoff for ChatGPT or the Product Owner, write explanatory prose in Hungarian while keeping raw technical evidence unchanged. Existing template labels and verdict tokens such as PASS may remain exact.
- If a document is primarily written for humans rather than tools/agents, prefer Hungarian unless there is a repository-specific reason to keep it English.
- Historical PRs and reports are not retroactively rewritten merely to translate them. New and materially updated human-facing output follows this policy.

## Repository-specific rule

- C++ symbols, export schema fields, observation/contract terminology, build output, compiler diagnostics, and source-level technical identifiers remain English.
- Human-facing implementation/review handoffs and PR descriptions must be Hungarian, with the full English PR body following the Hungarian body.
