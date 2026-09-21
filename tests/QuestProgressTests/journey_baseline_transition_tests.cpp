#include <Modules/QuestJourneyBaselineTransition.h>
#include <Modules/QuestProgressDomain.h>

#include "test_assert.h"

#include <string>
#include <vector>

using namespace QuestProgress;

namespace {

constexpr const char* kTs = "2026-09-13T10:00:00.000Z";
constexpr const char* kTs2 = "2026-09-13T10:01:00.000Z";
constexpr const char* kTs3 = "2026-09-13T10:02:00.000Z";
constexpr const char* kTs4 = "2026-09-13T11:00:00.000Z";
constexpr const char* kBadTs = "2026-09-13T10:00:00";

RawIdSetFamilyObservation UsableIds(std::vector<uint32_t> ids)
{
    return MakeRawIdSetFamilyObservation(true, true, std::move(ids));
}

RawIdSetFamilyObservation UnusableIds()
{
    return MakeRawIdSetFamilyObservation(true, false, {1, 2});
}

RawIdSetFamilyObservation MissingContextIds()
{
    return MakeRawIdSetFamilyObservation(false, true, {1, 2});
}

RawFlagFamilyObservation UsableFlag(bool value)
{
    return MakeRawFlagFamilyObservation(true, true, value);
}

IdSetBaselineTransitionResult StepMaps(
    const RawIdSetFamilyObservation& observation,
    const IdSetJourneyBaseline& baseline,
    const IdSetBaselineCandidate& candidate,
    const std::vector<JourneyEventRecord>& history,
    std::string_view observed_at)
{
    return TransitionIdSetJourneyBaseline(
        observation,
        baseline,
        candidate,
        history,
        "map_unlock",
        JourneyUnlockIdKind::Map,
        observed_at);
}

IdSetBaselineTransitionResult SealMapsAfterThree(
    const std::vector<uint32_t>& ids,
    const std::vector<JourneyEventRecord>& history = {})
{
    IdSetJourneyBaseline baseline{};
    IdSetBaselineCandidate candidate{};
    IdSetBaselineTransitionResult step{};
    for (int i = 0; i < 3; ++i) {
        step = StepMaps(UsableIds(ids), baseline, candidate, history, kTs);
        baseline = step.baseline;
        candidate = step.candidate;
        Expect(step.new_events.empty(), "seal_three_no_events");
    }
    return step;
}

void TestVeteranStableSealZeroEvents()
{
    const auto sealed = SealMapsAfterThree({10, 20, 30});
    Expect(sealed.baseline.state == JourneyBaselineSealState::Sealed, "veteran_sealed");
    Expect(sealed.baseline.ids.size() == 3, "veteran_ids");
    Expect(sealed.baseline.ids[0] == 10 && sealed.baseline.ids[2] == 30, "veteran_sorted");
    Expect(sealed.new_events.empty(), "veteran_zero_events");
    Expect(!sealed.candidate.active, "veteran_candidate_cleared");
}

void TestPartialLegacyUnionZeroEvents()
{
    std::vector<JourneyEventRecord> history;
    JourneyEventRecord legacy;
    legacy.kind = "map_unlock";
    legacy.subject_key = BuildMapSubjectKey(10);
    legacy.observed_at = "2026-01-01T00:00:00.000Z";
    legacy.map_id = 10;
    history.push_back(legacy);

    const auto sealed = SealMapsAfterThree({10, 20}, history);
    Expect(sealed.baseline.state == JourneyBaselineSealState::Sealed, "legacy_sealed");
    Expect(sealed.baseline.ids.size() == 2, "legacy_union_size");
    Expect(sealed.baseline.ids[0] == 10 && sealed.baseline.ids[1] == 20, "legacy_union_ids");
    Expect(sealed.new_events.empty(), "legacy_zero_events");
}

void TestUnsetGrowthAbsorbed()
{
    IdSetJourneyBaseline baseline{};
    IdSetBaselineCandidate candidate{};
    auto step = StepMaps(UsableIds({1}), baseline, candidate, {}, kTs);
    baseline = step.baseline;
    candidate = step.candidate;
    Expect(baseline.state == JourneyBaselineSealState::Unset, "growth_still_unset_1");

    step = StepMaps(UsableIds({1, 2}), baseline, candidate, {}, kTs);
    baseline = step.baseline;
    candidate = step.candidate;
    Expect(candidate.ids.size() == 2, "growth_candidate_restart");
    Expect(candidate.consecutive_matches == 1, "growth_streak_reset");
    Expect(candidate.seen_union.size() == 2, "growth_seen_union");

    step = StepMaps(UsableIds({1, 2}), baseline, candidate, {}, kTs);
    baseline = step.baseline;
    candidate = step.candidate;
    step = StepMaps(UsableIds({1, 2}), baseline, candidate, {}, kTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Sealed, "growth_absorbed_seal");
    Expect(step.baseline.ids.size() == 2, "growth_absorbed_ids");
    Expect(step.new_events.empty(), "growth_absorbed_no_events");
}

void TestNoSealOnShrinkEmptyUnusableContextBadTs()
{
    IdSetJourneyBaseline baseline{};
    IdSetBaselineCandidate candidate{};
    auto step = StepMaps(UsableIds({5, 6}), baseline, candidate, {}, kTs);
    baseline = step.baseline;
    candidate = step.candidate;
    step = StepMaps(UsableIds({5, 6}), baseline, candidate, {}, kTs);
    baseline = step.baseline;
    candidate = step.candidate;
    Expect(candidate.consecutive_matches == 2, "pre_break_streak");

    step = StepMaps(UsableIds({5}), baseline, candidate, {}, kTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "shrink_no_seal");
    Expect(step.candidate.consecutive_matches == 0, "shrink_breaks_streak");
    Expect(step.candidate.seen_union.size() == 2, "shrink_keeps_seen_union");
    Expect(step.new_events.empty(), "shrink_no_events");
    baseline = step.baseline;
    candidate = step.candidate;

    step = StepMaps(UsableIds({5}), baseline, candidate, {}, kTs);
    step = StepMaps(UsableIds({5}), step.baseline, step.candidate, {}, kTs);
    step = StepMaps(UsableIds({5}), step.baseline, step.candidate, {}, kTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "smaller_repeat_no_seal");

    step = StepMaps(UsableIds({}), baseline, candidate, {}, kTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "empty_no_seal");
    Expect(step.candidate.consecutive_matches == 0, "empty_breaks");

    step = StepMaps(UnusableIds(), baseline, candidate, {}, kTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "unusable_no_seal");
    Expect(step.candidate.consecutive_matches == 0, "unusable_breaks");

    step = StepMaps(MissingContextIds(), baseline, candidate, {}, kTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "missing_ctx_no_seal");
    Expect(step.candidate.consecutive_matches == 0, "missing_ctx_breaks");

    step = StepMaps(UsableIds({5, 6}), baseline, candidate, {}, kBadTs);
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "bad_ts_no_seal");
    Expect(step.candidate.consecutive_matches == 0, "bad_ts_breaks");
}

void TestFamilyIndependence()
{
    IdSetJourneyBaseline maps{};
    IdSetBaselineCandidate maps_c{};
    IdSetJourneyBaseline heroes{};
    IdSetBaselineCandidate heroes_c{};

    auto map_step = StepMaps(UnusableIds(), maps, maps_c, {}, kTs);
    auto hero_step = TransitionIdSetJourneyBaseline(
        UsableIds({7}),
        heroes,
        heroes_c,
        {},
        "hero_unlock",
        JourneyUnlockIdKind::Hero,
        kTs);
    Expect(map_step.baseline.state == JourneyBaselineSealState::Unset, "indep_maps_unset");
    Expect(hero_step.candidate.consecutive_matches == 1, "indep_heroes_progress");

    heroes = hero_step.baseline;
    heroes_c = hero_step.candidate;
    hero_step = TransitionIdSetJourneyBaseline(
        UsableIds({7}), heroes, heroes_c, {}, "hero_unlock", JourneyUnlockIdKind::Hero, kTs);
    heroes = hero_step.baseline;
    heroes_c = hero_step.candidate;
    hero_step = TransitionIdSetJourneyBaseline(
        UsableIds({7}), heroes, heroes_c, {}, "hero_unlock", JourneyUnlockIdKind::Hero, kTs);
    Expect(hero_step.baseline.state == JourneyBaselineSealState::Sealed, "indep_heroes_seal");
    Expect(map_step.baseline.state == JourneyBaselineSealState::Unset, "indep_maps_still_unset");
}

void TestPostSealDeltaAndDedupe()
{
    auto sealed = SealMapsAfterThree({10});
    Expect(sealed.baseline.state == JourneyBaselineSealState::Sealed, "delta_pre_sealed");

    const auto history_before = std::vector<JourneyEventRecord>{};
    auto step = StepMaps(UsableIds({10, 11}), sealed.baseline, sealed.candidate, history_before, kTs4);
    Expect(step.new_events.size() == 1, "delta_one_event");
    Expect(step.new_events[0].kind == "map_unlock", "delta_kind");
    Expect(step.new_events[0].map_id == 11, "delta_id");
    Expect(step.new_events[0].subject_key == BuildMapSubjectKey(11), "delta_subject");
    Expect(step.new_events[0].observed_at == kTs4, "delta_ts");
    Expect(IsCanonicalUtcTimestamp(step.new_events[0].observed_at), "delta_canonical_ts");
    Expect(step.baseline.ids.size() == 2, "delta_baseline_grew");

    auto again = StepMaps(UsableIds({10, 11}), step.baseline, step.candidate, step.new_events, kTs4);
    Expect(again.new_events.empty(), "delta_repeat_deduped");
    Expect(again.baseline.ids.size() == 2, "delta_repeat_ids");

    auto shrink = StepMaps(UsableIds({10}), again.baseline, again.candidate, step.new_events, kTs4);
    Expect(shrink.new_events.empty(), "delta_shrink_no_events");
    Expect(shrink.baseline.ids.size() == 2, "delta_shrink_keeps_baseline");

    auto restore = StepMaps(
        UsableIds({10, 11}),
        shrink.baseline,
        shrink.candidate,
        step.new_events,
        kTs4);
    Expect(restore.new_events.empty(), "delta_restore_no_dup");
    Expect(restore.baseline.ids.size() == 2, "delta_restore_ids");
}

void TestInputsUnchangedAndOtherFieldsUntouched()
{
    IdSetJourneyBaseline maps{};
    IdSetBaselineCandidate maps_c{};
    FlagJourneyBaseline hard{};
    const auto hard_before = hard;
    std::vector<JourneyEventRecord> history;
    JourneyEventRecord keep;
    keep.kind = "level_up";
    keep.subject_key = BuildLevelSubjectKey(5);
    keep.observed_at = kTs;
    keep.level = 5;
    history.push_back(keep);
    const auto history_before = history;

    auto step = StepMaps(UsableIds({1}), maps, maps_c, history, kTs);
    Expect(history.size() == history_before.size(), "history_input_size_unchanged");
    Expect(history[0].kind == history_before[0].kind, "history_input_kind_unchanged");
    Expect(history[0].observed_at == history_before[0].observed_at, "history_input_ts_unchanged");
    Expect(hard.state == hard_before.state, "hard_mode_untouched");
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "maps_progress_only");
}

void TestHardModeFalseSealThenTrueOnce()
{
    FlagJourneyBaseline baseline{};
    FlagBaselineCandidate candidate{};
    FlagBaselineTransitionResult step{};
    for (int i = 0; i < 3; ++i) {
        step = TransitionHardModeJourneyBaseline(
            UsableFlag(false), baseline, candidate, {}, kTs);
        baseline = step.baseline;
        candidate = step.candidate;
        Expect(step.new_events.empty(), "hm_false_seal_no_events");
    }
    Expect(step.baseline.state == JourneyBaselineSealState::Sealed, "hm_false_sealed");
    Expect(!step.baseline.unlocked, "hm_false_unlocked");

    step = TransitionHardModeJourneyBaseline(
        UsableFlag(true), step.baseline, step.candidate, {}, kTs2);
    Expect(step.new_events.size() == 1, "hm_true_one_event");
    Expect(step.new_events[0].kind == "hard_mode_unlock", "hm_true_kind");
    Expect(step.baseline.unlocked, "hm_true_unlocked");

    auto again = TransitionHardModeJourneyBaseline(
        UsableFlag(true), step.baseline, step.candidate, step.new_events, kTs3);
    Expect(again.new_events.empty(), "hm_true_deduped");
    Expect(again.baseline.unlocked, "hm_stays_unlocked");

    auto drop = TransitionHardModeJourneyBaseline(
        UsableFlag(false), again.baseline, again.candidate, step.new_events, kTs4);
    Expect(drop.new_events.empty(), "hm_false_after_no_event");
    Expect(drop.baseline.unlocked, "hm_false_after_keeps_unlocked");
}

void TestHardModeLegacyTruePreserved()
{
    std::vector<JourneyEventRecord> history;
    JourneyEventRecord legacy;
    legacy.kind = "hard_mode_unlock";
    legacy.subject_key = BuildHardModeSubjectKey();
    legacy.observed_at = "2026-01-01T00:00:00.000Z";
    history.push_back(legacy);

    FlagJourneyBaseline baseline{};
    FlagBaselineCandidate candidate{};
    FlagBaselineTransitionResult step{};
    for (int i = 0; i < 3; ++i) {
        step = TransitionHardModeJourneyBaseline(
            UsableFlag(false), baseline, candidate, history, kTs);
        baseline = step.baseline;
        candidate = step.candidate;
        Expect(step.new_events.empty(), "hm_legacy_seal_no_events");
    }
    Expect(step.baseline.state == JourneyBaselineSealState::Sealed, "hm_legacy_sealed");
    Expect(step.baseline.unlocked, "hm_legacy_unlocked_true");

    auto later = TransitionHardModeJourneyBaseline(
        UsableFlag(true), step.baseline, step.candidate, history, kTs2);
    Expect(later.new_events.empty(), "hm_legacy_no_reunlock");
    Expect(later.baseline.unlocked, "hm_legacy_stays_true");
}

void TestZeroIdNeverSealsOrFalseDelta()
{
    IdSetJourneyBaseline baseline{};
    IdSetBaselineCandidate candidate{};
    IdSetBaselineTransitionResult step{};
    for (int i = 0; i < 3; ++i) {
        step = StepMaps(UsableIds({0}), baseline, candidate, {}, kTs);
        baseline = step.baseline;
        candidate = step.candidate;
        Expect(step.new_events.empty(), "zero_id_no_events");
    }
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "zero_id_no_seal");
    Expect(step.baseline.ids.empty(), "zero_id_baseline_empty");

    const uint32_t bit0_words[1] = {0x1u};
    const auto bit0_obs = AssembleRawIdSetBitsetObservation(true, bit0_words, 1, 1);
    Expect(bit0_obs.sample_usable, "zero_bitset_usable");
    Expect(bit0_obs.value.size() == 1 && bit0_obs.value[0] == 0, "zero_bitset_id0");
    for (int i = 0; i < 3; ++i) {
        step = StepMaps(bit0_obs, baseline, candidate, {}, kTs);
        baseline = step.baseline;
        candidate = step.candidate;
    }
    Expect(step.baseline.state == JourneyBaselineSealState::Unset, "zero_bitset_no_seal");

    step = SealMapsAfterThree({0, 42});
    Expect(step.baseline.state == JourneyBaselineSealState::Sealed, "mixed_zero_real_seals");
    Expect(step.baseline.ids.size() == 1 && step.baseline.ids[0] == 42, "mixed_zero_real_baseline");
    Expect(step.new_events.empty(), "mixed_zero_real_bootstrap_zero_events");

    step = SealMapsAfterThree({42});
    Expect(step.baseline.state == JourneyBaselineSealState::Sealed, "real_id_after_zero_seals");
    Expect(step.baseline.ids.size() == 1 && step.baseline.ids[0] == 42, "real_id_baseline");
    Expect(step.new_events.empty(), "real_id_bootstrap_zero_events");

    auto delta = StepMaps(UsableIds({42}), step.baseline, step.candidate, {}, kTs4);
    Expect(delta.new_events.empty(), "real_id_no_false_delta");
}

void TestSealedPartialSampleNoEmitNoExpand()
{
    auto sealed = SealMapsAfterThree({10, 20});
    Expect(sealed.baseline.state == JourneyBaselineSealState::Sealed, "partial_pre_sealed");
    Expect(sealed.baseline.ids.size() == 2, "partial_pre_ids");

    auto partial = StepMaps(UsableIds({10, 30}), sealed.baseline, sealed.candidate, {}, kTs4);
    Expect(partial.new_events.empty(), "partial_no_events");
    Expect(partial.baseline.ids.size() == 2, "partial_baseline_unchanged_size");
    Expect(partial.baseline.ids[0] == 10 && partial.baseline.ids[1] == 20, "partial_baseline_unchanged_ids");

    auto full = StepMaps(UsableIds({10, 20, 30}), partial.baseline, partial.candidate, {}, kTs4);
    Expect(full.new_events.size() == 1, "full_one_event");
    Expect(full.new_events[0].kind == "map_unlock", "full_kind");
    Expect(full.new_events[0].map_id == 30, "full_id_30");
    Expect(full.baseline.ids.size() == 3, "full_baseline_grew");
    Expect(full.baseline.ids[2] == 30, "full_baseline_has_30");
}

void TestLegacyOrphanDoesNotBlockPostSealDelta()
{
    std::vector<JourneyEventRecord> history;
    JourneyEventRecord orphan;
    orphan.kind = "map_unlock";
    orphan.subject_key = BuildMapSubjectKey(99);
    orphan.observed_at = "2026-01-01T00:00:00.000Z";
    orphan.map_id = 99;
    history.push_back(orphan);

    auto sealed = SealMapsAfterThree({10, 20}, history);
    Expect(sealed.baseline.state == JourneyBaselineSealState::Sealed, "orphan_sealed");
    Expect(sealed.baseline.ids.size() == 2, "orphan_live_inventory_size");
    Expect(sealed.baseline.ids[0] == 10 && sealed.baseline.ids[1] == 20, "orphan_live_inventory_ids");
    Expect(sealed.new_events.empty(), "orphan_bootstrap_zero_events");

    auto delta = StepMaps(UsableIds({10, 20, 30}), sealed.baseline, sealed.candidate, history, kTs4);
    Expect(delta.new_events.size() == 1, "orphan_one_event");
    Expect(delta.new_events[0].map_id == 30, "orphan_id_30");
    Expect(delta.baseline.ids.size() == 3, "orphan_baseline_grew");

    auto resurface = StepMaps(
        UsableIds({10, 20, 30, 99}),
        delta.baseline,
        delta.candidate,
        history,
        kTs4);
    Expect(resurface.new_events.empty(), "orphan_legacy_no_reflood");
    Expect(resurface.baseline.ids.size() == 4, "orphan_legacy_absorbed_live");
    Expect(resurface.baseline.ids[3] == 99, "orphan_legacy_in_inventory");
}

} // namespace

void RunJourneyBaselineTransitionTests()
{
    TestVeteranStableSealZeroEvents();
    TestPartialLegacyUnionZeroEvents();
    TestUnsetGrowthAbsorbed();
    TestNoSealOnShrinkEmptyUnusableContextBadTs();
    TestFamilyIndependence();
    TestPostSealDeltaAndDedupe();
    TestInputsUnchangedAndOtherFieldsUntouched();
    TestHardModeFalseSealThenTrueOnce();
    TestHardModeLegacyTruePreserved();
    TestZeroIdNeverSealsOrFalseDelta();
    TestSealedPartialSampleNoEmitNoExpand();
    TestLegacyOrphanDoesNotBlockPostSealDelta();
}
