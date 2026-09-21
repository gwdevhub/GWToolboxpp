#include <Modules/QuestMissionSnapshot.h>

#include <Modules/QuestProgressDomain.h>

#include <algorithm>
#include <vector>

namespace QuestProgress {
namespace {

bool SameFlags(const MissionRecord& a, const MissionRecord& b)
{
    return a.completed_normal == b.completed_normal
        && a.completed_hard == b.completed_hard
        && a.bonus_normal == b.bonus_normal
        && a.bonus_hard == b.bonus_hard;
}

uint32_t MaxMapIndex(
    const MissionBitsetWords& completed_normal,
    const MissionBitsetWords& bonus_normal,
    const MissionBitsetWords& completed_hard,
    const MissionBitsetWords& bonus_hard)
{
    const size_t max_words = std::max({
        completed_normal.word_count,
        bonus_normal.word_count,
        completed_hard.word_count,
        bonus_hard.word_count,
    });
    if (max_words == 0) {
        return 0;
    }
    return static_cast<uint32_t>(max_words * 32);
}

} // namespace

bool MissionBitAt(const MissionBitsetWords& bitset, uint32_t map_id)
{
    if (!bitset.words || bitset.word_count == 0) {
        return false;
    }
    const auto word_index = map_id / 32;
    if (word_index >= bitset.word_count) {
        return false;
    }
    const auto bit_index = map_id % 32;
    return (bitset.words[word_index] & (1u << bit_index)) != 0;
}

std::vector<uint32_t> CollectSetBitMapIds(const MissionBitsetWords& bitset)
{
    std::vector<uint32_t> out;
    if (!bitset.words || bitset.word_count == 0) {
        return out;
    }
    const auto max_map = static_cast<uint32_t>(bitset.word_count * 32);
    out.reserve(32);
    for (uint32_t map_id = 0; map_id < max_map; ++map_id) {
        if (MissionBitAt(bitset, map_id)) {
            out.push_back(map_id);
        }
    }
    return out;
}

std::map<uint32_t, MissionRecord> BuildMissionRecordsFromBitsets(
    const MissionBitsetWords& completed_normal,
    const MissionBitsetWords& bonus_normal,
    const MissionBitsetWords& completed_hard,
    const MissionBitsetWords& bonus_hard,
    std::string_view observed_at_utc,
    const std::map<uint32_t, MissionRecord>* previous)
{
    std::map<uint32_t, MissionRecord> out;
    const auto max_map = MaxMapIndex(completed_normal, bonus_normal, completed_hard, bonus_hard);
    for (uint32_t map_id = 0; map_id < max_map; ++map_id) {
        MissionRecord row;
        row.map_id = map_id;
        row.completed_normal = MissionBitAt(completed_normal, map_id);
        row.bonus_normal = MissionBitAt(bonus_normal, map_id);
        row.completed_hard = MissionBitAt(completed_hard, map_id);
        row.bonus_hard = MissionBitAt(bonus_hard, map_id);
        if (!row.completed_normal && !row.bonus_normal && !row.completed_hard && !row.bonus_hard) {
            continue;
        }

        row.last_observed_at = std::string(observed_at_utc);
        if (previous) {
            const auto it = previous->find(map_id);
            if (it != previous->end() && SameFlags(it->second, row)) {
                row.last_observed_at = it->second.last_observed_at;
            }
        }
        out.emplace(map_id, std::move(row));
    }
    return out;
}

bool MergeMissionRecords(
    std::map<uint32_t, MissionRecord>& into,
    const std::map<uint32_t, MissionRecord>& incoming)
{
    bool changed = false;
    for (const auto& [map_id, row] : incoming) {
        const auto it = into.find(map_id);
        if (it == into.end()) {
            into.emplace(map_id, row);
            changed = true;
            continue;
        }
        if (SameFlags(it->second, row)) {
            continue;
        }
        it->second = row;
        changed = true;
    }
    return changed;
}

} // namespace QuestProgress
