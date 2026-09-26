# AI operating card — GWToolbox++

| Role | May do | Must not without explicit human instruction |
| --- | --- | --- |
| Human | Approve State 0 and each slice; merge; release | — |
| Cursor | Plan; implement **one** approved slice; run narrow build/checks; write handoff | Widen scope; mass refactors; alter CI/tooling/presets without approval |
| ChatGPT | Co-author State 0 / slices; read-only review | Edit repo; commit; push; merge; deploy |

## Hard rules

1. No State 0 for the touched area → inventory/plan only, no feature coding.
2. One chat = one approved slice (prefer one module / one behavior).
3. End coded work with a compact handoff (goal, non-goals, paths, build/test commands run, deferred).
4. Treat upstream vs your fork goals explicitly — do not silently diverge from upstream without documenting it in State 0.

Handoff template (minimal):

```md
# Review handoff: <slice>
- Goal / in scope:
- Non-goals:
- Changed paths:
- Checks run (cmake/build/tests):
- Known limitations / deferred:
- Next human decision: keep | repair | stop | update State 0
```
