#include <Modules/QuestChatEvidence.h>

#include <cwchar>

namespace QuestChatEvidence {
namespace {

const wchar_t* SegmentAfterMarker(const wchar_t* encoded_string, wchar_t marker, size_t* segment_length)
{
    if (!encoded_string) {
        return nullptr;
    }
    const auto* found = wcschr(encoded_string, marker);
    if (!found) {
        return nullptr;
    }
    ++found;
    if (segment_length) {
        *segment_length = EncodedSegmentLength(found);
    }
    return found;
}

} // namespace

size_t EncodedSegmentLength(const wchar_t* encoded_segment)
{
    if (!encoded_segment || *encoded_segment <= 0x100) {
        return 0;
    }
    size_t length = 0;
    do {
        ++length;
    } while (*encoded_segment++ & 0x8000);
    return length;
}

MessageKind Classify(const wchar_t* message)
{
    if (!message || !*message) {
        return MessageKind::None;
    }
    switch (message[0]) {
        case kRewardAcceptedStringId:
            return MessageKind::RewardAccepted;
        case kQuestUpdatedStringId:
            return MessageKind::QuestUpdated;
        default:
            return MessageKind::None;
    }
}

const wchar_t* ExtractQuestNameArgument(const wchar_t* message)
{
    if (!message || !*message) {
        return nullptr;
    }
    // Template id is message[0]; quest name is the first 0x10A string argument.
    return SegmentAfterMarker(message + 1, 0x10a, nullptr);
}

bool EncodedSegmentsEqual(const wchar_t* a, const wchar_t* b)
{
    const auto len_a = EncodedSegmentLength(a);
    const auto len_b = EncodedSegmentLength(b);
    if (len_a == 0 || len_a != len_b) {
        return false;
    }
    return wcsncmp(a, b, len_a) == 0;
}

uint32_t ResolveQuestIdByEncodedName(
    const wchar_t* name_argument,
    const std::vector<std::pair<uint32_t, std::wstring>>& quest_log_names)
{
    if (!name_argument || !*name_argument) {
        return 0;
    }
    for (const auto& [quest_id, encoded_name] : quest_log_names) {
        if (quest_id == 0) {
            continue;
        }
        if (EncodedSegmentsEqual(name_argument, encoded_name.c_str())) {
            return quest_id;
        }
    }
    return 0;
}

} // namespace QuestChatEvidence
