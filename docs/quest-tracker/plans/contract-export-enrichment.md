# Contract export enrichment — character life journey

Date: 2026-09-08  
Status: **Phase 3 complete (pending in-game verify)**

## Goal

Maximize observational Contract v1 export so a character’s chronicle captures exciting life-path milestones.

## Phase 1–2 (done)

secondaryProfession; map_enter; vanquish_area; skill/hero/map/profession/hard_mode unlocks.

## Phase 3 (this slice)

1. `dungeon_complete` — UI `kDungeonComplete` + current `mapId` (timed clear beat).
2. `mission_complete` — UI `kMissionComplete` + current `mapId` (timestamped; missions[] remains permanent bits).
3. `cartography_threshold` — continent fog bit coverage crosses 1/10/25/50/75/90/100% (+ optional `percent`, `mapId` = where observed).

## Deferred (honesty)

- Festival hats / minipets / HoM — dialog-string and network tallies; brittle provenance.
- Death stream, gold/XP spam.

## STOP after Phase 3 build/tests.
