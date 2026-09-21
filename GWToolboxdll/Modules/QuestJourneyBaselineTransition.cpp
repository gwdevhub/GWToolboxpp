#include <Modules/QuestJourneyBaselineTransition.h>

#include <Modules/QuestProgressDomain.h>

#include <algorithm>
#include <map>

namespace QuestProgress {
namespace {

bool ObservationEligible(
    bool context_available,
    bool sample_usable,
    std::string_view observed_at_utc)
{
    return context_available
        && sample_usable
        && IsCanonicalUtcTimestamp(observed_at_utc);
}

bool IsSortedUniqueSubset(
    const std::vector<uint32_t>& maybe_subset,
    const std::vector<uint32_t>& of_set)
{
    return std::includes(
        of_set.begin(),
        of_set.end(),
        maybe_subset.begin(),
        maybe_subset.end());
}

void UnionSortedUniqueInto(std::vector<uint32_t>& into, const std::vector<uint32_t>& add)
{
    into.insert(into.end(), add.begin(), add.end());
    CanonicalizeSortedUniqueIds(into);
}

void StripZeroIds(std::vector<uint32_t>& ids)
{
    ids.erase(std::remove(ids.begin(), ids.end(), 0u), ids.end());
    CanonicalizeSortedUniqueIds(ids);
}

std::map<uint32_t, bool> PriorMapFromIds(const std::vector<uint32_t>& ids)
{
    std::map<uint32_t, bool> out;
    for (const auto id : ids) {
        if (id != 0) {
            out[id] = true;
        }
    }
    return out;
}

std::vector<uint32_t> LegacyIdsFromEvents(
    const std::vector<JourneyEventRecord>& events,
    std::string_view kind)
{
    std::vector<uint32_t> out;
    const auto prior = PriorIdsFromJourneyEvents(events, kind);
    out.reserve(prior.size());
    for (const auto& [id, seen] : prior) {
        if (seen && id != 0) {
            out.push_back(id);
        }
    }
    CanonicalizeSortedUniqueIds(out);
    return out;
}

void BreakIdSetStreak(IdSetBaselineCandidate& candidate)
{
    candidate.active = false;
    candidate.ids.clear();
    candidate.consecutive_matches = 0;
}

void BreakFlagStreak(FlagBaselineCandidate& candidate)
{
    candidate.active = false;
    candidate.value = false;
    candidate.consecutive_matches = 0;
}

IdSetBaselineTransitionResult TransitionIdSetUnset(
    const RawIdSetFamilyObservation& observation,
    const IdSetJourneyBaseline& previous_baseline,
    const IdSetBaselineCandidate& previous_candidate,
    std::string_view observed_at_utc)
{
    IdSetBaselineTransitionResult out;
    out.baseline = previous_baseline;
    out.candidate = previous_candidate;

    if (!ObservationEligible(
            observation.context_available,
            observation.sample_usable,
            observed_at_utc)) {
        BreakIdSetStreak(out.candidate);
        return out;
    }

    auto current = observation.value;
    CanonicalizeSortedUniqueIds(current);
    StripZeroIds(current);

    if (current.empty()) {
        BreakIdSetStreak(out.candidate);
        return out;
    }

    if (!out.candidate.seen_union.empty()
        && !IsSortedUniqueSubset(out.candidate.seen_union, current)) {
        BreakIdSetStreak(out.candidate);
        return out;
    }

    UnionSortedUniqueInto(out.candidate.seen_union, current);

    if (out.candidate.active && out.candidate.ids == current) {
        ++out.candidate.consecutive_matches;
    }
    else {
        out.candidate.active = true;
        out.candidate.ids = current;
        out.candidate.consecutive_matches = 1;
    }

    if (out.candidate.consecutive_matches < kJourneyBaselineStableSampleCount) {
        return out;
    }

    auto sealed_ids = out.candidate.seen_union;
    UnionSortedUniqueInto(sealed_ids, current);
    out.baseline.state = JourneyBaselineSealState::Sealed;
    out.baseline.ids = std::move(sealed_ids);
    BreakIdSetStreak(out.candidate);
    out.candidate.seen_union.clear();
    return out;
}

IdSetBaselineTransitionResult TransitionIdSetSealed(
    const RawIdSetFamilyObservation& observation,
    const IdSetJourneyBaseline& previous_baseline,
    const IdSetBaselineCandidate& previous_candidate,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view kind,
    JourneyUnlockIdKind id_kind,
    std::string_view observed_at_utc)
{
    IdSetBaselineTransitionResult out;
    out.baseline = previous_baseline;
    out.candidate = previous_candidate;
    BreakIdSetStreak(out.candidate);
    out.candidate.seen_union.clear();

    if (!ObservationEligible(
            observation.context_available,
            observation.sample_usable,
            observed_at_utc)) {
        return out;
    }

    auto current = observation.value;
    CanonicalizeSortedUniqueIds(current);
    StripZeroIds(current);

    if (!IsSortedUniqueSubset(out.baseline.ids, current)) {
        return out;
    }

    auto prior = PriorMapFromIds(out.baseline.ids);
    for (const auto id : LegacyIdsFromEvents(existing_events, kind)) {
        prior[id] = true;
    }
    out.new_events = BuildNewlySeenIdEvents(
        kind,
        id_kind,
        prior,
        current,
        existing_events,
        observed_at_utc);
    UnionSortedUniqueInto(out.baseline.ids, current);
    out.baseline.state = JourneyBaselineSealState::Sealed;
    return out;
}

} // namespace

IdSetBaselineTransitionResult TransitionIdSetJourneyBaseline(
    const RawIdSetFamilyObservation& observation,
    const IdSetJourneyBaseline& previous_baseline,
    const IdSetBaselineCandidate& previous_candidate,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view kind,
    JourneyUnlockIdKind id_kind,
    std::string_view observed_at_utc)
{
    if (previous_baseline.state == JourneyBaselineSealState::Sealed) {
        return TransitionIdSetSealed(
            observation,
            previous_baseline,
            previous_candidate,
            existing_events,
            kind,
            id_kind,
            observed_at_utc);
    }
    return TransitionIdSetUnset(
        observation,
        previous_baseline,
        previous_candidate,
        observed_at_utc);
}

FlagBaselineTransitionResult TransitionHardModeJourneyBaseline(
    const RawFlagFamilyObservation& observation,
    const FlagJourneyBaseline& previous_baseline,
    const FlagBaselineCandidate& previous_candidate,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    FlagBaselineTransitionResult out;
    out.baseline = previous_baseline;
    out.candidate = previous_candidate;

    if (previous_baseline.state == JourneyBaselineSealState::Sealed) {
        BreakFlagStreak(out.candidate);
        if (out.baseline.unlocked) {
            return out;
        }
        if (!ObservationEligible(
                observation.context_available,
                observation.sample_usable,
                observed_at_utc)) {
            return out;
        }
        const bool legacy = HasJourneyKind(existing_events, "hard_mode_unlock");
        out.new_events = BuildHardModeUnlockEvents(
            out.baseline.unlocked || legacy,
            observation.value,
            existing_events,
            observed_at_utc);
        if (observation.value || legacy) {
            out.baseline.unlocked = true;
        }
        out.baseline.state = JourneyBaselineSealState::Sealed;
        return out;
    }

    if (!ObservationEligible(
            observation.context_available,
            observation.sample_usable,
            observed_at_utc)) {
        BreakFlagStreak(out.candidate);
        return out;
    }

    if (observation.value) {
        out.candidate.seen_true = true;
    }

    if (out.candidate.active && out.candidate.value == observation.value) {
        ++out.candidate.consecutive_matches;
    }
    else {
        out.candidate.active = true;
        out.candidate.value = observation.value;
        out.candidate.consecutive_matches = 1;
    }

    if (out.candidate.consecutive_matches < kJourneyBaselineStableSampleCount) {
        return out;
    }

    const bool legacy = HasJourneyKind(existing_events, "hard_mode_unlock");
    out.baseline.state = JourneyBaselineSealState::Sealed;
    out.baseline.unlocked = legacy || out.candidate.seen_true || observation.value;
    BreakFlagStreak(out.candidate);
    out.candidate.seen_true = false;
    return out;
}

} // namespace QuestProgress
