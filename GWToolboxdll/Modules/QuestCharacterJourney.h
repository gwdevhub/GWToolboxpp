#pragma once

// Pure title tier + level milestone detection (no GWCA).

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
};

struct JourneyEventRecord {
    std::string kind; // title_tier | level_up
    std::string subject_key;
    std::string observed_at;
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t level = 0;
};

struct TitleSnapshotInput {
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t current_points = 0;
};

struct JourneySnapshotResult {
    std::map<uint32_t, TitleStateRecord> titles;
    std::optional<uint32_t> level;
    std::vector<JourneyEventRecord> new_events;
};

std::string BuildTitleSubjectKey(uint32_t title_id);
std::string BuildLevelSubjectKey(uint32_t level);
std::string BuildJourneyEventFingerprint(const JourneyEventRecord& event);

// Detects tier increases and level-ups vs previous snapshot; appends deduped events.
JourneySnapshotResult MergeJourneySnapshot(
    const std::map<uint32_t, TitleStateRecord>& previous_titles,
    std::optional<uint32_t> previous_level,
    const std::vector<JourneyEventRecord>& existing_events,
    const std::vector<TitleSnapshotInput>& title_inputs,
    uint32_t current_level,
    std::string_view observed_at_utc);

void AppendUniqueJourneyEvents(
    std::vector<JourneyEventRecord>& into,
    const std::vector<JourneyEventRecord>& incoming);

} // namespace QuestProgress
