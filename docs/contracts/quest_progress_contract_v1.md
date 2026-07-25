# Quest Progress Contract v1

**Status:** Normative specification (GuildWarsCodex Phase B3 — Codex half)  
**Contract id:** `guild-wars-quest-progress`  
**Contract version:** `1`

This document is the canonical Quest Progress Contract v1. It MUST be maintained
**byte-for-byte identically** in GuildWarsCodex and GWToolboxpp. See
[Cross-repository synchronization](#19-cross-repository-synchronization).

## Normative language

The key words **MUST**, **MUST NOT**, **SHOULD**, and **MAY** are to be
interpreted as described in RFC 2119.

| Kind | Meaning in this document |
|------|--------------------------|
| Normative schema | Requirements for valid Contract v1 JSON |
| Producer guidance | Rules for emitters (primarily GWToolboxpp) |
| Consumer guidance | Rules for importers (primarily GuildWarsCodex) |
| Non-normative | Examples and notes; not requirements |

This Contract describes **what may be represented**. It does **not** claim that
GWToolboxpp currently observes every field, source, or event type. Producers
MUST only emit values supported by actual evidence available to them.

## Separation from Settings backup

This Contract is **not** the GuildWarsCodex Settings user-progress backup
(`formatVersion` 1 or 2).

| Format | Role |
|--------|------|
| Quest Progress Contract v1 | Interchange produced by GWToolboxpp; imported by GuildWarsCodex |
| Settings backup v2 | Local Codex active-progress safety export/import |

Consumers MUST NOT treat a Settings backup file as a Contract file, or the reverse.

## Design principles

- Observational and non-automating: the Contract records observed or user-supplied
  progress evidence; it does not instruct game automation.
- Character-scoped progress (unless a future contract version defines account-wide
  records).
- Source- and confidence-aware.
- Safe against false quest-completion inference.
- Canon quest metadata remains separate from progress observations.
- History is append-only; corrections supersede prior evidence without erasing it.
- Import MUST be idempotent and duplicate-safe when identity rules are followed.

---

## 1. Top-level JSON envelope

### Normative shape

```json
{
  "contract": "guild-wars-quest-progress",
  "contractVersion": 1,
  "producer": {
    "name": "GWToolboxpp",
    "version": "optional producer version"
  },
  "exportedAt": "2026-07-25T20:00:00.000Z",
  "characters": []
}
```

### Rules

- `contract` is required and MUST equal the string `guild-wars-quest-progress`.
- `contractVersion` is required and MUST be the integer `1`.
- `producer` is required and MUST be a JSON object.
- `exportedAt` is required file-export metadata (UTC ISO-8601; see
  [Timestamps](#11-timestamp-rules)).
- `characters` is required and MUST be a JSON array (MAY be empty).
- Unknown top-level fields MUST be tolerated by consumers per
  [Unknown fields and compatibility](#13-unknown-fields-and-compatibility).

There is no account envelope in Contract v1.

---

## 2. Producer object

```json
{
  "name": "GWToolboxpp",
  "version": "optional producer version"
}
```

### Rules

- `producer.name` is required, MUST be a non-empty string, and identifies the
  producer namespace used in logical event identity.
- `producer.version` is optional metadata.
- `producer.version` MUST NOT participate in logical event identity or fallback
  fingerprints.
- Producers SHOULD use a stable `name` across releases of the same product.

---

## 3. Character record

```json
{
  "characterKey": "opaque-stable-producer-key",
  "displayName": "Character Name",
  "isPreSearing": false,
  "quests": []
}
```

### Rules

- `characterKey` is required, non-empty, opaque, and MUST be stable within the
  producer namespace.
- `characterKey` is the external identity used for linking to Codex
  `toolboxCharacterKey`.
- `displayName` is required and non-empty. It is presentation metadata and MUST
  NOT be treated as stable identity.
- `isPreSearing` is optional. When absent, consumers MUST treat the value as
  **unknown** and MUST NOT invent `true` or `false`.
- `quests` is required and MUST be a JSON array (MAY be empty).
- Duplicate `characterKey` values within one file are invalid; consumers MUST
  reject the file.
- Consumers MUST NOT silently link ambiguous duplicate display names.
- Contract v1 does not introduce a second Codex-local character UUID.

---

## 4. Quest observation record

```json
{
  "gameQuestId": 12345,
  "questName": "Optional display-only quest name",
  "state": "active",
  "source": "game_snapshot",
  "confidence": "confirmed",
  "firstObservedAt": "2026-07-25T19:00:00.000Z",
  "observedAt": "2026-07-25T20:00:00.000Z",
  "objectives": [],
  "history": []
}
```

### Rules

- `gameQuestId` is required and MUST be a positive JSON integer.
- `gameQuestId` is the Guild Wars numeric quest identity. It is **not** a
  GuildWarsCodex canon entity id.
- `questName` is optional display metadata and MUST NOT be used as identity.
- `state`, `source`, `confidence`, and `observedAt` are required.
- `firstObservedAt` is optional.
- `objectives` is required and MUST be a JSON array (MAY be empty).
- `history` is required and MUST be a JSON array (MAY be empty).
- Duplicate `gameQuestId` values within one character record are invalid;
  consumers MUST reject the file.
- Consumers MUST NOT invent canon mappings from quest names or approximate
  matches.
- Unmapped numeric quest observations remain valid Contract data.

The quest object represents the producer's **latest known** observation for that
quest on that character. It is not necessarily an authoritative complete lifetime
history.

---

## 5. State vocabulary

Contract v1 defines exactly these `state` values. Producers and consumers MUST
NOT invent additional v1 states.

| Value | Meaning |
|-------|---------|
| `unknown` | Insufficient evidence to classify progress |
| `available` | Quest is known/available but not accepted/active |
| `active` | Quest is accepted / in progress without finer objective detail |
| `objective_progress` | Quest is in progress with objective-level evidence |
| `ready_for_reward` | Objectives satisfied; reward turn-in observed or strongly evidenced |
| `completed_observed` | Completion positively observed from game evidence |
| `completed_manual` | Completion asserted by explicit user input |
| `abandoned_observed` | Abandon positively observed or emitted only from an explicitly documented observation source |

### Mandatory safety rules

- Quest disappearance alone MUST NOT produce `completed_observed`.
- `completed_observed` MUST be backed by positively observed completion evidence.
- `completed_manual` MUST be backed by explicit user input.
- `abandoned_observed` MUST be backed by positive abandon evidence or an
  explicitly documented observation source; producers MUST NOT invent abandon
  from missing data alone.
- Absence from a snapshot is not completion evidence.
- Unknown or insufficient evidence MUST remain `unknown` or retain a
  lower-confidence state rather than escalating to completion.

---

## 6. Source vocabulary

### Contract v1 producer sources

| Value | Meaning |
|-------|---------|
| `game_snapshot` | State read from a current observable snapshot |
| `game_event` | A positively observed game event |
| `mission_completion_data` | Mission completion information when actually available |
| `toolbox_existing_data` | Older Toolbox-local data with weaker/incomplete original provenance |
| `manual_user_input` | Explicit user action |
| `imported_history` | Preserved earlier history, not a newly observed live event |

A producer MUST use the most honest source value supported by evidence.

`mission_completion_data` is valid only when mission completion information is
actually available to the producer. Producers MUST NOT label speculative
completion as `mission_completion_data`.

### Codex-internal sources (not Contract producer values)

| Value | Scope |
|-------|-------|
| `migration` | GuildWarsCodex internal schema/backfill writes only |
| `backup_restore` | GuildWarsCodex Settings backup restore only |

Contract v1 producers MUST NOT emit `migration` or `backup_restore`.

---

## 7. Confidence vocabulary

Contract v1 defines exactly:

| Value | Meaning |
|-------|---------|
| `confirmed` | Direct, reliable observation |
| `probable` | Strong but incomplete evidence |
| `uncertain` | Weak or ambiguous evidence |
| `manual` | Explicitly supplied or corrected by the user |

There is no `derived` confidence in v1.

Source and confidence are **separate dimensions**. A manual source SHOULD normally
use `manual` confidence; validation MUST follow the allowed-combination table
below rather than implication alone.

### Allowed source × confidence combinations

| Source | `confirmed` | `probable` | `uncertain` | `manual` |
|--------|:-----------:|:----------:|:-----------:|:--------:|
| `game_snapshot` | yes | yes | yes | no |
| `game_event` | yes | yes | yes | no |
| `mission_completion_data` | yes | yes | yes | no |
| `toolbox_existing_data` | yes | yes | yes | no |
| `manual_user_input` | no | no | no | yes |
| `imported_history` | yes | yes | yes | no |

Consumers MUST reject files that violate this table for current observations or
history events.

---

## 8. Objective record

```json
{
  "objectiveIndex": 0,
  "text": "Optional objective text",
  "isCompleted": false,
  "observedAt": "2026-07-25T20:00:00.000Z"
}
```

### Rules

- `objectiveIndex` is required, MUST be an integer, and MUST be `>= 0`.
- `objectiveIndex` is the stable objective identity within one quest observation
  or history event objective snapshot.
- `text` is optional; when present it MUST be a string.
- `isCompleted` is required and MUST be a boolean.
- `observedAt` is optional (UTC ISO-8601 when present).
- Duplicate `objectiveIndex` values in one objective array are invalid;
  consumers MUST reject the file.
- Objective arrays MUST be emitted in ascending `objectiveIndex` order.
- Consumers MUST canonicalize by `objectiveIndex`, not by arbitrary JSON array
  sorting of other fields.
- Consumers MUST NOT infer overall quest completion solely because all currently
  visible objectives are complete unless positive completion evidence exists.

Preserve source objective order only through the explicit index.

---

## 9. History event record

```json
{
  "eventId": "optional-producer-event-id",
  "eventType": "observation",
  "observedAt": "2026-07-25T20:00:00.000Z",
  "state": "active",
  "source": "game_snapshot",
  "confidence": "confirmed",
  "objectives": [],
  "payload": {}
}
```

### Rules

- `eventId` is optional. When present it MUST be a non-empty string and SHOULD be
  stable within the producer's event namespace.
- Duplicate non-null `eventId` values for the same producer namespace that are not
  semantically identical events are invalid within one file; consumers MUST
  reject the file. Semantically identical duplicates MAY be rejected or
  collapsed; consumers MUST NOT keep conflicting meanings for the same identity.
- `eventType` is required and MUST be a non-empty string.
- `eventType` is an **open** producer vocabulary in Contract v1.
- Consumers MUST preserve unknown `eventType` values and MUST NOT infer `state`
  from `eventType`.
- `state`, `source`, `confidence`, and `observedAt` carry the normative progress
  meaning.
- `objectives` is optional. When present it MUST be a complete objective snapshot
  for that event and MUST follow the objective schema.
- `payload` is optional. When present it MUST be a JSON object.
- `payload` MAY preserve producer-specific evidence that does not belong in the
  stable v1 fields.
- History events are append-only evidence.
- Corrections supersede previous evidence but MUST NOT erase stored history.

Non-normative note: `eventType` values such as `"observation"` in examples are
illustrative. This Contract does not claim that GWToolboxpp currently emits any
specific event type set.

---

## 10. Required and optional field tables

Legend:

- **R** = required; **O** = optional
- **Null** = whether JSON `null` is allowed
- **Identity** = participates in character/quest/event linking identity
- **FP** = participates in fallback fingerprint inputs defined in this Contract

### 10.1 Top-level envelope

| Field | Req | JSON type | Null | Absent default | Identity | FP |
|-------|-----|-----------|------|----------------|----------|----|
| `contract` | R | string | no | — | contract id | no |
| `contractVersion` | R | integer | no | — | yes (version scope) | yes |
| `producer` | R | object | no | — | via `name` | via `name` |
| `exportedAt` | R | string | no | — | no (file metadata) | no |
| `characters` | R | array | no | — | no | no |
| unknown fields | O | any | — | ignore | no | no |

### 10.2 Producer

| Field | Req | JSON type | Null | Absent default | Identity | FP |
|-------|-----|-----------|------|----------------|----------|----|
| `name` | R | string (non-empty) | no | — | producer namespace | yes (`producerName`) |
| `version` | O | string | no | unknown / absent | no | **MUST NOT** |
| unknown fields | O | any | — | ignore | no | no |

### 10.3 Character

| Field | Req | JSON type | Null | Absent default | Identity | FP |
|-------|-----|-----------|------|----------------|----------|----|
| `characterKey` | R | string (non-empty) | no | — | yes | yes |
| `displayName` | R | string (non-empty) | no | — | no | no |
| `isPreSearing` | O | boolean | no | **unknown** (do not invent) | no | no |
| `quests` | R | array | no | — | no | no |
| unknown fields | O | any | — | ignore | no | no |

### 10.4 Quest observation

| Field | Req | JSON type | Null | Absent default | Identity | FP |
|-------|-----|-----------|------|----------------|----------|----|
| `gameQuestId` | R | integer (`> 0`) | no | — | yes | yes |
| `questName` | O | string | no | absent | no | no |
| `state` | R | string (enum) | no | — | no | yes |
| `source` | R | string (enum) | no | — | no | yes |
| `confidence` | R | string (enum) | no | — | no | yes |
| `firstObservedAt` | O | string (timestamp) | no | absent | no | no |
| `observedAt` | R | string (timestamp) | no | — | no | yes |
| `objectives` | R | array | no | — | via indices | yes (canonical) |
| `history` | R | array | no | — | via events | no (separate events) |
| unknown fields | O | any | — | ignore | no | no |

### 10.5 Objective

| Field | Req | JSON type | Null | Absent default | Identity | FP |
|-------|-----|-----------|------|----------------|----------|----|
| `objectiveIndex` | R | integer (`>= 0`) | no | — | yes (within quest) | yes |
| `text` | O | string | no | absent | no | yes when present in canonical objectives |
| `isCompleted` | R | boolean | no | — | no | yes |
| `observedAt` | O | string (timestamp) | no | absent | no | yes when present |
| unknown fields | O | any | — | ignore | no | no |

### 10.6 History event

| Field | Req | JSON type | Null | Absent default | Identity | FP |
|-------|-----|-----------|------|----------------|----------|----|
| `eventId` | O | string (non-empty) | no | absent → fallback FP | yes when present | via structured uid when present |
| `eventType` | R | string (non-empty) | no | — | no (open vocab) | yes (fallback FP) |
| `observedAt` | R | string (timestamp) | no | — | no | yes |
| `state` | R | string (enum) | no | — | no | yes |
| `source` | R | string (enum) | no | — | no | yes |
| `confidence` | R | string (enum) | no | — | no | yes |
| `objectives` | O | array | no | absent (no objective snapshot) | via indices | yes when present |
| `payload` | O | object | no | absent / empty object for FP | no | yes (canonical) |
| unknown fields | O | any | — | ignore | no | no |

---

## 11. Timestamp rules

All Contract timestamps:

- MUST be UTC ISO-8601;
- MUST use a trailing `Z`;
- MUST include date and time;
- MAY contain fractional seconds;
- MUST be normalized by consumers to UTC before comparison;
- represent an instant, not local wall-clock time.

### Examples

- Valid: `2026-07-25T20:00:00Z`
- Valid: `2026-07-25T20:00:00.123Z`
- Invalid: timestamp without timezone (`2026-07-25T20:00:00`)
- Invalid: local-time-only or date-only strings

Ordering and precedence MUST be based on parsed instants, not lexical string
comparison.

---

## 12. Array ordering

| Array | Semantic order |
|-------|----------------|
| `characters` | No semantic meaning |
| `quests` | No semantic meaning |
| `objectives` | Ascending `objectiveIndex` (required emission order) |
| `history` | Ascending `observedAt`; equal timestamps retain producer order |

Consumers MUST NOT derive newer state merely from later array position.
Equal-timestamp history events retain producer order for presentation, but
consumers MUST use identity and reducer rules rather than array order for
precedence.

---

## 13. Unknown fields and compatibility

### Consumers MUST

- ignore unknown object fields;
- preserve producer-specific `payload` content when storing event evidence;
- reject missing required fields;
- reject wrong JSON types;
- reject unknown required state/source/confidence enum values;
- reject unsupported `contractVersion`;
- **MUST NOT** silently reinterpret unknown enum values as `unknown`.

### Producers MAY / MUST

- MAY add unknown optional fields;
- MUST NOT rename or remove v1 required fields;
- MUST NOT change the meaning or type of v1 fields.

A future breaking schema requires a new `contractVersion`.

---

## 14. Identity and idempotency

### 14.1 File identity

Importers MAY compute a file-level content hash for batch diagnostics and fast
no-op detection.

File identity is **not** event identity.

`exportedAt` and `producer.version` MAY cause file hashes to differ even when
events are logically identical.

### 14.2 History event identity with `eventId`

When `eventId` is present, the external logical identity scope is the canonical
structured tuple (not a delimiter-concatenated string):

```json
{
  "contractVersion": 1,
  "producerName": "GWToolboxpp",
  "characterKey": "opaque-key",
  "gameQuestId": 12345,
  "eventId": "producer-event-id"
}
```

Object keys in any serialized form of this tuple MUST be lexicographically
ordered when hashed.

### 14.3 History event fallback fingerprint

When `eventId` is absent, the fallback fingerprint MUST be SHA-256 over
canonical JSON containing **only**:

- `contractVersion`
- `producerName`
- `characterKey`
- `gameQuestId`
- `eventType`
- normalized UTC `observedAt`
- `state`
- `source`
- `confidence`
- canonical `objectives` (empty array if absent)
- canonical `payload` (empty object if absent)

Exclude:

- `producer.version`
- top-level `exportedAt`
- array position
- unknown sibling extension fields
- `displayName`
- `questName`
- `firstObservedAt`

Canonical objectives are ordered by ascending `objectiveIndex`. Each objective
object includes only defined objective fields that are present, with keys
lexicographically ordered.

Canonical JSON object keys are lexicographically ordered, strings use normal
JSON escaping, and no insignificant whitespace participates in hashing.

Consumers and producers MUST NOT use Dart or C++ runtime `hashCode` for these
fingerprints.

### 14.4 Current observation fallback fingerprint

A separate canonical fingerprint input for the quest's current observation MUST
contain:

- `recordType`: `"current_observation"`
- `contractVersion`
- `producerName`
- `characterKey`
- `gameQuestId`
- normalized `observedAt`
- `state`
- `source`
- `confidence`
- canonical `objectives`

Exclude `producer.version`, file `exportedAt`, names, array order, `history`,
and unknown extension fields.

---

## 15. Current record versus history

- Every quest record contains the producer's current/latest known observation.
- `history` contains zero or more earlier or supporting events.
- The current observation does not have to be duplicated inside `history`.
- Consumers normalize the current observation and each history entry as distinct
  candidate events.
- Duplicate-safe identity rules prevent repeated imports from duplicating
  evidence.
- Current state stored in Codex is selected by the Codex reducer, not by blindly
  trusting file order.

---

## 16. Character and quest linking (consumer guidance)

- First match `characterKey` to Codex `toolboxCharacterKey`.
- A unique display-name fallback MAY be offered only as an explicit linking aid.
- Ambiguous display names remain unresolved.
- No guessed character link is allowed.
- Numeric `gameQuestId` MAY link to a canon quest only through verified mapping
  data.
- No mapping is invented from `questName`.
- Unmapped observations remain importable using a deterministic progress subject
  key (Codex implementation detail; not defined as a Contract field).

This Contract does not specify importer UI.

---

## 17. Valid example

See the companion file:

[`docs/contracts/examples/quest_progress_contract_v1.example.json`](examples/quest_progress_contract_v1.example.json)

That example:

- is valid JSON conforming to this Contract;
- uses synthetic quest ids and text only;
- includes one character, one current quest observation, two objectives;
- includes one history event with `eventId` and one without;
- demonstrates optional `payload`;
- does not claim real Guild Wars quest data.

---

## 18. Invalid examples (non-normative)

### Quest disappearance as confirmed completion

```json
{
  "gameQuestId": 900001,
  "state": "completed_observed",
  "source": "game_snapshot",
  "confidence": "confirmed",
  "observedAt": "2026-07-25T20:00:00.000Z",
  "objectives": [],
  "history": []
}
```

Invalid if the only evidence is that the quest vanished from a snapshot.

### Duplicate character key

Two `characters[]` entries with the same `characterKey` in one file → reject.

### Duplicate objective index

Two objectives with `"objectiveIndex": 0` in one array → reject.

### Missing timezone

`"observedAt": "2026-07-25T20:00:00"` → reject.

### Unsupported state

`"state": "finished"` → reject (MUST NOT reinterpret as `unknown`).

### Non-object payload

`"payload": []` or `"payload": "text"` → reject when `payload` is present.

---

## 19. Cross-repository synchronization

- The canonical text is maintained identically in GuildWarsCodex and GWToolboxpp.
- Both repositories MUST contain the same relative files:
  - `docs/contracts/quest_progress_contract_v1.md`
  - `docs/contracts/examples/quest_progress_contract_v1.example.json`
- Semantic changes require updating both repositories before parser/exporter
  implementation proceeds.
- Phase C is blocked until byte-level parity is verified.
- Web links MUST NOT be the only source of the Contract; the files in-repo are
  authoritative.

---

## 20. Related Codex-internal formats

GuildWarsCodex Settings backup `formatVersion` 2 remains a separate local format
for active character progress and retained history. It is not interchangeable
with this Contract.
