#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace QuestProgress;

namespace {

int g_failed = 0;
int g_passed = 0;

void Expect(bool condition, const char* name)
{
    if (condition) {
        ++g_passed;
        std::printf("PASS %s\n", name);
    }
    else {
        ++g_failed;
        std::printf("FAIL %s\n", name);
    }
}

CharacterProgress EmptyCharacter(const std::string& key, const std::string& display = {})
{
    CharacterProgress character;
    character.character_key = key;
    character.display_name = display;
    return character;
}

LiveQuestObservation MakeObs(
    uint32_t id,
    bool ready = false,
    std::vector<ObjectiveObservation> objectives = {},
    bool missing = false)
{
    LiveQuestObservation obs;
    obs.game_quest_id = id;
    obs.in_log_completed = ready;
    obs.objectives = std::move(objectives);
    obs.objectives_missing = missing;
    return obs;
}

ObjectiveObservation MakeObj(uint32_t index, bool completed, const char16_t* text)
{
    ObjectiveObservation obj;
    obj.index = index;
    obj.completed = completed;
    obj.encoded_content = text;
    return obj;
}

ReducerInput BaseInput(CharacterProgress previous, std::string at = "2026-07-25T20:00:00.000Z")
{
    ReducerInput input;
    input.previous = std::move(previous);
    input.observed_at_utc = std::move(at);
    return input;
}

bool NoReserved(const CharacterProgress& character)
{
    for (const auto& [id, quest] : character.quests) {
        (void)id;
        if (IsReservedState(quest.state)) {
            return false;
        }
        for (const auto& event : quest.history) {
            if (IsReservedState(event.state)) {
                return false;
            }
        }
    }
    return true;
}

void TestFirstObservation()
{
    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input);
    Expect(out.next.quests.count(100) == 1, "first_observe_creates");
    Expect(out.next.quests.at(100).state == ProgressState::Active, "first_observe_active");
    Expect(out.next.quests.at(100).source == ProgressSource::GameSnapshot, "first_observe_source");
    Expect(out.next.quests.at(100).confidence == Confidence::Confirmed, "first_observe_confidence");
    Expect(out.next.quests.at(100).first_observed_at == input.observed_at_utc, "first_observe_first_at");
    Expect(out.next.quests.at(100).last_observed_at == input.observed_at_utc, "first_observe_last_at");
    Expect(out.appended.size() == 1, "first_observe_one_history");
    Expect(out.semantic_changed, "first_observe_semantic_dirty");
    Expect(NoReserved(out.next), "first_observe_no_reserved");
}

void TestDuplicateIdempotent()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:01:00.000Z");
    input2.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input2);
    Expect(out.appended.empty(), "dup_no_history");
    Expect(!out.semantic_changed, "dup_no_semantic_dirty");
    Expect(out.touch_last_observed, "dup_touch_last_observed");
    Expect(out.next.quests.at(100).history.size() == 1, "dup_history_size_one");
    Expect(out.next.quests.at(100).last_observed_at == "2026-07-25T20:01:00.000Z", "dup_last_at_updated");
}

void TestObjectiveFingerprintChange()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, false, {MakeObj(0, false, u"Kill rats")}));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:02:00.000Z");
    // Same completion flag, different encoded content → new semantic event.
    input2.observed_quests.push_back(MakeObs(100, false, {MakeObj(0, false, u"Kill more rats")}));
    const auto out = Reduce(input2);
    Expect(out.appended.size() == 1, "obj_text_change_one_append");
    Expect(out.next.quests.at(100).state == ProgressState::ObjectiveProgress, "obj_text_state");
    Expect(out.next.quests.at(100).history.size() == 2, "obj_text_history_len");
    Expect(
        out.next.quests.at(100).history[0].semantic_event_key
            != out.next.quests.at(100).history[1].semantic_event_key,
        "obj_text_keys_differ");
}

void TestReadyForReward()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:03:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, true));
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::ReadyForReward, "ready_state");
    Expect(out.next.quests.at(100).confidence == Confidence::Confirmed, "ready_confidence");
    Expect(out.appended.size() == 1, "ready_one_append");

    auto input3 = BaseInput(out.next, "2026-07-25T20:04:00.000Z");
    input3.observed_quests.push_back(MakeObs(100, true));
    const auto again = Reduce(input3);
    Expect(again.appended.empty(), "ready_idempotent");
}

void TestBareDisappearance()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:05:00.000Z");
    // empty observed_quests
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "bare_remove_unknown");
    Expect(out.next.quests.at(100).confidence == Confidence::Uncertain, "bare_remove_uncertain");
    Expect(!out.next.quests.at(100).completed_at.has_value(), "bare_remove_no_completed_at");
    Expect(out.appended.size() == 1, "bare_remove_one_history");
    Expect(out.appended[0].event_type == HistoryEventType::PresenceLost, "bare_remove_presence_lost");
}

void TestRepeatedDisappearanceIdempotent()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:05:00.000Z");
    auto lost = Reduce(input2);

    auto input3 = BaseInput(lost.next, "2026-07-25T20:06:00.000Z");
    const auto again = Reduce(input3);
    Expect(again.appended.empty(), "repeat_missing_no_append");
    Expect(!again.semantic_changed, "repeat_missing_no_semantic");
    Expect(again.touch_last_observed, "repeat_missing_touch");
    Expect(again.next.quests.at(100).history.size() == 2, "repeat_missing_history_stable");
}

void TestExactAbandon()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:07:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Abandon});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::AbandonedObserved, "abandon_state");
    Expect(out.next.quests.at(100).source == ProgressSource::GameEvent, "abandon_source");
    Expect(out.next.quests.at(100).confidence == Confidence::Probable, "abandon_confidence");
    Expect(out.appended.size() == 1, "abandon_one_append");
}

void TestNonMatchingAbandon()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    input1.observed_quests.push_back(MakeObs(200));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:08:00.000Z");
    input2.observed_quests.push_back(MakeObs(200)); // 100 disappears
    input2.evidence.push_back({200, EvidenceKind::Abandon}); // evidence for other quest
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "mismatch_abandon_unknown");
    Expect(out.next.quests.at(100).confidence == Confidence::Uncertain, "mismatch_abandon_uncertain");
    Expect(out.next.quests.at(100).state != ProgressState::AbandonedObserved, "mismatch_not_abandoned");
}

void TestExactReward()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:09:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Reward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::CompletedObserved, "reward_state");
    Expect(out.next.quests.at(100).source == ProgressSource::GameEvent, "reward_source");
    Expect(out.next.quests.at(100).confidence == Confidence::Probable, "reward_confidence");
    Expect(out.next.quests.at(100).completed_at == input2.observed_at_utc, "reward_completed_at");
}

void TestEnquireRewardRejected()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:10:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::EnquireReward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "enquire_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "enquire_not_completed");
    Expect(!out.next.quests.at(100).completed_at.has_value(), "enquire_no_completed_at");
}

void TestNonMatchingReward()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    input1.observed_quests.push_back(MakeObs(200, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:11:00.000Z");
    input2.observed_quests.push_back(MakeObs(200, true)); // 100 disappears
    input2.evidence.push_back({200, EvidenceKind::Reward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "mismatch_reward_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "mismatch_reward_not_complete");
}

void TestOfflineGap()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-26T10:00:00.000Z");
    input2.session_gap = true;
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "gap_unknown");
    Expect(out.next.quests.at(100).confidence == Confidence::Uncertain, "gap_uncertain");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "gap_not_completed");
    Expect(out.next.quests.at(100).state != ProgressState::AbandonedObserved, "gap_not_abandoned");
}

void TestCharacterIsolation()
{
    auto a1 = BaseInput(EmptyCharacter("acct/char-a", "Hero"));
    a1.observed_quests.push_back(MakeObs(100));
    auto a = Reduce(a1);

    auto b1 = BaseInput(EmptyCharacter("acct/char-b", "Hero")); // same display name
    b1.observed_quests.push_back(MakeObs(200));
    auto b = Reduce(b1);

    Expect(a.next.character_key == "acct/char-a", "iso_key_a");
    Expect(b.next.character_key == "acct/char-b", "iso_key_b");
    Expect(a.next.quests.count(100) == 1 && a.next.quests.count(200) == 0, "iso_a_quests");
    Expect(b.next.quests.count(200) == 1 && b.next.quests.count(100) == 0, "iso_b_quests");
    Expect(a.next.display_name == "Hero" && b.next.display_name == "Hero", "iso_same_display_ok");
}

void TestHistoryOrdering()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:05:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, false, {MakeObj(0, true, u"Done")}));
    auto out = Reduce(input2);

    Expect(out.next.quests.at(100).history.size() == 2, "order_len");
    Expect(out.next.quests.at(100).history[0].observed_at < out.next.quests.at(100).history[1].observed_at,
        "order_timestamps");
}

void TestSemanticKeyStableAndNoTimestamp()
{
    const auto objectives = std::vector{MakeObj(0, false, u"X")};
    const auto k1 = BuildSemanticEventKey(
        100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        ProgressSource::GameSnapshot, Confidence::Confirmed, objectives, EvidenceKind::None);
    const auto k2 = BuildSemanticEventKey(
        100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        ProgressSource::GameSnapshot, Confidence::Confirmed, objectives, EvidenceKind::None);
    Expect(k1 == k2, "semantic_key_stable");
    Expect(k1.size() == 16, "semantic_key_hex64");

    // Timestamp is not an input — keys match across different observation times by construction.
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100, false, objectives));
    auto a = Reduce(input1);
    auto input2 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T21:00:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, false, objectives));
    auto b = Reduce(input2);
    Expect(
        a.next.quests.at(100).history[0].semantic_event_key
            == b.next.quests.at(100).history[0].semantic_event_key,
        "semantic_key_excludes_timestamp");
}

void TestNoSelectedActiveQuestHistory()
{
    // ReducerInput has no selected_active_quest field; selection cannot create history.
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    input1.observed_quests.push_back(MakeObs(200));
    auto mid = Reduce(input1);
    const auto hist_len = mid.next.quests.at(100).history.size()
        + mid.next.quests.at(200).history.size();

    auto input2 = BaseInput(mid.next, "2026-07-25T20:12:00.000Z");
    input2.observed_quests.push_back(MakeObs(100));
    input2.observed_quests.push_back(MakeObs(200));
    const auto out = Reduce(input2);
    Expect(out.appended.empty(), "selection_not_in_input_no_append");
    Expect(
        out.next.quests.at(100).history.size() + out.next.quests.at(200).history.size() == hist_len,
        "selection_history_unchanged");
}

void TestReservedStateInvariants()
{
    Expect(IsReservedState(ProgressState::Available), "reserved_available");
    Expect(IsReservedState(ProgressState::CompletedManual), "reserved_completed_manual");
    Expect(!IsReservedState(ProgressState::Active), "not_reserved_active");

    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(100));
    input.observed_quests.push_back(MakeObs(200, true));
    const auto out = Reduce(input);
    Expect(NoReserved(out.next), "reducer_never_emits_reserved");
}

void TestSyntheticFilter()
{
    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(kSyntheticCustomMarkerQuestId));
    input.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input);
    Expect(out.next.quests.count(kSyntheticCustomMarkerQuestId) == 0, "synthetic_ignored");
    Expect(out.next.quests.count(100) == 1, "synthetic_other_kept");
}

void TestStaleObservation()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:10:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:00:00.000Z"); // older
    input2.observed_quests.push_back(MakeObs(100, true));
    const auto out = Reduce(input2);
    Expect(out.diagnostics.rejected_stale, "stale_rejected");
    Expect(out.appended.empty(), "stale_no_append");
    Expect(out.next.quests.at(100).state == ProgressState::Active, "stale_state_unchanged");
    Expect(out.next.quests.at(100).history.size() == mid.next.quests.at(100).history.size(),
        "stale_history_not_reordered");
}

void TestRewardWithoutReadyIsUnknown()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, false)); // not ready
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:13:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Reward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "reward_not_ready_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "reward_not_ready_not_complete");
}

} // namespace

int main()
{
    TestFirstObservation();
    TestDuplicateIdempotent();
    TestObjectiveFingerprintChange();
    TestReadyForReward();
    TestBareDisappearance();
    TestRepeatedDisappearanceIdempotent();
    TestExactAbandon();
    TestNonMatchingAbandon();
    TestExactReward();
    TestEnquireRewardRejected();
    TestNonMatchingReward();
    TestOfflineGap();
    TestCharacterIsolation();
    TestHistoryOrdering();
    TestSemanticKeyStableAndNoTimestamp();
    TestNoSelectedActiveQuestHistory();
    TestReservedStateInvariants();
    TestSyntheticFilter();
    TestStaleObservation();
    TestRewardWithoutReadyIsUnknown();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
