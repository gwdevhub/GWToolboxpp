#include <Modules/QuestCharacterJourney.h>

#include <Modules/QuestProgressDomain.h>

#include <bit>
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

void AssignUnlockId(JourneyEventRecord& ev, JourneyUnlockIdKind id_kind, uint32_t id)
{
    switch (id_kind) {
        case JourneyUnlockIdKind::Map:
            ev.map_id = id;
            ev.subject_key = BuildMapSubjectKey(id);
            break;
        case JourneyUnlockIdKind::Skill:
            ev.skill_id = id;
            ev.subject_key = BuildSkillSubjectKey(id);
            break;
        case JourneyUnlockIdKind::Hero:
            ev.hero_id = id;
            ev.subject_key = BuildHeroSubjectKey(id);
            break;
        case JourneyUnlockIdKind::Profession:
            ev.profession_id = id;
            ev.subject_key = BuildProfessionSubjectKey(id);
            break;
    }
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

std::string BuildMapSubjectKey(uint32_t map_id)
{
    return std::format("map:{}", map_id);
}

std::string BuildVanquishSubjectKey(uint32_t map_id)
{
    return std::format("vanquish:{}", map_id);
}

std::string BuildSkillSubjectKey(uint32_t skill_id)
{
    return std::format("skill:{}", skill_id);
}

std::string BuildHeroSubjectKey(uint32_t hero_id)
{
    return std::format("hero:{}", hero_id);
}

std::string BuildProfessionSubjectKey(uint32_t profession_id)
{
    return std::format("profession:{}", profession_id);
}

std::string BuildHardModeSubjectKey()
{
    return "hard_mode";
}

std::string BuildJourneyEventFingerprint(const JourneyEventRecord& event)
{
    if (event.kind == "title_tier") {
        return std::format("title_tier:{}:{}", event.title_id, event.tier_index);
    }
    if (event.kind == "level_up") {
        return std::format("level_up:{}", event.level);
    }
    if (event.kind == "map_enter") {
        return std::format("map_enter:{}:{}", event.map_id, event.observed_at);
    }
    if (event.kind == "vanquish_area") {
        return std::format("vanquish_area:{}", event.map_id);
    }
    if (event.kind == "map_unlock") {
        return std::format("map_unlock:{}", event.map_id);
    }
    if (event.kind == "skill_unlock") {
        return std::format("skill_unlock:{}", event.skill_id);
    }
    if (event.kind == "hero_unlock") {
        return std::format("hero_unlock:{}", event.hero_id);
    }
    if (event.kind == "profession_unlock") {
        return std::format("profession_unlock:{}", event.profession_id);
    }
    if (event.kind == "hard_mode_unlock") {
        return "hard_mode_unlock";
    }
    if (event.kind == "cartography_threshold") {
        return std::format("cartography_threshold:{}", event.percent);
    }
    if (event.kind == "dungeon_complete" || event.kind == "mission_complete") {
        return std::format("{}:{}:{}", event.kind, event.map_id, event.observed_at);
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

std::vector<JourneyEventRecord> BuildMapEnterEvents(
    std::optional<uint32_t> previous_map_id,
    uint32_t current_map_id,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    std::vector<JourneyEventRecord> out;
    if (current_map_id == 0) {
        return out;
    }
    if (previous_map_id.has_value() && *previous_map_id == current_map_id) {
        return out;
    }

    JourneyEventRecord ev;
    ev.kind = "map_enter";
    ev.map_id = current_map_id;
    ev.subject_key = BuildMapSubjectKey(current_map_id);
    ev.observed_at = std::string(observed_at_utc);
    const auto fp = BuildJourneyEventFingerprint(ev);
    if (!HasEventFingerprint(existing_events, fp)) {
        out.push_back(std::move(ev));
    }
    return out;
}

std::vector<JourneyEventRecord> BuildVanquishAreaEvents(
    const std::map<uint32_t, bool>& previous_vanquished,
    const std::vector<uint32_t>& currently_vanquished_map_ids,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    return BuildNewlySeenIdEvents(
        "vanquish_area",
        JourneyUnlockIdKind::Map,
        previous_vanquished,
        currently_vanquished_map_ids,
        existing_events,
        observed_at_utc);
}

std::vector<JourneyEventRecord> BuildNewlySeenIdEvents(
    std::string_view kind,
    JourneyUnlockIdKind id_kind,
    const std::map<uint32_t, bool>& previous_seen,
    const std::vector<uint32_t>& current_ids,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    std::vector<JourneyEventRecord> out;
    for (const uint32_t id : current_ids) {
        if (id == 0) {
            continue;
        }
        const auto prev = previous_seen.find(id);
        if (prev != previous_seen.end() && prev->second) {
            continue;
        }
        JourneyEventRecord ev;
        ev.kind = std::string(kind);
        AssignUnlockId(ev, id_kind, id);
        if (kind == "vanquish_area") {
            ev.subject_key = BuildVanquishSubjectKey(id);
        }
        ev.observed_at = std::string(observed_at_utc);
        const auto fp = BuildJourneyEventFingerprint(ev);
        if (!HasEventFingerprint(existing_events, fp)) {
            out.push_back(std::move(ev));
        }
    }
    return out;
}

std::vector<JourneyEventRecord> BuildHardModeUnlockEvents(
    bool previously_unlocked_or_recorded,
    bool currently_unlocked,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    std::vector<JourneyEventRecord> out;
    if (!currently_unlocked || previously_unlocked_or_recorded) {
        return out;
    }
    JourneyEventRecord ev;
    ev.kind = "hard_mode_unlock";
    ev.subject_key = BuildHardModeSubjectKey();
    ev.observed_at = std::string(observed_at_utc);
    const auto fp = BuildJourneyEventFingerprint(ev);
    if (!HasEventFingerprint(existing_events, fp)) {
        out.push_back(std::move(ev));
    }
    return out;
}

uint32_t ComputeCartographyCoveragePercent(
    const uint32_t* bits,
    size_t dword_count,
    uint32_t width,
    uint32_t height)
{
    if (!bits || dword_count == 0 || width == 0 || height == 0) {
        return 0;
    }
    const uint64_t total_bits = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (total_bits == 0) {
        return 0;
    }
    uint64_t explored = 0;
    for (size_t i = 0; i < dword_count; ++i) {
        explored += static_cast<uint64_t>(std::popcount(bits[i]));
    }
    if (explored == 0) {
        return 0;
    }
    if (explored >= total_bits) {
        return 100;
    }
    const auto pct = static_cast<uint32_t>((explored * 100ull + total_bits - 1) / total_bits);
    return pct == 0 ? 1u : pct;
}

std::vector<JourneyEventRecord> BuildCartographyThresholdEvents(
    uint32_t previous_max_percent,
    uint32_t current_percent,
    uint32_t current_map_id,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    static constexpr uint32_t kThresholds[] = {1, 10, 25, 50, 75, 90, 100};
    std::vector<JourneyEventRecord> out;
    if (current_percent == 0 || current_percent <= previous_max_percent) {
        return out;
    }
    for (const uint32_t threshold : kThresholds) {
        if (threshold <= previous_max_percent || threshold > current_percent) {
            continue;
        }
        JourneyEventRecord ev;
        ev.kind = "cartography_threshold";
        ev.percent = threshold;
        ev.map_id = current_map_id;
        ev.subject_key = std::format("cartography:{}", threshold);
        ev.observed_at = std::string(observed_at_utc);
        const auto fp = BuildJourneyEventFingerprint(ev);
        if (!HasEventFingerprint(existing_events, fp)) {
            out.push_back(std::move(ev));
        }
    }
    return out;
}

std::vector<JourneyEventRecord> BuildTimedMapClearEvents(
    std::string_view kind,
    uint32_t map_id,
    const std::vector<JourneyEventRecord>& existing_events,
    std::string_view observed_at_utc)
{
    std::vector<JourneyEventRecord> out;
    if (map_id == 0 || (kind != "dungeon_complete" && kind != "mission_complete")) {
        return out;
    }
    JourneyEventRecord ev;
    ev.kind = std::string(kind);
    ev.map_id = map_id;
    ev.subject_key = BuildMapSubjectKey(map_id);
    ev.observed_at = std::string(observed_at_utc);
    const auto fp = BuildJourneyEventFingerprint(ev);
    if (!HasEventFingerprint(existing_events, fp)) {
        out.push_back(std::move(ev));
    }
    return out;
}

uint32_t MaxCartographyPercentFromEvents(const std::vector<JourneyEventRecord>& events)
{
    uint32_t max_pct = 0;
    for (const auto& ev : events) {
        if (ev.kind == "cartography_threshold" && ev.percent > max_pct) {
            max_pct = ev.percent;
        }
    }
    return max_pct;
}

std::map<uint32_t, bool> PriorIdsFromJourneyEvents(
    const std::vector<JourneyEventRecord>& events,
    std::string_view kind)
{
    std::map<uint32_t, bool> out;
    for (const auto& ev : events) {
        if (ev.kind != kind) {
            continue;
        }
        uint32_t id = 0;
        if (kind == "vanquish_area" || kind == "map_unlock" || kind == "map_enter") {
            id = ev.map_id;
        }
        else if (kind == "skill_unlock") {
            id = ev.skill_id;
        }
        else if (kind == "hero_unlock") {
            id = ev.hero_id;
        }
        else if (kind == "profession_unlock") {
            id = ev.profession_id;
        }
        if (id != 0) {
            out[id] = true;
        }
    }
    return out;
}

bool HasJourneyKind(const std::vector<JourneyEventRecord>& events, std::string_view kind)
{
    for (const auto& ev : events) {
        if (ev.kind == kind) {
            return true;
        }
    }
    return false;
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
