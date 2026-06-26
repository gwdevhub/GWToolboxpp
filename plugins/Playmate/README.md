# Playmate for GWToolbox++

Playmate is an experimental GWToolbox++ plugin for building in-character AI companions inside Guild Wars 1.

The plugin is the in-game sensory layer for that system. It is designed to support whichever companion persona a player wants to run, whether that is a roleplay character, a tactical guide, a lore-aware party member, or a test persona used during development. It listens to GW1 through GWCA, turns useful game state into structured telemetry, and provides a safe local path for companion replies to appear in the party chat window.

## What It Does Today

Playmate currently captures:

- outgoing player party chat
- selected in-game chat-log events
- map load/change events
- quest add/detail-change events
- periodic map and active-quest snapshots

For early tuning, telemetry is written locally as JSON Lines:

```text
Documents/GWToolboxpp/<computer>/Playmate/telemetry-yyyy-mm-dd.jsonl
```

This local capture mode is intentionally the default. It lets us play GW1, inspect what the plugin sees, and trim noisy events before sending anything to a cloud backend.

The plugin can also POST events to a local companion service:

- `POST /v1/playmate/events` receives telemetry JSON.
- `GET /v1/playmate/replies` returns either `{"replies":["..."]}` or a plain text reply.

Replies are injected locally with `GW::Chat::WriteChat`, using the active companion persona as the sender. This writes to the client chat window; it does not send a message to ArenaNet servers.

## Where It Is Going

The intended architecture is:

```text
GW1 + GWToolbox++ Playmate plugin
        -> local JSONL capture for inspection
        -> local or LAN companion service
        -> Supabase game_logs / environment_alerts / companion_replies / companion memory storage
        -> LLM-driven in-character responses
        -> local in-game chat rendering
```

Next milestones:

- Add environment radar events from `GW::Agents` for nearby hostile/high-threat entities.
- Tune the client-side filter matrix so trade spam, combat noise, and ordinary item chatter stay out.
- Add rare loot and quest-state enrichment.
- Promote the local event schema into the Supabase ingestion service once the telemetry shape is stable.

## Design Guardrails

- Keep Supabase service credentials out of the injected DLL.
- Do expensive work outside the game process.
- Prefer local capture and review before cloud ingestion.
- Rate-limit proactive alerts so the system stays useful and cheap.
- Keep the companion grounded in actual GW1 context, not a generic chatbot loop.
