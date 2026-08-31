#include <Modules/QuestCharacterJourney.h>

#include <Modules/QuestProgressDomain.h>

#include <format>
#include <set>

namespace QuestProgress {
namespace {

bool HasEventFingerprint(const std::vector<JourneyEventRecord>& events, const std::string& fingerprint)
{
    for (const auto& ev : events) {
        if (BuildJourneyEventFingerprint(ev) == fingerprint) {
            return true;
        }
    }
    return false;
}

} // namespace

std::string BuildTitleSubjectKey(uint32_t title_id)
{
    return std::format("title:{}", title_id);
}

std::string BuildLevelSubjectKey(uint32_t level)
{
    return std::format("level:{}", level);
}

std::string BuildJourneyEventFingerprint(const JourneyEventRecord& event)
{
    if (event.kind == "title_tier") {
        return std::format("title_tier:{}:{}", event.title_id, event.tier_index);
    }
    if (event.kind == "level_up") {
        return std::format("level_up:{}", event.level);
    }
    return std::format("{}:{}", event.kind, event.subject_key);
}

JourneySnapshotResult MergeJourneySnapshot(
    const std::map<uint32_t, TitleStateRecord>& previous_titles,
    std::optional<uint32_t> previous_level,
    const std::vector<JourneyEventRecord>& existing_events,
    const std::vector<TitleSnapshotInput>& title_inputs,
    uint32_t current_level,
    std::string_view observed_at_utc)
{
    JourneySnapshotResult out;
    out.level = current_level > 0 ? std::optional<uint32_t>{current_level} : std::nullopt;

    for (const auto& input : title_inputs) {
        if (input.title_id == 0) {
            continue;
        }
        TitleStateRecord row;
        row.title_id = input.title_id;
        row.tier_index = input.tier_index;
        row.current_points = input.current_points;
        row.last_observed_at = std::string(observed_at_utc);

        const auto prev_it = previous_titles.find(input.title_id);
        if (prev_it != previous_titles.end()) {
            if (prev_it->second.tier_index == row.tier_index
                && prev_it->second.current_points == row.current_points) {
                row.last_observed_at = prev_it->second.last_observed_at;
            }
            else if (row.tier_index > prev_it->second.tier_index) {
                JourneyEventRecord ev;
                ev.kind = "title_tier";
                ev.title_id = row.title_id;
                ev.tier_index = row.tier_index;
                ev.subject_key = BuildTitleSubjectKey(row.title_id);
                ev.observed_at = row.last_observed_at;
                const auto fp = BuildJourneyEventFingerprint(ev);
                if (!HasEventFingerprint(existing_events, fp)) {
                    out.new_events.push_back(std::move(ev));
                }
            }
        }
        out.titles.emplace(row.title_id, std::move(row));
    }

    if (current_level > 0 && previous_level.has_value() && current_level > *previous_level) {
        for (uint32_t lvl = *previous_level + 1; lvl <= current_level; ++lvl) {
            JourneyEventRecord ev;
            ev.kind = "level_up";
            ev.level = lvl;
            ev.subject_key = BuildLevelSubjectKey(lvl);
            ev.observed_at = std::string(observed_at_utc);
            const auto fp = BuildJourneyEventFingerprint(ev);
            if (!HasEventFingerprint(existing_events, fp)) {
                out.new_events.push_back(std::move(ev));
            }
        }
    }

    return out;
}

void AppendUniqueJourneyEvents(
    std::vector<JourneyEventRecord>& into,
    const std::vector<JourneyEventRecord>& incoming)
{
    std::set<std::string> seen;
    for (const auto& ev : into) {
        seen.insert(BuildJourneyEventFingerprint(ev));
    }
    for (const auto& ev : incoming) {
        const auto fp = BuildJourneyEventFingerprint(ev);
        if (seen.insert(fp).second) {
            into.push_back(ev);
        }
    }
}

} // namespace QuestProgress
