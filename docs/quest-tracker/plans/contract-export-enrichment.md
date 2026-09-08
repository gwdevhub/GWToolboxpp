# Contract export enrichment — character life journey

Date: 2026-09-08  
Status: **Phase 4 complete (pending in-game verify)**

## Phase 1–3 (done)

secondaryProfession; unlocks; map/vanquish; dungeon/mission clear; cartography %.

## Phase 4 (this slice)

1. `vanquish_complete` — UI `kVanquishComplete` + `mapId` (timed; `vanquish_area` stays permanent).
2. `skill_point_threshold` — `total_earned_skill_points` crossings (+ `amount`).
3. `faction_threshold` — `total_earned_{kurzick,luxon,balth,imperial}` (+ `amount`, subjectKey `faction:<name>:<amount>`).
4. Codex: friendlier journey labels (hero/profession names); Contract `amount` field.

## Deferred

Festival/minipet/HoM, death spam, gold/XP spam, PvP account tallies as character journey.

## STOP after Phase 4 build/tests + push both forks.
