# Quest Progress Contract v1

Portable progress exchange between GWToolboxpp and GuildWarsCodex.

## Principles
- Canon metadata is separate from progress.
- Progress is character-scoped unless explicitly account-wide.
- History is append-only.
- Every state/event records source and confidence.
- Import is idempotent.
- Quest disappearance alone is not proof of completion.
- External timestamps are UTC ISO-8601.

## Envelope
```json
{
  "schemaVersion": 1,
  "exportedAt": "2026-07-24T12:00:00Z",
  "producer": {"name": "GWToolboxpp", "version": "string"},
  "account": {"id": null, "displayName": null},
  "characters": []
}
```

## Character
```json
{
  "characterKey": "stable-local-key",
  "name": "Character Name",
  "profession": null,
  "isPreSearing": false,
  "quests": []
}
```

## Quest
```json
{
  "questId": 0,
  "currentState": "active",
  "source": "game_snapshot",
  "confidence": "confirmed",
  "firstObservedAt": "2026-07-24T12:00:00Z",
  "lastObservedAt": "2026-07-24T12:05:00Z",
  "completedAt": null,
  "objectives": [],
  "history": []
}
```

## States
unknown, available, active, objective_progress, ready_for_reward, completed_observed, completed_manual, abandoned_observed

## Sources
game_snapshot, game_event, mission_completion_data, toolbox_existing_data, manual_user_input, imported_history

## Confidence
confirmed, probable, uncertain, manual

## Compatibility
Consumers reject unsupported major versions, may ignore unknown optional fields, and producers must bump schema versions when field meaning changes.
