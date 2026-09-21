# Pre-Searing in-game verification — Phase 6 producer

Date: 2026-09-08  
Goal: confirm live Contract export from a **new** character with Toolbox loaded from first login.

Use RelWithDebInfo `GWToolboxdll.dll` built from `feature/quest-tracker-phase-2-persistence` at or after `69305131c`.

## Setup

- [ ] Inject / load Toolbox before leaving character select (or immediately on first zone-in)
- [ ] Create a **new** Prophecies Pre-Searing character (do not use an existing alt)
- [ ] Note display name + that `isPreSearing` should export `true`
- [ ] Keep Toolbox running for the whole session (no offline play for this checklist)
- [ ] Export Contract JSON at least twice: mid-session and end-of-session
- [ ] Unchanged re-export is skipped (chat: unchanged since last export)
- [ ] Changed re-export writes a new stamped file under `QuestProgress/exports/`
  and updates `QuestProgress/quest_progress_contract_v1.json` as latest

## Live observation (Toolbox Quest Tracker window)

- [ ] Quest log lists starter quests (no custom marker `0xfdd` as progress)
- [ ] Clicking a quest sets the game active quest
- [ ] Objective bullets update when an objective completes
- [ ] Ready-for-reward (in-log completed) shows without claiming permanent turn-in
- [ ] Abandon (if tested): probable abandoned, not completed
- [ ] Quest removal without clear reward evidence: unknown/uncertain, **not** confirmed completed

## Journey / metadata expected in export

After Ascalon Academy / early Pre-Searing play (level a few times, accept/turn a few quests, visit a second map):

- [ ] `characterKey` stable across export #1 and #2
- [ ] `displayName` matches in-game name
- [ ] `isPreSearing: true`
- [ ] `primaryProfession` present
- [ ] `level` matches current level snapshot
- [ ] `experienceTotal` present and non-decreasing across exports
- [ ] `skillPointsEarned` present (may be 0 early)
- [ ] `journeyEvents` contains at least one `level_up`
- [ ] `journeyEvents` contains at least one `map_enter` with `mapId`
- [ ] Quest history is append-only across the two exports (no erased prior events)
- [ ] Re-import same file into Codex is idempotent (no duplicate history rows)

## Honesty checks (must fail closed)

- [ ] Leaving a quest without turn-in / without abandon does **not** invent `completed_observed`
- [ ] Playing a few minutes **without** Toolbox, then reloading Toolbox, does **not** backfill missed quests as completed
- [ ] HoM fields may be absent or lag; absence ≠ zero dedications

## Codex import

- [ ] Contract parses clean (no diagnostics)
- [ ] Character links by `toolboxCharacterKey` / explicit link
- [ ] Profile picks up professions + `experienceTotal` / `skillPointsEarned` when higher
- [ ] Journey timeline shows readable labels for `level_up` / `map_enter`

## Stop conditions

Pass if Setup + Live observation + Journey metadata + Honesty checks are green for one short Pre-Searing session.  
Do **not** block on HoM detail arrays, post-Searing content, vanquish, or festival items.
