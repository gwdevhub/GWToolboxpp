# Toolbox ↔ Tyrian Wayfarer — what is exported

Mirrored in both repos. Keep wording aligned when editing.

## Always collected (Quest Tracker / progress store)

Requires: fork build with Quest Tracker loaded, character logged in (persistent identity).

| Data | Notes |
|------|--------|
| Quest log observations + append-only history | Disappearance ≠ confirmed completion |
| Active quest / objectives | Current snapshot only |
| Mission / bonus / HM bits | From mission completion data |
| Journey milestones | level_up, map_enter, unlocks, clears, thresholds, HoM points, … |
| Character snapshots | professions, isPreSearing, isPvp, level, XP, skill points earned, faction totals |
| Hall of Monuments | Async ArenaNet fetch; may lag; dedications ≠ ownership history |

**Export:** Quest Tracker → **Export Contract v1**  
Writes `Documents/GWToolboxpp/<PC>/QuestProgress/exports/quest_progress_contract_v1_<UTC>_<key>.json`  
and updates `QuestProgress/quest_progress_contract_v1.json` (latest).  
Unchanged content is skipped (no new file).

**Import:** Tyrian Wayfarer → Settings → **GWToolbox quest progress** → select Contract JSON → link character → import.

## Optional (not in Contract v1 beta)

| Data | Enable in Toolbox | On-disk location | Wayfarer |
|------|-------------------|------------------|----------|
| Item drops / loot | Item Settings → **Drop Tracking Enabled** | `…/item_drops/YYYY-MM-DD_drops.csv` | Not imported in beta |
| Completion panel | Use Completion window (auto JSON) | `…/character_completion.json` | Not imported in beta |
| Account inventory | Account Inventory (outpost save) | `…/inventories/…` | Not imported in beta |
| Objective timer runs | Objectives → save to disk | `…/runs/…` | Not imported in beta |

## Honesty

- Observational only — no quest/combat automation.
- Missing optional files means “not recorded”, not empty progress.
- Manual Wayfarer corrections are not overwritten by older imports.
