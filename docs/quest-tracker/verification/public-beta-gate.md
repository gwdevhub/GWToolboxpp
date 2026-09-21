# Public beta gate — Toolbox + Tyrian Wayfarer

Date: 2026-09-09  
Goal: ship a **progress-bridge beta**, not a finished encyclopedia.

## In scope for beta

- [x] Fork Quest Tracker Contract v1 export (historical files + skip unchanged)
- [x] In-window **Data for Tyrian Wayfarer** help (what / enable / path)
- [x] Wayfarer Settings: matching help + Contract import + character link
- [x] Post-import **Open dossier** CTA + readable quest labels on Life journey
- [ ] Manual Pre-Searing smoke in UI: export → import → dossier Life journey
- [x] Honesty docs: disappearance not completion; optional drops not required

## Out of scope for beta

- Drop CSV / Completion / Inventory / Objective Timer ingest
- Cloud sync of progress
- Upstream `dev` rebase
- Perfect canon coverage

## Release actions

1. Tag / note fork build (`8.33+quest` RelWithDebInfo).
2. Wayfarer beta build (web and/or Android release).
3. Point users at mirrored `toolbox-wayfarer-export-guide.md`.
