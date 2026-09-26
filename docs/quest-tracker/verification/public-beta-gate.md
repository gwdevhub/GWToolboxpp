# Public beta gate — Toolbox + Tyrian Wayfarer

Date: 2026-09-26  
Goal: ship a **progress-bridge beta**, not a finished encyclopedia.

**Build identity for this gate:** fork RelWithDebInfo on `8.34` + `Beta2` with fork suffix `+quest` (`CMakeLists.txt`: `GWTOOLBOXDLL_VERSION` / `GWTOOLBOXDLL_VERSION_BETA` / `GWTOOLBOXDLL_VERSION_FORK_SUFFIX`). This is **not** an official `gwdevhub/GWToolboxpp` release.

### Version fields (do not collapse)

| Where | What the 2026-09-26 trial showed |
|-------|----------------------------------|
| In-game Settings | `8.34`, fork `+quest`, and `Beta2` as separate UI labels |
| Contract JSON `producer.version` | `8.34` only (exporter sets `GWTOOLBOXDLL_VERSION`; Beta2 / `+quest` are **not** part of that field) |

## In scope for beta

- [x] Fork Quest Tracker Contract v1 export (historical files + skip unchanged)
- [x] In-window **Data for Tyrian Wayfarer** help (what / enable / path)
- [x] Wayfarer Settings: matching help + Contract import + character link
- [x] Post-import **Open dossier** CTA + readable quest labels on Life journey
- [x] Manual Pre-Searing smoke: export → Wayfarer import → dossier Life journey (2026-09-26)
- [x] Same-file re-import does not duplicate evidence (2026-09-26)
- [x] Local `/localdll` fork DLL still loaded after full game exit + new `Gw.exe` (2026-09-26)
- [x] Honesty docs: disappearance not completion; optional drops not required

## Out of scope for beta

- Drop CSV / Completion / Inventory / Objective Timer ingest
- Cloud sync of progress
- Upstream `dev` rebase
- Perfect canon coverage
- Claiming `producer.version` encodes Beta2 or `+quest` (it does not)

## 2026-09-26 evidence (redacted)

Automated / build:

- `QuestProgressTests` RelWithDebInfo: **1198 passed / 0 failed**
- RelWithDebInfo `GWToolboxdll.dll` build succeeded
- DLL SHA256: `FA23EEBA1CAD8B3E258837CF3EF4BB7C3A5478ECDD8B12D7A2216247A7116577`

Closed gates (manual / observed; no character names or keys recorded here):

1. **Export → Wayfarer → Life journey** — In-game fork Quest Tracker exported Contract v1 (`producer.version` = `8.34`; 1 Pre-Searing character, 3 quest observations, 8 history events in the export). Local Wayfarer web import inserted 11 events; dossier showed **3 tracked / 0 completed / 11 history** with Life journey visible.
2. **Re-import** — Same file again: Contract already imported; writes committed false; 0 inserted.
3. **Updater / local DLL path** — After full game close, a new `Gw.exe` was started; Process Explorer showed the loaded module path `D:\Development\C++\GWToolboxpp\bin\RelWithDebInfo\GWToolboxdll.dll`; Quest Tracker worked again. Launcher path used local RelWithDebInfo DLL (not an official GitHub overwrite in this trial).

Not claimed by this gate: Android phone smoke, Cloudflare/Firebase production deploy, or a published GitHub Release/tag.

## Release actions

1. Tag / note fork RelWithDebInfo build as **`8.34_Beta2+quest`** (see `public-beta-prerelease-draft.md`). Verify SHA256 of the shipped DLL.
2. Wayfarer beta build (web and/or Android release) — separate consumer repo decision.
3. Point testers at `toolbox-wayfarer-export-guide.md` and `fork-beta-install.md`.
