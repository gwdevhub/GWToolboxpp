# AI operating card — GWToolboxpp fork

Use this as a short entry point. Authority and the full workflow live in [AI_SOFTWARE_FACTORY.md](AI_SOFTWARE_FACTORY.md), planning in [ARCHITECT_GOVERNANCE.md](ARCHITECT_GOVERNANCE.md), TaskSpec shape in [TASK_SPEC_TEMPLATE.md](TASK_SPEC_TEMPLATE.md), and independent review in [REVIEW_GOVERNANCE.md](REVIEW_GOVERNANCE.md).

| Role | Next deliverable |
| --- | --- |
| Human | Product goal, approved scope and final PR/release decision |
| GPT Architect | A bounded TaskSpec based on verified repository facts and the approved goal |
| Cursor | Implementation and factual evidence for one approved TaskSpec/slice |
| Deterministic gates | Exact build/test results and unavailable checks |
| GPT Reviewer | Independent PASS, REPAIR REQUIRED or HUMAN DECISION REQUIRED verdict |

[STATE_0.md](STATE_0.md) is a provisional inventory and planning aid. Verify material claims against the current branch, code, tests and issue before using them in a TaskSpec. A missing or stale inventory does not by itself block an unrelated approved task; material uncertainty is resolved by the Architect or escalated.

Keep fork goals distinct from upstream changes. Quest Tracker produces observations; quest disappearance does not establish completion. Preserve identity, source/confidence and Contract v1 boundaries. End implementation with the handoff defined in `REVIEW_GOVERNANCE.md`, then stop for independent review and the human decision. A chat session is not an authority boundary; each new slice needs its own approved TaskSpec and handoff.
