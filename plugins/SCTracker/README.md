# SCTracker

Client-side data collection for a GW1 speedclear run tracker (Go backend, Postgres, React
frontend, [gwsctracker.com](https://gwsctracker.com)). GWToolboxdll's own Objective Timer already
records per-run timing/objective data (`runs/ObjectiveTimerRuns_*.json`), but has no concept of
party composition or how a run ended — this plugin fills that gap without requiring any changes to
GWToolboxdll itself.

## What it does

- Captures who was in the party (players/heroes/henchmen, professions, death count) and how each
  tracked explorable-area run ended (`wipe`/`resign`/`completed`/`unknown`).
- Currently tracks **The Underworld** (8-man) and **The Fissure of Woe** (any party size from solo
  to 8-man) — the maps/sizes the backend has configs for. Other instances GWToolboxdll's Objective
  Timer tracks (more elite areas, dungeons, ToPK) plus random missions/vanquishes/DoA are skipped
  entirely.
- Periodically reads its own local log and GWToolboxdll's `ObjectiveTimerRuns_*.json`, and publishes
  the combined party + objective payload for each run to the backend, machine-key authenticated.
  Only real-player parties matching a supported size (8 for the Underworld; 1–8 for the Fissure
  of Woe) are published; anything else is dropped from the sync queue without an upload attempt.
  The post-run failure/MVP vote is skipped for runs with no role composition (every FoW run except
  the 2-person duo).

## Files written (in your `Documents\GWToolboxpp\<computer>\` folder)

- `runs\PartyLog_YYYY-MM-DD.json` — local record of every tracked run, keyed by UTC start time so it
  can be joined against GWToolboxdll's own `runs\ObjectiveTimerRuns_*.json`. This is the durable
  record; it's written regardless of whether backend sync is configured or succeeds.
- `SCTracker.log` — one line per failed publish attempt (HTTP status + truncated response body).
  Plugins can't write to GWToolboxdll's own `log.txt`, so this is the only place sync failures show
  up.

## Settings

Set a **Machine Key** in the plugin's settings panel to enable backend sync (registered per-machine
on gwsctracker.com). Leave it blank to keep local logging only — nothing is sent anywhere.

The backend base URL (`gwsctracker.com`) is hardcoded; there's no user-facing option for it.
