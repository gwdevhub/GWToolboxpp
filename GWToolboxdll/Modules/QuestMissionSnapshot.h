#pragma once

// Pure mission completion bitset projection (no GWCA).

#include <Modules/QuestProgressJsonCodec.h>

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace QuestProgress {

struct MissionBitsetWords {
    const uint32_t* words = nullptr;
    size_t word_count = 0;
};

bool MissionBitAt(const MissionBitsetWords& bitset, uint32_t map_id);

std::vector<uint32_t> CollectSetBitMapIds(const MissionBitsetWords& bitset);

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
