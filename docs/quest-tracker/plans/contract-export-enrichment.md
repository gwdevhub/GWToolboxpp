# Contract export enrichment — character life journey

Date: 2026-09-08  
Status: **Phase 5 complete (pending in-game verify)**

## Phase 1–4 (done)

secondaryProfession; unlocks; map/vanquish; dungeon/mission clear; cartography %;
vanquish_complete; skill_point_threshold; faction_threshold.

## Phase 5 (this slice)

Honesty: account-scoped signals must not be labeled as character-learned skills;
HoM is async HTTP (never Draw/Update blocking); XP is a snapshot, not a spam
timeline; `isPvp` is roster metadata (unknown when absent).

1. `isPvp` — optional character bool from `AvailableCharacterInfo::is_pvp()`.
2. `experienceTotal` — optional character uint snapshot from `WorldContext::experience`.
3. `account_skill_unlock` — journey kind from `AccountContext::unlocked_account_skills`
   (account-scoped; distinct from character `skill_unlock`).
4. `hallOfMonuments` — optional character snapshot (homCode + category point totals)
   via `HallOfMonumentsModule::AsyncGetAccountAchievements`; journey
   `hom_points` when category totals increase.
5. Codex: parse/import/labels; Contract docs mirrored.

## Deferred

Festival hats / minipets, death spam, gold-as-progress, PvP W/L/rating as journey.

## STOP after Phase 5 build/tests + push both forks.
