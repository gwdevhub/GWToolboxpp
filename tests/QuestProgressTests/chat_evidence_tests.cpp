#include <Modules/QuestChatEvidence.h>
#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>

#include "test_assert.h"

#include <string>
#include <utility>
#include <vector>

using namespace QuestProgress;

namespace {

constexpr wchar_t kSampleQuestName[] = {0x22D9, 0xE7B8, 0xE9DD, 0x2322, 0};

std::wstring MakeQuestChatMessage(wchar_t template_id)
{
    std::wstring message;
    message.push_back(template_id);
    message.push_back(0x10A);
    message.append(kSampleQuestName);
    message.push_back(0x1);
    return message;
}

CharacterProgress EmptyCharacter(const std::string& key)
{
    CharacterProgress character;
    character.character_key = key;
    return character;
}

LiveQuestObservation MakeObs(uint32_t id, bool ready = false)
{
    LiveQuestObservation obs;
    obs.game_quest_id = id;
    obs.in_log_completed = ready;
    return obs;
}

ReducerInput BaseInput(CharacterProgress previous, std::string at = "2026-07-25T20:00:00.000Z")
{
    ReducerInput input;
    input.previous = std::move(previous);
    input.observed_at_utc = std::move(at);
    return input;
}

void TestClassifyAndExtract()
{
    const auto reward_msg = MakeQuestChatMessage(QuestChatEvidence::kRewardAcceptedStringId);
    Expect(QuestChatEvidence::Classify(reward_msg.c_str()) == QuestChatEvidence::MessageKind::RewardAccepted,
        "chat_classify_reward");
    const auto updated_msg = MakeQuestChatMessage(QuestChatEvidence::kQuestUpdatedStringId);
    Expect(QuestChatEvidence::Classify(updated_msg.c_str()) == QuestChatEvidence::MessageKind::QuestUpdated,
        "chat_classify_updated");
    Expect(QuestChatEvidence::Classify(L"hello") == QuestChatEvidence::MessageKind::None, "chat_classify_none");

    const auto* extracted = QuestChatEvidence::ExtractQuestNameArgument(reward_msg.c_str());
    Expect(extracted != nullptr, "chat_extract_name");
    Expect(QuestChatEvidence::EncodedSegmentsEqual(extracted, kSampleQuestName), "chat_extract_name_match");

    const std::vector<std::pair<uint32_t, std::wstring>> log{
        {42, std::wstring(kSampleQuestName)},
        {99, L"\x8101\x730E"},
    };
    Expect(QuestChatEvidence::ResolveQuestIdByEncodedName(extracted, log) == 42, "chat_resolve_id");
    Expect(QuestChatEvidence::ResolveQuestIdByEncodedName(extracted, {}) == 0, "chat_resolve_missing");
}

void TestAcceptedEvidenceSetsAcceptedAt()
{
    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(100));
    input.evidence.push_back({100, EvidenceKind::Accepted});
    const auto out = Reduce(input);
    Expect(out.next.quests.at(100).accepted_at == input.observed_at_utc, "accepted_at_set");
    Expect(out.next.quests.at(100).history.size() == 2, "accepted_history_appended");
    Expect(out.next.quests.at(100).history.back().evidence_kind == EvidenceKind::Accepted, "accepted_history_kind");
    Expect(out.semantic_changed, "accepted_semantic_dirty");
}

void TestChatUpdatedEvidenceHistory()
{
    CharacterProgress prev = EmptyCharacter("acct/char-a");
    ::QuestProgress::QuestProgress existing;
    existing.game_quest_id = 100;
    existing.state = ProgressState::Active;
    existing.source = ProgressSource::GameSnapshot;
    existing.confidence = Confidence::Confirmed;
    existing.first_observed_at = "2026-07-25T19:00:00.000Z";
    existing.last_observed_at = existing.first_observed_at;
    prev.quests.emplace(100, existing);

    auto input = BaseInput(prev, "2026-07-25T20:00:00.000Z");
    input.observed_quests.push_back(MakeObs(100));
    input.evidence.push_back({100, EvidenceKind::ChatUpdated});
    const auto out = Reduce(input);
    Expect(out.next.quests.at(100).history.size() == 1, "chat_updated_history");
    Expect(out.next.quests.at(100).history.back().evidence_kind == EvidenceKind::ChatUpdated, "chat_updated_kind");
    Expect(out.next.quests.at(100).history.back().source == ProgressSource::GameEvent, "chat_updated_source");
}

void TestChatRewardMatchesRewardPath()
{
    CharacterProgress prev = EmptyCharacter("acct/char-a");
    ::QuestProgress::QuestProgress existing;
    existing.game_quest_id = 100;
    existing.state = ProgressState::ReadyForReward;
    existing.source = ProgressSource::GameSnapshot;
    existing.confidence = Confidence::Confirmed;
    existing.first_observed_at = "2026-07-25T19:00:00.000Z";
    existing.last_observed_at = existing.first_observed_at;
    prev.quests.emplace(100, existing);
    prev.last_reduced_at = existing.last_observed_at;

    auto input = BaseInput(prev, "2026-07-25T20:00:00.000Z");
    input.evidence.push_back({100, EvidenceKind::ChatReward});
    const auto out = Reduce(input);
    Expect(out.next.quests.at(100).state == ProgressState::CompletedObserved, "chat_reward_complete");
    Expect(out.next.quests.at(100).completed_at == input.observed_at_utc, "chat_reward_completed_at");
}

} // namespace

void RunChatEvidenceTests()
{
    TestClassifyAndExtract();
    TestAcceptedEvidenceSetsAcceptedAt();
    TestChatUpdatedEvidenceHistory();
    TestChatRewardMatchesRewardPath();
}
