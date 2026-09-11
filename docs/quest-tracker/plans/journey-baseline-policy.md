# Journey baseline policy

Date: 2026-09-11  
Status: **design only** (no production C++ in this change)  
Base tip: `7b8d93d72dfc3eca87e46c8fccde77c3d698d089` (`feature/quest-tracker-phase-2-persistence`)  
Review amendments: C1–C3, W4–W6; **A7–A10** (2026-09-11)

## Problem

On a long-lived character’s **first** Toolbox observation, several journey builders treat an empty prior as “never seen” and emit catch-up milestones for every currently unlocked map/skill/hero/profession/threshold. Those events share one `observedAt` (sample time). Wayfarer’s dossier Life journey shows only the first **40** timeline entries (`character_timeline_section.dart`), so unlock noise can crowd out quest and session history. `observedAt` is observation time, but the UI reads like milestone time.

## Preferred direction

1. First **complete** gated observation **seals** a persisted baseline **per flood family** for unlock/threshold state.
2. Baseline alone does **not** become timed `journeyEvents[]` milestones.
3. Only later **newly observed deltas after seal** append journey events.
4. Current lifetime state remains exportable via Contract **snapshot** fields where they already exist (`level`, `skillPointsEarned`, `factionTotals`, `hallOfMonuments`, `missions`, titles projection).
5. Missing world/account context must not seal an empty baseline or clear existing data.
6. Uncertain/incomplete samples must not create confirmed-looking catch-up floods (**must not seal**).
7. No quest/combat/movement automation.

This matches title/level’s existing first-sample suppress pattern and extends it to unlock/threshold kinds that currently catch up via `BuildNewlySeenIdEvents` / absolute thresholds / HoM-from-zero.

### C1 — Unlock visibility tradeoff (accepted)

**Decision (a):** Accept the visibility tradeoff for this phase.

After seal, Contract v1 still has **no** unlock inventory snapshot arrays for map / character skill / hero / profession / vanquish. Those families are therefore visible in Life journey **only as deltas observed since tracking began** (plus any pre-policy catch-up noise already on disk). Permanent mission bits remain in `missions[]`; lifetime totals remain in existing snapshot fields.

**Accepted accuracy–noise tradeoff (A8):** journey tracking for a flood family **starts at successful seal**. Changes seen only during the `Unset` stabilization window are absorbed into the sealed baseline and are **never** replayed later as journey events.

**Not in this phase:** Contract unlock-inventory enrichment (exporting sealed unlock sets as snapshot arrays). That requires a **separate future Contract sync** with Wayfarer before producer work. Until then, “what was already unlocked before Toolbox” is intentionally not reconstructible from Contract journey milestones alone.

---

## 1. Data flow

### 1.1 Current (problem)

```
QuestTrackerWindow::Update (Persistent + world_ready)
  → SampleLiveJourneySnapshot(previous titles/level/map, existing journey_events)
       → MergeJourneySnapshot (title_tier / level_up)          // OK (non-flood)
       → BuildMapEnterEvents                                   // OK (non-flood)
       → BuildVanquishAreaEvents / BuildNewlySeenIdEvents / …  // FLOOD: events built HERE
       → BuildCartographyThresholdEvents / BuildAbsoluteThresholdEvents
  → IngestJourneySnapshot (append already-built flood events)

UI callbacks → PushJourneyMilestoneHint → BuildTimedMapClearEvents → IngestJourneyEvents
Async HoM   → IngestHomSnapshot → BuildHomPointsEvents
```

**Contradiction with W4:** flood-family events are created inside Live **before** `IngestJourneySnapshot`. Today’s `JourneySnapshotResult` does not carry raw unlock/vanquish/hero/profession/account id-sets or per-family availability, so the service cannot reconstruct/seal **before** builders. Priors for unlocks are reconstructed from emitted `journey_events` (`PriorIdsFromJourneyEvents`, …). Store format: `gwtoolbox-quest-progress` **1.1**.

### 1.2 Target architecture (A7) — preferred

Live collects **owned raw per-family observations** only for flood families. A **service-owned pure baseline transition / reducer** performs seal-before-delta inside one ingest transaction.

```
QuestTrackerWindow::Update
  → SampleLiveJourneyObservation()   // NEW shape: raw families + non-flood events only
       → titles / level / map_enter builders MAY remain in Live (non-flood)
       → flood families: raw id-set / scalar / flag + context_available + C3 eligibility inputs
       → MUST NOT BuildNewlySeenIdEvents / vanquish/threshold/hom flood builders here
  → IngestJourneySnapshot(observation)   // single transactional step
       → for each flood family, pure BaselineTransition:
            1. legacy / bootstrap examination (A9 / W4)
            2. C3 stability gate (uses transient candidate; A8)
            3. decide seal OR delta (or no-op)
            4. create journey events (delta only; never emit-all on Unset)
            5. update baseline sets + snapshot fields (W5 read-before-write)
       → persist character / account store

UI timed clears (unchanged separate path)
  → PushJourneyMilestoneHint → BuildTimedMapClearEvents → IngestJourneyEvents

Async HoM (separate ingest, same seal-before-delta rule)
  → raw HoM observation → IngestHomSnapshot → BaselineTransition(hom family)
```

**Hard rules:**

- `QuestProgressLive` must **not** create flood-family journey events from empty or event-reconstructed priors.
- W4 “before builders” is satisfied because flood builders run only inside `BaselineTransition` after bootstrap/C3.
- Timed UI callbacks stay on their own path.
- HoM stays async but must use the same seal-before-delta algorithm for family `hom`.

---

## 2. Kind table

| Kind | Source | Scope | First snapshot today | Desired | Required baseline |
|------|--------|-------|----------------------|---------|-------------------|
| `title_tier` | Snapshot `world->titles` | Character | No event until tier rises vs persisted `titles` | Keep suppress-until-delta (Live or ingest; non-flood) | `titles` map (exists) |
| `level_up` | Snapshot level | Character | No event until level rises vs `last_known_level` | Keep | `last_known_level` (exists) |
| `map_enter` | Snapshot `GetMapID` | Character | Emits when prior map missing or changed | Keep visit emit (one event, not flood) | `last_map_id` (exists) |
| `map_unlock` | Snapshot `unlocked_map` bits | Character | Emit-all vs empty prior from events | Per-family seal via BaselineTransition; emit only post-seal deltas | Map-unlock id set + family sealed flag |
| `skill_unlock` | Snapshot `unlocked_character_skills` | Character | Emit-all catch-up | Same | Skill-unlock id set + family sealed flag |
| `account_skill_unlock` | Snapshot account skill list | Account semantic, stored under active character | Emit-all under that character | **Account-family** seal; emit delta under observing character only | Account skill id set + account-family sealed flag |
| `hero_unlock` | Snapshot `hero_info` | Character | Emit-all catch-up | Same as map_unlock | Hero id set + family sealed flag |
| `profession_unlock` | Snapshot profession bits | Character | Emit-all catch-up | Same | Profession id set + family sealed flag |
| `hard_mode_unlock` | Snapshot flag | Character | Emit if unlocked and kind absent | Per-family seal; emit only false→true after seal | HM unlocked bool + family sealed flag |
| `vanquish_area` | Snapshot vanquish bits | Character | Emit-all catch-up | Same as map_unlock | Vanquish map id set + family sealed flag |
| `cartography_threshold` | Snapshot fog % | Character | Catch-up all thresholds from 0 | Per-family seal; emit only newly crossed thresholds after seal | Single sealed prior % (no dual mirror) |
| `skill_point_threshold` | Snapshot earned SP | Character | Catch-up from 0 via events | Per-family seal; W5 snapshot prior | Sealed flag + `skill_points_earned` |
| `faction_threshold` | Snapshot earned factions | Character | Catch-up from 0 | Per-family seal; W5 snapshot prior | Sealed flag + `faction_totals` |
| `hom_points` | Async HoM snapshot | Character envelope | First sample treats previous as 0 → emit all non-zero | HoM-family BaselineTransition; seal-before-delta | Sealed flag + `hall_of_monuments` |
| `mission_complete` | UI game message | Character | Event-only | Keep separate timed path | None |
| `dungeon_complete` | UI game message | Character | Event-only | Keep | None |
| `vanquish_complete` | UI game message | Character | Event-only | Keep | None |

**Massive baseline risk today:** unlock / vanquish / threshold / hom flood families above.

**Timed / low flood risk:** timed clears, `map_enter`, `title_tier`, `level_up`.

---

## 3. Proposed domain / store model

### 3.1 Persisted states vs transitions

| Name | Kind | Meaning |
|------|------|---------|
| `Unset` | **Persisted** enum | Family never sealed |
| `Sealed` | **Persisted** enum | Baseline captured; no milestone implied by seal itself |
| `DeltaObserved` | **Transition/result only** — **not** a third persisted state | After seal, observation contains new ids/amounts → append events and advance baseline |

Do **not** encode Sealed as a journey event.

### 3.2 Flood families (C2)

Seal independently (`Unset` | `Sealed`, optional audit timestamp for debugging only):

| Family | Scope | Covers kinds / priors |
|--------|-------|------------------------|
| `maps` | Character | `map_unlock` |
| `character_skills` | Character | `skill_unlock` |
| `heroes` | Character | `hero_unlock` |
| `professions` | Character | `profession_unlock` |
| `hard_mode` | Character | `hard_mode_unlock` |
| `vanquish` | Character | `vanquish_area` |
| `cartography` | Character | `cartography_threshold` |
| `skill_points` | Character | `skill_point_threshold` |
| `faction` | Character | `faction_threshold` |
| `hom` | Character envelope | `hom_points` |
| `account_skills` | Account | `account_skill_unlock` |

**Rejected:** a single sticky `journeyBaselineSealedAt`.

### 3.3 Raw observation payload (A7)

Each flood family observation carried from Live → ingest must include at least:

- current value (id-set / scalar / flag as appropriate);
- `context_available` (required world/account pointers present for that family);
- C3 eligibility inputs (enough for stability / shrink checks — e.g. sample usable flag).

Live must **not** attach pre-built flood journey events for those families.

### 3.4 Suggested persisted fields (additive)

On `StoredCharacter`:

- Per character flood family: `*BaselineState` = `Unset` | `Sealed` (+ optional audit ts)
- Unlock id sets for maps / character skills / heroes / professions / vanquish
- `hardModeBaseline` bool only meaningful when `hard_mode` is `Sealed`
- Threshold / HoM: **no duplicate amount fields** (W5)

On `AccountProgressStore`:

- `accountSkillsBaselineState` = `Unset` | `Sealed` (+ optional audit ts)
- `baselineAccountSkills` id set

Existing: `titles`, `last_known_level`, `last_map_id`, `hall_of_monuments`, `skill_points_earned`, `faction_totals`, `journey_events`.

### 3.5 Transient C3 candidate ownership (A8)

Stability across consecutive samples needs **non-persisted** process state:

| Field | Purpose |
|-------|---------|
| Family id | Which flood family |
| Identity scope | Character families: `(account_key, character_key, family)`; account skills: `(account_key, family)` |
| Candidate value / id-set | Last eligible observation being stabilized |
| Stable sample count (or equivalent) | How many consecutive matching eligible samples |

**Lifecycle:**

- Isolated by identity scope above — **no** cross-character process-global/static candidate.
- Account or character switch must drop / not reuse another identity’s candidate.
- Missing or invalid world context for that family **breaks or resets** the stability streak.
- Process restart rebuilds candidates from scratch.
- Persisted `Sealed` must **not** revert to `Unset` because candidates were lost.

**Unset window tradeoff (accepted):** any change observed while still `Unset` is folded into the eventual sealed baseline and is **not** emitted later. Journey follow for that family truly begins at successful seal. Document and test this explicitly.

### 3.6 Incomplete first-tick gating (C3) — C++ merge blocker

Bitset / list completeness on first Persistent tick remains empirically **unknown**. Implementation must still ship conservative policy; **C++ merge blocked** until coded and tested:

1. Do not seal when `context_available` is false.
2. No seal-on-shrink: suspicious empty/near-empty vs prior non-empty working candidate → stay `Unset`, emit nothing.
3. Seal only after stable consecutive eligible samples (exact N conservative; tune in-game).
4. While `Unset`, **never** emit-all against empty event priors.
5. Partial/uncertain → `Unset`; never invent catch-up milestones.

### 3.7 W5 — Threshold / HoM single-source + read-before-write

Do **not** dual-write mirrored baseline amounts beside snapshots.

| Family | Prior after seal |
|--------|------------------|
| `skill_points` | `skill_points_earned` |
| `faction` | `faction_totals` |
| `hom` | `hall_of_monuments` |
| `cartography` | Single sealed prior percent for that family (no snapshot field today) |

**Normal sealed ingest order** for threshold/HoM families:

1. Copy **previous** snapshot prior (or sealed cartography prior) into a local variable.
2. Compute delta vs current observation using that copy.
3. Append journey events.
4. Write current snapshot / baseline forward.

The builder must **never** re-read a snapshot field that this same ingest already overwrote as its prior.

### 3.8 Sealing and delta (post-A7)

All flood seal/delta decisions run inside `BaselineTransition` during ingest (§1.2). After `Sealed`, unlock families use persisted id sets as `previous_*`; threshold/HoM use W5 priors; fingerprint dedupe via `AppendUniqueJourneyEvents` remains.

---

## 4. JSON codec and version compatibility (A10)

| Item | Decision |
|------|----------|
| Format id | Keep `gwtoolbox-quest-progress` |
| Version | **Minor bump** `1.1 → 1.2` for additive per-family baseline fields |
| Unknown JSON keys | Current glaze read opts use `error_on_unknown_keys = false` — extra fields alone are tolerated by the parser |
| Newer minor reject | Codec **explicitly rejects** `store_version.minor >` supported minor (treated as unsupported; not auto-migrated) |
| New 1.2 build → old 1.0/1.1 files | Reads and migrates **without data loss**; family states absent → `Unset` |
| Old 1.1 build → 1.2 file | **Rejects** the store (newer minor). **No automatic rollback compatibility** |
| `.bak` | Must **not** be used as an automatic downgrade path when a newer minor is rejected |
| Journey event schema | Unchanged |
| Do not | Rewrite or delete existing `journey_events` during codec migrate |

Minor stays **1.2**; do **not** claim that a legacy 1.1 reader accepts 1.2 files. Forward compatibility (new reader / old file) yes; backward reader compatibility no.

### A9 / W4 — Full bootstrap algorithm (first successful ingest for `Unset` family)

Codec migrate `1.1 → 1.2` only introduces empty/`Unset` fields. Catch-up prevention is entirely in **BaselineTransition** on first gated bootstrap ingest.

For an unlock/vanquish/hero/profession/account/hard_mode family in `Unset`:

1. If observation fails C3 / context gates → remain `Unset`; **no events**; no seal.
2. Else compute:
   - `legacy =` ids (or flag) reconstructed from existing journey evidence for that family (may be empty or partial);
   - `current =` gated observation value;
   - `sealed_baseline = legacy ∪ current` (for flags: sealed to observed unlocked state without emitting).
3. Mark family `Sealed` with that baseline.
4. Emit **zero** journey events on this bootstrap ingest — even when `current` is a strict superset of partial `legacy`.
5. **Normal delta builder must not run** for that family before seal completes in this ingest.
6. Only a **later** sealed observation that introduces ids/flags beyond `sealed_baseline` may export deltas.

Equivalent for threshold/HoM families on bootstrap:

- If gated observation OK: seal using W5 snapshot prior policy (initialize snapshot from current observation if needed **after** deciding no catch-up events); emit **zero** threshold/hom catch-up events.
- Missing/unstable → no seal, no events.
- Subsequent sealed ingest uses §3.7 read-before-write.

**Forbidden:** running normal delta/emit-all for an `Unset` family with empty or partial legacy prior as if it were already sealed.

---

## 5. Integration points (A7)

| Component | Change (planned) |
|-----------|------------------|
| `QuestProgressLive` | Rename/reshape to raw observation sampler for flood families; keep non-flood title/level/map_enter as today or move equivalently; **no flood event creation** |
| Pure `BaselineTransition` / journey helpers | Bootstrap (A9), C3 gate, seal/delta, event build; unit-testable without GWCA |
| `QuestProgressService::IngestJourneySnapshot` | Own the single transactional BaselineTransition loop; persist baselines + snapshots + events |
| `IngestHomSnapshot` | Same transition for `hom` family |
| Transient C3 candidates (A8) | Service- or window-owned map keyed by identity scope; cleared on identity switch / bad context |
| `QuestProgressStore` merge | Per-family `Sealed` sticky; union unlock id sets; never drop events; never unseal because candidates missing |
| `QuestProgressJsonCodec` | 1.2 fields; migrate → `Unset`; A10 reject semantics unchanged in spirit |
| `QuestProgressContractExporter` | No Contract schema change; fewer false milestones; no unlock inventory arrays (C1) |
| Timed hint path | Unchanged |
| `QuestProgressReducer` | No journey work |
| `QuestTrackerWindow` | Call observation sampler + ingest only; no automation |

---

## 6. Contract export consequences

- Contract **v1 document unchanged**.
- Producer emits fewer first-track flood milestones.
- Snapshot fields still carry lifetime state where they exist.
- **C1:** no unlock inventory snapshot arrays in this phase.
- `account_skill_unlock` deltas still nest under the observing character; account-family baseline prevents re-flood.
- Fingerprints unchanged.
- Pre-policy noisy events remain exportable; display = W6 consumer follow-up.

---

## 7. Wayfarer UX consequences (W6)

**Toolbox plan non-goal:** no Wayfarer / Codex code here.

Observed today: dossier timeline `entries.take(40)`.

**Codex follow-up:** display collapse for already-imported unlock bursts; observation-time copy; **no DB delete/rewrite**.

---

## 8. Migration and backward compatibility

| Scenario | Behavior |
|----------|----------|
| New character, empty store | Per-family bootstrap seal on first gated observation; 0 flood events; non-flood as today |
| Veteran first Toolbox session | Bootstrap seal (`∅ ∪ current`); 0 flood events |
| Store wipe | Re-bootstrap; C1 inventory gap accepted |
| Upgrade 1.1 → 1.2 with partial legacy unlock events | Keep events; A9 `legacy ∪ current` seal; 0 new catch-up |
| Character / account switch | Drop foreign C3 candidates; no baseline bleed; account_skills shared per account |
| Missing / unstable context | `Unset`; no seal; no events |
| Old 1.1 Toolbox opens 1.2 store | Reject (A10); no auto `.bak` downgrade |
| Already-imported Codex noise | W6 display collapse only |

---

## 9. Scenario matrix

| Scenario | Expected |
|----------|----------|
| New character first session | Gated bootstrap seal; 0 flood events; later deltas only |
| Old character first session | Same; no mass milestones |
| Restart with sealed store | Candidates rebuild; `Sealed` persists; quiet unless delta |
| Restart without store | Re-bootstrap |
| Old store upgrade | A9 bootstrap on first ingest |
| Identity switch | No candidate/baseline leakage |
| Unset-window changes | Absorbed into baseline; no later replay (A8 tradeoff) |
| Return after offline play | Post-seal deltas only |

---

## 10. Test matrix

Pure logic:

1. Unlock family: gated bootstrap seals, **0** events; next observation with +1 id → exactly **1** event.
2. Independent family sealing (maps sealed, skills still `Unset`).
3. Account skills: account-family seal; no cross-character re-flood.
4. Thresholds: W5 read-before-write; sealed crossing emits correct new events using pre-write prior.
5. HoM: first gated fetch seals with 0 `hom_points` flood; later increase emits.
6. C3: shrink/empty/unstable → no seal, no emit-all; streak reset on bad context.
7. **A9:** legacy events proper subset of current unlocks → bootstrap 0 events; baseline = union.
8. **A9:** 1.1 family with no legacy events → gated seal from current; 0 events.
9. **A8:** unstable candidate changes during `Unset` fold into baseline; no later replay of those ids.
10. Identity switch: no candidate/baseline leakage across characters/accounts.
11. Title/level/map_enter + timed clears regressions green.
12. Codec: 1.2 reads/migrates 1.1; asserting A10 reject of newer minor remains; round-trip keeps `journey_events`.
13. Merge: `Sealed` sticky; unlock sets unioned; events kept.

**Unknown empirically:** real GWCA first-tick completeness — in-game checklist after C3 lands.

---

## 11. Implementation slices (order)

1. **Domain + codec 1.2** — per-family persisted `Unset|Sealed`, unlock sets, migrate to `Unset`, A10 semantics documented/tested.
2. **Raw observation types + Live reshape (A7)** — Live stops emitting flood events.
3. **Pure BaselineTransition** — A9 bootstrap + C3 + delta; unit tests (merge blocker with C3/W4/A9).
4. **Transient C3 candidates (A8)** — identity-scoped lifecycle + switch reset tests.
5. **Service ingest wiring** — single transactional `IngestJourneySnapshot` / HoM path.
6. **Threshold/cartography/HM W5** read-before-write.
7. **Account skill family** on account store.
8. **Store merge** per-family rules.
9. **In-game verification** checklist.
10. **Codex display collapse** — separate repo (W6).

Do not combine with unlock-array Contract expansions or automation.

---

## 12. Risks and open questions

### Decided

- Today: Live builds flood events before ingest; empty event-derived priors → emit-all.
- Title/level suppress-until-delta; timed clears separate.
- Store minor 1.1; journey has no source/confidence.
- Account skills sampled from account, stored under active character events.
- Wayfarer truncates to 40 entries.
- **C1–C3, W4–W6** as previously amended.
- **A7:** Live = raw flood observations; service BaselineTransition = bootstrap → C3 → seal/delta → events → persist in one ingest; Live must not flood-build.
- **A8:** transient identity-scoped candidates; persisted enum only `Unset|Sealed`; `DeltaObserved` is transition-only; Unset-window changes fold into baseline (accepted tradeoff).
- **A9:** bootstrap sealed baseline ≥ `legacy ∪ current`; 0 events on bootstrap; no normal delta builder before seal.
- **A10:** 1.2 reads 1.0/1.1; 1.1 rejects 1.2; no automatic `.bak` downgrade; unknown keys tolerated but newer minor still rejected.
- Contaminated-store hygiene ≠ journey baseline.

### Unknown (empirical / not invent)

- Unlock bitset/list completeness on first Persistent tick (C3 policy still required).
- Temporary omission in `hero_info` / profession masks.
- HoM async vs live observation ordering races.
- Exact stability N / equality parameters (conservative default; tune in-game).
- Compact on-disk bitset encoding.
- Whether `map_enter` first sample should change (recommended keep).

### Risks

- Over-conservative C3 delays first legitimate post-seal deltas (accepted vs false catch-up).
- Unset-window absorption permanently hides pre-seal unlock timing (accepted A8/C1 tradeoff).
- Legacy noisy timestamps remain on disk (W6 display-only).
- Account baseline persistence must not break accountKey validation.
- Operators on mixed Toolbox versions: 1.1 cannot open 1.2 stores (A10) — communicate in release notes when implementing.

---

## 13. Explicit non-goals

- Production C++ in this documentation PR
- Contract v1 schema / meaning / version changes (including unlock inventory arrays)
- Automatic deletion/rewrite of journey evidence
- Codex/Wayfarer code in this Toolbox plan
- DB delete of imported journey rows
- Automatic store downgrade / `.bak` rollback mechanism
- Quest/combat/movement/reward automation
- Festival hats, deaths, gold-as-progress, PvP W/L journey kinds
- Implementation merge without senior architecture approval and without A7/A8/A9/C3 gates satisfied

---

## 14. Answers to posed questions (summary)

1. **Snapshot-derived flood kinds** vs **timed UI clears** — see §2.
2. **Mass baseline risk** — unlock/vanquish/threshold/hom families.
3. **Scopes** — account_skills account-family; others character / HoM envelope.
4. **Store today** — cannot distinguish never-observed / baseline-without-milestone / new-since-baseline; events double as prior.
5. **Needed** — per-family sealed state + unlock sets; thresholds sealed + snapshots; raw observation + BaselineTransition (A7).
6. **Version** — minor 1.2; forward read of old files; no old-reader accept of 1.2 (A10).
7. **Old files** — 1.2 migrates 1.0/1.1; A9 bootstrap on first ingest; keep events.
8. **Existing noise** — retain; W6 display collapse only.
9. **Account unlocks** — account-family baseline; deltas under observing character.
10. **Append-only** — seal without events; deltas append; history never erased.
11. **Scenarios** — §9.
12. **Tests** — §10.
