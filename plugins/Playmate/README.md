# Playmate GWToolbox++ Plugin

Playmate is a standalone GWToolbox++ UI plugin scaffold for the Azele companion pipeline.

The injected plugin intentionally talks only to a local backend service:

- `POST /v1/playmate/events` receives JSON telemetry events.
- `GET /v1/playmate/replies` returns either `{"replies":["..."]}` or a plain text reply.

The backend is responsible for Supabase, pgvector, summarization, and LLM calls. Keeping those responsibilities outside the game process avoids embedding Supabase credentials in the DLL and keeps the in-game path lightweight.

Telemetry currently includes outgoing player party chat, selected chat-log messages, map changes, quest events, and periodic map/quest snapshots. Replies are written locally into party chat with `GW::Chat::WriteChat`, using `Azele` as the sender name.
