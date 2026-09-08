#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace QuestProgress {

struct TitleStateRecord {
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t current_points = 0;
    std::string last_observed_at;

    friend bool operator==(const TitleStateRecord& a, const TitleStateRecord& b)
    {
        return a.title_id == b.title_id
            && a.tier_index == b.tier_index
            && a.current_points == b.current_points
            && a.last_observed_at == b.last_observed_at;
    }
};

struct JourneyEventRecord {
    std::string kind;
    std::string subject_key;
    std::string observed_at;
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t level = 0;
    uint32_t map_id = 0;
    uint32_t skill_id = 0;
    uint32_t hero_id = 0;
    uint32_t profession_id = 0;
    uint32_t percent = 0;
    uint32_t amount = 0;
};

struct TitleSnapshotInput {
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t current_points = 0;
};

struct HomSnapshotRecord {
    std::string hom_code;
    std::string observed_at;
    uint32_t resilience_points = 0;
    uint32_t fellowship_points = 0;
    uint32_t honor_points = 0;
    uint32_t valor_points = 0;
    uint32_t devotion_points = 0;
    std::vector<uint8_t> resilience_dedicated;
    std::vector<uint8_t> fellowship_dedicated;
    std::vector<uint8_t> honor_dedicated;
    std::vector<uint8_t> valor_dedicated;
    std::vector<uint32_t> devotion_counts;

    friend bool operator==(const HomSnapshotRecord& a, const HomSnapshotRecord& b)
    {
        return a.hom_code == b.hom_code
            && a.resilience_points == b.resilience_points
            && a.fellowship_points == b.fellowship_points
            && a.honor_points == b.honor_points
            && a.valor_points == b.valor_points
            && a.devotion_points == b.devotion_points
            && a.resilience_dedicated == b.resilience_dedicated
            && a.fellowship_dedicated == b.fellowship_dedicated
            && a.honor_dedicated == b.honor_dedicated
            && a.valor_dedicated == b.valor_dedicated
            && a.devotion_counts == b.devotion_counts;
    }
};

struct FactionTotalsRecord {
    uint32_t kurzick = 0;
    uint32_t luxon = 0;
    uint32_t balthazar = 0;
    uint32_t imperial = 0;

    friend bool operator==(const FactionTotalsRecord& a, const FactionTotalsRecord& b)
    {
        return a.kurzick == b.kurzick
            && a.luxon == b.luxon
            && a.balthazar == b.balthazar
            && a.imperial == b.imperial;
    }
};

struct JourneySnapshotResult {
    std::map<uint32_t, TitleStateRecord> titles;
    std::optional<uint32_t> level;
    std::optional<uint32_t> observed_map_id;
    std::optional<uint32_t> experience_total;
    std::optional<uint32_t> skill_points_earned;
    std::optional<FactionTotalsRecord> faction_totals;
    std::vector<JourneyEventRecord> new_events;
};

enum class JourneyUnlockIdKind : uint8_t {
    Map,
    Skill,
    Hero,
    Profession,
};

std::string BuildTitleSubjectKey(uint32_t title_id);
std::string BuildLevelSubjectKey(uint32_t level);
std::string BuildMapSubjectKey(uint32_t map_id);
std::string BuildVanquishSubjectKey(uint32_t map_id);
std::string BuildSkillSubjectKey(uint32_t skill_id);
std::string BuildHeroSubjectKey(uint32_t hero_id);
std::string BuildProfessionSubjectKey(uint32_t profession_id);
std::string BuildHardModeSubjectKey();
std::string BuildHomPointsSubjectKey(std::string_view category);
std::string BuildJourneyEventFingerprint(const JourneyEventRecord& event);

JourneySnapshotResult MergeJourneySnapshot(
    const std::map<uint32_t, TitleStateRecord>& previous_titles,
    std::optional<uint32_t> previous_level,
    const std::vector<JourneyEventRecord>& existing_events,
    const std::vector<TitleSnapshotInput>& title_inputs,
    uint32_t current_level,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildMapEnterEvents(
    std::optional<uint32_t> previous_map_id,
    uint32_t current_map_id,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildVanquishAreaEvents(
    const std::map<uint32_t, bool>& previous_vanquished,
    const std::vector<uint32_t>& currently_vanquished_map_ids,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildNewlySeenIdEvents(
    std::string_view kind,
    JourneyUnlockIdKind id_kind,
    const std::map<uint32_t, bool>& previous_seen,
    const std::vector<uint32_t>& current_ids,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildHardModeUnlockEvents(
    bool previously_unlocked_or_recorded,
    bool currently_unlocked,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

uint32_t ComputeCartographyCoveragePercent(
    const uint32_t* bits,
    size_t dword_count,
    uint32_t width,
    uint32_t height);

std::vector<JourneyEventRecord> BuildCartographyThresholdEvents(
    uint32_t previous_max_percent,
    uint32_t current_percent,
    uint32_t current_map_id,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildTimedMapClearEvents(
    std::string_view kind,
    uint32_t map_id,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildAbsoluteThresholdEvents(
    std::string_view kind,
    std::string_view subject_prefix,
    uint32_t previous_max_amount,
    uint32_t current_amount,
    const std::vector<uint32_t>& thresholds,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

std::vector<JourneyEventRecord> BuildHomPointsEvents(
    const std::optional<HomSnapshotRecord>& previous,
    const HomSnapshotRecord& current,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc);

uint32_t MaxCartographyPercentFromEvents(const std::vector<JourneyEventRecord>& events);

uint32_t MaxAmountFromJourneyEvents(
    const std::vector<JourneyEventRecord>& events,
    std::string_view kind,
    std::string_view subject_prefix);

std::map<uint32_t, bool> PriorIdsFromJourneyEvents(
    const std::vector<JourneyEventRecord>& events,
    std::string_view kind);

bool HasJourneyKind(const std::vector<JourneyEventRecord>& events, std::string_view kind);

void AppendUniqueJourneyEvents(
    std::vector<JourneyEventRecord>& into,
    const std::vector<JourneyEventRecord>& incoming);

} // namespace QuestProgress
