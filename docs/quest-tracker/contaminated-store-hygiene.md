# Quest progress store hygiene (Batch 2C.2)

## Contaminated stores before the identity-scoped snapshot barrier

Development or test `QuestProgress` JSON written **before** the post-bind snapshot identity barrier may contain:

- quest projections copied from another character after a character switch
- `presence_lost` / `unknown` regressions on terminal `abandoned_observed` / `completed_observed` rows
- empty `displayName` / profession that were never backfilled

This batch **does not** add an automatic destructive cleanup migration.

### Clean validation procedure

1. Quit Guild Wars / unload Toolbox.
2. Archive the existing folder (rename), e.g.  
   `Documents/GWToolboxpp/<PC>/QuestProgress` → `QuestProgress.pre-2c2-archive`
3. Recreate an empty `QuestProgress` directory (Toolbox will recreate files on next save).
4. Reload Toolbox RelWithDebInfo and re-validate with a clean store.

Do not delete archive copies until multi-character in-game validation of the barrier is complete.
