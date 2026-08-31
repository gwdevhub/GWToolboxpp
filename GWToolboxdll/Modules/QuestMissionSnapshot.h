#pragma once

// Pure mission completion bitset projection (no GWCA).

#include <Modules/QuestProgressJsonCodec.h>

#include <cstdint>
#include <map>
#include <string_view>

namespace QuestProgress {

struct MissionBitsetWords {
    const uint32_t* words = nullptr;
    size_t word_count = 0;
};

bool MissionBitAt(const MissionBitsetWords& bitset, uint32_t map_id);

// Builds mapId → MissionRecord for any map with a completion/bonus flag set.
// Preserves prior lastObservedAt when flags are unchanged.
std::map<uint32_t, MissionRecord> BuildMissionRecordsFromBitsets(
    const MissionBitsetWords& completed_normal,
    const MissionBitsetWords& bonus_normal,
    const MissionBitsetWords& completed_hard,
    const MissionBitsetWords& bonus_hard,
    std::string_view observed_at_utc,
    const std::map<uint32_t, MissionRecord>* previous = nullptr);

bool MergeMissionRecords(
    std::map<uint32_t, MissionRecord>& into,
    const std::map<uint32_t, MissionRecord>& incoming);

} // namespace QuestProgress
