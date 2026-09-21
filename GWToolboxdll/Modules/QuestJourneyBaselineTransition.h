#pragma once

#include <Modules/QuestCharacterJourney.h>
#include <Modules/QuestProgressJsonCodec.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace QuestProgress {

inline constexpr uint32_t kJourneyBaselineStableSampleCount = 3;

struct IdSetBaselineCandidate {
    bool active = false;
    std::vector<uint32_t> ids;
    std::vector<uint32_t> seen_union;
    uint32_t consecutive_matches = 0;

    friend bool operator==(const IdSetBaselineCandidate& a, const IdSetBaselineCandidate& b)
    {
        return a.active == b.active
            && a.ids == b.ids
            && a.seen_union == b.seen_union
            && a.consecutive_matches == b.consecutive_matches;
    }
};

struct FlagBaselineCandidate {
    bool active = false;
    bool value = false;
    bool seen_true = false;
    uint32_t consecutive_matches = 0;

    friend bool operator==(const FlagBaselineCandidate& a, const FlagBaselineCandidate& b)
    {
        return a.active == b.active
            && a.value == b.value
            && a.seen_true == b.seen_true
            && a.consecutive_matches == b.consecutive_matches;
    }
};

struct IdSetBaselineTransitionResult {
    IdSetJourneyBaseline baseline;
    IdSetBaselineCandidate candidate;
    std::vector<JourneyEventRecord> new_events;
};

struct FlagBaselineTransitionResult {
    FlagJourneyBaseline baseline;
    FlagBaselineCandidate candidate;
    std::vector<JourneyEventRecord> new_events;
};

IdSetBaselineTransitionResult TransitionIdSetJourneyBaseline(
    const RawIdSetFamilyObservation& observation,
    const IdSetJourneyBaseline& previous_baseline,
    const IdSetBaselineCandidate& previous_candidate,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view kind,
    JourneyUnlockIdKind id_kind,
    std::string_view observed_at_utc);

FlagBaselineTransitionResult TransitionHardModeJourneyBaseline(
    const RawFlagFamilyObservation& observation,
    const FlagJourneyBaseline& previous_baseline,
    const FlagBaselineCandidate& previous_candidate,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

} // namespace QuestProgress
