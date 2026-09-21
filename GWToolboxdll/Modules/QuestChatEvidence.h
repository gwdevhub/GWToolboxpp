#pragma once

// Pure quest chat message classification and name extraction (no GWCA).

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace QuestChatEvidence {

enum class MessageKind : uint8_t {
    None = 0,
    RewardAccepted, // GW string id 0x7C8 — "Quest Reward Accepted: <quest>"
    QuestUpdated,   // GW string id 0x7C9 — "Quest Updated: <quest>"
};

inline constexpr wchar_t kRewardAcceptedStringId = 0x7C8;
inline constexpr wchar_t kQuestUpdatedStringId = 0x7C9;

MessageKind Classify(const wchar_t* message);

// Encoded quest-name argument after the leading template string id (0x10A segment).
const wchar_t* ExtractQuestNameArgument(const wchar_t* message);

size_t EncodedSegmentLength(const wchar_t* encoded_segment);

bool EncodedSegmentsEqual(const wchar_t* a, const wchar_t* b);

// Match encoded quest log names (id + encoded name pairs).
uint32_t ResolveQuestIdByEncodedName(
    const wchar_t* name_argument,
    const std::vector<std::pair<uint32_t, std::wstring>>& quest_log_names);

} // namespace QuestChatEvidence
