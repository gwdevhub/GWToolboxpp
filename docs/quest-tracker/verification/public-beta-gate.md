# Public beta gate — Toolbox + Tyrian Wayfarer

Date: 2026-09-09  
Goal: ship a **progress-bridge beta**, not a finished encyclopedia.

## In scope for beta

- [ ] Fork Quest Tracker always-on Contract v1 export (historical files + skip unchanged)
- [ ] In-window **Data for Tyrian Wayfarer** help (what / enable / path)
- [ ] Wayfarer Settings: matching help + Contract import + character link
- [ ] Pre-Searing smoke: play → export → import → journey/quests visible
- [ ] Honesty: disappearance not auto-completed; optional drops not required

## Out of scope for beta

- Drop CSV / Completion / Inventory / Objective Timer ingest
- Cloud sync of progress
- Upstream `dev` rebase
- Perfect canon coverage

## Release actions

1. Tag / note fork build (`8.33+quest` RelWithDebInfo).
2. Wayfarer beta build (web and/or Android release).
3. Point users at mirrored `toolbox-wayfarer-export-guide.md`.
